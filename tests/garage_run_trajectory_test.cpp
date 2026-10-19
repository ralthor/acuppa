#include "acuppa/lua_project.hpp"

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct Keys {
    bool forward = false;
    bool reverse = false;
    bool left = false;
    bool right = false;
};

struct Stage {
    int ticks;
    Keys keys;
};

struct Pose {
    float x;
    float z;
    float heading;
};

void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

Pose car_pose(const acuppa::SceneFrame& frame) {
    for (const auto& item : frame.imported) {
        if (item.id != 4) continue;
        const auto& m = item.transform;
        return {m[3].x, m[3].z, std::atan2(m[2].x, -m[2].z)};
    }
    throw std::runtime_error("Garage Run car model missing from frame");
}

void set_keys(acuppa::LuaProject& game, const Keys& previous, const Keys& next) {
    const std::array<std::pair<const char*, bool>, 4> before = {{{"w", previous.forward},
        {"s", previous.reverse}, {"a", previous.left}, {"d", previous.right}}};
    const std::array<bool, 4> after = {next.forward, next.reverse, next.left, next.right};
    for (size_t i = 0; i < before.size(); ++i) {
        if (before[i].second != after[i])
            game.inject({acuppa::InputDevice::Keyboard, before[i].first,
                         after[i] ? 1.0f : 0.0f});
    }
}

std::string key_name(const Keys& keys) {
    std::string result;
    if (keys.forward) result += "W";
    if (keys.reverse) result += "S";
    if (keys.left) result += "A";
    if (keys.right) result += "D";
    return result.empty() ? "none" : result;
}

