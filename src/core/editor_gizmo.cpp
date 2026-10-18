#include "threepoint/editor_gizmo.hpp"
#include "threepoint/input.hpp"

#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace threepoint {
namespace {

glm::vec3 direction(GizmoAxis axis) {
    switch (axis) {
    case GizmoAxis::X: return {1, 0, 0};
    case GizmoAxis::Y: return {0, 1, 0};
    case GizmoAxis::Z: return {0, 0, 1};
    }
    throw std::invalid_argument("unknown gizmo axis");
}

glm::mat4 cube_model(const CubeInstance& cube) {
    glm::mat4 model = cube.parent_transform;
    model = glm::translate(model, cube.position);
    model = glm::rotate(model, cube.rotation.y, {0, 1, 0});
    model = glm::rotate(model, cube.rotation.x, {1, 0, 0});
    model = glm::rotate(model, cube.rotation.z, {0, 0, 1});
    return glm::scale(model, cube.scale);
}

float ray_box(const WorldRay& ray, const glm::mat4& model,
              glm::vec3 low, glm::vec3 high) {
    if (std::abs(glm::determinant(model)) < 1e-9f)
        return std::numeric_limits<float>::infinity();
    const glm::mat4 inverse = glm::inverse(model);
    const glm::vec3 origin = glm::vec3(inverse * glm::vec4(ray.origin, 1));
    const glm::vec3 direction = glm::vec3(inverse * glm::vec4(ray.direction, 0));
    float enter = 0;
    float leave = std::numeric_limits<float>::infinity();
    for (int i = 0; i < 3; ++i) {
        if (std::abs(direction[i]) < 1e-8f) {
            if (origin[i] < low[i] || origin[i] > high[i])
                return std::numeric_limits<float>::infinity();
        } else {
            float first = (low[i] - origin[i]) / direction[i];
            float second = (high[i] - origin[i]) / direction[i];
            if (first > second) std::swap(first, second);
            enter = std::max(enter, first);
            leave = std::min(leave, second);
            if (enter > leave) return std::numeric_limits<float>::infinity();
        }
    }
    return leave >= 0 ? enter : std::numeric_limits<float>::infinity();
}

void validate_pointer(uint32_t width, uint32_t height, glm::vec2 pointer) {
    if (width == 0 || height == 0 || !std::isfinite(pointer.x) ||
        !std::isfinite(pointer.y))
        throw std::invalid_argument("gizmo requires a finite pointer and viewport");
}

}  // namespace

std::optional<glm::vec2> project_to_screen(const PerspectiveCamera& camera,
                                           uint32_t width, uint32_t height,
                                           glm::vec3 point) {
    if (width == 0 || height == 0 || !std::isfinite(point.x) ||
        !std::isfinite(point.y) || !std::isfinite(point.z))
        throw std::invalid_argument("invalid gizmo projection input");
    const glm::vec4 clip = camera.view_projection(float(width) / height) *
                           glm::vec4(point, 1);
    if (clip.w <= 0 || clip.z < 0 || clip.z > clip.w) return std::nullopt;
    const glm::vec2 ndc = glm::vec2(clip) / clip.w;
    return glm::vec2((ndc.x + 1) * width * 0.5f,
                     (1 - ndc.y) * height * 0.5f);
}

std::optional<uint64_t> pick_visible_entity(const SceneFrame& scene,
                                            uint32_t width, uint32_t height,
                                            glm::vec2 pointer) {
    validate_pointer(width, height, pointer);
    const WorldRay ray = screen_ray(scene.camera, width, height, pointer.x, pointer.y);
    float nearest = std::numeric_limits<float>::infinity();
    std::optional<uint64_t> selected;
    for (const auto& cube : scene.cubes) {
        const float distance = ray_box(ray, cube_model(cube), {-1, -1, -1}, {1, 1, 1});
        if (distance < nearest) { nearest = distance; selected = cube.id; }
    }
    for (const auto& imported : scene.imported) {
        if (!imported.asset) continue;
        for (const auto& primitive : imported.asset->primitives) {
            if (primitive.vertices.empty()) continue;
            glm::vec3 low(std::numeric_limits<float>::infinity());
            glm::vec3 high(-std::numeric_limits<float>::infinity());
            for (const auto& vertex : primitive.vertices) {
                const glm::vec3 point(vertex.position[0], vertex.position[1],
                                      vertex.position[2]);
                low = glm::min(low, point);
                high = glm::max(high, point);
            }
            const float distance = ray_box(ray, imported.transform * primitive.node_transform,
                                            low, high);
            if (distance < nearest) { nearest = distance; selected = imported.id; }
        }
    }
    return selected;
}

std::optional<GizmoAxis> pick_gizmo_axis(const PerspectiveCamera& camera,
                                         uint32_t width, uint32_t height,
                                         glm::vec3 pivot, glm::vec2 pointer) {
    validate_pointer(width, height, pointer);
    const auto center = project_to_screen(camera, width, height, pivot);
    if (!center) return std::nullopt;
    float best_distance = 12.0f;
    std::optional<GizmoAxis> best;
    for (GizmoAxis axis : {GizmoAxis::X, GizmoAxis::Y, GizmoAxis::Z}) {
        const auto endpoint = project_to_screen(camera, width, height,
                                                pivot + direction(axis) * 2.2f);
        if (!endpoint) continue;
        const glm::vec2 segment = *endpoint - *center;
        const float length_squared = glm::dot(segment, segment);
        if (length_squared < 15 * 15) continue;
        const float projection = glm::dot(pointer - *center, segment) / length_squared;
        if (projection < 0.18f || projection > 1.15f) continue;
        const float distance = glm::length(pointer - (*center + segment * projection));
        if (distance < best_distance) { best_distance = distance; best = axis; }
    }
    return best;
}

glm::vec3 drag_gizmo_axis(const PerspectiveCamera& camera,
                          uint32_t width, uint32_t height,
                          glm::vec3 start_position, GizmoAxis axis,
                          glm::vec2 start_pointer, glm::vec2 pointer) {
    validate_pointer(width, height, start_pointer);
    validate_pointer(width, height, pointer);
    const auto center = project_to_screen(camera, width, height, start_position);
    const auto endpoint = project_to_screen(camera, width, height,
                                            start_position + direction(axis));
    if (!center || !endpoint) throw std::invalid_argument("gizmo is behind the camera");
    const glm::vec2 projected = *endpoint - *center;
    const float length_squared = glm::dot(projected, projected);
    if (length_squared < 16) throw std::invalid_argument("gizmo axis is edge-on to camera");
    const float units = glm::dot(pointer - start_pointer, projected) / length_squared;
    return start_position + direction(axis) * units;
}

}  // namespace threepoint
