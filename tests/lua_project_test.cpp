#include "threepoint/lua_project.hpp"

#include <cmath>
#include <stdexcept>

void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main(int argc, char* argv[]) {
    if (argc != 2) throw std::runtime_error("feature scene path required");
    threepoint::LuaProject project(argv[1]);
    expect(project.width() == 1280 && project.height() == 720,
           "window settings were not read from game.lua");
    const auto startup = project.step(0.0);
    expect(!startup.quit && startup.cubes.size() == 1,
           "first draw before a fixed update failed");
    auto first = project.step(1.0 / 60.0);
    expect(!first.quit && first.cubes.size() == 1, "start() did not create one cube");
    expect(first.cubes[0].id == 1, "cube ID is unstable");
    expect(std::abs(first.cubes[0].rotation.y - 0.015f) < 0.001f,
           "fixed/update time was not applied");
    for (int i = 1; i < 60; ++i) project.step(1.0 / 60.0);
    const auto last = project.step(0.0);
    expect(std::abs(last.cubes[0].rotation.y - 0.9f) < 0.001f,
           "the code-only scene did not animate deterministically");
    project.inject({threepoint::InputDevice::Keyboard, "space", 1.0f});
    project.inject({threepoint::InputDevice::Keyboard, "d", 1.0f});
    project.inject({threepoint::InputDevice::MouseButton, "left", 1.0f});
    project.inject({threepoint::InputDevice::GamepadButton, "b", 1.0f});
    project.inject({threepoint::InputDevice::GamepadAxis, "left_x", 0.5f});
    project.set_pointer(640.0, 360.0);
    const auto controlled = project.step(0.0);
    expect(std::abs(controlled.cubes[0].rotation.y - 1.8f) < 0.001f,
           "injected keyboard input did not turn the cube");
    expect(std::abs(controlled.camera.eye.x - 1.5f) < 0.001f &&
           std::abs(controlled.camera.eye.z - 6.5f) < 0.001f,
           "injected keyboard, mouse, and controller input did not move the camera");
    project.inject({threepoint::InputDevice::Keyboard, "space", 0.0f});
    project.inject({threepoint::InputDevice::GamepadButton, "a", 1.0f});
    expect(std::abs(project.step(0.0).cubes[0].rotation.y - 1.8f) < 0.001f,
           "controller binding did not substitute for keyboard");
    project.inject({threepoint::InputDevice::GamepadButton, "a", 0.0f});
    expect(std::abs(project.step(0.0).cubes[0].rotation.y - 0.9f) < 0.001f,
           "released controls did not restore base rotation");
    project.set_viewport(640, 360);
    expect(project.width() == 640 && project.height() == 360,
           "resize did not update Lua's screen-ray viewport");
    bool bad_viewport = false;
    try { project.set_viewport(0, 360); }
    catch (const std::invalid_argument&) { bad_viewport = true; }
    expect(bad_viewport, "zero-size viewport was accepted");
    bool rejected = false;
    try { project.step(-1.0); }
    catch (const std::invalid_argument&) { rejected = true; }
    expect(rejected, "negative time delta was accepted");
}
