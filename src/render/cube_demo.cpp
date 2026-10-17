#include "cube_demo.hpp"

#include "threepoint/camera.hpp"
#include "cube_vert_spv.hpp"
#include "cube_frag_spv.hpp"
#include "pbr_vert_spv.hpp"
#include "pbr_frag_spv.hpp"
#include "tonemap_vert_spv.hpp"
#include "tonemap_frag_spv.hpp"

#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace threepoint {
namespace {

struct Vertex {
    float position[3];
    float color[3];
};

struct PbrVertex {
    float position[3];
    float color[3];
    float normal[3];
    float uv[2];
};
static_assert(sizeof(PbrVertex) == sizeof(GltfVertex));

struct PbrFrame {
    float camera[4]{};
    float light_position_type[8][4]{};
    float light_color_intensity[8][4]{};
    int32_t light_count[4]{};
    float environment_params[4]{};
};
static_assert(sizeof(PbrFrame) == 304);

struct PbrMaterial {
    float base_color[4]{};
    float factors[4]{};
    float emissive_alpha[4]{};
    int32_t maps0[4]{};
    int32_t maps1[4]{};
};
static_assert(sizeof(PbrMaterial) == 80);

struct PbrTransform {
    glm::mat4 mvp;
    glm::mat4 model;
};
static_assert(sizeof(PbrTransform) == 128);

struct CubeMesh {
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
};

CubeMesh make_cube() {
    CubeMesh mesh;
    const auto face = [&mesh](std::array<glm::vec3, 4> corners, glm::vec3 color) {
        const auto base = uint16_t(mesh.vertices.size());
        for (const glm::vec3& position : corners) {
            mesh.vertices.push_back({{position.x, position.y, position.z},
                                     {color.r, color.g, color.b}});
        }
        mesh.indices.insert(mesh.indices.end(), {base, uint16_t(base + 1), uint16_t(base + 2),
                                                  base, uint16_t(base + 2), uint16_t(base + 3)});
    };
    // The rear face is submitted last: the center-pixel test detects a missing depth test.
    face({{{-1,-1, 1}, { 1,-1, 1}, { 1, 1, 1}, {-1, 1, 1}}}, {1.0f, 0.35f, 0.18f});
    face({{{ 1,-1, 1}, { 1,-1,-1}, { 1, 1,-1}, { 1, 1, 1}}}, {0.16f, 0.83f, 0.35f});
    face({{{-1,-1,-1}, {-1,-1, 1}, {-1, 1, 1}, {-1, 1,-1}}}, {1.0f, 0.81f, 0.18f});
    face({{{-1, 1, 1}, { 1, 1, 1}, { 1, 1,-1}, {-1, 1,-1}}}, {0.17f, 0.77f, 1.0f});
    face({{{-1,-1,-1}, { 1,-1,-1}, { 1,-1, 1}, {-1,-1, 1}}}, {0.95f, 0.27f, 0.73f});
    face({{{ 1,-1,-1}, {-1,-1,-1}, {-1, 1,-1}, { 1, 1,-1}}}, {0.52f, 0.30f, 0.95f});
    return mesh;
}

void require(bool ok, const char* action) {
    if (!ok) throw std::runtime_error(std::string("could not ") + action);
}

}  // namespace

