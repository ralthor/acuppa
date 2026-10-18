#include "acuppa/lua_project.hpp"

#include <stdexcept>

int main(int argc, char* argv[]) {
    if (argc != 2) throw std::runtime_error("feature scene path required");
    acuppa::LuaProject game(argv[1], "vehicle", acuppa::LuaProject::Mode::Game, false);
    for (int i = 0; i < 211; ++i) game.step(1.0 / 60.0);
}
