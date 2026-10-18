#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
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

struct Runtime {
  SDL_Renderer* renderer = nullptr;
  bool drawing = false;
  bool quit = false;
  double camera_x = 0;
  double camera_y = 0;
  double camera_zoom = 1;
};

struct SdlSession {
  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;

  ~SdlSession() {
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
  }
};

static bool parse_color(const char* value, SDL_Color& color) {
  const std::string text(value);
  if ((text.size() != 7 && text.size() != 9) || text[0] != '#') return false;
  unsigned bytes[4] = {0, 0, 0, 255};
  for (size_t index = 0; index < (text.size() - 1) / 2; ++index) {
    unsigned parsed = 0;
    for (size_t digit = 1 + index * 2; digit < 3 + index * 2; ++digit) {
      const char ch = text[digit];
      const unsigned nibble = ch >= '0' && ch <= '9' ? ch - '0'
                            : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10
                            : ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 : 16;
      if (nibble == 16) return false;
      parsed = parsed * 16 + nibble;
    }
    bytes[index] = parsed;
  }
  color = {static_cast<Uint8>(bytes[0]), static_cast<Uint8>(bytes[1]),
           static_cast<Uint8>(bytes[2]), static_cast<Uint8>(bytes[3])};
  return true;
}

static Runtime& runtime(lua_State* lua) {
  return *static_cast<Runtime*>(lua_touserdata(lua, lua_upvalueindex(1)));
}

static double finite_number(lua_State* lua, int index) {
  const double value = luaL_checknumber(lua, index);
  if (!std::isfinite(value)) luaL_argerror(lua, index, "must be finite");
  return value;
}

static void require_drawing(lua_State* lua) {
  if (!runtime(lua).drawing) luaL_error(lua, "drawing functions may only run in draw()");
}

static SDL_Color draw_color(lua_State* lua, int index) {
  SDL_Color color;
  if (!parse_color(luaL_checkstring(lua, index), color)) {
    luaL_argerror(lua, index, "expected #RRGGBB or #RRGGBBAA");
  }
  if (!SDL_SetRenderDrawColor(runtime(lua).renderer, color.r, color.g, color.b, color.a)) {
    luaL_error(lua, "could not set drawing color: %s", SDL_GetError());
  }
  return color;
}

static int lua_quit(lua_State* lua) {
  runtime(lua).quit = true;
  return 0;
}

static int lua_camera_set(lua_State* lua) {
  Runtime& state = runtime(lua);
  const double x = finite_number(lua, 1);
  const double y = finite_number(lua, 2);
  const double zoom = finite_number(lua, 3);
  if (zoom <= 0) return luaL_argerror(lua, 3, "must be positive");
  state.camera_x = x;
  state.camera_y = y;
  state.camera_zoom = zoom;
  return 0;
}

static int lua_camera_to_world(lua_State* lua) {
  Runtime& state = runtime(lua);
  const double x = finite_number(lua, 1);
  const double y = finite_number(lua, 2);
  lua_pushnumber(lua, x / state.camera_zoom + state.camera_x);
  lua_pushnumber(lua, y / state.camera_zoom + state.camera_y);
  return 2;
}

static int lua_draw_rect(lua_State* lua) {
  require_drawing(lua);
  Runtime& state = runtime(lua);
  const double x = finite_number(lua, 1);
  const double y = finite_number(lua, 2);
  const double width = finite_number(lua, 3);
  const double height = finite_number(lua, 4);
  if (width <= 0 || height <= 0) return luaL_error(lua, "rectangle size must be positive");
  draw_color(lua, 5);
  const SDL_FRect rect{static_cast<float>((x - state.camera_x) * state.camera_zoom),
                       static_cast<float>((y - state.camera_y) * state.camera_zoom),
                       static_cast<float>(width * state.camera_zoom),
                       static_cast<float>(height * state.camera_zoom)};
  if (!SDL_RenderFillRect(state.renderer, &rect)) {
    return luaL_error(lua, "could not draw rectangle: %s", SDL_GetError());
  }
  return 0;
}