CubeDemo::CubeDemo(nvrhi::IDevice* device) : device_(device) {
    vertex_shader_ = device_->createShader(
        nvrhi::ShaderDesc().setShaderType(nvrhi::ShaderType::Vertex).setDebugName("cube vertex"),
        cube_vert_spv, sizeof(cube_vert_spv));
    fragment_shader_ = device_->createShader(
        nvrhi::ShaderDesc().setShaderType(nvrhi::ShaderType::Pixel).setDebugName("cube fragment"),
        cube_frag_spv, sizeof(cube_frag_spv));
    require(vertex_shader_ && fragment_shader_, "create cube shaders");

    const nvrhi::VertexAttributeDesc attributes[] = {
        nvrhi::VertexAttributeDesc().setName("POSITION").setFormat(nvrhi::Format::RGB32_FLOAT)
            .setOffset(0).setElementStride(sizeof(Vertex)),
        nvrhi::VertexAttributeDesc().setName("COLOR").setFormat(nvrhi::Format::RGB32_FLOAT)
            .setOffset(3 * sizeof(float)).setElementStride(sizeof(Vertex)),
    };
    input_layout_ = device_->createInputLayout(attributes, 2, vertex_shader_);
    binding_layout_ = device_->createBindingLayout(
        nvrhi::BindingLayoutDesc().setVisibility(nvrhi::ShaderType::Vertex)
            .addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(glm::mat4))));
    require(input_layout_ && binding_layout_, "create cube input and binding layouts");
    binding_set_ = device_->createBindingSet(
        nvrhi::BindingSetDesc().addItem(
            nvrhi::BindingSetItem::PushConstants(0, sizeof(glm::mat4))), binding_layout_);
    require(binding_set_, "create cube binding set");

    pbr_vertex_shader_ = device_->createShader(
        nvrhi::ShaderDesc().setShaderType(nvrhi::ShaderType::Vertex).setDebugName("PBR vertex"),
        pbr_vert_spv, sizeof(pbr_vert_spv));
    pbr_fragment_shader_ = device_->createShader(
        nvrhi::ShaderDesc().setShaderType(nvrhi::ShaderType::Pixel).setDebugName("PBR fragment"),
        pbr_frag_spv, sizeof(pbr_frag_spv));
    require(pbr_vertex_shader_ && pbr_fragment_shader_, "create PBR shaders");
    const nvrhi::VertexAttributeDesc pbr_attributes[] = {
        nvrhi::VertexAttributeDesc().setName("POSITION").setFormat(nvrhi::Format::RGB32_FLOAT)
            .setOffset(offsetof(PbrVertex, position)).setElementStride(sizeof(PbrVertex)),
        nvrhi::VertexAttributeDesc().setName("COLOR").setFormat(nvrhi::Format::RGB32_FLOAT)
            .setOffset(offsetof(PbrVertex, color)).setElementStride(sizeof(PbrVertex)),
        nvrhi::VertexAttributeDesc().setName("NORMAL").setFormat(nvrhi::Format::RGB32_FLOAT)
            .setOffset(offsetof(PbrVertex, normal)).setElementStride(sizeof(PbrVertex)),
        nvrhi::VertexAttributeDesc().setName("TEXCOORD").setFormat(nvrhi::Format::RG32_FLOAT)
            .setOffset(offsetof(PbrVertex, uv)).setElementStride(sizeof(PbrVertex)),
    };
    pbr_input_layout_ = device_->createInputLayout(pbr_attributes, 4, pbr_vertex_shader_);
    nvrhi::BindingLayoutDesc pbr_layout;
    pbr_layout.setVisibility(nvrhi::ShaderType::AllGraphics)
        .setBindingOffsets(nvrhi::VulkanBindingOffsets()
            .setSamplerOffset(0).setConstantBufferOffset(0))
        .addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(PbrTransform)));
    for (uint32_t i = 0; i < 5; ++i)
        pbr_layout.addItem(nvrhi::BindingLayoutItem::Texture_SRV(i));
    pbr_layout.addItem(nvrhi::BindingLayoutItem::Sampler(5))
        .addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(6))
        .addItem(nvrhi::BindingLayoutItem::ConstantBuffer(7));
    pbr_binding_layout_ = device_->createBindingLayout(pbr_layout);
    nvrhi::BufferDesc frame_desc;
    frame_desc.byteSize = sizeof(PbrFrame);
    frame_desc.isConstantBuffer = true;
    frame_desc.isVolatile = true;
    frame_desc.maxVersions = 16;
    frame_desc.debugName = "PBR frame";
    pbr_frame_buffer_ = device_->createBuffer(frame_desc);
    pbr_sampler_ = device_->createSampler(nvrhi::SamplerDesc()
        .setAllAddressModes(nvrhi::SamplerAddressMode::Wrap));
    environment_binding_layout_ = device_->createBindingLayout(
        nvrhi::BindingLayoutDesc().setVisibility(nvrhi::ShaderType::Pixel)
            .setBindingOffsets(nvrhi::VulkanBindingOffsets().setSamplerOffset(0))
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(0))
            .addItem(nvrhi::BindingLayoutItem::Sampler(1)));
    environment_sampler_ = device_->createSampler(nvrhi::SamplerDesc()
        .setAddressU(nvrhi::SamplerAddressMode::Wrap)
        .setAddressV(nvrhi::SamplerAddressMode::Clamp));
    require(pbr_input_layout_ && pbr_binding_layout_ && pbr_frame_buffer_ && pbr_sampler_,
            "create PBR input and material resources");
    require(environment_binding_layout_ && environment_sampler_,
            "create environment lighting resources");

    tonemap_vertex_shader_ = device_->createShader(
        nvrhi::ShaderDesc().setShaderType(nvrhi::ShaderType::Vertex).setDebugName("tonemap vertex"),
        tonemap_vert_spv, sizeof(tonemap_vert_spv));
    tonemap_fragment_shader_ = device_->createShader(
        nvrhi::ShaderDesc().setShaderType(nvrhi::ShaderType::Pixel).setDebugName("tonemap fragment"),
        tonemap_frag_spv, sizeof(tonemap_frag_spv));
    tonemap_binding_layout_ = device_->createBindingLayout(
        nvrhi::BindingLayoutDesc().setVisibility(nvrhi::ShaderType::AllGraphics)
            .setBindingOffsets(nvrhi::VulkanBindingOffsets().setSamplerOffset(0))
            .addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(float)))
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(0))
            .addItem(nvrhi::BindingLayoutItem::Sampler(1)));
    tonemap_sampler_ = device_->createSampler(nvrhi::SamplerDesc()
        .setAllAddressModes(nvrhi::SamplerAddressMode::Clamp));
    require(tonemap_vertex_shader_ && tonemap_fragment_shader_ && tonemap_binding_layout_ &&
            tonemap_sampler_, "create tonemap resources");

    const CubeMesh mesh = make_cube();
    nvrhi::BufferDesc vertex_desc;
    vertex_desc.byteSize = mesh.vertices.size() * sizeof(Vertex);
    vertex_desc.isVertexBuffer = true;
    vertex_desc.debugName = "built-in cube vertices";
    vertex_desc.enableAutomaticStateTracking(nvrhi::ResourceStates::Common);
    vertices_ = device_->createBuffer(vertex_desc);
    nvrhi::BufferDesc index_desc;
    index_desc.byteSize = mesh.indices.size() * sizeof(uint16_t);
    index_desc.isIndexBuffer = true;
    index_desc.debugName = "built-in cube indices";
    index_desc.enableAutomaticStateTracking(nvrhi::ResourceStates::Common);
    indices_ = device_->createBuffer(index_desc);
    require(vertices_ && indices_, "create cube mesh buffers");

    auto upload = device_->createCommandList();
    require(upload, "create cube upload command list");
    upload->open();
    upload->writeBuffer(vertices_, mesh.vertices.data(), vertex_desc.byteSize);
    upload->writeBuffer(indices_, mesh.indices.data(), index_desc.byteSize);
    nvrhi::TextureDesc white_desc;
    white_desc.width = white_desc.height = 1;
    white_desc.format = nvrhi::Format::RGBA8_UNORM;
    white_desc.debugName = "PBR fallback white";
    white_texture_ = device_->createTexture(white_desc);
    require(white_texture_, "create fallback texture");
    const uint8_t white[4]{255, 255, 255, 255};
    upload->beginTrackingTextureState(white_texture_, nvrhi::AllSubresources,
                                      nvrhi::ResourceStates::Common);
    upload->writeTexture(white_texture_, 0, 0, white, 4);
    upload->setPermanentTextureState(white_texture_, nvrhi::ResourceStates::ShaderResource);
    upload->close();
    device_->executeCommandList(upload);
    require(device_->waitForIdle(), "upload cube mesh");
    environment_binding_set_ = device_->createBindingSet(
        nvrhi::BindingSetDesc()
            .addItem(nvrhi::BindingSetItem::Texture_SRV(0, white_texture_))
            .addItem(nvrhi::BindingSetItem::Sampler(1, environment_sampler_)),
        environment_binding_layout_);
    require(environment_binding_set_, "create fallback environment binding set");
}

