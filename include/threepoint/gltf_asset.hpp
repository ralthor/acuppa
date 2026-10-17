#pragma once

#include <glm/mat4x4.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace threepoint {

struct GltfVertex {
    float position[3];
    float color[3];
    float normal[3];
    float uv[2];
};

struct GltfImage {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> rgba;
};

struct GltfMaterial {
    float base_color[4]{1.0f, 1.0f, 1.0f, 1.0f};
    float metallic = 1.0f;
    float roughness = 1.0f;
    float emissive[3]{0.0f, 0.0f, 0.0f};
    int base_color_image = -1;
    int metallic_roughness_image = -1;
    int normal_image = -1;
    float normal_scale = 1.0f;
    int occlusion_image = -1;
    float occlusion_strength = 1.0f;
    int emissive_image = -1;
    bool double_sided = false;
    bool alpha_mask = false;
    float alpha_cutoff = 0.5f;
};

struct GltfPrimitive {
    std::vector<GltfVertex> vertices;
    std::vector<uint32_t> indices;
    glm::mat4 node_transform{1.0f};
    GltfMaterial material;
};

struct GltfAsset {
    std::string source;
    std::vector<GltfPrimitive> primitives;
    std::vector<GltfImage> images;
};

// Loads the default glTF scene. Paths and external buffers must stay in project_root.
GltfAsset load_gltf(const std::filesystem::path& project_root,
                    const std::string& relative_path);

}  // namespace threepoint
