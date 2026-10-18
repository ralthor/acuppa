#include "threepoint/lua_project.hpp"
#include "threepoint/scene_document.hpp"

#include <filesystem>
#include <fstream>
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
    {
        threepoint::SceneDocument document(work, "editor");
        const auto entities = document.tree();
        expect(entities.size() == 2 && entities[0].name == "anchor" &&
               entities[1].name == "actor" && entities[1].parent == "anchor",
               "scene tree did not expose source hierarchy");
        expect(document.inspect("actor").find("builtin:cube") != std::string::npos,
               "inspector omitted mesh component");
        bool rejected = false;
        try { document.add_entity("actor"); }
        catch (const std::invalid_argument&) { rejected = true; }
        expect(rejected, "duplicate editor entity accepted");
        document.add_entity("editor cube");
        rejected = false;
        try { document.set_field("editor cube", "transform", "position", "function() end"); }
        catch (const std::runtime_error&) { rejected = true; }
        expect(rejected, "unsupported source value accepted");
        document.set_field("editor cube", "transform", "position", "{2, 0, 0}");
        document.set_field("editor cube", "mesh", "mesh", "\"builtin:cube\"");
        document.save();
    }
    {
        threepoint::SceneDocument reopened(work, "editor");
        expect(reopened.tree().size() == 3 &&
               reopened.inspect("editor cube").find("builtin:cube") != std::string::npos,
               "editor scene did not survive reopen");
    }
    {
        threepoint::LuaProject runtime(work, "editor");
        const auto frame = runtime.step(0);
        expect(frame.cubes.size() == 2 && frame.cubes[1].position.x == 2,
               "editor-saved scene did not run identically from source");
    }
    {
        threepoint::LuaProject preview(work, "main", threepoint::LuaProject::Mode::EditorPreview);
        expect(preview.step(0.2).cubes.empty(),
               "editor preview executed a gameplay start callback");
        threepoint::LuaProject static_scene(work, "editor",
                                            threepoint::LuaProject::Mode::EditorPreview);
        expect(static_scene.step(0.2).cubes.size() == 2,
               "editor preview omitted authored scene geometry");
    }
    bool rejected = false;
    try { threepoint::SceneDocument dynamic(work, "shadow"); }
    catch (const std::runtime_error&) { rejected = true; }
    expect(rejected, "computed Lua scene was rewritten as static data");
    {
        threepoint::SceneDocument document(work, "editor");
        std::ofstream(work / "scenes/editor.scene.lua", std::ios::app) << "-- external change\n";
        rejected = false;
        try { document.save(); }
        catch (const std::runtime_error&) { rejected = true; }
        expect(rejected, "editor overwrote an external scene change");
    }
}
