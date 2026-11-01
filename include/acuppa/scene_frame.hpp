#pragma once

#include "acuppa/camera.hpp"
#include "acuppa/gltf_asset.hpp"
#include "acuppa/hdr_image.hpp"
#include "acuppa/sky.hpp"

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace acuppa {

struct CubeInstance {
    uint64_t id = 0;
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f}; // Euler angles in radians, applied Y, X, then Z.
    glm::vec3 scale{1.0f};
    glm::mat4 parent_transform{1.0f};
    bool casts_shadows = true;
};

struct ImportedInstance {
    uint64_t id = 0;
    std::string source;
    glm::mat4 transform{1.0f};
    std::shared_ptr<const GltfAsset> asset;
    bool casts_shadows = true;
};

struct LightInstance {
    bool directional = false;
    glm::vec3 position{0.0f};
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    bool shadows = false;
    float shadow_range = 25.0f;
};

struct UiElement {
    enum class Type { Rectangle, Text, Button } type = Type::Rectangle;
    float x = 0, y = 0, width = 0, height = 0;
    float scale = 1;
    std::string text;
    uint8_t red = 255, green = 255, blue = 255, alpha = 255;
    bool focused = false;
};

struct SceneFrame {
    PerspectiveCamera camera;
    float exposure = 1.0f;
    int ssao_quality = 0;
    float ssao_radius = 0.35f;
    float ssao_strength = 2.0f;
    std::shared_ptr<const HdrImage> environment;
    float environment_intensity = 0.0f;
    // Unset draws the original fixed clear color behind geometry.
    std::optional<SkyGradient> sky;
    std::vector<CubeInstance> cubes;
    std::vector<ImportedInstance> imported;
    std::vector<LightInstance> lights;
    std::vector<UiElement> ui;
    bool quit = false;
};

}  // namespace acuppa
