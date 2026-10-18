#pragma once

#include "threepoint/editor_gizmo.hpp"
#include "threepoint/lua_project.hpp"
#include "threepoint/scene_document.hpp"

#include <glm/mat4x4.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace threepoint {

class EditorViewport {
public:
    EditorViewport(const std::filesystem::path& project_root, std::string scene);
    void resize(uint32_t width, uint32_t height);
    void pointer(double x, double y);
    void input(const InputEvent& event);
    void select(const std::string& name);
    std::string selected_name() const;
    uint32_t width() const { return width_; }
    uint32_t height() const { return height_; }
    SceneFrame frame();

private:
    struct Target {
        glm::vec3 pivot;
        glm::vec3 local;
        glm::mat4 parent_inverse{1};
        bool movable = true;
    };
    std::optional<Target> target() const;
    void finish_drag();
    SceneDocument document_;
    std::filesystem::path root_;
    std::string scene_name_;
    std::unique_ptr<LuaProject> project_;
    std::vector<EditorEntity> entities_;
    SceneFrame raw_;
    PerspectiveCamera orbit_camera_;
    float orbit_yaw_ = 0, orbit_pitch_ = 0, orbit_distance_ = 1;
    bool right_down_ = false;
    uint32_t width_ = 0, height_ = 0;
    glm::vec2 pointer_{0};
    uint64_t selected_ = 0;
    std::optional<GizmoAxis> dragging_;
    glm::vec2 drag_start_pointer_{0};
    glm::vec3 drag_start_world_{0};
    glm::mat4 drag_parent_inverse_{1};
    glm::vec3 preview_local_{0};
};

}  // namespace threepoint
