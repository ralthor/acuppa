#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace acuppa {

struct PerspectiveCamera {
    glm::vec3 eye{0.0f, 0.0f, 4.5f};
    glm::vec3 target{0.0f, 0.0f, 0.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    float vertical_fov_degrees = 60.0f;
    float near_plane = 0.1f;
    float far_plane = 100.0f;

    // Vulkan clip depth is [0, 1]. nvrhi flips the Vulkan viewport Y axis.
    glm::mat4 view_projection(float aspect_ratio) const;
};

}  // namespace acuppa
