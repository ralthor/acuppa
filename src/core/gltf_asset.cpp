#include "acuppa/gltf_asset.hpp"

#include <cgltf.h>
#include <stb_image.h>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace acuppa {
namespace fs = std::filesystem;
namespace {

fs::path inside_file(const fs::path& root, const fs::path& relative) {
    if (relative.empty() || relative.is_absolute())
        throw std::runtime_error("asset path must be project-relative");
    for (const auto& part : relative) {
        if (part == "..") throw std::runtime_error("asset path escapes project");
    }
    std::error_code error;
    const fs::path base = fs::canonical(root);
    const fs::path file = fs::weakly_canonical(base / relative, error);
    if (error || !fs::is_regular_file(file))
        throw std::runtime_error("asset file is missing: " + relative.string());
    const fs::path local = fs::relative(file, base, error);
    if (error || local.empty() || *local.begin() == "..")
        throw std::runtime_error("asset path escapes project: " + relative.string());
    if (fs::file_size(file) > 64 * 1024 * 1024)
        throw std::runtime_error("asset file exceeds 64 MiB limit");
    return file;
}

void require_result(cgltf_result result, const char* action) {
    if (result != cgltf_result_success)
        throw std::runtime_error(std::string(action) + " failed (cgltf result " +
                                 std::to_string(int(result)) + ")");
}

void check_accessor(const cgltf_accessor* accessor, const char* role) {
    if (!accessor || accessor->is_sparse || !accessor->buffer_view ||
        accessor->buffer_view->has_meshopt_compression || accessor->count == 0 ||
        accessor->count > 1000000) {
        throw std::runtime_error(std::string("unsupported or empty ") + role + " accessor");
    }
    const auto* view = accessor->buffer_view;
    const auto* buffer = view->buffer;
    const size_t element = cgltf_calc_size(accessor->type, accessor->component_type);
    const size_t stride = accessor->stride;
    if (!buffer || !buffer->data || !element || stride < element ||
        view->offset > buffer->size || view->size > buffer->size - view->offset ||
        accessor->offset > view->size || element > view->size - accessor->offset ||
        (accessor->count - 1) >
            (view->size - accessor->offset - element) / stride) {
        throw std::runtime_error(std::string("out-of-bounds ") + role + " accessor");
    }
}

std::vector<uint8_t> decode_data_uri(const std::string& uri) {
    const auto comma = uri.find(',');
    if (comma == std::string::npos || uri.substr(0, comma).find(";base64") == std::string::npos)
        throw std::runtime_error("image data URI must use base64");
    const std::string payload = uri.substr(comma + 1);
    if (payload.size() % 4 != 0 || payload.size() > 24 * 1024 * 1024)
        throw std::runtime_error("invalid or oversized image data URI");
    std::vector<uint8_t> bytes;
    bytes.reserve(payload.size() / 4 * 3);
    unsigned bits = 0;
    unsigned buffered = 0;
    bool padding = false;
    for (char ch : payload) {
        if (ch == '=') { padding = true; continue; }
        if (padding) throw std::runtime_error("invalid image base64 padding");
        int value = -1;
        if (ch >= 'A' && ch <= 'Z') value = ch - 'A';
        else if (ch >= 'a' && ch <= 'z') value = ch - 'a' + 26;
        else if (ch >= '0' && ch <= '9') value = ch - '0' + 52;
        else if (ch == '+') value = 62;
        else if (ch == '/') value = 63;
        if (value < 0) throw std::runtime_error("invalid image base64 character");
        bits = (bits << 6) | unsigned(value);
        buffered += 6;
        if (buffered >= 8) {
            buffered -= 8;
            bytes.push_back(uint8_t(bits >> buffered));
            bits &= (1u << buffered) - 1u;
        }
    }
    if (bytes.empty() || bytes.size() > 16 * 1024 * 1024)
        throw std::runtime_error("empty or oversized image data");
    return bytes;
}

GltfImage read_image(const cgltf_image& image, const fs::path& root,
                     const fs::path& model_relative) {
    std::vector<uint8_t> bytes;
    if (image.buffer_view) {
        const auto* view = image.buffer_view;
        if (!view->buffer || !view->buffer->data ||
            view->offset > view->buffer->size ||
            view->size > view->buffer->size - view->offset ||
            view->size == 0 || view->size > 16 * 1024 * 1024)
            throw std::runtime_error("invalid glTF image buffer view");
        const auto* first = static_cast<const uint8_t*>(view->buffer->data) + view->offset;
        bytes.assign(first, first + view->size);
    } else if (image.uri && std::string(image.uri).rfind("data:", 0) == 0) {
        bytes = decode_data_uri(image.uri);
    } else if (image.uri) {
        const std::string uri = image.uri;
        if (uri.find_first_of("%?#:\\") != std::string::npos)
            throw std::runtime_error("unsupported image URI: " + uri);
        const fs::path file = inside_file(root, model_relative.parent_path() / uri);
        if (fs::file_size(file) > 16 * 1024 * 1024)
            throw std::runtime_error("image file exceeds 16 MiB limit: " + uri);
        std::ifstream stream(file, std::ios::binary);
        bytes.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    } else {
        throw std::runtime_error("glTF image has no data");
    }
    if (bytes.empty() || bytes.size() > 16 * 1024 * 1024)
        throw std::runtime_error("invalid glTF image size");
    int width = 0, height = 0, channels = 0;
    if (!stbi_info_from_memory(bytes.data(), int(bytes.size()), &width, &height, &channels) ||
        width < 1 || height < 1 || width > 4096 || height > 4096)
        throw std::runtime_error("invalid or oversized PNG/JPEG image");
    stbi_uc* decoded = stbi_load_from_memory(bytes.data(), int(bytes.size()),
                                             &width, &height, &channels, 4);
    if (!decoded) throw std::runtime_error("could not decode PNG/JPEG image");
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> guard(decoded, stbi_image_free);
    GltfImage result;
    result.width = uint32_t(width);
    result.height = uint32_t(height);
    result.rgba.assign(decoded, decoded + size_t(width) * size_t(height) * 4);
    return result;
}

int image_index(const cgltf_texture_view& view, const cgltf_data& data) {
    if (!view.texture) return -1;
    if (view.texcoord != 0 || view.has_transform || !view.texture->image)
        throw std::runtime_error("texture requires unsupported UV set, transform, or image format");
    if (const cgltf_sampler* sampler = view.texture->sampler) {
        if ((sampler->mag_filter != cgltf_filter_type_undefined &&
             sampler->mag_filter != cgltf_filter_type_linear) ||
            (sampler->min_filter != cgltf_filter_type_undefined &&
             sampler->min_filter != cgltf_filter_type_linear &&
             sampler->min_filter != cgltf_filter_type_linear_mipmap_linear) ||
            sampler->wrap_s != cgltf_wrap_mode_repeat ||
            sampler->wrap_t != cgltf_wrap_mode_repeat)
            throw std::runtime_error("texture sampler requires unsupported filter or wrap mode");
    }
    const ptrdiff_t index = view.texture->image - data.images;
    if (index < 0 || size_t(index) >= data.images_count)
        throw std::runtime_error("invalid glTF texture image reference");
    return int(index);
}

GltfMaterial read_material(const cgltf_material* source, const cgltf_data& data) {
    GltfMaterial result;
    if (!source) return result;
    if (source->has_pbr_specular_glossiness || source->unlit || source->has_clearcoat ||
        source->has_transmission || source->has_volume || source->has_sheen)
        throw std::runtime_error("material extension is not yet supported");
    if (source->has_pbr_metallic_roughness) {
        const auto& pbr = source->pbr_metallic_roughness;
        for (int i = 0; i < 4; ++i) result.base_color[i] = pbr.base_color_factor[i];
        result.metallic = pbr.metallic_factor;
        result.roughness = pbr.roughness_factor;
        result.base_color_image = image_index(pbr.base_color_texture, data);
        result.metallic_roughness_image = image_index(pbr.metallic_roughness_texture, data);
    }
    for (int i = 0; i < 3; ++i) result.emissive[i] = source->emissive_factor[i];
    result.normal_image = image_index(source->normal_texture, data);
    result.normal_scale = source->normal_texture.scale;
    result.occlusion_image = image_index(source->occlusion_texture, data);
    result.occlusion_strength = source->occlusion_texture.scale;
    result.emissive_image = image_index(source->emissive_texture, data);
    result.double_sided = source->double_sided;
    result.alpha_mask = source->alpha_mode == cgltf_alpha_mode_mask;
    result.alpha_blend = source->alpha_mode == cgltf_alpha_mode_blend;
    result.alpha_cutoff = source->alpha_cutoff;
    for (float value : result.base_color) if (!std::isfinite(value))
        throw std::runtime_error("nonfinite material base color");
    for (float value : result.emissive) if (!std::isfinite(value))
        throw std::runtime_error("nonfinite material emissive factor");
    if (!std::isfinite(result.metallic) || !std::isfinite(result.roughness) ||
        result.metallic < 0 || result.metallic > 1 || result.roughness < 0 ||
        result.roughness > 1)
        throw std::runtime_error("invalid metallic or roughness factor");
    return result;
}

GltfPrimitive read_primitive(const cgltf_primitive& source, const cgltf_data& data,
                             const glm::mat4& transform) {
    if (source.type != cgltf_primitive_type_triangles || source.has_draco_mesh_compression ||
        source.targets_count != 0)
        throw std::runtime_error("only uncompressed triangle primitives are supported");
    const cgltf_accessor* positions = nullptr;
    const cgltf_accessor* colors = nullptr;
    const cgltf_accessor* normals = nullptr;
    const cgltf_accessor* texcoords = nullptr;
    for (size_t i = 0; i < source.attributes_count; ++i) {
        const auto& attribute = source.attributes[i];
        if (attribute.type == cgltf_attribute_type_position) positions = attribute.data;
        if (attribute.type == cgltf_attribute_type_color && attribute.index == 0)
            colors = attribute.data;
        if (attribute.type == cgltf_attribute_type_normal) normals = attribute.data;
        if (attribute.type == cgltf_attribute_type_texcoord && attribute.index == 0)
            texcoords = attribute.data;
    }
    check_accessor(positions, "POSITION");
    if (positions->type != cgltf_type_vec3 ||
        positions->component_type != cgltf_component_type_r_32f)
        throw std::runtime_error("POSITION must be VEC3 float");
    if (colors) {
        check_accessor(colors, "COLOR_0");
        if (colors->count != positions->count ||
            (colors->type != cgltf_type_vec3 && colors->type != cgltf_type_vec4))
            throw std::runtime_error("COLOR_0 count/type does not match POSITION");
    }
    if (normals) {
        check_accessor(normals, "NORMAL");
        if (normals->count != positions->count || normals->type != cgltf_type_vec3)
            throw std::runtime_error("NORMAL count/type does not match POSITION");
    }
    if (texcoords) {
        check_accessor(texcoords, "TEXCOORD_0");
        if (texcoords->count != positions->count || texcoords->type != cgltf_type_vec2)
            throw std::runtime_error("TEXCOORD_0 count/type does not match POSITION");
    }
    GltfPrimitive result;
    result.node_transform = transform;
    result.material = read_material(source.material, data);
    if (!texcoords && (result.material.base_color_image >= 0 ||
                       result.material.metallic_roughness_image >= 0 ||
                       result.material.normal_image >= 0 ||
                       result.material.occlusion_image >= 0 ||
                       result.material.emissive_image >= 0))
        throw std::runtime_error("textured material requires TEXCOORD_0");
    result.vertices.reserve(positions->count);
    for (size_t i = 0; i < positions->count; ++i) {
        cgltf_float position[3]{};
        cgltf_float color[4]{1.0f, 1.0f, 1.0f, 1.0f};
        cgltf_float normal[3]{0.0f, 0.0f, 1.0f};
        cgltf_float uv[2]{};
        if (!cgltf_accessor_read_float(positions, i, position, 3) ||
            (colors && !cgltf_accessor_read_float(colors, i, color, 4)) ||
            (normals && !cgltf_accessor_read_float(normals, i, normal, 3)) ||
            (texcoords && !cgltf_accessor_read_float(texcoords, i, uv, 2)))
            throw std::runtime_error("cannot read glTF vertex");
        for (float value : position) {
            if (!std::isfinite(value)) throw std::runtime_error("nonfinite glTF position");
        }
        for (float value : color) {
            if (!std::isfinite(value)) throw std::runtime_error("nonfinite glTF color");
        }
        for (float value : normal)
            if (!std::isfinite(value)) throw std::runtime_error("nonfinite glTF normal");
        for (float value : uv)
            if (!std::isfinite(value)) throw std::runtime_error("nonfinite glTF UV");
        result.vertices.push_back({{position[0], position[1], position[2]},
                                   {color[0], color[1], color[2]},
                                   {normal[0], normal[1], normal[2]}, {uv[0], uv[1]}});
    }
    if (source.indices) {
        check_accessor(source.indices, "index");
        if (source.indices->type != cgltf_type_scalar ||
            (source.indices->component_type != cgltf_component_type_r_8u &&
             source.indices->component_type != cgltf_component_type_r_16u &&
             source.indices->component_type != cgltf_component_type_r_32u))
            throw std::runtime_error("indices must be unsigned scalar values");
        result.indices.reserve(source.indices->count);
        for (size_t i = 0; i < source.indices->count; ++i) {
            const size_t index = cgltf_accessor_read_index(source.indices, i);
            if (index >= result.vertices.size())
                throw std::runtime_error("glTF index exceeds vertex count");
            result.indices.push_back(uint32_t(index));
        }
    } else {
        for (size_t i = 0; i < result.vertices.size(); ++i)
            result.indices.push_back(uint32_t(i));
    }
    if (result.indices.empty() || result.indices.size() % 3 != 0)
        throw std::runtime_error("triangle index count must be divisible by three");
    if (!normals) {
        std::vector<glm::vec3> sums(result.vertices.size(), glm::vec3(0.0f));
        for (size_t i = 0; i < result.indices.size(); i += 3) {
            const auto& a = result.vertices[result.indices[i]];
            const auto& b = result.vertices[result.indices[i + 1]];
            const auto& c = result.vertices[result.indices[i + 2]];
            const glm::vec3 pa(a.position[0], a.position[1], a.position[2]);
            const glm::vec3 pb(b.position[0], b.position[1], b.position[2]);
            const glm::vec3 pc(c.position[0], c.position[1], c.position[2]);
            const glm::vec3 face = glm::cross(pb - pa, pc - pa);
            for (size_t j = 0; j < 3; ++j) sums[result.indices[i + j]] += face;
        }
        for (size_t i = 0; i < sums.size(); ++i) {
            const glm::vec3 normal = glm::length(sums[i]) > 1e-8f
                ? glm::normalize(sums[i]) : glm::vec3(0.0f, 0.0f, 1.0f);
            result.vertices[i].normal[0] = normal.x;
            result.vertices[i].normal[1] = normal.y;
            result.vertices[i].normal[2] = normal.z;
        }
    }
    return result;
}

}  // namespace

