#include "acuppa/lua_project.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace {
void expect(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
std::string slurp(const fs::path& file) {
    std::ifstream input(file);
    std::stringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}
}  // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc != 3) throw std::runtime_error("source and temporary project paths required");
        const fs::path work = argv[2];
        fs::remove_all(work);
        fs::copy(argv[1], work, fs::copy_options::recursive);
        fs::remove_all(work / "saves");
        fs::remove_all(work / "levels/generated");
        std::string first;
        for (int run = 1; run <= 2; ++run) {
            acuppa::LuaProject project(work, "data_files");
            const auto frame = project.step(0.0);
            expect(frame.cubes.size() == 1 && frame.cubes[0].position.x == float(run),
                   "data file did not persist across fresh project instances");
            const std::string text = slurp(work / "levels/generated/written.lua");
            expect(text.find("  runs = " + std::to_string(run) + ",\n") != std::string::npos,
                   "run count missing:\n" + text);
            if (run == 1) first = text;
        }
        const std::string text = slurp(work / "levels/generated/written.lua");
        for (const char* line : {"return {\n", "  [10] = \"sparse\",\n", "  big = 9007199254740992.0,\n",
                                 "  flags = {true, false},\n", "  gravity = -9.81,\n",
                                 "  name = \"written \\\"quoted\\\"\\n\",\n", "  [\"odd key\"] = 1,\n",
                                 "  pos = {1.5, -2.25, 3},\n", "      x = -0.3,\n"})
            expect(text.find(line) != std::string::npos, std::string("missing line: ") + line + text);
        expect(text.find("[10]") < text.find("big") && text.find("big") < text.find("blocks"),
               "keys are not sorted");
        expect(!fs::exists(fs::path(argv[1]) / "levels/generated"), "test wrote into the source tree");
        std::cout << "Lua data_write/data_read scene passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "data_files_lua_test: " << error.what() << '\n';
        return 1;
    }
}
