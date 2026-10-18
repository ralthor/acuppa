#pragma once

#include "acuppa/scene_frame.hpp"

#include <glm/vec2.hpp>

#include <cstdint>
#include <optional>

namespace acuppa {

enum class GizmoAxis { X, Y, Z };

std::optional<glm::vec2> project_to_screen(const PerspectiveCamera& camera,
                                           uint32_t width, uint32_t height,
                                           glm::vec3 point);
std::optional<uint64_t> pick_visible_entity(const SceneFrame& scene,
                                            uint32_t width, uint32_t height,
                                            glm::vec2 pointer);
std::optional<GizmoAxis> pick_gizmo_axis(const PerspectiveCamera& camera,
                                         uint32_t width, uint32_t height,
                                         glm::vec3 pivot, glm::vec2 pointer);
glm::vec3 drag_gizmo_axis(const PerspectiveCamera& camera,
                          uint32_t width, uint32_t height,
                          glm::vec3 start_position, GizmoAxis axis,
                          glm::vec2 start_pointer, glm::vec2 pointer);

}  // namespace acuppa