GltfAsset load_gltf(const fs::path& project_root, const std::string& relative_path) {
    try {
        const fs::path relative(relative_path);
        if (relative.extension() != ".gltf" && relative.extension() != ".glb")
            throw std::runtime_error("mesh path must end in .gltf or .glb");
        const fs::path file = inside_file(project_root, relative);
        cgltf_options options{};
        cgltf_data* raw = nullptr;
        require_result(cgltf_parse_file(&options, file.string().c_str(), &raw), "parse glTF");
        const std::unique_ptr<cgltf_data, decltype(&cgltf_free)> data(raw, cgltf_free);
        if (data->extensions_required_count || data->buffers_count > 1024 ||
            data->nodes_count > 10000 || data->meshes_count > 10000 ||
            data->images_count > 128)
            throw std::runtime_error("required extension or asset count is unsupported");
        for (size_t i = 0; i < data->buffers_count; ++i) {
            const auto& buffer = data->buffers[i];
            if (buffer.size > 64 * 1024 * 1024)
                throw std::runtime_error("glTF buffer exceeds 64 MiB limit");
            if (!buffer.uri || std::string(buffer.uri).rfind("data:", 0) == 0) continue;
            const std::string uri = buffer.uri;
            if (uri.find_first_of("%?#:\\") != std::string::npos)
                throw std::runtime_error("unsupported external buffer URI: " + uri);
            inside_file(project_root, relative.parent_path() / uri);
        }
        require_result(cgltf_load_buffers(&options, data.get(), file.string().c_str()),
                       "load glTF buffers");
        const cgltf_scene* scene = data->scene;
        if (!scene && data->scenes_count) scene = &data->scenes[0];
        if (!scene) throw std::runtime_error("glTF has no scene");
        GltfAsset asset;
        asset.source = relative_path;
        asset.images.reserve(data->images_count);
        for (size_t i = 0; i < data->images_count; ++i) {
            try {
                asset.images.push_back(read_image(data->images[i], project_root, relative));
            } catch (const std::exception& error) {
                throw std::runtime_error("image " + std::to_string(i) + ": " + error.what());
            }
        }
        size_t total_vertices = 0;
        size_t total_primitives = 0;
        struct Pending { const cgltf_node* node; glm::mat4 parent; size_t depth; };
        std::vector<Pending> pending;
        for (size_t i = scene->nodes_count; i > 0; --i)
            pending.push_back({scene->nodes[i - 1], glm::mat4(1.0f), 0});
        size_t visited = 0;
        while (!pending.empty()) {
            const Pending current = pending.back();
            pending.pop_back();
            if (!current.node || ++visited > 10000 || current.depth > 128)
                throw std::runtime_error("invalid or deep glTF node hierarchy");
            cgltf_float local_values[16];
            cgltf_node_transform_local(current.node, local_values);
            for (float value : local_values)
                if (!std::isfinite(value)) throw std::runtime_error("nonfinite glTF node transform");
            const glm::mat4 transform = current.parent * glm::make_mat4(local_values);
            if (current.node->skin || current.node->has_mesh_gpu_instancing)
                throw std::runtime_error("skinning and GPU instancing are not yet supported");
            if (current.node->mesh) {
                for (size_t i = 0; i < current.node->mesh->primitives_count; ++i) {
                    if (++total_primitives > 256)
                        throw std::runtime_error("glTF exceeds 256 primitive limit");
                    auto primitive = read_primitive(current.node->mesh->primitives[i],
                                                    *data, transform);
                    total_vertices += primitive.vertices.size();
                    if (total_vertices > 1000000)
                        throw std::runtime_error("glTF exceeds one million vertices");
                    asset.primitives.push_back(std::move(primitive));
                }
            }
            for (size_t i = current.node->children_count; i > 0; --i)
                pending.push_back({current.node->children[i - 1], transform, current.depth + 1});
        }
        if (asset.primitives.empty()) throw std::runtime_error("glTF scene has no triangle meshes");
        return asset;
    } catch (const std::exception& error) {
        throw std::runtime_error(relative_path + ": " + error.what());
    }
}

}  // namespace acuppa
