#include <SDL3/SDL_version.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

namespace fs = std::filesystem;

static void write_file(const fs::path& path, const char* contents) {
  std::ofstream file(path);
  file << contents;
  file.close();
  if (!file) {
    throw std::runtime_error("could not write " + path.string());
  }
}

static void init_project(const fs::path& path) {
  if (path.empty()) {
    throw std::runtime_error("init target must be a directory path");
  }
  if (fs::exists(path)) {
    throw std::runtime_error("init target already exists: " + path.string());
  }
  fs::create_directories(path / "scenes");
  fs::create_directories(path / "scripts");
  fs::create_directories(path / "assets");
  fs::create_directories(path / "tests");

  write_file(path / "game.lua", R"(return {
  api = 1,
  title = "New Game",
  window = {width = 800, height = 600},
  first_scene = "main",
  scenes = {main = "scenes/main.scene.lua"},
  input = {}
}
)");
  write_file(path / "scenes/main.scene.lua", R"(return {
  background = "#202030",
  script = "scripts/main.lua",
  entities = {},
  ui = {}
}
)");
  write_file(path / "scripts/main.lua", R"(return {
  start = function() print("Hello from ThreePoint") end
}
)");
  write_file(path / "tests/smoke.test.lua", R"(local game = dofile("game.lua")
assert(game.first_scene == "main")
)");
  std::cout << "Created ThreePoint project: " << path.string() << '\n';
}

static std::string project_file(const std::string& name) {
  const fs::path path(name);
  if (path.empty() || path.is_absolute()) {
    throw std::runtime_error(name + ": path must be relative to the project");
  }
  for (const auto& part : path) {
    if (part == "..") {
      throw std::runtime_error(name + ": path must stay inside the project");
    }
  }
  if (!fs::is_regular_file(path)) {
    throw std::runtime_error(name + ": file not found");
  }
  return path.string();
}

static int load_table(lua_State* lua, const std::string& name) {
  const std::string path = project_file(name);
  lua_settop(lua, 0);
  if (luaL_loadfile(lua, path.c_str()) != LUA_OK || lua_pcall(lua, 0, 1, 0) != LUA_OK) {
    const char* error = lua_tostring(lua, -1);
    throw std::runtime_error(error ? error : name + ": Lua error");
  }
  if (!lua_istable(lua, -1)) {
    throw std::runtime_error(name + ": expected a returned table");
  }
  return lua_gettop(lua);
}

static std::string string_field(lua_State* lua, int table, const char* key,
                                const std::string& file) {
  lua_pushstring(lua, key);
  lua_rawget(lua, table);
  if (lua_type(lua, -1) != LUA_TSTRING || lua_rawlen(lua, -1) == 0) {
    throw std::runtime_error(file + ": '" + key + "' must be a nonempty string");
  }
  std::string value = lua_tostring(lua, -1);
  lua_pop(lua, 1);
  return value;
}

static lua_Integer positive_int_field(lua_State* lua, int table, const char* key,
                                      const std::string& file) {
  lua_pushstring(lua, key);
  lua_rawget(lua, table);
  if (!lua_isinteger(lua, -1) || lua_tointeger(lua, -1) <= 0) {
    throw std::runtime_error(file + ": '" + key + "' must be a positive integer");
  }
  const lua_Integer value = lua_tointeger(lua, -1);
  lua_pop(lua, 1);
  return value;
}

static int table_field(lua_State* lua, int table, const char* key,
                       const std::string& file) {
  lua_pushstring(lua, key);
  lua_rawget(lua, table);
  if (!lua_istable(lua, -1)) {
    throw std::runtime_error(file + ": '" + key + "' must be a table");
  }
  return lua_gettop(lua);
}

