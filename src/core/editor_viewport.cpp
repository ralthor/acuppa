#include "threepoint/editor_viewport.hpp"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace threepoint {
namespace {

glm::vec3 axis_vector(GizmoAxis axis) {
    switch (axis) {
    case GizmoAxis::X: return {1, 0, 0};
    case GizmoAxis::Y: return {0, 1, 0};
    case GizmoAxis::Z: return {0, 0, 1};
    }
    throw std::invalid_argument("unknown gizmo axis");
}

void label(SceneFrame& frame, float x, float y, const std::string& value,
           uint8_t red = 255, uint8_t green = 255, uint8_t blue = 255) {
    UiElement item;
    item.type = UiElement::Type::Text;
    item.x = x;
    item.y = y;
    item.text = value;
    item.scale = 2;
    item.red = red;
    item.green = green;
    item.blue = blue;
    frame.ui.push_back(std::move(item));
}

std::string position_literal(glm::vec3 position) {
    if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
        !std::isfinite(position.z)) throw std::invalid_argument("invalid dragged position");
    std::ostringstream output;
    output << std::setprecision(9) << '{' << position.x << ", " << position.y
           << ", " << position.z << '}';
    return output.str();
}

}  // namespace

EditorViewport::EditorViewport(const std::filesystem::path& project_root, std::string scene)
    : document_(project_root, scene), root_(project_root), scene_name_(std::move(scene)),
      project_(std::make_unique<LuaProject>(root_, scene_name_, LuaProject::Mode::EditorPreview)),
      entities_(document_.tree()) {
    width_ = project_->width();
    height_ = project_->height();
    raw_ = project_->step(0);
    orbit_camera_ = raw_.camera;
    const glm::vec3 offset = orbit_camera_.eye - orbit_camera_.target;
    orbit_distance_ = glm::length(offset);
    if (!std::isfinite(orbit_distance_) || orbit_distance_ <= 0)
        throw std::invalid_argument("editor camera needs a finite eye and target");
    orbit_yaw_ = std::atan2(offset.x, offset.z);
    orbit_pitch_ = std::asin(offset.y / orbit_distance_);
}

void EditorViewport::resize(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) throw std::invalid_argument("invalid editor viewport size");
    width_ = width;
    height_ = height;
    project_->set_viewport(width, height);
}

void EditorViewport::pointer(double x, double y) {
    if (!std::isfinite(x) || !std::isfinite(y))
        throw std::invalid_argument("invalid editor pointer");
    const glm::vec2 next{float(x), float(y)};
    if (right_down_) {
        const glm::vec2 delta = next - pointer_;
        orbit_yaw_ += delta.x * 0.006f;
        orbit_pitch_ = std::clamp(orbit_pitch_ - delta.y * 0.006f, -1.45f, 1.45f);
        orbit_camera_.eye = orbit_camera_.target + orbit_distance_ * glm::vec3(
            std::sin(orbit_yaw_) * std::cos(orbit_pitch_), std::sin(orbit_pitch_),
            std::cos(orbit_yaw_) * std::cos(orbit_pitch_));
        raw_.camera = orbit_camera_;
    }
    pointer_ = next;
    if (dragging_) {
        const glm::vec3 world = drag_gizmo_axis(raw_.camera, width_, height_,
            drag_start_world_, *dragging_, drag_start_pointer_, pointer_);
        preview_local_ = glm::vec3(drag_parent_inverse_ * glm::vec4(world, 1));
    }
}

std::string EditorViewport::selected_name() const {
    return selected_ > 0 && selected_ <= entities_.size() ?
        entities_[size_t(selected_ - 1)].name : std::string{};
}

void EditorViewport::select(const std::string& name) {
    const auto found = std::find_if(entities_.begin(), entities_.end(),
        [&](const EditorEntity& entity) { return entity.name == name; });
    if (found == entities_.end()) throw std::invalid_argument("unknown editor entity: " + name);
    selected_ = uint64_t(found - entities_.begin()) + 1;
    dragging_.reset();
}

std::optional<EditorViewport::Target> EditorViewport::target() const {
    if (selected_ == 0) return std::nullopt;
    for (const auto& cube : raw_.cubes) if (cube.id == selected_) {
        const glm::vec3 pivot = glm::vec3(cube.parent_transform *
                                          glm::vec4(cube.position, 1));
        return Target{pivot, cube.position, glm::inverse(cube.parent_transform), true};
    }
    for (const auto& imported : raw_.imported) if (imported.id == selected_) {
        const bool unparented = entities_[size_t(selected_ - 1)].parent.empty();
        return Target{glm::vec3(imported.transform[3]),
                      glm::vec3(imported.transform[3]), glm::mat4(1), unparented};
    }
    return std::nullopt;
}

