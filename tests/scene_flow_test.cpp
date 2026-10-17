#include "threepoint/lua_project.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void write(const fs::path& path, const std::string& contents) {
    std::ofstream output(path);
    if (!output || !(output << contents)) throw std::runtime_error("could not write fixture");
}

int main(int argc, char* argv[]) {
    if (argc != 3) throw std::runtime_error("source and temporary fixture paths required");
    const fs::path source = argv[1];
    const fs::path work = argv[2];
    fs::remove_all(work);
    fs::copy(source, work, fs::copy_options::recursive);
    write(work / "game.lua", R"(return {
      window={width=1280,height=720}, first_scene="alternate",
      scenes={main="scenes/main.scene.lua", alternate="scenes/alternate.scene.lua"},
      prefabs={probe="prefabs/probe.prefab.lua"}, input={}
    })");
    {
        threepoint::LuaProject project(work);
        auto frame = project.step(1.0 / 60.0);
        expect(frame.cubes.size() == 1 && frame.cubes[0].id == 2,
               "source scene did not create its cube");
        expect(std::abs(frame.cubes[0].parent_transform[3].x - 1.0f) < 0.001f,
               "source parent transform was not applied");
        frame = project.step(1.0 / 60.0);
        expect(frame.cubes.size() == 1 && frame.cubes[0].id == 2 &&
               std::abs(frame.cubes[0].position.x) < 0.001f,
               "scene reload did not recreate the source cube");
        frame = project.step(1.0 / 60.0);
        expect(frame.cubes.size() == 1 && frame.cubes[0].id == 2,
               "reloaded scene did not run per-entity scripts");
        frame = project.step(1.0 / 60.0);
        expect(frame.cubes.size() == 1 && frame.cubes[0].id == 1,
               "scene switch did not reset the world and run main scene start");
        expect(std::abs(frame.cubes[0].rotation.y) < 0.001f,
               "switched scene should start at time zero");
    }

    write(work / "scenes/alternate.scene.lua", R"(return {
      entities={{name="same"},{name="same"}}, script="scripts/alternate.lua"
    })");
    bool duplicate_rejected = false;
    try { threepoint::LuaProject project(work); }
    catch (const std::runtime_error& error) {
        duplicate_rejected = std::string(error.what()).find("duplicate scene entity name") !=
                             std::string::npos;
    }
    expect(duplicate_rejected, "duplicate source entity name was accepted");

    write(work / "scenes/alternate.scene.lua", R"(return {
      entities={{name="child", transform={parent="absent"}}},
      script="scripts/alternate.lua"
    })");
    bool parent_rejected = false;
    try { threepoint::LuaProject project(work); }
    catch (const std::runtime_error& error) {
        parent_rejected = std::string(error.what()).find("unknown parent entity") !=
                          std::string::npos;
    }
    expect(parent_rejected, "unknown source parent was accepted");

    write(work / "scenes/alternate.scene.lua", R"(return {
      entities={{name="child", tranform={position={1,0,0}}}},
      script="scripts/alternate.lua"
    })");
    bool typo_rejected = false;
    try { threepoint::LuaProject project(work); }
    catch (const std::runtime_error& error) {
        typo_rejected = std::string(error.what()).find("unknown entity field") !=
                        std::string::npos;
    }
    expect(typo_rejected, "misspelled source entity field was accepted");

    write(work / "scenes/alternate.scene.lua", R"(return {
      entities={[2]={name="child"}}, script="scripts/alternate.lua"
    })");
    bool sparse_rejected = false;
    try { threepoint::LuaProject project(work); }
    catch (const std::runtime_error& error) {
        sparse_rejected = std::string(error.what()).find("dense array") !=
                          std::string::npos;
    }
    expect(sparse_rejected, "sparse source entity array was accepted");
    fs::remove_all(work);
}
