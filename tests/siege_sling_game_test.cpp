#include "acuppa/lua_project.hpp"

#include <glm/geometric.hpp>

#include <cmath>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using acuppa::InputDevice;
using acuppa::LuaProject;
using acuppa::SceneFrame;

namespace {

constexpr double tick = 1.0 / 60.0;

void expect(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

bool has_text(const SceneFrame& frame, const std::string& value) {
    for (const auto& item : frame.ui)
        if (item.text.find(value) != std::string::npos) return true;
    return false;
}

bool uses(const acuppa::ImportedInstance& item, const char* mesh) {
    return item.source.find(mesh) != std::string::npos;
}

// Fort blocks and grumps by entity id: position and the X axis (rotation).
struct Pose { glm::vec3 position; glm::vec3 x_axis; };
std::map<uint64_t, Pose> fort(const SceneFrame& frame) {
    std::map<uint64_t, Pose> poses;
    for (const auto& item : frame.imported)
        if (uses(item, "assets/wood.gltf") || uses(item, "assets/stone.gltf") ||
            uses(item, "assets/glass.gltf") ||
            uses(item, "grump.gltf"))
            poses[item.id] = {glm::vec3(item.transform[3]),
                              glm::normalize(glm::vec3(item.transform[0]))};
    return poses;
}

std::vector<glm::vec3> dots(const SceneFrame& frame) {
    std::vector<glm::vec3> points;  // imported items are in entity id order
    for (const auto& item : frame.imported)
        if (uses(item, "dot.gltf")) points.push_back(glm::vec3(item.transform[3]));
    return points;
}

// The pouch ball is the ball mesh present at start; a fired ball is newer.
uint64_t pouch_ball = 0;
const acuppa::ImportedInstance* flying_ball(const SceneFrame& frame) {
    const acuppa::ImportedInstance* found = nullptr;
    for (const auto& item : frame.imported)
        if (uses(item, "ball.gltf") && item.id > pouch_ball && (!found || item.id > found->id))
            found = &item;
    return found;
}

float max_move(const std::map<uint64_t, Pose>& a, const std::map<uint64_t, Pose>& b,
               float* turn = nullptr) {
    float moved = 0, turned = 0;
    for (const auto& [id, pose] : a) {
        const auto other = b.find(id);
        if (other == b.end()) throw std::runtime_error("fort body disappeared");
        moved = std::max(moved, glm::length(other->second.position - pose.position));
        turned = std::max(turned, glm::length(other->second.x_axis - pose.x_axis));
    }
    if (turn) *turn = turned;
    return moved;
}

SceneFrame run(LuaProject& game, int ticks) {
    SceneFrame frame;
    for (int i = 0; i < ticks; ++i) frame = game.step(tick);
    return frame;
}

void key(LuaProject& game, const char* name, float value) {
    game.inject({InputDevice::Keyboard, name, value});
}

// Fires with `release`, then checks the ball follows the dots shown just
// before it. Dot i is the predicted position i * 0.07 s after launch.
SceneFrame check_flight(LuaProject& game, const std::vector<glm::vec3>& predicted,
                        const char* label) {
    expect(predicted.size() >= 10, std::string(label) + ": aim arc dots were not shown");
    SceneFrame frame = game.step(tick);  // launch tick
    const auto* ball = flying_ball(frame);
    expect(ball != nullptr, std::string(label) + ": firing did not spawn a ball");
    expect(dots(frame).empty(), std::string(label) + ": arc dots stayed visible in flight");
    // 21 ticks = 0.35 s = dot 5; 42 ticks = 0.70 s = dot 10.
    frame = run(game, 21);
    ball = flying_ball(frame);
    float early = glm::length(glm::vec3(ball->transform[3]) - predicted[4]);
    frame = run(game, 21);
    ball = flying_ball(frame);
    float later = glm::length(glm::vec3(ball->transform[3]) - predicted[9]);
    expect(early < 0.08f && later < 0.15f,
           std::string(label) + ": ball left the predicted arc by " + std::to_string(early) +
               " m and " + std::to_string(later) + " m");
    return frame;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 3) throw std::runtime_error("Siege Sling source and test copy required");
    const fs::path work = argv[2];
    fs::remove_all(work);
    fs::copy(argv[1], work, fs::copy_options::recursive);
    fs::remove_all(work / "saves");

    // Start: lit, sky, shadows, HUD, and an aim arc.
    LuaProject game(work, "level1", LuaProject::Mode::Game, false);
    SceneFrame frame = game.step(0);
    for (const auto& item : frame.imported)
        if (uses(item, "ball.gltf")) pouch_ball = item.id;
    expect(pouch_ball != 0, "no ball waits in the sling pouch");
    expect(frame.environment && frame.sky && frame.sky->sun_size > 0 && frame.lights.size() == 1 &&
               frame.lights[0].directional && frame.lights[0].shadows && frame.ssao_quality == 0,
           "level lacks the HDRI, sky sun disc, shadowed sun, or SSAO-off setting");
    expect(has_text(frame, "SHOTS 4") && has_text(frame, "SCORE 0") && has_text(frame, "GRUMPS 3"),
           "HUD did not show four shots, zero score, and three grumps");
    const auto start = fort(frame);
    expect(start.size() == 16, "fort should have 13 blocks and 3 grumps, found " +
                                   std::to_string(start.size()));
    expect(dots(frame).size() >= 10 && !flying_ball(frame), "aim view lacks the arc or has a ball");

    // The fort rests untouched for 10 s: no collapse, slide, or jitter.
    frame = run(game, 300);
    const auto half = fort(frame);
    frame = run(game, 300);
    const auto settled = fort(frame);
    float turn = 0;
    const float drift = max_move(start, settled, &turn);
    expect(drift < 0.02f && turn < 0.02f,
           "fort moved at rest: " + std::to_string(drift) + " m, turn " + std::to_string(turn));
    expect(max_move(half, settled) < 0.002f, "fort jittered between 5 s and 10 s");
    expect(has_text(frame, "SHOTS 4") && has_text(frame, "SCORE 0"), "resting fort scored");

    // Keyboard shot with the level's aim: follows the arc and hits the fort.
    key(game, "space", 1);
    check_flight(game, dots(frame), "keyboard shot");
    key(game, "space", 0);
    // The level's aim strikes the center keep about 1.4 s after launch; a
    // ball that falls short and rolls in arrives much later.
    frame = run(game, 54);
    const float hit = max_move(start, fort(frame));
    expect(hit > 0.1f, "a well-aimed shot did not move the fort: " + std::to_string(hit));
    expect(has_text(frame, "SHOTS 3"), "a shot did not use up one of four shots");
    bool aiming = false;
    for (int i = 0; i < 60 * 16 && !aiming; ++i) {
        frame = game.step(tick);
        aiming = has_text(frame, "PITCH");
    }
    expect(aiming, "shot never settled back to aiming");
    expect(!has_text(frame, "SCORE 0 "), "knocking the fort did not score");

    // Keys change aim: holding E raises power and the arc reaches farther.
    const auto before = dots(frame);
    key(game, "e", 1);
    frame = run(game, 20);
    key(game, "e", 0);
    const auto after = dots(frame);
    expect(before.size() > 4 && after.size() > 4 &&
               glm::length(after[4] - after[0]) > glm::length(before[4] - before[0]) + 0.1f,
           "power key did not lengthen the arc");

    // Restart rebuilds the level from source.
    key(game, "r", 1);
    frame = game.step(0);
    key(game, "r", 0);
    frame = game.step(tick);
    expect(has_text(frame, "SHOTS 4") && has_text(frame, "SCORE 0") && !flying_ball(frame),
           "restart did not reset shots, score, and ball");
    expect(max_move(start, fort(frame)) < 0.01f, "restart did not rebuild the fort");

    // A tiny drag cancels and does not fire.
    LuaProject mouse(work, "level1", LuaProject::Mode::Game, false);
    mouse.step(0);
    mouse.set_pointer(640, 400);
    mouse.inject({InputDevice::MouseButton, "left", 1});
    run(mouse, 2);
    mouse.set_pointer(646, 405);
    run(mouse, 2);
    mouse.inject({InputDevice::MouseButton, "left", 0});
    frame = run(mouse, 3);
    expect(!flying_ball(frame) && has_text(frame, "SHOTS 4"), "a tiny drag fired a shot");

    // A real drag (down and left) previews, then fires on release.
    mouse.set_pointer(640, 400);
    mouse.inject({InputDevice::MouseButton, "left", 1});
    run(mouse, 2);
    mouse.set_pointer(620, 500);
    frame = run(mouse, 2);
    expect(has_text(frame, "POWER  42%") && has_text(frame, "YAW  +4"),
           "drag preview did not show the pulled aim");
    const auto pulled = dots(frame);
    expect(pulled.size() >= 10 && pulled[5].x > 0.1f, "a down-left pull did not aim right");
    mouse.inject({InputDevice::MouseButton, "left", 0});
    check_flight(mouse, pulled, "mouse drag shot");
    frame = mouse.step(tick);
    expect(has_text(frame, "SHOTS 3"), "the drag shot did not use a shot");

    // Right-drag orbits the aim camera once the shot settles back to aiming.
    for (int i = 0; i < 60 * 16 && !has_text(frame, "PITCH"); ++i) frame = mouse.step(tick);
    expect(has_text(frame, "PITCH"), "drag shot never settled back to aiming");
    frame = run(mouse, 120);
    const glm::vec3 eye = frame.camera.eye;
    mouse.set_pointer(500, 300);
    mouse.inject({InputDevice::MouseButton, "right", 1});
    mouse.step(tick);
    mouse.set_pointer(700, 300);
    frame = run(mouse, 120);
    mouse.inject({InputDevice::MouseButton, "right", 0});
    expect(std::abs(frame.camera.eye.x - eye.x) > 2.0f && has_text(frame, "SHOTS 3"),
           "right-drag did not orbit the camera, or it fired");

    key(mouse, "escape", 1);
    expect(mouse.step(0).quit, "Escape did not exit");
}
