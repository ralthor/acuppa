#include "acuppa/camera.hpp"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include <cmath>
#include <stdexcept>

namespace acuppa {
namespace {

bool finite(const glm::vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

}  // namespace

glm::mat4 PerspectiveCamera::view_projection(float aspect_ratio) const {
    if (!std::isfinite(aspect_ratio) || aspect_ratio <= 0.0f ||
        !std::isfinite(vertical_fov_degrees) || vertical_fov_degrees <= 0.0f ||
        vertical_fov_degrees >= 180.0f || !std::isfinite(near_plane) ||
        !std::isfinite(far_plane) || near_plane <= 0.0f || far_plane <= near_plane ||
        !finite(eye) || !finite(target) || !finite(up) ||
        glm::length(eye - target) <= 0.0f ||
        glm::length(glm::cross(target - eye, up)) <= 0.0f) {
        throw std::invalid_argument("invalid perspective camera parameters");
    }
    const glm::mat4 projection = glm::perspectiveRH_ZO(
        glm::radians(vertical_fov_degrees), aspect_ratio, near_plane, far_plane);
    return projection * glm::lookAtRH(eye, target, up);
}

}  // namespace acuppa
