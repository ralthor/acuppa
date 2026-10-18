#include "threepoint/lua_project.hpp"

#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

float car_x(const threepoint::SceneFrame& frame) {
    for (const auto& item : frame.imported)
        if (item.id == 3) return item.transform[3].x;
    throw std::runtime_error("car model missing from frame");
}

float car_z(const threepoint::SceneFrame& frame) {
    for (const auto& item : frame.imported)
        if (item.id == 3) return item.transform[3].z;
    throw std::runtime_error("car model missing from frame");
}

bool has_text(const threepoint::SceneFrame& frame, const std::string& value) {
    for (const auto& item : frame.ui)
        if (item.text.find(value) != std::string::npos) return true;
    return false;
}

int main(int argc, char* argv[]) {
    if (argc != 3) throw std::runtime_error("Garage Run source and test copy required");
    const fs::path work = argv[2];
    fs::remove_all(work);
    fs::copy(argv[1], work, fs::copy_options::recursive);
    threepoint::LuaProject game(work, "course", threepoint::LuaProject::Mode::Game, false);
    auto frame = game.step(0);
    expect(frame.imported.size() >= 15 && frame.environment && frame.ssao_quality == 2 &&
           frame.lights.size() == 1 && frame.lights[0].shadows,
           "source car scene omitted PBR assets, HDRI, shadows, or SSAO");
    expect(std::abs(car_x(frame)) < 0.01f && std::abs(car_z(frame) - 8) < 0.01f &&
           has_text(frame, "GATE 1"), "car did not start at the first checkpoint");

    game.inject({threepoint::InputDevice::Keyboard, "w", 1});
    for (int i = 0; i < 65; ++i) frame = game.step(1.0 / 60.0);
    if (car_z(frame) >= 5.5f)
        throw std::runtime_error("throttle did not drive the rigid-body car forward: z=" +
                                 std::to_string(car_z(frame)));
    game.inject({threepoint::InputDevice::Keyboard, "a", 1});
    for (int i = 0; i < 30; ++i) frame = game.step(1.0 / 60.0);
    expect(car_x(frame) < -0.25f, "steering did not turn the 3D car left");
    game.inject({threepoint::InputDevice::Keyboard, "w", 0});
    game.inject({threepoint::InputDevice::Keyboard, "a", 0});
    game.inject({threepoint::InputDevice::Keyboard, "r", 1});
    frame = game.step(0);
    expect(std::abs(car_x(frame)) < 0.01f && std::abs(car_z(frame) - 8) < 0.01f,
           "restart did not rebuild the course from source");
    game.inject({threepoint::InputDevice::Keyboard, "r", 0});

    threepoint::LuaProject crash(work, "course", threepoint::LuaProject::Mode::Game, false);
    crash.inject({threepoint::InputDevice::Keyboard, "w", 1});
    crash.inject({threepoint::InputDevice::Keyboard, "a", 1});
    for (int i = 0; i < 60; ++i) crash.step(1.0 / 60.0);
    crash.inject({threepoint::InputDevice::Keyboard, "a", 0});
    bool hit_wall = false;
    for (int i = 0; i < 180; ++i) {
        const auto collision_frame = crash.step(1.0 / 60.0);
        if (has_text(collision_frame, "HITS 1")) {
            hit_wall = true;
            expect(car_x(collision_frame) > -4.5f,
                   "car passed through the static course wall");
            break;
        }
    }
    expect(hit_wall, "wall collision did not reach the game UI");

    game.inject({threepoint::InputDevice::Keyboard, "w", 1});
    bool reached_parking = false;
    for (int i = 0; i < 360; ++i) {
        frame = game.step(1.0 / 60.0);
        if (has_text(frame, "BRAKE TO PARK")) { reached_parking = true; break; }
    }
    expect(reached_parking && has_text(frame, "PARK"),
           "straight run did not cross the ordered physics checkpoints");
    game.inject({threepoint::InputDevice::Keyboard, "w", 0});
    game.inject({threepoint::InputDevice::Keyboard, "space", 1});
    for (int i = 0; i < 60 && !has_text(frame, "PARKED!"); ++i)
        frame = game.step(1.0 / 60.0);
    expect(has_text(frame, "PARKED!") && fs::exists(work / "saves/best.tpsave"),
           "low-speed parking did not finish and save the best run");
    game.inject({threepoint::InputDevice::Keyboard, "space", 0});
    threepoint::LuaProject reopened(work, "course", threepoint::LuaProject::Mode::Game, false);
    expect(has_text(reopened.step(0), "BEST"), "best time did not restore in a fresh game");
    reopened.inject({threepoint::InputDevice::Keyboard, "escape", 1});
    expect(reopened.step(0).quit, "Escape did not exit the game");
}
