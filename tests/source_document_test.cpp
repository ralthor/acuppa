#include "threepoint/lua_project.hpp"
#include "threepoint/source_document.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <sys/stat.h>

namespace fs = std::filesystem;

void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main(int argc, char* argv[]) {
    if (argc != 3) throw std::runtime_error("source and temporary project paths required");
    const fs::path work = argv[2];
    fs::remove_all(work);
    fs::copy(argv[1], work, fs::copy_options::recursive);
    const fs::path scene = work / "scenes/editor.scene.lua";
    const fs::path script = work / "scripts/editor.lua";

    // A human edits the source, then the editor reads and saves the same bytes.
    std::ofstream(script, std::ios::app) << "-- hand edit\n";
    const std::string replacement = R"(local tp = require("threepoint")
return {
  start = function()
    local actor = tp.entity_find("actor")
    tp.component_set(actor, "transform", {position = {2, 0, 0}})
  end,
  fixed_update = function() end,
  update = function() end,
  draw = function() end,
}
-- hand edit
)";
    const auto permissions_before = fs::status(script).permissions();
    {
        threepoint::SourceDocument document(work, "scripts/editor.lua");
        expect(document.text().find("-- hand edit") != std::string::npos,
               "editor lost a manual script edit");
        bool rejected = false;
        try { document.replace("return {\n"); }
        catch (const std::invalid_argument&) { rejected = true; }
        expect(rejected, "editor accepted invalid Lua syntax");
        document.replace(replacement);
        expect(document.text() == replacement, "editor returned different saved bytes");
    }
    expect(fs::status(script).permissions() == permissions_before,
           "editor changed script permissions");
    {
        threepoint::SourceDocument reopened(work, "scripts/editor.lua");
        expect(reopened.text() == replacement, "script changed across save and reopen");
    }
    {
        threepoint::LuaProject runtime(work, "editor");
        const auto frame = runtime.step(0);
        expect(frame.cubes.size() == 1 && frame.cubes[0].position.x == 2 &&
               frame.cubes[0].parent_transform[3].x == 1,
               "editor script did not run from source after reopen");
    }
    {
        threepoint::SourceDocument document(work, "scenes/editor.scene.lua");
        auto updated = document.text();
        const auto position = updated.find("{1, 0, 0}");
        expect(position != std::string::npos, "fixture anchor position missing");
        updated.replace(position, 9, "{4, 0, 0}");
        updated += "-- editor comment\n";
        document.replace(updated);
        threepoint::SourceDocument reopened(work, "scenes/editor.scene.lua");
        expect(reopened.text() == updated, "scene source was reformatted or truncated");
        threepoint::LuaProject runtime(work, "editor");
        const auto frame = runtime.step(0);
        expect(frame.cubes[0].position.x == 2 && frame.cubes[0].parent_transform[3].x == 4,
               "raw scene edit did not affect runtime behavior");
    }
    {
        threepoint::SourceDocument document(work, "scripts/editor.lua");
        std::ofstream(script, std::ios::app) << "-- second hand edit\n";
        bool rejected = false;
        try { document.replace(replacement); }
        catch (const std::runtime_error&) { rejected = true; }
        expect(rejected, "editor overwrote a newer manual edit");
    }
    {
        // Computed scenes can be inspected and changed as source, even though
        // the scene-tree editor cannot flatten them into declarative data.
        threepoint::SourceDocument computed(work, "scenes/shadow.scene.lua");
        expect(!computed.text().empty(), "computed scene source unavailable");
    }
    bool rejected = false;
    try { threepoint::SourceDocument outside(work, "../outside.lua"); }
    catch (const std::invalid_argument&) { rejected = true; }
    expect(rejected, "project path traversal accepted");
    rejected = false;
    try { threepoint::SourceDocument wrong_type(work, "assets/triangle.gltf"); }
    catch (const std::invalid_argument&) { rejected = true; }
    expect(rejected, "non-Lua source accepted");
    fs::create_symlink(script, work / "script-link.lua");
    rejected = false;
    try { threepoint::SourceDocument linked(work, "script-link.lua"); }
    catch (const std::invalid_argument&) { rejected = true; }
    expect(rejected, "symlink source accepted");
}