void CubeDemo::prepare(const SceneFrame& scene) {
    if (scene.environment != environment_source_) {
        environment_source_ = scene.environment;
        environment_texture_ = nullptr;
        if (scene.environment) {
            nvrhi::TextureDesc desc;
            desc.width = scene.environment->width;
            desc.height = scene.environment->height;
            desc.format = nvrhi::Format::RGBA32_FLOAT;
            desc.isShaderResource = true;
            desc.debugName = "project HDRI";
            environment_texture_ = device_->createTexture(desc);
            require(environment_texture_, "create environment texture");
            auto upload = device_->createCommandList();
            require(upload, "create environment upload command list");
            upload->open();
            upload->beginTrackingTextureState(environment_texture_, nvrhi::AllSubresources,
                                              nvrhi::ResourceStates::Common);
            upload->writeTexture(environment_texture_, 0, 0,
                                 scene.environment->rgba.data(), size_t(desc.width) * 4 * sizeof(float));
            upload->setPermanentTextureState(environment_texture_,
                                              nvrhi::ResourceStates::ShaderResource);
            upload->close();
            device_->executeCommandList(upload);
            require(device_->waitForIdle(), "upload environment texture");
        }
        environment_binding_set_ = device_->createBindingSet(
            nvrhi::BindingSetDesc()
                .addItem(nvrhi::BindingSetItem::Texture_SRV(
                    0, environment_texture_ ? environment_texture_ : white_texture_))
                .addItem(nvrhi::BindingSetItem::Sampler(1, environment_sampler_)),
            environment_binding_layout_);
        require(environment_binding_set_, "create environment binding set");
    }
    for (const auto& instance : scene.imported) {
        if (!instance.asset) throw std::runtime_error("imported mesh asset was not loaded");
        if (imported_.count(instance.asset.get())) continue;
        GpuAsset gpu;
        gpu.source = instance.asset;
        gpu.textures.resize(instance.asset->images.size());
        auto upload = device_->createCommandList();
        require(upload, "create glTF upload command list");
        upload->open();
        const auto texture_for = [&](int index, bool srgb) -> nvrhi::TextureHandle {
            if (index < 0) return white_texture_;
            auto& cached = gpu.textures.at(size_t(index))[srgb ? 1 : 0];
            if (cached) return cached;
            const GltfImage& image = instance.asset->images.at(size_t(index));
            nvrhi::TextureDesc desc;
            desc.width = image.width;
            desc.height = image.height;
            desc.format = srgb ? nvrhi::Format::SRGBA8_UNORM : nvrhi::Format::RGBA8_UNORM;
            desc.debugName = srgb ? "glTF color image" : "glTF data image";
            cached = device_->createTexture(desc);
            require(cached, "create glTF texture");
            upload->beginTrackingTextureState(cached, nvrhi::AllSubresources,
                                              nvrhi::ResourceStates::Common);
            upload->writeTexture(cached, 0, 0, image.rgba.data(), size_t(image.width) * 4);
            upload->setPermanentTextureState(cached, nvrhi::ResourceStates::ShaderResource);
            return cached;
        };
        for (const auto& primitive : instance.asset->primitives) {
            if (primitive.indices.size() > std::numeric_limits<uint32_t>::max())
                throw std::runtime_error("glTF index count exceeds renderer limit");
            nvrhi::BufferDesc vertex_desc;
            vertex_desc.byteSize = primitive.vertices.size() * sizeof(GltfVertex);
            vertex_desc.isVertexBuffer = true;
            vertex_desc.debugName = "glTF vertices";
            vertex_desc.enableAutomaticStateTracking(nvrhi::ResourceStates::Common);
            nvrhi::BufferDesc index_desc;
            index_desc.byteSize = primitive.indices.size() * sizeof(uint32_t);
            index_desc.isIndexBuffer = true;
            index_desc.debugName = "glTF indices";
            index_desc.enableAutomaticStateTracking(nvrhi::ResourceStates::Common);
            GpuPrimitive part;
            part.vertices = device_->createBuffer(vertex_desc);
            part.indices = device_->createBuffer(index_desc);
            part.index_count = uint32_t(primitive.indices.size());
            part.node_transform = primitive.node_transform;
            const GltfMaterial& material = primitive.material;
            PbrMaterial values{};
            for (int i = 0; i < 4; ++i) values.base_color[i] = material.base_color[i];
            values.factors[0] = material.metallic;
            values.factors[1] = material.roughness;
            values.factors[2] = material.normal_scale;
            values.factors[3] = material.occlusion_strength;
            for (int i = 0; i < 3; ++i) values.emissive_alpha[i] = material.emissive[i];
            values.emissive_alpha[3] = material.alpha_cutoff;
            values.maps0[0] = material.base_color_image >= 0;
            values.maps0[1] = material.metallic_roughness_image >= 0;
            values.maps0[2] = material.normal_image >= 0;
            values.maps0[3] = material.occlusion_image >= 0;
            values.maps1[0] = material.emissive_image >= 0;
            values.maps1[1] = material.alpha_mask;
            nvrhi::BufferDesc material_desc;
            material_desc.byteSize = sizeof(PbrMaterial);
            material_desc.isConstantBuffer = true;
            material_desc.debugName = "glTF material";
            material_desc.enableAutomaticStateTracking(nvrhi::ResourceStates::Common);
            part.material_buffer = device_->createBuffer(material_desc);
            require(part.vertices && part.indices && part.material_buffer,
                    "create glTF mesh and material buffers");
            upload->writeBuffer(part.vertices, primitive.vertices.data(), vertex_desc.byteSize);
            upload->writeBuffer(part.indices, primitive.indices.data(), index_desc.byteSize);
            upload->writeBuffer(part.material_buffer, &values, sizeof(values));
            nvrhi::BindingSetDesc binding;
            binding.addItem(nvrhi::BindingSetItem::PushConstants(0, sizeof(PbrTransform)))
                .addItem(nvrhi::BindingSetItem::Texture_SRV(0,
                    texture_for(material.base_color_image, true)))
                .addItem(nvrhi::BindingSetItem::Texture_SRV(1,
                    texture_for(material.metallic_roughness_image, false)))
                .addItem(nvrhi::BindingSetItem::Texture_SRV(2,
                    texture_for(material.normal_image, false)))
                .addItem(nvrhi::BindingSetItem::Texture_SRV(3,
                    texture_for(material.occlusion_image, false)))
                .addItem(nvrhi::BindingSetItem::Texture_SRV(4,
                    texture_for(material.emissive_image, true)))
                .addItem(nvrhi::BindingSetItem::Sampler(5, pbr_sampler_))
                .addItem(nvrhi::BindingSetItem::ConstantBuffer(6, pbr_frame_buffer_))
                .addItem(nvrhi::BindingSetItem::ConstantBuffer(7, part.material_buffer));
            part.binding_set = device_->createBindingSet(binding, pbr_binding_layout_);
            require(part.binding_set, "create glTF material binding set");
            gpu.primitives.push_back(std::move(part));
        }
        upload->close();
        device_->executeCommandList(upload);
        require(device_->waitForIdle(), "upload glTF mesh");
        imported_.emplace(instance.asset.get(), std::move(gpu));
    }
}

