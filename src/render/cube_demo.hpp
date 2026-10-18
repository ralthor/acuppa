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
    nvrhi::ShaderHandle shadow_cube_vertex_shader_;
    nvrhi::ShaderHandle shadow_cube_fragment_shader_;
    nvrhi::ShaderHandle shadow_pbr_vertex_shader_;
    nvrhi::ShaderHandle shadow_pbr_fragment_shader_;
    nvrhi::ShaderHandle tonemap_vertex_shader_;
    nvrhi::ShaderHandle tonemap_fragment_shader_;
    nvrhi::ShaderHandle ssao_fragment_shader_;
    nvrhi::InputLayoutHandle pbr_input_layout_;
    nvrhi::BindingLayoutHandle pbr_binding_layout_;
    nvrhi::BindingLayoutHandle environment_binding_layout_;
    nvrhi::BindingSetHandle environment_binding_set_;
    nvrhi::BindingLayoutHandle shadow_binding_layout_;
    nvrhi::BindingSetHandle shadow_binding_set_;
    nvrhi::BindingLayoutHandle tonemap_binding_layout_;
    nvrhi::BindingSetHandle tonemap_binding_set_;
    nvrhi::BindingLayoutHandle ssao_binding_layout_;
    nvrhi::BindingSetHandle ssao_binding_set_;
    nvrhi::BufferHandle pbr_frame_buffer_;
    nvrhi::BufferHandle shadow_frame_buffer_;
    nvrhi::SamplerHandle pbr_sampler_;
    nvrhi::SamplerHandle environment_sampler_;
    nvrhi::SamplerHandle shadow_sampler_;
    nvrhi::SamplerHandle tonemap_sampler_;
    nvrhi::SamplerHandle depth_sampler_;
    nvrhi::TextureHandle white_texture_;
    nvrhi::TextureHandle environment_texture_;
    nvrhi::TextureHandle environment_diffuse_texture_;
    nvrhi::TextureHandle shadow_atlas_;
    uint32_t shadow_capacity_rows_ = 0;
    nvrhi::FramebufferHandle shadow_framebuffer_;
    nvrhi::GraphicsPipelineHandle shadow_cube_pipeline_;
    nvrhi::GraphicsPipelineHandle shadow_pbr_pipeline_;
    std::shared_ptr<const HdrImage> environment_source_;
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
    nvrhi::TextureHandle hdr_color_;
    nvrhi::TextureHandle ssao_color_;
    nvrhi::TextureHandle ui_color_;
    bool ui_uploaded_ = false;
    bool ui_had_draws_ = false;
    nvrhi::GraphicsPipelineHandle pipeline_;
    nvrhi::GraphicsPipelineHandle pbr_pipeline_;
    nvrhi::GraphicsPipelineHandle tonemap_pipeline_;
    nvrhi::GraphicsPipelineHandle ssao_pipeline_;
    nvrhi::FramebufferHandle hdr_framebuffer_;
    nvrhi::FramebufferHandle ssao_framebuffer_;
    std::vector<nvrhi::FramebufferHandle> framebuffers_;
};

}  // namespace threepoint
