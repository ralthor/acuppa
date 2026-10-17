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
};

struct GltfPrimitive {
    std::vector<GltfVertex> vertices;
    std::vector<uint32_t> indices;
    glm::mat4 node_transform{1.0f};
};

struct GltfAsset {
    std::string source;
    std::vector<GltfPrimitive> primitives;
};

// Loads the default glTF scene. Paths and external buffers must stay in project_root.
GltfAsset load_gltf(const std::filesystem::path& project_root,
                    const std::string& relative_path);

}  // namespace threepoint
