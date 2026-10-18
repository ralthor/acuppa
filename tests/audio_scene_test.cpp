#include "threepoint/lua_project.hpp"

#include <stdexcept>

int main(int argc, char* argv[]) {
    if (argc != 2) throw std::runtime_error("feature scene path required");
    threepoint::LuaProject game(argv[1], "audio", threepoint::LuaProject::Mode::Game, false);
    const auto first = game.step(0);
    if (first.cubes.size() != 2) throw std::runtime_error("audio scene geometry missing");
    for (int i = 0; i < 150; ++i) game.step(1.0 / 60.0);
    if (game.step(0).quit) throw std::runtime_error("audio scene quit unexpectedly");
}