void CubeDemo::release_size_dependent() {
    framebuffers_.clear();
    hdr_framebuffer_ = nullptr;
    tonemap_binding_set_ = nullptr;
    pipeline_ = nullptr;
    pbr_pipeline_ = nullptr;
    tonemap_pipeline_ = nullptr;
    depth_ = nullptr;
    hdr_color_ = nullptr;
}

void CubeDemo::resize(const std::vector<nvrhi::TextureHandle>& colors, uint32_t width,
                      uint32_t height) {
    release_size_dependent();
    width_ = width;
    height_ = height;
    nvrhi::TextureDesc depth_desc;
    depth_desc.width = width;
    depth_desc.height = height;
    depth_desc.format = nvrhi::Format::D32;
    depth_desc.isShaderResource = false;
    depth_desc.isRenderTarget = true;
    depth_desc.debugName = "cube depth";
    depth_desc.enableAutomaticStateTracking(nvrhi::ResourceStates::DepthWrite);
    depth_ = device_->createTexture(depth_desc);
    require(depth_, "create cube depth image");
    nvrhi::TextureDesc hdr_desc;
    hdr_desc.width = width;
    hdr_desc.height = height;
    hdr_desc.format = nvrhi::Format::RGBA16_FLOAT;
    hdr_desc.isShaderResource = true;
    hdr_desc.isRenderTarget = true;
    hdr_desc.debugName = "HDR scene color";
    hdr_desc.enableAutomaticStateTracking(nvrhi::ResourceStates::RenderTarget);
    hdr_color_ = device_->createTexture(hdr_desc);
    require(hdr_color_, "create HDR scene image");
    hdr_framebuffer_ = device_->createFramebuffer(
        nvrhi::FramebufferDesc().addColorAttachment(hdr_color_).setDepthAttachment(depth_));
    require(hdr_framebuffer_, "create HDR scene framebuffer");
    for (const auto& color : colors) {
        auto framebuffer = device_->createFramebuffer(
            nvrhi::FramebufferDesc().addColorAttachment(color));
        require(framebuffer, "create display framebuffer");
        framebuffers_.push_back(framebuffer);
    }
    require(!framebuffers_.empty(), "create display framebuffers");
    tonemap_binding_set_ = device_->createBindingSet(
        nvrhi::BindingSetDesc()
            .addItem(nvrhi::BindingSetItem::PushConstants(0, sizeof(float)))
            .addItem(nvrhi::BindingSetItem::Texture_SRV(0, hdr_color_))
            .addItem(nvrhi::BindingSetItem::Sampler(1, tonemap_sampler_)),
        tonemap_binding_layout_);
    require(tonemap_binding_set_, "create tonemap binding set");
    nvrhi::GraphicsPipelineDesc pipeline_desc;
    pipeline_desc.setVertexShader(vertex_shader_).setFragmentShader(fragment_shader_)
        .setInputLayout(input_layout_).addBindingLayout(binding_layout_);
    pipeline_desc.renderState.depthStencilState.enableDepthTest().enableDepthWrite()
        .setDepthFunc(nvrhi::ComparisonFunc::Less);
    pipeline_desc.renderState.rasterState.setCullNone().enableDepthClip();
    pipeline_ = device_->createGraphicsPipeline(
        pipeline_desc, hdr_framebuffer_->getFramebufferInfo());
    require(pipeline_, "create cube graphics pipeline");
    nvrhi::GraphicsPipelineDesc pbr_desc;
    pbr_desc.setVertexShader(pbr_vertex_shader_).setFragmentShader(pbr_fragment_shader_)
        .setInputLayout(pbr_input_layout_).addBindingLayout(pbr_binding_layout_)
        .addBindingLayout(environment_binding_layout_);
    pbr_desc.renderState.depthStencilState.enableDepthTest().enableDepthWrite()
        .setDepthFunc(nvrhi::ComparisonFunc::Less);
    pbr_desc.renderState.rasterState.setCullNone().enableDepthClip();
    pbr_pipeline_ = device_->createGraphicsPipeline(
        pbr_desc, hdr_framebuffer_->getFramebufferInfo());
    require(pbr_pipeline_, "create PBR graphics pipeline");
    nvrhi::GraphicsPipelineDesc tonemap_desc;
    tonemap_desc.setVertexShader(tonemap_vertex_shader_)
        .setFragmentShader(tonemap_fragment_shader_)
        .addBindingLayout(tonemap_binding_layout_);
    tonemap_desc.renderState.rasterState.setCullNone();
    tonemap_pipeline_ = device_->createGraphicsPipeline(
        tonemap_desc, framebuffers_.front()->getFramebufferInfo());
    require(tonemap_pipeline_, "create tonemap graphics pipeline");
}

