#include "cube_demo.hpp"

#include "threepoint/camera.hpp"
#include "cube_vert_spv.hpp"
#include "cube_frag_spv.hpp"

#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <stdexcept>
#include <string>
#include <vector>

namespace threepoint {
namespace {

struct Vertex {
    float position[3];
    float color[3];
};

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
    upload->close();
    device_->executeCommandList(upload);
    require(device_->waitForIdle(), "upload cube mesh");
}

void CubeDemo::release_size_dependent() {
    framebuffers_.clear();
    pipeline_ = nullptr;
    depth_ = nullptr;
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
    for (const auto& color : colors) {
        auto framebuffer = device_->createFramebuffer(
            nvrhi::FramebufferDesc().addColorAttachment(color).setDepthAttachment(depth_));
        require(framebuffer, "create cube framebuffer");
        framebuffers_.push_back(framebuffer);
    }
    require(!framebuffers_.empty(), "create cube framebuffers");
    nvrhi::GraphicsPipelineDesc pipeline_desc;
    pipeline_desc.setVertexShader(vertex_shader_).setFragmentShader(fragment_shader_)
        .setInputLayout(input_layout_).addBindingLayout(binding_layout_);
    pipeline_desc.renderState.depthStencilState.enableDepthTest().enableDepthWrite()
        .setDepthFunc(nvrhi::ComparisonFunc::Less);
    pipeline_desc.renderState.rasterState.setCullNone().enableDepthClip();
    pipeline_ = device_->createGraphicsPipeline(
        pipeline_desc, framebuffers_.front()->getFramebufferInfo());
    require(pipeline_, "create cube graphics pipeline");
}

void CubeDemo::record(nvrhi::ICommandList* commands, uint32_t image_index,
                      const SceneFrame& scene) {
    if (image_index >= framebuffers_.size()) {
        throw std::runtime_error("invalid cube framebuffer index");
    }
    commands->clearDepthStencilTexture(depth_, nvrhi::AllSubresources, true, 1.0f, false, 0);
    const glm::mat4 view_projection = scene.camera.view_projection(
        float(width_) / float(height_));
    nvrhi::GraphicsState state;
    state.setPipeline(pipeline_).setFramebuffer(framebuffers_[image_index]);
    state.viewport.addViewportAndScissorRect(nvrhi::Viewport(float(width_), float(height_)));
    state.addBindingSet(binding_set_)
        .addVertexBuffer(nvrhi::VertexBufferBinding().setBuffer(vertices_).setSlot(0).setOffset(0))
        .setIndexBuffer(nvrhi::IndexBufferBinding().setBuffer(indices_)
            .setFormat(nvrhi::Format::R16_UINT).setOffset(0));
    commands->setGraphicsState(state);
    for (const CubeInstance& cube : scene.cubes) {
        const glm::mat4 model = glm::scale(
            glm::rotate(glm::rotate(glm::rotate(
                glm::translate(glm::mat4(1.0f), cube.position), cube.rotation.y,
                glm::vec3(0, 1, 0)), cube.rotation.x, glm::vec3(1, 0, 0)),
                cube.rotation.z, glm::vec3(0, 0, 1)), cube.scale);
        const glm::mat4 mvp = view_projection * model;
        commands->setPushConstants(glm::value_ptr(mvp), sizeof(mvp));
        commands->drawIndexed(nvrhi::DrawArguments().setVertexCount(36));
    }
}

}  // namespace threepoint
