#include "threepoint/lua_project.hpp"

#include "threepoint/project.hpp"

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>

namespace threepoint {
namespace fs = std::filesystem;

struct LuaProject::Impl {
    explicit Impl(const fs::path& directory);
    ~Impl() { if (lua) lua_close(lua); }

    SceneFrame step(double dt);
    fs::path project_file(const std::string& relative) const;
    void load_table(const fs::path& file);
    void call(int reference, const char* name, double dt, bool pass_dt);
    void install_module();

    static Impl& context(lua_State* lua) {
        return *static_cast<Impl*>(lua_touserdata(lua, lua_upvalueindex(1)));
    }
    static int open_module(lua_State* lua);
    static int spawn(lua_State* lua);
    static int set_rotation(lua_State* lua);
    static int quit(lua_State* lua);
    static int time(lua_State* lua);
    static glm::vec3 vector_field(lua_State* lua, int table, const char* field,
                                  glm::vec3 fallback, bool positive);
    static glm::vec3 vector_arg(lua_State* lua, int index, bool positive);

    fs::path root;
    lua_State* lua = nullptr;
    uint32_t width = 960;
    uint32_t height = 540;
    uint64_t next_id = 1;
    double time_seconds = 0.0;
    double fixed_accumulator = 0.0;
    bool wants_quit = false;
    int start_ref = LUA_NOREF;
    int fixed_ref = LUA_NOREF;
    int update_ref = LUA_NOREF;
    int draw_ref = LUA_NOREF;
    SceneFrame frame;
};

namespace {

std::string checked_string(lua_State* lua, int table, const char* field) {
    lua_getfield(lua, table, field);
    if (lua_type(lua, -1) != LUA_TSTRING) {
        lua_pop(lua, 1);
        throw std::runtime_error(std::string("project field '") + field + "' must be a string");
    }
    std::string value(lua_tostring(lua, -1));
    lua_pop(lua, 1);
    return value;
}

uint32_t dimension(lua_State* lua, int table, const char* field) {
    lua_getfield(lua, table, field);
    if (!lua_isinteger(lua, -1) || lua_tointeger(lua, -1) < 1 ||
        lua_tointeger(lua, -1) > 8192) {
        lua_pop(lua, 1);
        throw std::runtime_error(std::string("window.") + field +
                                 " must be an integer from 1 to 8192");
    }
    const auto value = uint32_t(lua_tointeger(lua, -1));
    lua_pop(lua, 1);
    return value;
}

int callback_ref(lua_State* lua, const char* name, bool required) {
    lua_getfield(lua, -1, name);
    if (lua_isnil(lua, -1) && !required) {
        lua_pop(lua, 1);
        return LUA_NOREF;
    }
    if (!lua_isfunction(lua, -1)) {
        lua_pop(lua, 1);
        throw std::runtime_error(std::string("script callback '") + name +
                                 "' must be a function");
    }
    return luaL_ref(lua, LUA_REGISTRYINDEX);
}

}  // namespace

LuaProject::Impl::Impl(const fs::path& directory) {
    const auto paths = require_project(directory);
    root = fs::canonical(paths.root);
    lua = luaL_newstate();
    if (!lua) throw std::runtime_error("could not create Lua state");
    std::unique_ptr<lua_State, decltype(&lua_close)> guard(lua, lua_close);
    luaL_openlibs(lua);
    install_module();

    load_table(paths.manifest);
    lua_getfield(lua, -1, "window");
    if (!lua_istable(lua, -1)) throw std::runtime_error("game.lua window must be a table");
    width = dimension(lua, -1, "width");
    height = dimension(lua, -1, "height");
    lua_pop(lua, 1);
    const std::string first_scene = checked_string(lua, -1, "first_scene");
    lua_getfield(lua, -1, "scenes");
    if (!lua_istable(lua, -1)) throw std::runtime_error("game.lua scenes must be a table");
    const std::string scene_path = checked_string(lua, -1, first_scene.c_str());
    lua_pop(lua, 2);

    load_table(project_file(scene_path));
    lua_getfield(lua, -1, "entities");
    if (!lua_istable(lua, -1)) throw std::runtime_error("scene entities must be a table");
    lua_pushnil(lua);
    const bool has_entities = lua_next(lua, -2) != 0;
    if (has_entities) lua_pop(lua, 2);
    lua_pop(lua, 1);
    if (has_entities) {
        throw std::runtime_error("source scene entity declarations arrive in Phase 6; "
                                 "create cubes in start() for now");
    }
    const std::string script_path = checked_string(lua, -1, "script");
    lua_pop(lua, 1);
    load_table(project_file(script_path));
    start_ref = callback_ref(lua, "start", true);
    fixed_ref = callback_ref(lua, "fixed_update", true);
    update_ref = callback_ref(lua, "update", true);
    draw_ref = callback_ref(lua, "draw", false);
    lua_pop(lua, 1);
    call(start_ref, "start", 0.0, false);
    guard.release();
}

fs::path LuaProject::Impl::project_file(const std::string& relative) const {
    const fs::path path(relative);
    if (relative.empty() || path.is_absolute()) {
        throw std::runtime_error("project file path must be relative: " + relative);
    }
    std::error_code error;
    const fs::path resolved = fs::weakly_canonical(root / path, error);
    if (error) throw std::runtime_error("cannot resolve project file: " + relative);
    const fs::path inside = fs::relative(resolved, root, error);
    if (error || inside.empty() || *inside.begin() == ".." ||
        !fs::is_regular_file(resolved)) {
        throw std::runtime_error("project file is missing or outside project: " + relative);
    }
    return resolved;
}

void LuaProject::Impl::load_table(const fs::path& file) {
    const std::string name = file.string();
    if (luaL_loadfilex(lua, name.c_str(), "t") != LUA_OK ||
        lua_pcall(lua, 0, 1, 0) != LUA_OK) {
        const std::string error = lua_tostring(lua, -1);
        lua_pop(lua, 1);
        throw std::runtime_error(name + ": " + error);
    }
    if (!lua_istable(lua, -1)) {
        lua_pop(lua, 1);
        throw std::runtime_error(name + " must return a table");
    }
}

void LuaProject::Impl::call(int reference, const char* name, double dt, bool pass_dt) {
    if (reference == LUA_NOREF) return;
    lua_rawgeti(lua, LUA_REGISTRYINDEX, reference);
    if (pass_dt) lua_pushnumber(lua, dt);
    if (lua_pcall(lua, pass_dt ? 1 : 0, 0, 0) != LUA_OK) {
        const std::string error = lua_tostring(lua, -1);
        lua_pop(lua, 1);
        throw std::runtime_error(std::string("Lua ") + name + "(): " + error);
    }
}

void LuaProject::Impl::install_module() {
    lua_getglobal(lua, "package");
    lua_getfield(lua, -1, "preload");
    lua_pushlightuserdata(lua, this);
    lua_pushcclosure(lua, &open_module, 1);
    lua_setfield(lua, -2, "threepoint");
    lua_pop(lua, 2);
}

int LuaProject::Impl::open_module(lua_State* lua) {
    auto* state = &context(lua);
    lua_newtable(lua);
    const auto add_function = [lua, state](const char* name, lua_CFunction function) {
        lua_pushlightuserdata(lua, state);
        lua_pushcclosure(lua, function, 1);
        lua_setfield(lua, -2, name);
    };
    add_function("spawn", &spawn);
    add_function("set_rotation", &set_rotation);
    add_function("quit", &quit);
    add_function("time", &time);
    return 1;
}

glm::vec3 LuaProject::Impl::vector_arg(lua_State* lua, int index, bool positive) {
    luaL_checktype(lua, index, LUA_TTABLE);
    if (lua_rawlen(lua, index) != 3) luaL_argerror(lua, index, "expected three numbers");
    glm::vec3 value;
    for (int i = 0; i < 3; ++i) {
        lua_rawgeti(lua, index, i + 1);
        const double component = luaL_checknumber(lua, -1);
        if (!std::isfinite(component) || std::abs(component) > 1000000.0 ||
            (positive && component <= 0.0)) {
            luaL_argerror(lua, index, positive ? "scale must be positive and finite"
                                              : "vector components must be finite");
        }
        value[i] = float(component);
        lua_pop(lua, 1);
    }
    return value;
}

glm::vec3 LuaProject::Impl::vector_field(lua_State* lua, int table, const char* field,
                                         glm::vec3 fallback, bool positive) {
    lua_getfield(lua, table, field);
    if (lua_isnil(lua, -1)) {
        lua_pop(lua, 1);
        return fallback;
    }
    const glm::vec3 result = vector_arg(lua, lua_gettop(lua), positive);
    lua_pop(lua, 1);
    return result;
}

int LuaProject::Impl::spawn(lua_State* lua) {
    if (lua_gettop(lua) != 1) return luaL_error(lua, "spawn() expects one table");
    Impl& state = context(lua);
    luaL_checktype(lua, 1, LUA_TTABLE);
    lua_getfield(lua, 1, "mesh");
    const char* mesh = luaL_checkstring(lua, -1);
    if (std::strcmp(mesh, "builtin:cube") != 0) {
        return luaL_argerror(lua, 1, "mesh must be 'builtin:cube'");
    }
    lua_pop(lua, 1);
    CubeInstance cube;
    cube.position = vector_field(lua, 1, "position", glm::vec3(0), false);
    cube.rotation = vector_field(lua, 1, "rotation", glm::vec3(0), false);
    cube.scale = vector_field(lua, 1, "scale", glm::vec3(1), true);
    if (state.frame.cubes.size() >= 10000) return luaL_error(lua, "cube limit reached");
    cube.id = state.next_id++;
    state.frame.cubes.push_back(cube);
    lua_pushinteger(lua, lua_Integer(cube.id));
    return 1;
}

int LuaProject::Impl::set_rotation(lua_State* lua) {
    if (lua_gettop(lua) != 2) return luaL_error(lua, "set_rotation() expects an ID and a vector");
    Impl& state = context(lua);
    const lua_Integer id = luaL_checkinteger(lua, 1);
    const glm::vec3 value = vector_arg(lua, 2, false);
    const auto found = std::find_if(state.frame.cubes.begin(), state.frame.cubes.end(),
        [id](const CubeInstance& cube) { return cube.id == uint64_t(id); });
    if (id <= 0 || found == state.frame.cubes.end()) return luaL_error(lua, "unknown cube ID");
    found->rotation = value;
    return 0;
}

int LuaProject::Impl::quit(lua_State* lua) {
    if (lua_gettop(lua) != 0) return luaL_error(lua, "quit() expects no arguments");
    context(lua).wants_quit = true;
    return 0;
}

int LuaProject::Impl::time(lua_State* lua) {
    if (lua_gettop(lua) != 0) return luaL_error(lua, "time() expects no arguments");
    lua_pushnumber(lua, context(lua).time_seconds);
    return 1;
}

SceneFrame LuaProject::Impl::step(double dt) {
    if (!std::isfinite(dt) || dt < 0.0 || dt > 0.25) {
        throw std::invalid_argument("frame delta must be finite and between 0 and 0.25 seconds");
    }
    if (wants_quit) {
        frame.quit = true;
        return frame;
    }
    constexpr double fixed_dt = 1.0 / 60.0;
    time_seconds += dt;
    fixed_accumulator += dt;
    unsigned fixed_steps = 0;
    while (fixed_accumulator + 1e-12 >= fixed_dt && fixed_steps < 8 && !wants_quit) {
        call(fixed_ref, "fixed_update", fixed_dt, true);
        fixed_accumulator -= fixed_dt;
        ++fixed_steps;
    }
    if (fixed_steps == 8) fixed_accumulator = std::min(fixed_accumulator, fixed_dt);
    if (!wants_quit) call(update_ref, "update", dt, true);
    if (!wants_quit) call(draw_ref, "draw", 0.0, false);
    frame.quit = wants_quit;
    return frame;
}

LuaProject::LuaProject(const fs::path& root) : impl_(std::make_unique<Impl>(root)) {}
LuaProject::~LuaProject() = default;
uint32_t LuaProject::width() const { return impl_->width; }
uint32_t LuaProject::height() const { return impl_->height; }
SceneFrame LuaProject::step(double dt) { return impl_->step(dt); }

}  // namespace threepoint
