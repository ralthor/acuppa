#include "acuppa/lua_project.hpp"

#include <glm/geometric.hpp>

#include <cmath>
#include <stdexcept>

void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main(int argc, char* argv[]) {
    if (argc != 2) throw std::runtime_error("feature scene path required");
    acuppa::LuaProject project(argv[1]);
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
    project.inject({acuppa::InputDevice::Keyboard, "space", 1.0f});
    project.inject({acuppa::InputDevice::Keyboard, "d", 1.0f});
    project.inject({acuppa::InputDevice::MouseButton, "left", 1.0f});
    project.inject({acuppa::InputDevice::GamepadButton, "b", 1.0f});
    project.inject({acuppa::InputDevice::GamepadAxis, "left_x", 0.5f});
    project.set_pointer(640.0, 360.0);
    const auto controlled = project.step(0.0);
    expect(std::abs(controlled.cubes[0].rotation.y - 1.8f) < 0.001f,
           "injected keyboard input did not turn the cube");
    expect(std::abs(controlled.camera.eye.x - 1.5f) < 0.001f &&
           std::abs(controlled.camera.eye.z - 6.5f) < 0.001f,
           "injected keyboard, mouse, and controller input did not move the camera");
    project.inject({acuppa::InputDevice::Keyboard, "space", 0.0f});
    project.inject({acuppa::InputDevice::GamepadButton, "a", 1.0f});
    expect(std::abs(project.step(0.0).cubes[0].rotation.y - 1.8f) < 0.001f,
           "controller binding did not substitute for keyboard");
    project.inject({acuppa::InputDevice::GamepadButton, "a", 0.0f});
    expect(std::abs(project.step(0.0).cubes[0].rotation.y - 0.9f) < 0.001f,
           "released controls did not restore base rotation");
    acuppa::LuaProject hdr(argv[1], "hdr");
    acuppa::LuaProject hdr_dim(argv[1], "hdr_dim");
    expect(std::abs(hdr.step(0.0).exposure - 1.0f) < 0.001f &&
           std::abs(hdr_dim.step(0.0).exposure - 0.15f) < 0.001f,
           "source-authored scene exposure did not reach the render frame");
    acuppa::LuaProject ssao(argv[1], "ssao"), ssao_off(argv[1], "ssao_off");
    const auto ao = ssao.step(0.0);
    expect(ao.ssao_quality == 3 && std::abs(ao.ssao_radius - 0.35f) < 0.001f &&
           std::abs(ao.ssao_strength - 2.0f) < 0.001f &&
           ssao_off.step(0.0).ssao_quality == 0 &&
           project.step(0.0).ssao_quality == 0,
           "Lua SSAO settings did not reach the frame or reset for a new scene");
    acuppa::LuaProject ibl(argv[1], "ibl"), ibl_off(argv[1], "ibl_off");
    const auto environment = ibl.step(0.0);
    const auto no_environment = ibl_off.step(0.0);
    expect(environment.environment && environment.environment->width == 16 &&
           environment.environment_intensity == 1.0f &&
           !no_environment.environment && no_environment.environment_intensity == 0.0f,
           "source-authored HDRI did not reach the render frame");
    {
        acuppa::LuaProject sky(argv[1], "sky");
        const auto frame = sky.step(0.0);
        expect(frame.sky && std::abs(frame.sky->zenith.b - 1.1f) < 1e-6f &&
               std::abs(frame.sky->horizon.r - 0.8f) < 1e-6f &&
               std::abs(frame.sky->ground.g - 0.3f) < 1e-6f &&
               frame.sky->sun_color.r == 14.0f &&
               std::abs(frame.sky->sun_size - 2.2f) < 1e-6f &&
               std::abs(glm::length(frame.sky->sun_direction) - 1.0f) < 1e-5f &&
               frame.sky->sun_direction.z < -0.8f,
               "Lua sky did not reach the frame or invalid calls replaced it");
        for (int i = 0; i < 3; ++i) sky.step(1.0 / 60.0);
        expect(sky.step(0.0).sky.has_value(), "sky did not persist across frames");
        acuppa::LuaProject sky_off(argv[1], "sky_off");
        expect(!sky_off.step(0.0).sky, "scene without set_sky drew a sky");
        acuppa::LuaProject sky_clear(argv[1], "sky_clear");
        expect(sky_clear.step(0.0).sky.has_value(), "sky_clear did not start with a sky");
        sky_clear.step(1.0 / 60.0);
        expect(!sky_clear.step(0.0).sky, "set_sky(nil) did not clear the sky");
        acuppa::LuaProject sky_switch(argv[1], "sky_switch");
        expect(sky_switch.step(0.0).sky.has_value(), "sky_switch did not start with a sky");
        sky_switch.step(1.0 / 60.0);
        expect(!sky_switch.step(0.0).sky, "scene switch kept the previous scene's sky");
        acuppa::LuaProject preview(argv[1], "sky", acuppa::LuaProject::Mode::EditorPreview);
        expect(!preview.step(0.0).sky, "editor preview ran the sky start callback");
    }
    acuppa::LuaProject physics(argv[1], "physics");
    const auto initial_physics = physics.step(0.0);
    expect(initial_physics.cubes.size() == 2 &&
           std::abs(initial_physics.cubes[1].position.y - 4.0f) < 0.001f,
           "source physics scene did not create its visible rigid bodies");
    for (int i = 0; i < 180; ++i) physics.step(1.0 / 60.0);
    const auto settled_physics = physics.step(0.0);
    expect(std::abs(settled_physics.cubes[1].position.y - 0.4f) < 0.05f,
           "Lua-authored dynamic body did not visibly settle on the floor");
    acuppa::LuaProject timers(argv[1], "timers");
    for (int i = 0; i < 6; ++i) timers.step(1.0 / 60.0);
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
