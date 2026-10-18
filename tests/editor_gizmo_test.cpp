#include "acuppa/editor_gizmo.hpp"

#include <glm/ext/matrix_transform.hpp>

#include <cmath>
#include <memory>
#include <stdexcept>

void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    acuppa::SceneFrame scene;
    scene.camera.eye = {4, 3, 6};
    scene.camera.target = {0, 0, 0};
    scene.cubes.push_back({1, {0, 0, -2}, {}, {0.5f, 0.5f, 0.5f}});
    scene.cubes.push_back({2, {0, 0, 0}, {}, {0.5f, 0.5f, 0.5f}});
    constexpr uint32_t width = 1280, height = 720;
    const auto center = acuppa::project_to_screen(scene.camera, width, height, {0, 0, 0});
    expect(center && std::abs(center->x - 640) < 1 && std::abs(center->y - 360) < 1,
           "world origin did not project to viewport center");
    expect(acuppa::pick_visible_entity(scene, width, height, *center) == 2,
           "nearest visible cube was not selected");
    const auto x_end = acuppa::project_to_screen(scene.camera, width, height, {1, 0, 0});
    expect(x_end.has_value(), "X gizmo endpoint did not project");
    const auto x_handle = *center + (*x_end - *center) * 0.7f;
    expect(acuppa::pick_gizmo_axis(scene.camera, width, height, {}, x_handle) ==
           acuppa::GizmoAxis::X, "X gizmo handle could not be selected");
    const glm::vec3 moved = acuppa::drag_gizmo_axis(scene.camera, width, height,
        {}, acuppa::GizmoAxis::X, *center, *x_end);
    expect(std::abs(moved.x - 1) < 0.01f && moved.y == 0 && moved.z == 0,
           "screen drag did not map to one world-space X unit");
    expect(!acuppa::pick_gizmo_axis(scene.camera, width, height, {}, {10, 10}),
           "off-handle pointer selected a gizmo axis");
    auto asset = std::make_shared<acuppa::GltfAsset>();
    acuppa::GltfPrimitive primitive;
    acuppa::GltfVertex low{}, high{};
    low.position[0] = low.position[1] = low.position[2] = -1;
    high.position[0] = high.position[1] = high.position[2] = 1;
    primitive.vertices = {low, high};
    asset->primitives.push_back(primitive);
    scene.cubes.clear();
    scene.imported.push_back({3, "fixture", glm::mat4(1), asset});
    expect(acuppa::pick_visible_entity(scene, width, height, *center) == 3,
           "imported mesh bounds could not be selected");
    bool rejected = false;
    try { acuppa::pick_visible_entity(scene, 0, height, *center); }
    catch (const std::invalid_argument&) { rejected = true; }
    expect(rejected, "invalid viewport accepted for picking");
}