void CubeDemo::record(nvrhi::ICommandList* commands, uint32_t image_index,
                      const SceneFrame& scene) {
    if (image_index >= framebuffers_.size()) {
        throw std::runtime_error("invalid cube framebuffer index");
    }
    commands->clearTextureFloat(hdr_color_, nvrhi::AllSubresources,
                                nvrhi::Color(0.08f, 0.18f, 0.38f, 1.0f));
    commands->clearDepthStencilTexture(depth_, nvrhi::AllSubresources, true, 1.0f, false, 0);
    const glm::mat4 view_projection = scene.camera.view_projection(
        float(width_) / float(height_));
    nvrhi::GraphicsState state;
    state.setPipeline(pipeline_).setFramebuffer(hdr_framebuffer_);
    state.viewport.addViewportAndScissorRect(nvrhi::Viewport(float(width_), float(height_)));
    state.addBindingSet(binding_set_)
        .addVertexBuffer(nvrhi::VertexBufferBinding().setBuffer(vertices_).setSlot(0).setOffset(0))
        .setIndexBuffer(nvrhi::IndexBufferBinding().setBuffer(indices_)
            .setFormat(nvrhi::Format::R16_UINT).setOffset(0));
    commands->setGraphicsState(state);
    for (const CubeInstance& cube : scene.cubes) {
        const glm::mat4 model = cube.parent_transform * glm::scale(
            glm::rotate(glm::rotate(glm::rotate(
                glm::translate(glm::mat4(1.0f), cube.position), cube.rotation.y,
                glm::vec3(0, 1, 0)), cube.rotation.x, glm::vec3(1, 0, 0)),
                cube.rotation.z, glm::vec3(0, 0, 1)), cube.scale);
        const glm::mat4 mvp = view_projection * model;
        commands->setPushConstants(glm::value_ptr(mvp), sizeof(mvp));
        commands->drawIndexed(nvrhi::DrawArguments().setVertexCount(36));
    }
    PbrFrame frame{};
    frame.camera[0] = scene.camera.eye.x;
    frame.camera[1] = scene.camera.eye.y;
    frame.camera[2] = scene.camera.eye.z;
    frame.light_count[0] = int32_t(scene.lights.size());
    frame.environment_params[0] = scene.environment ? scene.environment_intensity : 0.0f;
    if (scene.lights.size() > 8) throw std::runtime_error("scene exceeds eight PBR lights");
    for (size_t i = 0; i < scene.lights.size(); ++i) {
        const auto& light = scene.lights[i];
        const glm::vec3 position = light.directional ? light.direction : light.position;
        frame.light_position_type[i][0] = position.x;
        frame.light_position_type[i][1] = position.y;
        frame.light_position_type[i][2] = position.z;
        frame.light_position_type[i][3] = light.directional ? 0.0f : 1.0f;
        frame.light_color_intensity[i][0] = light.color.r;
        frame.light_color_intensity[i][1] = light.color.g;
        frame.light_color_intensity[i][2] = light.color.b;
        frame.light_color_intensity[i][3] = light.intensity;
    }
    commands->writeBuffer(pbr_frame_buffer_, &frame, sizeof(frame));
    for (const auto& instance : scene.imported) {
        const auto found = imported_.find(instance.asset.get());
        if (found == imported_.end()) throw std::runtime_error("glTF mesh was not prepared");
        for (const auto& primitive : found->second.primitives) {
            nvrhi::GraphicsState imported_state;
            imported_state.setPipeline(pbr_pipeline_).setFramebuffer(hdr_framebuffer_);
            imported_state.viewport.addViewportAndScissorRect(
                nvrhi::Viewport(float(width_), float(height_)));
            imported_state.addBindingSet(primitive.binding_set)
                .addBindingSet(environment_binding_set_)
                .addVertexBuffer(nvrhi::VertexBufferBinding().setBuffer(primitive.vertices)
                    .setSlot(0).setOffset(0))
                .setIndexBuffer(nvrhi::IndexBufferBinding().setBuffer(primitive.indices)
                    .setFormat(nvrhi::Format::R32_UINT).setOffset(0));
            commands->setGraphicsState(imported_state);
            const glm::mat4 model = instance.transform * primitive.node_transform;
            const PbrTransform transform{view_projection * model, model};
            commands->setPushConstants(&transform, sizeof(transform));
            commands->drawIndexed(nvrhi::DrawArguments().setVertexCount(primitive.index_count));
        }
    }
    nvrhi::GraphicsState tonemap_state;
    tonemap_state.setPipeline(tonemap_pipeline_).setFramebuffer(framebuffers_[image_index]);
    tonemap_state.viewport.addViewportAndScissorRect(
        nvrhi::Viewport(float(width_), float(height_)));
    tonemap_state.addBindingSet(tonemap_binding_set_);
    commands->setGraphicsState(tonemap_state);
    commands->setPushConstants(&scene.exposure, sizeof(scene.exposure));
    commands->draw(nvrhi::DrawArguments().setVertexCount(3));
}

}  // namespace threepoint
