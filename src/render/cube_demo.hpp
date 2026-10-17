#pragma once

#include "threepoint/scene_frame.hpp"

#include <nvrhi/nvrhi.h>

#include <cstdint>
#include <array>
#include <map>
#include <memory>
#include <vector>

namespace threepoint {

class CubeDemo {
public:
    explicit CubeDemo(nvrhi::IDevice* device);
    void release_size_dependent();
    void resize(const std::vector<nvrhi::TextureHandle>& colors, uint32_t width,
                uint32_t height);
    void prepare(const SceneFrame& scene);
    void record(nvrhi::ICommandList* commands, uint32_t image_index,
                const SceneFrame& scene);

private:
    nvrhi::IDevice* device_;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    nvrhi::ShaderHandle vertex_shader_;
    nvrhi::ShaderHandle fragment_shader_;
    nvrhi::InputLayoutHandle input_layout_;
    nvrhi::BindingLayoutHandle binding_layout_;
    nvrhi::BindingSetHandle binding_set_;
    nvrhi::ShaderHandle pbr_vertex_shader_;
    nvrhi::ShaderHandle pbr_fragment_shader_;
    nvrhi::InputLayoutHandle pbr_input_layout_;
    nvrhi::BindingLayoutHandle pbr_binding_layout_;
    nvrhi::BufferHandle pbr_frame_buffer_;
    nvrhi::SamplerHandle pbr_sampler_;
    nvrhi::TextureHandle white_texture_;
    nvrhi::BufferHandle vertices_;
    nvrhi::BufferHandle indices_;
    struct GpuPrimitive {
        nvrhi::BufferHandle vertices;
        nvrhi::BufferHandle indices;
        uint32_t index_count = 0;
        glm::mat4 node_transform{1.0f};
        nvrhi::BufferHandle material_buffer;
        nvrhi::BindingSetHandle binding_set;
    };
    struct GpuAsset {
        std::shared_ptr<const GltfAsset> source;
        std::vector<GpuPrimitive> primitives;
        std::vector<std::array<nvrhi::TextureHandle, 2>> textures;
    };
    std::map<const GltfAsset*, GpuAsset> imported_;
    nvrhi::TextureHandle depth_;
    nvrhi::GraphicsPipelineHandle pipeline_;
    nvrhi::GraphicsPipelineHandle pbr_pipeline_;
    std::vector<nvrhi::FramebufferHandle> framebuffers_;
};

}  // namespace threepoint
