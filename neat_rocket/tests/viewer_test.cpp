// Runs the NEAT viewer project headlessly through the unchanged engine core:
// waiting state, demo and live sources, replay motion, the network toggle,
// generation browsing, live updates, and damaged run files.
#include "acuppa/lua_project.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message.c_str());
        ++failures;
    }
}

bool has_text(const acuppa::SceneFrame& frame, const std::string& value) {
    for (const auto& item : frame.ui)
        if (item.text.find(value) != std::string::npos) return true;
    return false;
}

const acuppa::ImportedInstance* find(const acuppa::SceneFrame& frame, const std::string& source) {
    for (const auto& item : frame.imported)
        if (item.source.find(source) != std::string::npos) return &item;
    return nullptr;
}

size_t count(const acuppa::SceneFrame& frame, const std::string& source) {
    size_t total = 0;
    for (const auto& item : frame.imported)
        if (item.source.find(source) != std::string::npos) ++total;
    return total;
}

acuppa::SceneFrame run(acuppa::LuaProject& game, int frames) {
    auto frame = game.step(0);
    for (int i = 0; i < frames; ++i) frame = game.step(1.0 / 60.0);
    return frame;
}

acuppa::SceneFrame tap(acuppa::LuaProject& game, const std::string& key) {
    game.inject({acuppa::InputDevice::Keyboard, key, 1});
    game.step(1.0 / 60.0);
    game.inject({acuppa::InputDevice::Keyboard, key, 0});
    return game.step(1.0 / 60.0);
}

void write(const fs::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    out << text;
    if (!out) throw std::runtime_error("cannot write " + path.string());
}