static void validate_scene(lua_State* lua, const std::string& name) {
  const int scene = load_table(lua, name);
  string_field(lua, scene, "background", name);
  const std::string script = string_field(lua, scene, "script", name);
  table_field(lua, scene, "entities", name);
  lua_pop(lua, 1);
  table_field(lua, scene, "ui", name);
  lua_pop(lua, 1);

  const std::string path = project_file(script);
  if (luaL_loadfile(lua, path.c_str()) != LUA_OK) {
    const char* error = lua_tostring(lua, -1);
    throw std::runtime_error(error ? error : script + ": Lua syntax error");
  }
  lua_pop(lua, 1);  // Syntax-check only; callbacks run in the next step.
}

static void run_project(const fs::path& path) {
  if (!fs::is_directory(path)) {
    throw std::runtime_error("project directory not found: " + path.string());
  }
  fs::current_path(path);
  std::unique_ptr<lua_State, decltype(&lua_close)> lua(luaL_newstate(), lua_close);
  if (!lua) {
    throw std::runtime_error("could not initialize Lua");
  }
  luaL_openlibs(lua.get());

  const int game = load_table(lua.get(), "game.lua");
  if (positive_int_field(lua.get(), game, "api", "game.lua") != 1) {
    throw std::runtime_error("game.lua: unsupported API version (expected 1)");
  }
  const std::string title = string_field(lua.get(), game, "title", "game.lua");
  const std::string first_scene = string_field(lua.get(), game, "first_scene", "game.lua");
  const int window = table_field(lua.get(), game, "window", "game.lua");
  positive_int_field(lua.get(), window, "width", "game.lua");
  positive_int_field(lua.get(), window, "height", "game.lua");
  lua_pop(lua.get(), 1);
  table_field(lua.get(), game, "input", "game.lua");
  lua_pop(lua.get(), 1);

  const int scenes_table = table_field(lua.get(), game, "scenes", "game.lua");
  std::vector<std::pair<std::string, std::string>> scenes;
  bool found_first = false;
  lua_pushnil(lua.get());
  while (lua_next(lua.get(), scenes_table)) {
    if (lua_type(lua.get(), -2) != LUA_TSTRING || lua_type(lua.get(), -1) != LUA_TSTRING) {
      throw std::runtime_error("game.lua: 'scenes' must map names to relative file paths");
    }
    std::string name = lua_tostring(lua.get(), -2);
    std::string scene_path = lua_tostring(lua.get(), -1);
    if (name.empty() || scene_path.empty()) {
      throw std::runtime_error("game.lua: scene names and paths must be nonempty");
    }
    found_first |= name == first_scene;
    scenes.emplace_back(std::move(name), std::move(scene_path));
    lua_pop(lua.get(), 1);
  }
  if (!found_first) {
    throw std::runtime_error("game.lua: first_scene '" + first_scene + "' is not in scenes");
  }
  for (const auto& scene : scenes) {
    validate_scene(lua.get(), scene.second);
  }

  std::cout << "ThreePoint: started '" << title << "' at scene '" << first_scene << "'\n";
  std::cout << "ThreePoint: stopped\n";
}

int main(int argc, char* argv[]) {
  if (argc == 2 && std::string(argv[1]) == "--version") {
    const int sdl = SDL_GetVersion();
    std::cout << "ThreePoint: SDL " << SDL_VERSIONNUM_MAJOR(sdl) << '.'
              << SDL_VERSIONNUM_MINOR(sdl) << '.' << SDL_VERSIONNUM_MICRO(sdl)
              << ", " << LUA_VERSION << '\n';
    return 0;
  }
  if (argc != 3) {
    std::cerr << "Usage: threepoint init <directory> | threepoint run <directory>\n";
    return 2;
  }
  try {
    const std::string command = argv[1];
    if (command == "init") {
      init_project(argv[2]);
    } else if (command == "run") {
      run_project(argv[2]);
    } else {
      std::cerr << "Unknown command: " << command << '\n';
      return 2;
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "threepoint: " << error.what() << '\n';
    return 1;
  }
}