static int lua_draw_line(lua_State* lua) {
  require_drawing(lua);
  Runtime& state = runtime(lua);
  const double x1 = finite_number(lua, 1);
  const double y1 = finite_number(lua, 2);
  const double x2 = finite_number(lua, 3);
  const double y2 = finite_number(lua, 4);
  draw_color(lua, 5);
  if (!SDL_RenderLine(state.renderer,
                      static_cast<float>((x1 - state.camera_x) * state.camera_zoom),
                      static_cast<float>((y1 - state.camera_y) * state.camera_zoom),
                      static_cast<float>((x2 - state.camera_x) * state.camera_zoom),
                      static_cast<float>((y2 - state.camera_y) * state.camera_zoom))) {
    return luaL_error(lua, "could not draw line: %s", SDL_GetError());
  }
  return 0;
}

static int lua_draw_circle(lua_State* lua) {
  require_drawing(lua);
  Runtime& state = runtime(lua);
  const double x = finite_number(lua, 1);
  const double y = finite_number(lua, 2);
  const double radius = finite_number(lua, 3);
  if (radius <= 0) return luaL_argerror(lua, 3, "must be positive");
  const SDL_Color color = draw_color(lua, 4);
  const SDL_FColor vertex_color{color.r / 255.0f, color.g / 255.0f,
                                color.b / 255.0f, color.a / 255.0f};
  constexpr int segments = 48;
  constexpr double full_circle = 6.283185307179586;
  std::array<SDL_Vertex, segments * 3> vertices;
  const float center_x = static_cast<float>((x - state.camera_x) * state.camera_zoom);
  const float center_y = static_cast<float>((y - state.camera_y) * state.camera_zoom);
  const float screen_radius = static_cast<float>(radius * state.camera_zoom);
  for (int i = 0; i < segments; ++i) {
    const double first = full_circle * i / segments;
    const double second = full_circle * (i + 1) / segments;
    vertices[i * 3] = {{center_x, center_y}, vertex_color, {0, 0}};
    vertices[i * 3 + 1] = {{center_x + screen_radius * static_cast<float>(std::cos(first)),
                           center_y + screen_radius * static_cast<float>(std::sin(first))},
                          vertex_color, {0, 0}};
    vertices[i * 3 + 2] = {{center_x + screen_radius * static_cast<float>(std::cos(second)),
                           center_y + screen_radius * static_cast<float>(std::sin(second))},
                          vertex_color, {0, 0}};
  }
  if (!SDL_RenderGeometry(state.renderer, nullptr, vertices.data(),
                          static_cast<int>(vertices.size()), nullptr, 0)) {
    return luaL_error(lua, "could not draw circle: %s", SDL_GetError());
  }
  return 0;
}

static int lua_draw_text(lua_State* lua) {
  require_drawing(lua);
  Runtime& state = runtime(lua);
  const char* value = luaL_checkstring(lua, 1);
  const double x = finite_number(lua, 2);
  const double y = finite_number(lua, 3);
  draw_color(lua, 4);
  if (!SDL_SetRenderScale(state.renderer, static_cast<float>(state.camera_zoom),
                          static_cast<float>(state.camera_zoom))) {
    return luaL_error(lua, "could not scale text: %s", SDL_GetError());
  }
  const bool drawn = SDL_RenderDebugText(state.renderer,
                                         static_cast<float>(x - state.camera_x),
                                         static_cast<float>(y - state.camera_y), value);
  const bool reset = SDL_SetRenderScale(state.renderer, 1, 1);
  if (!drawn) return luaL_error(lua, "could not draw text: %s", SDL_GetError());
  if (!reset) return luaL_error(lua, "could not reset text scale: %s", SDL_GetError());
  return 0;
}

static void set_lua_method(lua_State* lua, Runtime* state, const char* name,
                           lua_CFunction method) {
  lua_pushlightuserdata(lua, state);
  lua_pushcclosure(lua, method, 1);
  lua_setfield(lua, -2, name);
}

