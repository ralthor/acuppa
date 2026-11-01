#include "acuppa/lua_project.hpp"

#include <stdexcept>

// The scene script asserts material, damping, and CCD effects at fixed ticks.
int main(int argc, char* argv[]) {
    if (argc != 2) throw std::runtime_error("feature scene path required");
    acuppa::LuaProject game(argv[1], "physics_materials", acuppa::LuaProject::Mode::Game,
                            false);
    for (int i = 0; i < 240; ++i) game.step(1.0 / 60.0);
}
