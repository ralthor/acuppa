#include "acuppa/lua_project.hpp"

#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <fstream>
#include <iterator>
#include <glm/geometric.hpp>

namespace fs = std::filesystem;

void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

float car_x(const acuppa::SceneFrame& frame) {
    for (const auto& item : frame.imported)
        if (item.id == 4) return item.transform[3].x;
    throw std::runtime_error("car model missing from frame");
}

float car_z(const acuppa::SceneFrame& frame) {
    for (const auto& item : frame.imported)
        if (item.id == 4) return item.transform[3].z;
    throw std::runtime_error("car model missing from frame");
}

bool has_text(const acuppa::SceneFrame& frame, const std::string& value) {
    for (const auto& item : frame.ui)
        if (item.text.find(value) != std::string::npos) return true;
    return false;
}

int main(int argc, char* argv[]) {
    if (argc != 3) throw std::runtime_error("Garage Run source and test copy required");
    const fs::path work = argv[2];
    fs::remove_all(work);
    fs::copy(argv[1], work, fs::copy_options::recursive);
    fs::remove_all(work / "saves"); // local play must not leak into the fixture
    acuppa::LuaProject game(work, "course", acuppa::LuaProject::Mode::Game, false);
    auto frame = game.step(0);
    expect(frame.imported.size() >= 15 && frame.environment && frame.ssao_quality == 0 &&
           frame.lights.size() == 1 && frame.lights[0].shadows,
           "source car scene omitted PBR assets, HDRI, shadows, or clean-surface SSAO setting");
    expect(std::abs(car_x(frame)) < 0.01f && std::abs(car_z(frame) - 16) < 0.01f &&
           has_text(frame, "GATE 1"), "car did not start at the first checkpoint");

    game.inject({acuppa::InputDevice::Keyboard, "d", 1});
    for (int i = 0; i < 90; ++i) frame = game.step(1.0 / 60.0);
    expect(std::abs(car_x(frame)) < 0.08f && std::abs(car_z(frame) - 16) < 0.08f,
           "steering at rest moved or spun the car");
    game.inject({acuppa::InputDevice::Keyboard, "d", 0});

    game.inject({acuppa::InputDevice::Keyboard, "w", 1});
    for (int i = 0; i < 65; ++i) frame = game.step(1.0 / 60.0);
    if (car_z(frame) >= 14.0f)
        throw std::runtime_error("throttle did not drive the rigid-body car forward: z=" +
                                 std::to_string(car_z(frame)));
    game.inject({acuppa::InputDevice::Keyboard, "d", 1});
    for (int i = 0; i < 100; ++i) frame = game.step(1.0 / 60.0);
    if (car_x(frame) <= 0.25f)
        throw std::runtime_error("steering did not turn the 3D car right: x=" +
                                 std::to_string(car_x(frame)));
    game.inject({acuppa::InputDevice::Keyboard, "w", 0});
    game.inject({acuppa::InputDevice::Keyboard, "d", 0});
    game.inject({acuppa::InputDevice::Keyboard, "r", 1});
    frame = game.step(0);
    expect(std::abs(car_x(frame)) < 0.01f && std::abs(car_z(frame) - 16) < 0.01f,
           "restart did not rebuild the course from source");
    game.inject({acuppa::InputDevice::Keyboard, "r", 0});

    acuppa::LuaProject grip(work, "course", acuppa::LuaProject::Mode::Game, false);
    acuppa::LuaProject slide(work, "course", acuppa::LuaProject::Mode::Game, false);
    grip.inject({acuppa::InputDevice::Keyboard, "w", 1});
    slide.inject({acuppa::InputDevice::Keyboard, "w", 1});
    for (int i = 0; i < 80; ++i) {
        grip.step(1.0 / 60.0);
        slide.step(1.0 / 60.0);
    }
    grip.inject({acuppa::InputDevice::Keyboard, "d", 1});
    slide.inject({acuppa::InputDevice::Keyboard, "d", 1});
    slide.inject({acuppa::InputDevice::Keyboard, "left_shift", 1});
    acuppa::SceneFrame grip_frame, slide_frame;
    for (int i = 0; i < 100; ++i) {
        grip_frame = grip.step(1.0 / 60.0);
        slide_frame = slide.step(1.0 / 60.0);
    }
    if (std::abs(car_x(grip_frame) - car_x(slide_frame)) < 0.2f)
        throw std::runtime_error("handbrake did not change the moving turn: grip=" +
                                 std::to_string(car_x(grip_frame)) + " slide=" +
                                 std::to_string(car_x(slide_frame)));

    game.inject({acuppa::InputDevice::Keyboard, "w", 1});
    bool reached_parking = false;
    for (int i = 0; i < 360; ++i) {
        frame = game.step(1.0 / 60.0);
        if (has_text(frame, "PARK AHEAD") && car_z(frame) < -14.0f) {
            reached_parking = true;
            break;
        }
    }
    expect(reached_parking && has_text(frame, "PARK"),
           "straight run did not cross the ordered physics checkpoints");
    game.inject({acuppa::InputDevice::Keyboard, "w", 0});
    game.inject({acuppa::InputDevice::Keyboard, "space", 1});
    for (int i = 0; i < 240 && !has_text(frame, "PARKED!"); ++i)
        frame = game.step(1.0 / 60.0);
    if (!has_text(frame, "PARKED!") || !fs::exists(work / "saves/best.tpsave"))
        throw std::runtime_error("low-speed parking did not finish and save: z=" +
                                 std::to_string(car_z(frame)) +
                                 " in_bay=" + std::to_string(has_text(frame, "BRAKE TO PARK")));
    game.inject({acuppa::InputDevice::Keyboard, "space", 0});
    acuppa::LuaProject reopened(work, "course", acuppa::LuaProject::Mode::Game, false);
    expect(has_text(reopened.step(0), "BEST"), "best time did not restore in a fresh game");
    reopened.inject({acuppa::InputDevice::Keyboard, "escape", 1});
    expect(reopened.step(0).quit, "Escape did not exit the game");

    const fs::path crash_work = work.parent_path() / "garage_run_crash_fixture";
    fs::remove_all(crash_work);
    fs::copy(argv[1], crash_work, fs::copy_options::recursive);
    const fs::path crash_scene = crash_work / "scenes/course.scene.lua";
    std::ifstream source(crash_scene);
    std::string scene(std::istreambuf_iterator<char>{source}, {});
    const auto start = scene.find("position = {0, 0.75, 16}");
    expect(start != std::string::npos, "crash fixture could not find car start");
    scene.replace(start, std::string("position = {0, 0.75, 16}").size(),
                  "position = {0, 0.75, -35}");
    source.close();
    std::ofstream(crash_scene, std::ios::trunc) << scene;
    acuppa::LuaProject crash(crash_work, "course", acuppa::LuaProject::Mode::Game, false);
    crash.step(0);
    crash.inject({acuppa::InputDevice::Keyboard, "w", 1});
    bool hit_wall = false, shocked = false;
    for (int i = 0; i < 360; ++i) {
        const auto hit_frame = crash.step(1.0 / 60.0);
        hit_wall |= has_text(hit_frame, "HITS 1");
        for (const auto& item : hit_frame.imported) {
            if (item.id != 4) continue;
            const glm::vec3 forward = glm::normalize(glm::vec3(item.transform[2]));
            const glm::vec3 expected = glm::vec3(item.transform[3]) - 9.0f * forward +
                                       glm::vec3(0, 5.2f, 0);
            if (hit_wall && glm::length(hit_frame.camera.eye - expected) > 0.02f)
                shocked = true;
        }
        if (shocked) break;
    }
    expect(hit_wall && shocked, "wall collision did not move the camera from its chase pose");
}
