#pragma once

#include "threepoint/scene_frame.hpp"

#include <nvrhi/nvrhi.h>

#include <cstdint>
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
    nvrhi::BufferHandle vertices_;
    nvrhi::BufferHandle indices_;
    struct GpuPrimitive {
        nvrhi::BufferHandle vertices;
        nvrhi::BufferHandle indices;
        uint32_t index_count = 0;
        glm::mat4 node_transform{1.0f};
    };
    struct GpuAsset {
        std::shared_ptr<const GltfAsset> source;
        std::vector<GpuPrimitive> primitives;
    };
    std::map<const GltfAsset*, GpuAsset> imported_;
    nvrhi::TextureHandle depth_;
    nvrhi::GraphicsPipelineHandle pipeline_;
    std::vector<nvrhi::FramebufferHandle> framebuffers_;
};

}  // namespace threepoint
