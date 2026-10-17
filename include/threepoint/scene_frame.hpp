#pragma once

#include "threepoint/camera.hpp"

#include <glm/vec3.hpp>

#include <cstdint>
#include <vector>

namespace threepoint {

struct CubeInstance {
    uint64_t id = 0;
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f}; // Euler angles in radians, applied Y, X, then Z.
    glm::vec3 scale{1.0f};
};

struct SceneFrame {
    PerspectiveCamera camera;
    std::vector<CubeInstance> cubes;
    bool quit = false;
};

}  // namespace threepoint