void EditorViewport::finish_drag() {
    if (!dragging_) return;
    dragging_.reset();
    document_.set_field(selected_name(), "transform", "position",
                        position_literal(preview_local_));
    document_.save();
    project_ = std::make_unique<LuaProject>(root_, scene_name_, LuaProject::Mode::EditorPreview);
    project_->set_viewport(width_, height_);
    raw_ = project_->step(0);
    raw_.camera = orbit_camera_;
}

void EditorViewport::input(const InputEvent& event) {
    if (event.device == InputDevice::MouseButton && event.control == "right") {
        if (event.value != 0 && event.value != 1)
            throw std::invalid_argument("invalid editor mouse value");
        right_down_ = event.value == 1;
        return;
    }
    if (event.device == InputDevice::Keyboard && event.control == "escape" && event.value == 1) {
        selected_ = 0;
        dragging_.reset();
        return;
    }
    if (event.device != InputDevice::MouseButton || event.control != "left") return;
    if (event.value == 0) { finish_drag(); return; }
    if (event.value != 1) throw std::invalid_argument("invalid editor mouse value");
    if (const auto current = target(); current && current->movable) {
        const auto axis = pick_gizmo_axis(raw_.camera, width_, height_,
                                          current->pivot, pointer_);
        if (axis) {
            dragging_ = axis;
            drag_start_pointer_ = pointer_;
            drag_start_world_ = current->pivot;
            drag_parent_inverse_ = current->parent_inverse;
            preview_local_ = current->local;
            return;
        }
    }
    const auto picked = pick_visible_entity(raw_, width_, height_, pointer_);
    selected_ = picked && *picked <= entities_.size() ? *picked : 0;
}

SceneFrame EditorViewport::frame() {
    raw_ = project_->step(0);
    raw_.camera = orbit_camera_;
    SceneFrame shown = raw_;
    if (dragging_) {
        for (auto& cube : shown.cubes) if (cube.id == selected_) cube.position = preview_local_;
        for (auto& imported : shown.imported) if (imported.id == selected_)
            imported.transform[3] = glm::vec4(preview_local_, 1);
    }
    const auto current = target();
    if (current) {
        const glm::vec3 pivot = dragging_
            ? glm::vec3(glm::inverse(drag_parent_inverse_) * glm::vec4(preview_local_, 1))
            : current->pivot;
        const auto center = project_to_screen(shown.camera, width_, height_, pivot);
        for (GizmoAxis axis : {GizmoAxis::X, GizmoAxis::Y, GizmoAxis::Z}) {
            const glm::vec3 vector = axis_vector(axis);
            if (const auto screen = project_to_screen(shown.camera, width_, height_,
                                                      pivot + vector * 2.2f); center && screen) {
                const uint8_t red = axis == GizmoAxis::X ? 255 : 110;
                const uint8_t green = axis == GizmoAxis::Y ? 255 : 110;
                const uint8_t blue = axis == GizmoAxis::Z ? 255 : 110;
                for (int i = 1; i <= 48; ++i) {
                    const glm::vec2 dot = *center + (*screen - *center) * (float(i) / 48);
                    UiElement mark;
                    mark.x = dot.x - 3;
                    mark.y = dot.y - 3;
                    mark.width = mark.height = 6;
                    mark.red = red;
                    mark.green = green;
                    mark.blue = blue;
                    shown.ui.push_back(mark);
                }
                label(shown, screen->x, screen->y, axis == GizmoAxis::X ? "X" :
                      axis == GizmoAxis::Y ? "Y" : "Z", red, green, blue);
            }
        }
    }
    UiElement banner;
    banner.width = float(width_);
    banner.height = 72;
    banner.red = 14;
    banner.green = 25;
    banner.blue = 42;
    banner.alpha = 225;
    shown.ui.insert(shown.ui.begin(), banner);
    label(shown, 20, 12, "EDITOR  " + scene_name_ +
          "  SELECT: CLICK  MOVE: DRAG AXIS  ORBIT: RIGHT DRAG");
    label(shown, 20, 38, selected_ ? "SELECTED: " + selected_name() : "SELECTED: NONE");
    if (current && !current->movable)
        label(shown, 20, 64, "PARENTED GLTF MOVE UNAVAILABLE", 255, 140, 100);
    return shown;
}

}  // namespace threepoint
