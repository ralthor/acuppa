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
    auto first = project.step(1.0 / 60.0);
    expect(!first.quit && first.cubes.size() == 1, "start() did not create one cube");
    expect(first.cubes[0].id == 1, "cube ID is unstable");
    expect(std::abs(first.cubes[0].rotation.y - 0.015f) < 0.001f,
           "fixed/update time was not applied");
    for (int i = 1; i < 60; ++i) project.step(1.0 / 60.0);
    const auto last = project.step(0.0);
    expect(std::abs(last.cubes[0].rotation.y - 0.9f) < 0.001f,
           "the code-only scene did not animate deterministically");
    bool rejected = false;
    try { project.step(-1.0); }
    catch (const std::invalid_argument&) { rejected = true; }
    expect(rejected, "negative time delta was accepted");
}