int main(int argc, char* argv[]) {
    if (argc != 3) throw std::runtime_error("Garage Run source and trace directory required");
    const fs::path output = argv[2];
    const fs::path project = output / "project";
    fs::remove_all(output);
    fs::create_directories(output);
    fs::copy(argv[1], project, fs::copy_options::recursive);
    std::ofstream csv(output / "trajectory.csv");
    if (!csv) throw std::runtime_error("cannot write trajectory.csv");
    csv << "scenario,time,keys,x,z,heading,speed\n";

    const std::vector<std::pair<std::string, std::vector<Stage>>> scenarios = {
        {"forward", {{90, {true, false, false, false}}}},
        {"forward_left", {{90, {true, false, true, false}}}},
        {"forward_right", {{90, {true, false, false, true}}}},
        {"reverse", {{120, {false, true, false, false}}}},
        {"reverse_left", {{120, {false, true, true, false}}}},
        {"reverse_right", {{120, {false, true, false, true}}}},
        {"reverse_then_left", {{120, {false, true, false, false}},
                               {60, {false, false, true, false}}}},
        {"reverse_left_release_reverse", {{120, {false, true, true, false}},
                                          {60, {false, false, true, false}}}},
        {"reverse_left_release_left", {{120, {false, true, true, false}},
                                       {60, {false, true, false, false}}}},
        {"forward_both_steer", {{90, {true, false, true, true}}}},
        {"both_steer_release_left", {{60, {true, false, true, true}},
                                     {60, {true, false, false, true}}}},
        {"reverse_both_steer", {{120, {false, true, true, true}}}},
        {"reverse_both_steer_release_right", {{60, {false, true, true, true}},
                                               {60, {false, true, true, false}}}},
        {"reverse_both_steer_release_left", {{60, {false, true, true, true}},
                                              {60, {false, true, false, true}}}},
        {"both_pedals", {{90, {true, true, false, false}}}},
        {"both_pedals_release_reverse", {{60, {true, true, false, false}},
                                          {60, {true, false, false, false}}}},
        {"both_pedals_release_forward", {{60, {true, true, false, false}},
                                          {60, {false, true, false, false}}}},
    };
    std::map<std::string, Pose> ends;
    std::map<std::string, Pose> first_stage;
    for (const auto& [name, stages] : scenarios) {
        acuppa::LuaProject game(project, "course", acuppa::LuaProject::Mode::Game, false);
        Pose previous = car_pose(game.step(0));
        Keys held;
        int tick = 0;
        csv << name << ",0,none," << previous.x << ',' << previous.z << ','
            << previous.heading << ",0\n";
        for (size_t stage_index = 0; stage_index < stages.size(); ++stage_index) {
            const Stage& stage = stages[stage_index];
            set_keys(game, held, stage.keys);
            held = stage.keys;
            for (int i = 0; i < stage.ticks; ++i) {
                const Pose pose = car_pose(game.step(1.0 / 60.0));
                const float distance = std::hypot(pose.x - previous.x,
                                                   pose.z - previous.z);
                expect(std::isfinite(pose.x) && std::isfinite(pose.z) &&
                       std::isfinite(pose.heading) && distance < 0.5f,
                       "car trajectory jumped or became nonfinite");
                ++tick;
                if (tick % 6 == 0 || i + 1 == stage.ticks) {
                    csv << name << ',' << tick / 60.0 << ',' << key_name(held) << ','
                        << pose.x << ',' << pose.z << ',' << pose.heading << ','
                        << distance * 60.0f << '\n';
                }
                previous = pose;
            }
            if (stage_index == 0) first_stage[name] = previous;
        }
        ends[name] = previous;
        std::cout << name << ": x=" << previous.x << " z=" << previous.z
                  << " heading=" << previous.heading << '\n';
    }

    const Pose forward = ends.at("forward");
    const Pose forward_left = ends.at("forward_left");
    const Pose forward_right = ends.at("forward_right");
    expect(forward.z < 12 && std::abs(forward.x) < 0.1f &&
           std::abs(forward.heading) < 0.1f, "straight forward drive veered");
    expect(forward_left.x < -1.5f && forward_left.heading < -0.35f &&
           forward_right.x > 1.5f && forward_right.heading > 0.35f &&
           std::abs(forward_left.x + forward_right.x) < 0.6f &&
           std::abs(forward_left.z - forward_right.z) < 0.6f,
           "forward left/right steering was not approximately symmetric");
    const Pose reverse = ends.at("reverse");
    const Pose reverse_left = ends.at("reverse_left");
    const Pose reverse_right = ends.at("reverse_right");
    expect(reverse.z > 22 && std::abs(reverse.x) < 0.1f &&
           std::abs(reverse.heading) < 0.1f, "straight reverse drive veered");
    expect(reverse_left.x < -1.5f && reverse_left.heading > 0.6f &&
           reverse_right.x > 1.5f && reverse_right.heading < -0.6f &&
           std::abs(reverse_left.x + reverse_right.x) < 0.8f &&
           std::abs(reverse_left.z - reverse_right.z) < 0.8f &&
           std::abs(reverse_left.heading + reverse_right.heading) < 0.2f,
           "reverse left/right steering spun or became asymmetric");
    const Pose release_left = ends.at("reverse_left_release_left");
    expect(std::abs(release_left.heading - reverse_left.heading) < 0.5f &&
           release_left.x < reverse_left.x - 1.0f,
           "releasing reverse steering did not settle into a coast");
    const Pose release_reverse = ends.at("reverse_left_release_reverse");
    expect(release_reverse.x < reverse_left.x - 1.0f &&
           release_reverse.heading > reverse_left.heading,
           "releasing reverse throttle unexpectedly stopped the turn");
    expect(std::abs(first_stage.at("reverse_then_left").x - reverse.x) < 0.01f &&
           ends.at("reverse_then_left").x < -0.5f &&
           ends.at("reverse_then_left").heading > 0.3f,
           "steering after releasing reverse did not follow the moving car");
    const Pose both_steer = ends.at("forward_both_steer");
    expect(std::abs(both_steer.x - forward.x) < 0.05f &&
           std::abs(both_steer.z - forward.z) < 0.05f &&
           ends.at("both_steer_release_left").x > 1.0f,
           "opposing steering keys did not cancel or release independently");
    const Pose reverse_both_steer = ends.at("reverse_both_steer");
    expect(std::abs(reverse_both_steer.x - reverse.x) < 0.05f &&
           std::abs(reverse_both_steer.z - reverse.z) < 0.05f &&
           ends.at("reverse_both_steer_release_right").x < -0.1f &&
           ends.at("reverse_both_steer_release_left").x > 0.1f,
           "reverse with opposing steering keys did not cancel or release independently");
    const Pose both_pedals = ends.at("both_pedals");
    expect(std::abs(both_pedals.x) < 0.01f &&
           std::abs(both_pedals.z - 16.0f) < 0.01f &&
           ends.at("both_pedals_release_reverse").z < 14.0f &&
           ends.at("both_pedals_release_forward").z > 17.5f,
           "opposing pedals did not cancel or release independently");
}
