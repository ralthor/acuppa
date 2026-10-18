#include "threepoint/lua_project.hpp"

#include <stdexcept>

int main(int argc, char* argv[]) {
    if (argc != 2) throw std::runtime_error("feature scene path required");
    threepoint::LuaProject project(argv[1], "api_contract",
                                    threepoint::LuaProject::Mode::Game, false);
    if (!project.step(1.0 / 60.0).quit)
        throw std::runtime_error("public quit call did not stop the scene");
}
