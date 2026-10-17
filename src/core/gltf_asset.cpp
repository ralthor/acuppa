#include "threepoint/gltf_asset.hpp"

#include <cgltf.h>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace threepoint {
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

GltfPrimitive read_primitive(const cgltf_primitive& source, const glm::mat4& transform) {
    if (source.type != cgltf_primitive_type_triangles || source.has_draco_mesh_compression ||
        source.targets_count != 0)
        throw std::runtime_error("only uncompressed triangle primitives are supported");
    const cgltf_accessor* positions = nullptr;
    const cgltf_accessor* colors = nullptr;
    for (size_t i = 0; i < source.attributes_count; ++i) {
        const auto& attribute = source.attributes[i];
        if (attribute.type == cgltf_attribute_type_position) positions = attribute.data;
        if (attribute.type == cgltf_attribute_type_color && attribute.index == 0)
            colors = attribute.data;
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
    GltfPrimitive result;
    result.node_transform = transform;
    result.vertices.reserve(positions->count);
    for (size_t i = 0; i < positions->count; ++i) {
        cgltf_float position[3]{};
        cgltf_float color[4]{0.68f, 0.77f, 0.9f, 1.0f};
        if (!cgltf_accessor_read_float(positions, i, position, 3) ||
            (colors && !cgltf_accessor_read_float(colors, i, color, 4)))
            throw std::runtime_error("cannot read glTF vertex");
        for (float value : position) {
            if (!std::isfinite(value)) throw std::runtime_error("nonfinite glTF position");
        }
        for (float value : color) {
            if (!std::isfinite(value)) throw std::runtime_error("nonfinite glTF color");
        }
        result.vertices.push_back({{position[0], position[1], position[2]},
                                   {color[0], color[1], color[2]}});
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
            data->nodes_count > 10000 || data->meshes_count > 10000)
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
        size_t total_vertices = 0;
        size_t total_primitives = 0;
        struct Pending { const cgltf_node* node; glm::mat4 parent; size_t depth; };
        std::vector<Pending> pending;
        for (size_t i = 0; i < scene->nodes_count; ++i)
            pending.push_back({scene->nodes[i], glm::mat4(1.0f), 0});
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
                    auto primitive = read_primitive(current.node->mesh->primitives[i], transform);
                    total_vertices += primitive.vertices.size();
                    if (total_vertices > 1000000)
                        throw std::runtime_error("glTF exceeds one million vertices");
                    asset.primitives.push_back(std::move(primitive));
                }
            }
            for (size_t i = 0; i < current.node->children_count; ++i)
                pending.push_back({current.node->children[i], transform, current.depth + 1});
        }
        if (asset.primitives.empty()) throw std::runtime_error("glTF scene has no triangle meshes");
        return asset;
    } catch (const std::exception& error) {
        throw std::runtime_error(relative_path + ": " + error.what());
    }
}

}  // namespace threepoint
