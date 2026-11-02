#pragma once

#include "acuppa/save_data.hpp"

#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

struct lua_State;

namespace acuppa {

// Converts the Lua value at index with the save-data rules: finite numbers,
// integers, booleans, strings, and acyclic tables with string or integer
// keys, at most 16 levels and 10,000 entries. Throws std::invalid_argument.
SaveValue save_value_from_lua(lua_State* lua, int index);

// Readable, deterministic Lua data text: "return { ... }" with two-space
// indentation, arrays first, sorted string keys, and shortest round-trip
// floats. Throws std::invalid_argument for invalid values or over 1 MiB.
std::string serialize_data(const SaveValue& value);

// Runs text as data only: a fresh Lua state without libraries or globals,
// text chunks only, and memory and instruction caps. The result must be a
// table. Errors name the file.
SaveValue parse_data(std::string_view text, const std::string& name);

// Checks a game.lua `writable` entry: a relative directory without "..",
// hidden components, or the export-skipped saves/tests/build roots.
std::string normalize_writable_directory(const std::string& directory);

struct DataWritePolicy {
    std::vector<std::string> writable;        // normalized directories
    std::set<std::string> protected_files;    // normalized scene/prefab/script paths
};

// Atomically replaces a project-relative .lua file inside a writable
// directory, preserving an existing file's permissions. The existing file,
// if any, must itself be valid data. Throws without touching existing files.
void write_data_file(const std::filesystem::path& project_root, const std::string& relative,
                     const SaveValue& value, const DataWritePolicy& policy);

// Reads any project-relative .lua data file without following symlinks.
// Returns nullopt when the file does not exist.
std::optional<SaveValue> read_data_file(const std::filesystem::path& project_root,
                                        const std::string& relative);

}  // namespace acuppa
