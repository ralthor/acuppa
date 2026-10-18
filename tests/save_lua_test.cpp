#include "threepoint/lua_project.hpp"

#include <filesystem>
#include <stdexcept>

namespace fs = std::filesystem;

void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main(int argc, char* argv[]) {
    if (argc != 3) throw std::runtime_error("source and temporary project paths required");
    const fs::path work = argv[2];
    fs::remove_all(work);
    fs::copy(argv[1], work, fs::copy_options::recursive);
    for (int run = 0; run < 3; ++run) {
        threepoint::LuaProject project(work, "save");
        const auto frame = project.step(0.0);
        expect(frame.cubes.size() == 1 && frame.cubes[0].position.x == float(run),
               "saved game state did not survive a fresh project instance");
    }
}