static int open_acuppa(lua_State* lua) {
  auto* state = static_cast<Runtime*>(lua_touserdata(lua, lua_upvalueindex(1)));
  lua_newtable(lua);
  lua_newtable(lua);
  set_lua_method(lua, state, "quit", lua_quit);
  lua_setfield(lua, -2, "app");
  lua_newtable(lua);
  set_lua_method(lua, state, "rect", lua_draw_rect);
  set_lua_method(lua, state, "circle", lua_draw_circle);
  set_lua_method(lua, state, "line", lua_draw_line);
  set_lua_method(lua, state, "text", lua_draw_text);
  lua_setfield(lua, -2, "draw");
  lua_newtable(lua);
  set_lua_method(lua, state, "set", lua_camera_set);
  set_lua_method(lua, state, "to_world", lua_camera_to_world);
  lua_setfield(lua, -2, "camera");
  return 1;
}

static void register_acuppa(lua_State* lua, Runtime* state) {
  lua_getglobal(lua, "package");
  lua_getfield(lua, -1, "preload");
  lua_pushlightuserdata(lua, state);
  lua_pushcclosure(lua, open_acuppa, 1);
  lua_setfield(lua, -2, "acuppa");
  lua_pop(lua, 2);
}

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
  write_file(path / "scripts/main.lua", R"(local tp = require("acuppa")
local x = 40

return {
  start = function() print("Hello from ACuppa") end,
  fixed_update = function(dt)
    x = (x + 120 * dt) % 800
  end,
  draw = function()
    tp.draw.rect(x, 260, 160, 70, "#55bbff")
    tp.draw.text("ACuppa", x + 34, 290, "#102030")
  end
}
)");
  write_file(path / "tests/smoke.test.lua", R"(local game = dofile("game.lua")
assert(game.first_scene == "main")
)");
  std::cout << "Created ACuppa project: " << path.string() << '\n';
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

static int lua_traceback(lua_State* lua) {
  const char* error = lua_tostring(lua, 1);
  luaL_traceback(lua, lua, error ? error : "Lua error", 1);
  return 1;
}

static void call_lua(lua_State* lua, int arguments, int results, const std::string& file) {
  const int function = lua_gettop(lua) - arguments;
  lua_pushcfunction(lua, lua_traceback);
  lua_insert(lua, function);
  if (lua_pcall(lua, arguments, results, function) != LUA_OK) {
    const char* error = lua_tostring(lua, -1);
    const std::string message = error ? error : "Lua error";
    throw std::runtime_error(message.rfind(file + ":", 0) == 0 ? message
                                                               : file + ": " + message);
  }
  lua_remove(lua, function);
}

