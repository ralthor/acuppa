#include "acuppa/lua_project.hpp"

#include <glm/geometric.hpp>

#include <cmath>
#include <filesystem>
#include <functional>
#include <fstream>
#include <map>
#include <optional>
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
            uses(item, "assets/glass.gltf") || uses(item, "assets/tnt.gltf") ||
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
        if (other == b.end()) {  // broken and removed
            moved = 100;
            continue;
        }
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

void tap(LuaProject& game, const char* name) {
    key(game, name, 1);
    game.step(tick);
    key(game, name, 0);
    game.step(tick);
}

SceneFrame click(LuaProject& game, double x, double y) {
    game.set_pointer(x, y);
    game.step(tick);
    game.inject({InputDevice::MouseButton, "left", 1});
    game.step(tick);
    game.inject({InputDevice::MouseButton, "left", 0});
    return game.step(tick);
}

std::string ui_dump(const SceneFrame& frame) {
    std::string all;
    for (const auto& item : frame.ui) all += item.text + " | ";
    return all;
}

// Block meshes (any material or boulder) by entity id: position.
std::map<uint64_t, glm::vec3> blocks(const SceneFrame& frame) {
    std::map<uint64_t, glm::vec3> found;
    for (const auto& item : frame.imported)
        for (const char* mesh : {"assets/wood.gltf", "assets/stone.gltf", "assets/brick.gltf", "assets/glass.gltf",
                                 "assets/ice.gltf", "assets/metal.gltf", "assets/rubber.gltf", "assets/tnt.gltf",
                                 "assets/boulder_", "assets/grump.gltf"})
            if (uses(item, mesh)) found[item.id] = glm::vec3(item.transform[3]);
    return found;
}

bool is_stable(const SceneFrame& frame) {
    for (const auto& item : frame.ui)
        if (item.text == "STABLE") return true;
    return false;
}