std::string read(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

fs::path fresh_copy(const fs::path& source, const fs::path& work) {
    fs::remove_all(work);
    fs::create_directories(work.parent_path());
    fs::copy(source, work, fs::copy_options::recursive);
    fs::remove_all(work / "runs");
    fs::remove_all(work / "saves");
    return work;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: neat_viewer_test <viewer project> <work directory>\n");
        return 2;
    }
    const fs::path source = argv[1], work_root = argv[2];

    {
        // Without runs/ or demo/, the viewer waits for a trainer.
        const auto work = fresh_copy(source, work_root / "empty");
        fs::remove_all(work / "demo");
        acuppa::LuaProject game(work, "main", acuppa::LuaProject::Mode::Game, false);
        auto frame = run(game, 5);
        check(has_text(frame, "WAITING FOR TRAINING"), "empty viewer should wait for training");
        check(!find(frame, "champion.gltf"), "no lander should show without a run");
    }

    {
        // The checked-in demo plays, the champion moves, and the panel toggles.
        const auto work = fresh_copy(source, work_root / "demo");
        acuppa::LuaProject game(work, "main", acuppa::LuaProject::Mode::Game, false);
        auto frame = run(game, 2);
        check(has_text(frame, "DEMO RUN") && has_text(frame, "GEN 400") &&
                  has_text(frame, "LIVE: SHOWING THE NEWEST"),
              "viewer should start live on the newest demo generation");
        check(has_text(frame, "CHAMPION NETWORK") && has_text(frame, "FUEL") &&
                  has_text(frame, "THROTTLE") && has_text(frame, "TILT X"), "network panel should show by default");
        check(frame.ui.size() <= 256, "UI stays within the engine's item limit");
        const auto* start = find(frame, "champion.gltf");
        check(start != nullptr, "champion lander should be visible");
        check(count(frame, "ghost_") >= 1, "species ghosts should be visible");
        check(has_text(frame, "WIND 0.") && has_text(frame, "TOWARD 0") &&
                  has_text(frame, "ENGINE X") && has_text(frame, "DIFFICULTY 23%") &&
                  has_text(frame, "SCORE ") && has_text(frame, "+ SPEED "),
              "wind, engine, difficulty, and the champion's score parts should show");
        check(count(frame, "scorch.gltf") >= 2, "rockets should show their engine bells");
        const float y0 = start ? start->transform[3].y : 0;
        frame = run(game, 60);
        const auto* later = find(frame, "champion.gltf");
        check(later && std::abs(later->transform[3].y - y0) > 0.5f,
              "champion should move during the replay");
        check(count(frame, "flame.gltf") >= 1, "a firing engine should show its flame");
        // The gen 150 champion lands: its beacon blinks and the label says so.
        bool beacon = false, landed_label = false;
        for (int i = 0; i < 60 * 9; ++i) {
            frame = game.step(1.0 / 60.0);
            beacon = beacon || count(frame, "beacon.gltf") > 0;
            landed_label = landed_label || has_text(frame, "LANDED") && !has_text(frame, "CRASHED");
        }
        check(beacon && landed_label, "a landed champion should light its beacon and label");
        frame = tap(game, "h");
        check(has_text(frame, "HOW A FLIGHT IS SCORED") && has_text(frame, "SPEED    UP TO 30"),
              "H should show the scoring rules");
        frame = tap(game, "h");
        check(!has_text(frame, "HOW A FLIGHT IS SCORED"), "H should hide the scoring rules");

        frame = tap(game, "n");
        check(!has_text(frame, "CHAMPION NETWORK") && has_text(frame, "SHOW NETWORK"),
              "N should hide the network panel");
        frame = tap(game, "n");
        check(has_text(frame, "CHAMPION NETWORK") && has_text(frame, "HIDE NETWORK"),
              "N should show the network panel again");

        // The corner button toggles the same panel with a mouse click.
        game.set_pointer(1280 - 100, 28);
        game.inject({acuppa::InputDevice::MouseButton, "left", 1});
        game.step(1.0 / 60.0);
        game.inject({acuppa::InputDevice::MouseButton, "left", 0});
        frame = game.step(1.0 / 60.0);
        check(!has_text(frame, "CHAMPION NETWORK"), "clicking the corner button should hide the panel");

        frame = tap(game, "left");
        check(has_text(frame, "GEN 300") && has_text(frame, "REWATCHING GEN 300 (10 OF 11") &&
                  has_text(frame, "L: BACK TO LIVE"),
              "left arrow should step back one listed generation and say how to return");
        for (int i = 0; i < 12; ++i) frame = tap(game, "left");
        check(has_text(frame, "GEN 1 ") && has_text(frame, "(1 OF 11"),
              "browsing should stop at the first generation");
        // Gen 3's champion and species leader both crash: they are blown into
        // debris and labeled.
        frame = tap(game, "right");
        check(has_text(frame, "GEN 3 "), "right arrow should step to a newer generation");
        bool wrecked = false;
        for (int i = 0; i < 60 * 10 && !wrecked; ++i) {
            frame = game.step(1.0 / 60.0);
            // Six debris pieces per rocket (three in its color, three scorched)
            // replace the hull, nose, engine bell, and flame.
            wrecked = count(frame, "champion.gltf") == 3 && count(frame, "scorch.gltf") == 6 &&
                      !find(frame, "cabin.gltf") && has_text(frame, "CRASHED");
        }
        check(wrecked, "a crashed lander should be replaced by debris and a CRASHED label");
        for (int i = 0; i < 9; ++i) frame = tap(game, "right");
        check(has_text(frame, "GEN 400") && has_text(frame, "(11 OF 11"),
              "right arrow should reach the newest generation");
        frame = tap(game, "right");
        check(has_text(frame, "LIVE: SHOWING THE NEWEST"),
              "right arrow past the newest generation should return to live mode");
        frame = tap(game, "left");
        frame = tap(game, "l");
        check(has_text(frame, "GEN 400") && has_text(frame, "LIVE"), "L should return to live");
        frame = tap(game, "f");
        check(has_text(frame, "SPEED 3X"), "F should speed up the replay");
    }

    {
        // runs/ wins over demo/, and live mode follows new generations.
        const auto work = fresh_copy(source, work_root / "live");
        fs::create_directories(work / "runs");
        fs::copy_file(work / "demo" / "gen_0001.lua", work / "runs" / "gen_0001.lua");
        fs::copy_file(work / "demo" / "gen_0003.lua", work / "runs" / "gen_0003.lua");
        write(work / "runs" / "status.lua",
              "return {format = 1, latest = 1, generations = {1}, running = true}\n");
        acuppa::LuaProject game(work, "main", acuppa::LuaProject::Mode::Game, false);
        auto frame = run(game, 2);
        check(has_text(frame, "GEN 1 ") && !has_text(frame, "DEMO") &&
                  has_text(frame, "TRAINING IS AT GEN 1"), "runs/ should replace the demo");
        write(work / "runs" / "status.lua",
              "return {format = 1, latest = 3, generations = {1, 3}, running = true}\n");
        frame = tap(game, "f");  // Triple speed keeps the wait for the replay end short.
        bool advanced = false;
        for (int i = 0; i < 60 * 8 && !advanced; ++i) {
            frame = game.step(1.0 / 60.0);
            advanced = has_text(frame, "GEN 3 ");
        }
        check(advanced, "live mode should load the next generation after the replay");

        // A damaged generation shows an error and keeps the last good replay.
        write(work / "runs" / "gen_0005.lua", "return {format = 2, landers = {{frames = \"1,2\"}}}\n");
        write(work / "runs" / "status.lua",
              "return {format = 1, latest = 5, generations = {1, 3, 5}, running = true}\n");
        frame = tap(game, "l");
        check(has_text(frame, "RUN FILE ERROR") && has_text(frame, "GEN 3 "),
              "a bad run file should be reported without dropping the replay");
        write(work / "runs" / "status.lua", "return {format = 1, generations = {}}\n");
        frame = run(game, 70);
        check(has_text(frame, "not a run status file"), "an empty status file should be reported");
        write(work / "runs" / "status.lua", "return {format = 1, latest = 1,\n");
        frame = run(game, 70);
        check(has_text(frame, "RUN FILE ERROR"), "unparsable status file should be reported");
    }

    {
        // Viewer files stay unchanged by playing them.
        const auto before = read(source / "demo" / "gen_0400.lua");
        check(!before.empty() && before.rfind("return {", 0) == 0, "demo run files are Lua data");
    }

    if (failures) {
        std::fprintf(stderr, "%d viewer check(s) failed\n", failures);
        return 1;
    }
    std::puts("viewer checks passed");
    return 0;
}
