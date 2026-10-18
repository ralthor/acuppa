#include "acuppa/editor_viewport.hpp"

#include <filesystem>
#include <stdexcept>

namespace fs = std::filesystem;

void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main(int argc, char* argv[]) {
    if (argc != 3) throw std::runtime_error("source and temporary project paths required");
    const fs::path work = argv[2];
    fs::remove_all(work);
    fs::copy(argv[1], work, fs::copy_options::recursive);
    acuppa::EditorViewport viewport(work, "editor");
    auto frame = viewport.frame();
    expect(frame.cubes.size() == 1 && frame.ui.size() >= 3,
           "editor viewport did not show authored geometry and instructions");
    viewport.select("actor");
    frame = viewport.frame();
    expect(frame.cubes.size() == 1 && frame.ui.size() > 50 &&
           viewport.selected_name() == "actor",
           "editor did not show transform handles for selection");
    const auto pivot = acuppa::project_to_screen(frame.camera, 1280, 720, {1, 0, 0});
    const auto endpoint = acuppa::project_to_screen(frame.camera, 1280, 720, {2, 0, 0});
    expect(pivot && endpoint, "editor fixture axis did not project");
    const auto handle = *pivot + (*endpoint - *pivot) * 0.7f;
    viewport.pointer(handle.x, handle.y);
    viewport.input({acuppa::InputDevice::MouseButton, "left", 1});
    const glm::vec2 moved = handle + (*endpoint - *pivot);
    viewport.pointer(moved.x, moved.y);
    viewport.input({acuppa::InputDevice::MouseButton, "left", 0});
    acuppa::SceneDocument reopened(work, "editor");
    expect(reopened.inspect("actor").find("[1] = 1") != std::string::npos,
           "gizmo drag did not save the actor's local X position");
    frame = viewport.frame();
    expect(frame.cubes[0].position.x == 1 && frame.cubes[0].parent_transform[3].x == 1,
           "editor viewport did not reload the saved parented transform");
    viewport.input({acuppa::InputDevice::Keyboard, "escape", 1});
    viewport.pointer(endpoint->x, endpoint->y);
    viewport.input({acuppa::InputDevice::MouseButton, "left", 1});
    expect(viewport.selected_name() == "actor", "click did not select a visible cube");
    viewport.input({acuppa::InputDevice::MouseButton, "left", 0});
    const glm::vec3 eye_before = viewport.frame().camera.eye;
    viewport.pointer(100, 100);
    viewport.input({acuppa::InputDevice::MouseButton, "right", 1});
    viewport.pointer(180, 130);
    viewport.input({acuppa::InputDevice::MouseButton, "right", 0});
    expect(viewport.frame().camera.eye.x > eye_before.x + 0.2f,
           "right drag did not orbit the editor camera toward the Z handle");
}