// Runs until the editor shows STABLE (or the tick budget ends).
SceneFrame until_stable(LuaProject& game, int ticks) {
    SceneFrame frame;
    for (int i = 0; i < ticks; ++i) {
        frame = game.step(tick);
        if (is_stable(frame)) break;
    }
    return frame;
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


// A shot adjusts the aim with held keys (signed tick counts: yaw D/A at
// 1 degree, pitch W/S at 0.75 degrees, power E/Q at 0.8/60 per tick), fires
// with Space, then waits until the shot resolves.
struct Shot { int yaw, pitch, power; };

int hud_number(const SceneFrame& frame, const std::string& label) {
    for (const auto& item : frame.ui) {
        const auto at = item.text.find(label + " ");
        if (at != std::string::npos) return std::stoi(item.text.substr(at + label.size() + 1));
    }
    return -1;
}

void hold(LuaProject& game, const char* positive, const char* negative, int ticks) {
    const char* name = ticks >= 0 ? positive : negative;
    if (ticks == 0) return;
    key(game, name, 1);
    run(game, std::abs(ticks));
    key(game, name, 0);
    game.step(tick);
}

int count_mesh(const SceneFrame& frame, const char* mesh) {
    int count = 0;
    for (const auto& item : frame.imported) count += uses(item, mesh);
    return count;
}

int chips(const SceneFrame& frame) {
    int count = 0;
    for (const auto& item : frame.imported) count += uses(item, "assets/chip_");
    return count;
}

SceneFrame shoot(LuaProject& game, const Shot& shot, int* most_chips = nullptr,
                 const std::function<void(const SceneFrame&)>& watch = {}) {
    hold(game, "d", "a", shot.yaw);
    hold(game, "w", "s", shot.pitch);
    hold(game, "e", "q", shot.power);
    key(game, "space", 1);
    SceneFrame frame = run(game, 2);
    key(game, "space", 0);
    for (int i = 0; i < 60 * 20; ++i) {
        frame = game.step(tick);
        if (most_chips) *most_chips = std::max(*most_chips, chips(frame));
        if (watch) watch(frame);
        if (has_text(frame, "PITCH") || has_text(frame, "VICTORY") ||
            has_text(frame, "OUT OF SHOTS")) break;
    }
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
    LuaProject game(work, "play", LuaProject::Mode::Game, false);
    SceneFrame frame = game.step(0);
    for (const auto& item : frame.imported)
        if (uses(item, "ball.gltf")) pouch_ball = item.id;
    expect(pouch_ball != 0, "no ball waits in the sling pouch");
    expect(frame.environment && frame.sky && frame.sky->sun_size > 0 && frame.lights.size() == 1 &&
               frame.lights[0].directional && frame.lights[0].shadows && frame.ssao_quality == 0,
           "level lacks the HDRI, sky sun disc, shadowed sun, or SSAO-off setting");
    expect(has_text(frame, "SHOTS 4") && has_text(frame, "SCORE 0") && has_text(frame, "GRUMPS 4"),
           "HUD did not show four shots, zero score, and four grumps");
    const auto start = fort(frame);
    expect(start.size() == 55, "fort should have 51 blocks and 4 grumps, found " +
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
    if (!aiming) {
        std::string all;
        for (const auto& item : frame.ui) all += item.text + " | ";
        throw std::runtime_error("shot never settled back to aiming: " + all);
    }
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
    LuaProject mouse(work, "play", LuaProject::Mode::Game, false);
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

    // Removing a support from the sleeping fortress makes what it held fall.
    // A fixture-only entity script deletes the far-left ground pillar after
    // 10 s; the lintel on it (center x -5.625, y 1.52) must drop or break.
    {
        const fs::path fixture = work.parent_path() / "siege_sling_support_fixture";
        fs::remove_all(fixture);
        fs::copy(work, fixture, fs::copy_options::recursive);
        std::ofstream(fixture / "scripts/remove_support.lua") <<
            "local tp = require(\"acuppa\")\nlocal ticks = 0\n"
            "return {fixed_update = function(id, dt)\n"
            "  ticks = ticks + 1\n"
            "  if ticks == 600 then tp.entity_destroy(assert(tp.entity_find(\"g_pillar_1\"))) end\n"
            "end}\n";
        const fs::path scene = fixture / "scenes/play.scene.lua";
        std::ifstream in(scene);
        std::string text(std::istreambuf_iterator<char>{in}, {});
        in.close();
        const auto at = text.find("entities = {");
        expect(at != std::string::npos, "support fixture could not edit the scene");
        text.insert(at + 12, "\n    {name = \"test remover\", script = {path = \"scripts/remove_support.lua\"}},");
        std::ofstream(scene, std::ios::trunc) << text;
        LuaProject support(fixture, "play", LuaProject::Mode::Game, false);
        frame = support.step(0);
        auto lintel = [&](const SceneFrame& f) -> std::optional<glm::vec3> {
            for (const auto& item : f.imported) {
                const glm::vec3 p(item.transform[3]);
                if (uses(item, "assets/wood.gltf") && glm::length(p - glm::vec3(-5.625f, 1.52f, -18)) < 0.6f)
                    return p;
            }
            return std::nullopt;
        };
        expect(lintel(frame).has_value(), "support fixture lintel missing");
        frame = run(support, 598);
        const auto before = lintel(frame);
        expect(before && std::abs(before->y - 1.52f) < 0.03f, "lintel sagged before removal");
        frame = run(support, 90);
        const auto after = lintel(frame);
        expect(!after || after->y < before->y - 0.3f,
               "blocks above a removed support stayed floating");
    }

    // One real shot at the left crates sets off a chain: both crates blow,
    // the fortress's left side collapses, and every grump falls, including
    // ones the ball never came near. The aim (key-tick offsets from the
    // level's aim) was found by searching this deterministic level.
    {
        const fs::path saves = work / "saves";
        fs::remove_all(saves);
        LuaProject winner(work, "play", LuaProject::Mode::Game, false);
        frame = winner.step(0);
        expect(!has_text(frame, "BEST"), "a fresh project showed a best result");
        const int crates = count_mesh(frame, "assets/tnt.gltf");
        std::map<uint64_t, float> nearest;  // grump id -> closest ball distance
        bool combo_shown = false;
        for (const auto& item : frame.imported)
            if (uses(item, "assets/grump.gltf")) nearest[item.id] = 1e9f;
        int most_chips = 0;
        frame = shoot(winner, {-9, -4, -4}, &most_chips, [&](const SceneFrame& f) {
            combo_shown |= has_text(f, "COMBO x");
            const auto* ball = flying_ball(f);
            if (!ball) return;
            for (const auto& item : f.imported)
                if (uses(item, "assets/grump.gltf"))
                    nearest[item.id] = std::min(nearest[item.id],
                        glm::length(glm::vec3(item.transform[3]) - glm::vec3(ball->transform[3])));
        });
        expect(count_mesh(frame, "assets/tnt.gltf") <= crates - 2, "the crate chain did not blow");
        expect(combo_shown, "no COMBO popup during the chain");
        int untouched = 0;
        for (const auto& [id, distance] : nearest) {
            bool alive = false;
            for (const auto& item : frame.imported) alive |= item.id == id;
            if (!alive && distance > 1.2f) ++untouched;
        }
        expect(untouched >= 1, "the crate shot defeated no grump the ball never touched");
        expect(has_text(frame, "VICTORY!") && hud_number(frame, "GRUMPS") == 0,
               "the crate chain did not win in one shot");
        expect(fort(frame).size() < start.size() / 2, "broken pieces were not removed");
        const int score = hud_number(frame, "SCORE");
        expect(has_text(frame, "INCLUDES 3000 SHOT BONUS") && score >= 2000 + 3000 &&
                   has_text(frame, "3 STARS"),
               "a one-shot chain win lacks grumps, the shot bonus, or three stars");
        expect(has_text(frame, "STARS") && fs::exists(saves / "progress.tpsave"),
               "win did not show stars or save the best result");
        expect(has_text(frame, "NEXT LEVEL") && has_text(frame, "PLAY AGAIN") && has_text(frame, "MENU"),
               "win screen lacks next level, play again, or menu buttons");
        expect(most_chips > 0, "breaking blocks threw no debris");
        frame = run(winner, 75);
        expect(chips(frame) == 0, "debris did not despawn after a second");

        LuaProject reopened(work, "play", LuaProject::Mode::Game, false);
        frame = reopened.step(0);
        expect(hud_number(frame, "BEST") == score, "best score did not reload in a fresh game");

        key(winner, "space", 1);
        frame = run(winner, 3);
        key(winner, "space", 0);
        frame = run(winner, 3);
        expect(hud_number(frame, "SHOTS") == 4 && hud_number(frame, "SCORE") == 0 &&
                   hud_number(frame, "BEST") == score && fort(frame).size() == start.size() &&
                   !flying_ball(frame),
               "Space on the win screen did not start a fresh round");

        // Next level: win level 1 again, press N, and level 2 starts fresh;
        // its fort rests without scoring. Escape returns to the menu.
        {
            LuaProject next(work, "play", LuaProject::Mode::Game, false);
            next.step(0);
            frame = shoot(next, {-9, -4, -4});
            expect(has_text(frame, "VICTORY!"), "repeated winning shot did not win");
            key(next, "n", 1);
            run(next, 2);
            key(next, "n", 0);
            frame = run(next, 2);
            expect(has_text(frame, "LEVEL 2  FROZEN FOUNDRY") && hud_number(frame, "SHOTS") == 4 &&
                       hud_number(frame, "GRUMPS") == 5 && !has_text(frame, "BEST"),
                   "next level did not start level 2 fresh");
            expect(count_mesh(frame, "boulder_stone.gltf") == 1 && count_mesh(frame, "assets/metal.gltf") == 2 &&
                       count_mesh(frame, "assets/ice.gltf") == 3 && count_mesh(frame, "assets/rubber.gltf") == 2 &&
                       count_mesh(frame, "assets/brick.gltf") == 4,
                   "level 2 did not spawn its boulder and new-material blocks");
            frame = run(next, 300);
            expect(hud_number(frame, "SCORE") == 0 && hud_number(frame, "GRUMPS") == 5,
                   "level 2 fort scored or lost grumps while resting");
            // Level 2 can be won: two shots found by a search over aims.
            frame = shoot(next, {8, -12, 6});
            frame = shoot(next, {-16, -12, 20});
            expect(has_text(frame, "VICTORY!") && hud_number(frame, "GRUMPS") == 0,
                   "level 2 was not won by its known two-shot solution");
            key(next, "escape", 1);
            next.step(tick);
            key(next, "escape", 0);
            frame = run(next, 2);
            expect(!frame.quit && has_text(frame, "LEVELS") && has_text(frame, "PLAY"),
                   "Escape in a level did not return to the menu");
        }

        // A loss: four shots wasted far to the left. A ball that rolls away
        // from the fort is removed rather than kept in the settle check.
        LuaProject loser(work, "play", LuaProject::Mode::Game, false);
        loser.step(0);
        frame = shoot(loser, {-45, 0, 0});
        expect(hud_number(frame, "SHOTS") == 3 && hud_number(frame, "GRUMPS") == 4 &&
                   !flying_ball(frame), "a wasted shot changed the fort or left its ball");
        for (int i = 0; i < 3; ++i) frame = shoot(loser, {0, 0, 0});
        expect(has_text(frame, "OUT OF SHOTS") && has_text(frame, "0 STARS") &&
                   hud_number(frame, "BEST") == score,
               "wasting every shot did not lose with zero stars and keep the best");
        key(loser, "enter", 1);
        frame = run(loser, 3);
        key(loser, "enter", 0);
        frame = run(loser, 2);
        expect(hud_number(frame, "SHOTS") == 4, "Enter on the lose screen did not play again");
    }

    // Title menu: Play, Levels, Editor (source games only), Quit. Clicking
    // Levels lists both indexed levels with level 1's saved best; Enter
    // plays the focused first level.
    {
        LuaProject menu(work, "menu", LuaProject::Mode::Game, false);
        frame = menu.step(0);
        frame = menu.step(tick);
        expect(has_text(frame, "SIEGE SLING") && has_text(frame, "PLAY") && has_text(frame, "LEVELS") &&
                   has_text(frame, "EDITOR") && has_text(frame, "QUIT") && frame.sky,
               "title menu lacks its buttons or sky");
        menu.set_pointer(640, 368);
        menu.inject({InputDevice::MouseButton, "left", 1});
        menu.step(tick);
        menu.inject({InputDevice::MouseButton, "left", 0});
        frame = run(menu, 2);
        expect(has_text(frame, "CHOOSE A LEVEL") && has_text(frame, "1  GRASSY HILL") &&
                   has_text(frame, "2  FROZEN FOUNDRY") && !has_text(frame, "NEW"),
               "level select did not list both levels");
        bool best_shown = false;
        for (const auto& item : frame.ui) best_shown |= item.text.find_first_not_of("0123456789") ==
            std::string::npos && !item.text.empty() && std::stoi(item.text) >= 5000;
        expect(best_shown, "level select did not show level 1's best score");
        key(menu, "enter", 1);
        menu.step(tick);
        key(menu, "enter", 0);
        frame = run(menu, 2);
        expect(has_text(frame, "LEVEL 1  GRASSY HILL") && hud_number(frame, "SHOTS") == 4 &&
                   hud_number(frame, "BEST") > 0,
               "choosing level 1 did not play it with its best result");
        key(menu, "escape", 1);
        menu.step(tick);
        key(menu, "escape", 0);
        frame = run(menu, 2);
        expect(has_text(frame, "QUIT") && !frame.quit, "Escape did not return to the title menu");
        key(menu, "escape", 1);
        expect(menu.step(tick).quit, "Escape on the title menu did not quit");
    }

    // Level editor: opened from the menu; physics stays on while building.
    {
        LuaProject ed(work, "menu", LuaProject::Mode::Game, false);
        ed.step(0);
        ed.step(tick);
        frame = click(ed, 640, 438);  // EDITOR
        frame = run(ed, 2);
        expect(has_text(frame, "LEVEL EDITOR") && has_text(frame, "BLOCKS 3  GRUMPS 1"),
               "editor did not open with its starter layout: " + ui_dump(frame));
        auto row = [](int i) { return 56 + i * 36 + 15; };
        click(ed, 1160, row(12));  // CLEAR ALL asks first
        frame = run(ed, 2);
        expect(has_text(frame, "CLICK CLEAR ALL AGAIN") && has_text(frame, "BLOCKS 3"),
               "clear all did not ask for confirmation");
        click(ed, 1160, row(12));
        frame = run(ed, 2);
        expect(has_text(frame, "BLOCKS 0  GRUMPS 0"), "confirmed clear all left blocks");

        // A stone pillar on the ground, then a long wood plank stacked on it.
        tap(ed, "2");
        for (int i = 0; i < 4; ++i) tap(ed, "tab");
        frame = click(ed, 640, 360);
        frame = run(ed, 30);
        auto only = [](const SceneFrame& f, const char* mesh) {
            std::vector<glm::vec3> found;
            for (const auto& item : f.imported)
                if (uses(item, mesh)) found.push_back(glm::vec3(item.transform[3]));
            return found;
        };
        ed.set_pointer(100, 100);  // over the palette: no preview mesh
        frame = run(ed, 2);
        auto pillar = only(frame, "assets/stone.gltf");
        expect(pillar.size() == 1 && std::abs(pillar[0].y - 0.7f) < 0.01f &&
                   std::fmod(std::abs(pillar[0].x) + 1e-4f, 0.25f) < 1e-3f,
               "pillar did not rest on the ground at a grid position");
        tap(ed, "1");
        click(ed, 130, 362 + 3 * 34 + 14);  // LONG PLANK
        frame = click(ed, 640, 360);
        ed.set_pointer(100, 100);
        frame = run(ed, 30);
        auto plank = only(frame, "assets/wood.gltf");
        expect(plank.size() == 1 && std::abs(plank[0].y - 1.52f) < 0.01f &&
                   std::abs(plank[0].x - pillar[0].x) < 0.3f,
               "plank did not stack on the pillar's top face");
        frame = until_stable(ed, 240);
        expect(is_stable(frame), "pillar and plank never became STABLE");

        // Refusals: no grump yet.
        click(ed, 1160, row(8));  // SAVE
        frame = run(ed, 2);
        expect(has_text(frame, "CANNOT SAVE: ADD AT LEAST ONE GRUMP"), "saved a level without grumps");

        // A heavy stone cube on the plank's end tips it: UNSTABLE, then Reset.
        tap(ed, "2");
        click(ed, 130, 362 + 14);  // CUBE
        frame = click(ed, 670, 313);
        bool unstable = false;
        for (int i = 0; i < 120 && !unstable; ++i) unstable = has_text(ed.step(tick), "UNSTABLE (");
        expect(unstable, "an overhanging cube did not make the layout UNSTABLE");
        frame = run(ed, 180);
        click(ed, 1160, row(10));  // RESET
        ed.set_pointer(100, 100);
        frame = until_stable(ed, 240);
        plank = only(frame, "assets/wood.gltf");
        expect(has_text(frame, "BLOCKS 2") && plank.size() == 1 && std::abs(plank[0].y - 1.52f) < 0.01f &&
                   is_stable(frame),
               "reset did not restore the stable pillar and plank: " + ui_dump(frame));

        // A grump beside the pillar; saving right away is refused until stable.
        click(ed, 130, 362 + 7 * 34 + 14);  // GRUMP
        frame = click(ed, 560, 380);
        click(ed, 1160, row(8));
        frame = run(ed, 2);
        expect(has_text(frame, "GRUMPS 1") && has_text(frame, "CANNOT SAVE: WAIT UNTIL STABLE"),
               "saved while unstable: " + ui_dump(frame));
        ed.set_pointer(100, 100);
        frame = until_stable(ed, 300);
        click(ed, 1160, row(8));
        frame = run(ed, 2);
        expect(has_text(frame, "SAVED LEVELS/PEBBLE_KEEP.LUA"), "editor did not save: " + ui_dump(frame));
        std::ifstream saved_file(work / "levels/pebble_keep.lua");
        std::string saved_text(std::istreambuf_iterator<char>{saved_file}, {});
        std::ifstream index_file(work / "levels/index.lua");
        std::string index_text(std::istreambuf_iterator<char>{index_file}, {});
        expect(saved_text.rfind("return {", 0) == 0 && saved_text.find("half_extents = {1.1, 0.12, 0.4}") !=
                   std::string::npos && index_text.find("levels/pebble_keep.lua") != std::string::npos &&
                   index_text.find("Pebble Keep") != std::string::npos,
               "saved level or index entry is wrong:\n" + saved_text + index_text);

        // Play test twice; each return restores the exact pre-test layout.
        const auto layout = blocks(frame);
        expect(layout.size() == 3, "editor layout should hold two blocks and a grump");
        for (int round = 0; round < 2; ++round) {
            tap(ed, "p");
            frame = run(ed, 2);
            expect(has_text(frame, "PLAY TEST  PEBBLE KEEP") && hud_number(frame, "GRUMPS") == 1 &&
                       hud_number(frame, "SHOTS") == 4,
                   "P did not play-test the editor layout");
            frame = run(ed, 30);
            tap(ed, "escape");
            ed.set_pointer(100, 100);
            frame = run(ed, 1);
            expect(has_text(frame, "LEVEL EDITOR"), "Escape in a play test did not return to the editor");
            const auto back = blocks(frame);
            std::vector<glm::vec3> a, b;
            for (const auto& [id, p] : layout) a.push_back(p);
            for (const auto& [id, p] : back) b.push_back(p);
            bool same = a.size() == b.size();
            for (size_t i = 0; same && i < a.size(); ++i) same = glm::length(a[i] - b[i]) < 1e-3f;  // contact settling only
            expect(same, "play test did not restore the exact editor layout");
        }

        // Undo removes the last edit (the grump).
        key(ed, "left_control", 1);
        tap(ed, "z");
        key(ed, "left_control", 0);
        frame = run(ed, 2);
        expect(has_text(frame, "GRUMPS 0") && has_text(frame, "BLOCKS 2"), "Ctrl+Z did not undo: " + ui_dump(frame));

        // Select mode: click the pillar, Backspace deletes it, Ctrl+Z restores.
        tap(ed, "q");
        click(ed, 640, 362);
        frame = run(ed, 2);
        expect(has_text(frame, "SELECT: CLICK A BLOCK"), "Q did not switch to select mode");
        tap(ed, "backspace");
        frame = run(ed, 2);
        expect(has_text(frame, "BLOCKS 1"), "Backspace did not delete the selected block: " + ui_dump(frame));
        key(ed, "left_control", 1);
        tap(ed, "z");
        key(ed, "left_control", 0);
        frame = run(ed, 2);
        expect(has_text(frame, "BLOCKS 2"), "undo did not restore the deleted block");
        tap(ed, "q");

        // Load cycles through indexed levels; level 1 opens with its whole fort.
        click(ed, 1160, row(6));
        frame = run(ed, 2);
        expect(has_text(frame, "LOADED GRASSY HILL") && has_text(frame, "BLOCKS 51  GRUMPS 4"),
               "Load did not open level 1: " + ui_dump(frame));
        frame = until_stable(ed, 300);
        expect(is_stable(frame), "level 1 was not STABLE in the editor");
        click(ed, 640, 300);  // one more block makes the level dirty

        // Escape warns about unsaved changes, a second Escape leaves.
        tap(ed, "escape");
        frame = run(ed, 2);
        expect(has_text(frame, "UNSAVED CHANGES") && has_text(frame, "LEVEL EDITOR"), "no unsaved-changes warning");
        tap(ed, "escape");
        frame = run(ed, 2);
        expect(has_text(frame, "LEVELS") && has_text(frame, "QUIT"), "second Escape did not leave the editor");

        // The saved level is listed and playable.
        click(ed, 640, 368);  // LEVELS
        frame = run(ed, 2);
        expect(has_text(frame, "3  PEBBLE KEEP"), "saved level missing from level select");
        click(ed, 540, 221 + 2 * 66 + 26);
        frame = run(ed, 2);
        expect(has_text(frame, "LEVEL 3  PEBBLE KEEP") && hud_number(frame, "GRUMPS") == 1,
               "saved level did not play: " + ui_dump(frame));
        frame = run(ed, 300);
        expect(hud_number(frame, "GRUMPS") == 1 && hud_number(frame, "SCORE") == 0,
               "saved level did not start at rest");
    }

    key(mouse, "escape", 1);
    mouse.step(0);
    key(mouse, "escape", 0);
    frame = run(mouse, 2);
    expect(!frame.quit && has_text(frame, "LEVELS"), "Escape in a level did not open the menu");
}