static int load_table(lua_State* lua, const std::string& name) {
  const std::string path = project_file(name);
  lua_settop(lua, 0);
  if (luaL_loadfile(lua, path.c_str()) != LUA_OK) {
    const char* error = lua_tostring(lua, -1);
    throw std::runtime_error(error ? error : name + ": Lua error");
  }
  call_lua(lua, 0, 1, name);
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

struct SceneInfo {
  std::string script;
  SDL_Color background;
};

static SceneInfo validate_scene(lua_State* lua, const std::string& name) {
  const int scene = load_table(lua, name);
  const std::string background = string_field(lua, scene, "background", name);
  SDL_Color color;
  if (!parse_color(background.c_str(), color)) {
    throw std::runtime_error(name + ": 'background' must be #RRGGBB or #RRGGBBAA");
  }
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
  lua_pop(lua, 1);
  return {script, color};
}

static void call_callback(lua_State* lua, int script_table, const std::string& script,
                          const char* name, bool has_dt, double dt = 1.0 / 60.0) {
  lua_pushstring(lua, name);
  lua_rawget(lua, script_table);
  if (lua_isnil(lua, -1)) {
    lua_pop(lua, 1);
    return;
  }
  if (!lua_isfunction(lua, -1)) {
    throw std::runtime_error(script + ": '" + name + "' must be a function");
  }
  if (has_dt) {
    lua_pushnumber(lua, dt);
  }
  call_lua(lua, has_dt ? 1 : 0, 0, script);
}

static void run_project(const fs::path& path) {
  if (!fs::is_directory(path)) {
    throw std::runtime_error("project directory not found: " + path.string());
  }
  fs::current_path(path);
  Runtime state;
  std::unique_ptr<lua_State, decltype(&lua_close)> lua(luaL_newstate(), lua_close);
  if (!lua) {
    throw std::runtime_error("could not initialize Lua");
  }
  luaL_openlibs(lua.get());
  register_acuppa(lua.get(), &state);

  const int game = load_table(lua.get(), "game.lua");
  if (positive_int_field(lua.get(), game, "api", "game.lua") != 1) {
    throw std::runtime_error("game.lua: unsupported API version (expected 1)");
  }
  const std::string title = string_field(lua.get(), game, "title", "game.lua");
  const std::string first_scene = string_field(lua.get(), game, "first_scene", "game.lua");
  const int window = table_field(lua.get(), game, "window", "game.lua");
  const lua_Integer width = positive_int_field(lua.get(), window, "width", "game.lua");
  const lua_Integer height = positive_int_field(lua.get(), window, "height", "game.lua");
  if (width > std::numeric_limits<int>::max() || height > std::numeric_limits<int>::max()) {
    throw std::runtime_error("game.lua: window dimensions are too large");
  }
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
  SceneInfo first;
  for (const auto& scene : scenes) {
    const SceneInfo info = validate_scene(lua.get(), scene.second);
    if (scene.first == first_scene) {
      first = info;
    }
  }

  const int script_table = load_table(lua.get(), first.script);
  SdlSession sdl;
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    throw std::runtime_error(std::string("could not initialize video: ") + SDL_GetError());
  }
  if (!SDL_CreateWindowAndRenderer(title.c_str(), static_cast<int>(width),
                                    static_cast<int>(height), 0, &sdl.window, &sdl.renderer)) {
    throw std::runtime_error(std::string("could not create window: ") + SDL_GetError());
  }
  state.renderer = sdl.renderer;
  if (!SDL_SetRenderDrawBlendMode(sdl.renderer, SDL_BLENDMODE_BLEND)) {
    throw std::runtime_error(std::string("could not enable blending: ") + SDL_GetError());
  }

  std::cout << "ACuppa: started '" << title << "' at scene '" << first_scene << "'\n";
  call_callback(lua.get(), script_table, first.script, "start", false);
  constexpr double fixed_step = 1.0 / 60.0;
  constexpr Uint64 frame_ns = 16666667;
  Uint64 previous = SDL_GetTicksNS();
  double accumulated = 0;
  while (!state.quit) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        state.quit = true;
      }
    }
    if (state.quit) break;

    const Uint64 frame_start = SDL_GetTicksNS();
    const double dt = std::min((frame_start - previous) / 1000000000.0, 0.25);
    previous = frame_start;
    accumulated += dt;
    while (accumulated >= fixed_step && !state.quit) {
      call_callback(lua.get(), script_table, first.script, "fixed_update", true);
      accumulated -= fixed_step;
    }
    if (state.quit) break;
    call_callback(lua.get(), script_table, first.script, "update", true, dt);
    if (state.quit) break;

    if (!SDL_SetRenderDrawColor(sdl.renderer, first.background.r, first.background.g,
                                 first.background.b, first.background.a) ||
        !SDL_RenderClear(sdl.renderer)) {
      throw std::runtime_error(std::string("could not clear frame: ") + SDL_GetError());
    }
    state.drawing = true;
    call_callback(lua.get(), script_table, first.script, "draw", false);
    state.drawing = false;
    if (!SDL_RenderPresent(sdl.renderer)) {
      throw std::runtime_error(std::string("could not present frame: ") + SDL_GetError());
    }
    const Uint64 elapsed = SDL_GetTicksNS() - frame_start;
    if (elapsed < frame_ns) SDL_DelayNS(frame_ns - elapsed);
  }
  std::cout << "ACuppa: stopped\n";
}

int main(int argc, char* argv[]) {
  if (argc == 2 && std::string(argv[1]) == "--version") {
    const int sdl = SDL_GetVersion();
    std::cout << "ACuppa: SDL " << SDL_VERSIONNUM_MAJOR(sdl) << '.'
              << SDL_VERSIONNUM_MINOR(sdl) << '.' << SDL_VERSIONNUM_MICRO(sdl)
              << ", " << LUA_VERSION << '\n';
    return 0;
  }
  if (argc != 3) {
    std::cerr << "Usage: acuppa init <directory> | acuppa run <directory>\n";
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
    std::cerr << "acuppa: " << error.what() << '\n';
    return 1;
  }
}
