#include "threepoint/lua_project.hpp"

#include "threepoint/project.hpp"
#include "threepoint/world.hpp"

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>
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
    static int action(lua_State* lua);
    static int pointer(lua_State* lua);
    static int ray(lua_State* lua);
    static int set_camera(lua_State* lua);
    static int entity_create(lua_State* lua);
    static int entity_destroy(lua_State* lua);
    static int entity_exists(lua_State* lua);
    static int entities(lua_State* lua);
    static int entity_name(lua_State* lua);
    static int component_set(lua_State* lua);
    static int component_get(lua_State* lua);
    static int component_remove(lua_State* lua);
    static glm::vec3 vector_field(lua_State* lua, int table, const char* field,
                                  glm::vec3 fallback, bool positive);
    static glm::vec3 vector_arg(lua_State* lua, int index, bool positive);

    fs::path root;
    lua_State* lua = nullptr;
    uint32_t width = 960;
    uint32_t height = 540;
    double time_seconds = 0.0;
    double fixed_accumulator = 0.0;
    bool wants_quit = false;
    int start_ref = LUA_NOREF;
    int fixed_ref = LUA_NOREF;
    int update_ref = LUA_NOREF;
    int draw_ref = LUA_NOREF;
    SceneFrame frame;
    World world;
    InputState input;
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

float optional_number(lua_State* lua, int table, const char* field, float fallback,
                      const char* section = "camera") {
    lua_getfield(lua, table, field);
    if (lua_isnil(lua, -1)) {
        lua_pop(lua, 1);
        return fallback;
    }
    if (!lua_isnumber(lua, -1) || !std::isfinite(lua_tonumber(lua, -1))) {
        throw std::invalid_argument(std::string(section) + "." + field + " must be finite");
    }
    const float value = float(lua_tonumber(lua, -1));
    lua_pop(lua, 1);
    return value;
}

void push_vec3(lua_State* lua, const glm::vec3& value) {
    lua_createtable(lua, 3, 0);
    for (int i = 0; i < 3; ++i) {
        lua_pushnumber(lua, value[i]);
        lua_rawseti(lua, -2, i + 1);
    }
}

EntityId checked_id(lua_State* lua, int index) {
    const lua_Integer id = luaL_checkinteger(lua, index);
    if (id <= 0) luaL_argerror(lua, index, "entity ID must be positive");
    return EntityId(id);
}

bool optional_boolean(lua_State* lua, int table, const char* field, bool fallback) {
    lua_getfield(lua, table, field);
    if (lua_isnil(lua, -1)) {
        lua_pop(lua, 1);
        return fallback;
    }
    if (!lua_isboolean(lua, -1)) {
        throw std::invalid_argument(std::string(field) + " must be boolean");
    }
    const bool value = lua_toboolean(lua, -1) != 0;
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
    lua_pop(lua, 1);

    lua_getfield(lua, -1, "input");
    if (!lua_istable(lua, -1)) throw std::runtime_error("game.lua input must be a table");
    lua_pushnil(lua);
    while (lua_next(lua, -2) != 0) {
        if (lua_type(lua, -2) != LUA_TSTRING || !lua_istable(lua, -1)) {
            throw std::runtime_error("input actions must map names to binding tables");
        }
        const std::string name(lua_tostring(lua, -2));
        bool bound = false;
        const struct {
            const char* field;
            InputDevice device;
        } sources[] = {{"keyboard", InputDevice::Keyboard},
                       {"mouse_button", InputDevice::MouseButton},
                       {"gamepad_button", InputDevice::GamepadButton},
                       {"gamepad_axis", InputDevice::GamepadAxis}};
        for (const auto& source : sources) {
            lua_getfield(lua, -1, source.field);
            if (!lua_isnil(lua, -1)) {
                if (lua_type(lua, -1) != LUA_TSTRING) {
                    throw std::runtime_error("input binding must name a control");
                }
                input.bind(name, source.device, lua_tostring(lua, -1));
                bound = true;
            }
            lua_pop(lua, 1);
        }
        lua_pushnil(lua);
        while (lua_next(lua, -2) != 0) {
            if (lua_type(lua, -2) != LUA_TSTRING) {
                throw std::runtime_error("input binding field must be a string");
            }
            const char* field = lua_tostring(lua, -2);
            const bool recognized = std::any_of(std::begin(sources), std::end(sources),
                [field](const auto& source) { return std::strcmp(field, source.field) == 0; });
            if (!recognized) {
                throw std::runtime_error("unknown input binding field: " + std::string(field));
            }
            lua_pop(lua, 1);
        }
        if (!bound) throw std::runtime_error("input action has no binding: " + name);
        lua_pop(lua, 1);
    }
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
    add_function("action", &action);
    add_function("pointer", &pointer);
    add_function("screen_ray", &ray);
    add_function("set_camera", &set_camera);
    add_function("entity_create", &entity_create);
    add_function("entity_destroy", &entity_destroy);
    add_function("entity_exists", &entity_exists);
    add_function("entities", &entities);
    add_function("entity_name", &entity_name);
    add_function("component_set", &component_set);
    add_function("component_get", &component_get);
    add_function("component_remove", &component_remove);
    return 1;
}

glm::vec3 LuaProject::Impl::vector_arg(lua_State* lua, int index, bool positive) {
    if (!lua_istable(lua, index) || lua_rawlen(lua, index) != 3)
        throw std::invalid_argument("expected a vector of three numbers");
    glm::vec3 value;
    for (int i = 0; i < 3; ++i) {
        lua_rawgeti(lua, index, i + 1);
        if (!lua_isnumber(lua, -1))
            throw std::invalid_argument("expected a vector of three numbers");
        const double component = lua_tonumber(lua, -1);
        if (!std::isfinite(component) || std::abs(component) > 1000000.0 ||
            (positive && component <= 0.0)) {
            throw std::invalid_argument(positive ? "scale must be positive and finite"
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
    try {
        TransformComponent transform;
        transform.position = vector_field(lua, 1, "position", glm::vec3(0), false);
        transform.rotation = vector_field(lua, 1, "rotation", glm::vec3(0), false);
        transform.scale = vector_field(lua, 1, "scale", glm::vec3(1), true);
        const EntityId id = state.world.create();
        state.world.set_transform(id, transform);
        state.world.set_mesh(id, {"builtin:cube", true});
        lua_pushinteger(lua, lua_Integer(id));
    } catch (const std::exception& error) {
        return luaL_error(lua, "%s", error.what());
    }
    return 1;
}

int LuaProject::Impl::set_rotation(lua_State* lua) {
    if (lua_gettop(lua) != 2) return luaL_error(lua, "set_rotation() expects an ID and a vector");
    Impl& state = context(lua);
    const EntityId id = checked_id(lua, 1);
    try {
        const glm::vec3 value = vector_arg(lua, 2, false);
        auto transform = state.world.transform(id);
        if (!transform) throw std::invalid_argument("entity has no transform");
        transform->rotation = value;
        state.world.set_transform(id, *transform);
    } catch (const std::invalid_argument& error) {
        return luaL_error(lua, "%s", error.what());
    }
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

int LuaProject::Impl::action(lua_State* lua) {
    if (lua_gettop(lua) != 1) return luaL_error(lua, "action() expects one name");
    const char* name = luaL_checkstring(lua, 1);
    try {
        lua_pushnumber(lua, context(lua).input.action(name));
    } catch (const std::invalid_argument& error) {
        return luaL_error(lua, "%s", error.what());
    }
    return 1;
}

int LuaProject::Impl::pointer(lua_State* lua) {
    if (lua_gettop(lua) != 0) return luaL_error(lua, "pointer() expects no arguments");
    const auto [x, y] = context(lua).input.pointer();
    lua_pushnumber(lua, x);
    lua_pushnumber(lua, y);
    return 2;
}

int LuaProject::Impl::ray(lua_State* lua) {
    if (lua_gettop(lua) != 2) return luaL_error(lua, "screen_ray() expects x and y");
    const double x = luaL_checknumber(lua, 1);
    const double y = luaL_checknumber(lua, 2);
    Impl& state = context(lua);
    try {
        PerspectiveCamera camera = state.frame.camera;
        if (const auto active = state.world.active_camera()) camera = state.world.camera(*active)->camera;
        const WorldRay world = screen_ray(camera, state.width, state.height, x, y);
        lua_createtable(lua, 0, 2);
        push_vec3(lua, world.origin);
        lua_setfield(lua, -2, "origin");
        push_vec3(lua, world.direction);
        lua_setfield(lua, -2, "direction");
    } catch (const std::invalid_argument& error) {
        return luaL_error(lua, "%s", error.what());
    }
    return 1;
}

int LuaProject::Impl::set_camera(lua_State* lua) {
    if (lua_gettop(lua) != 1) return luaL_error(lua, "set_camera() expects one table");
    luaL_checktype(lua, 1, LUA_TTABLE);
    Impl& state = context(lua);
    try {
        PerspectiveCamera camera = state.frame.camera;
        if (const auto active = state.world.active_camera()) camera = state.world.camera(*active)->camera;
        camera.eye = vector_field(lua, 1, "eye", camera.eye, false);
        camera.target = vector_field(lua, 1, "target", camera.target, false);
        camera.up = vector_field(lua, 1, "up", camera.up, false);
        camera.vertical_fov_degrees = optional_number(lua, 1, "fov", camera.vertical_fov_degrees);
        camera.near_plane = optional_number(lua, 1, "near", camera.near_plane);
        camera.far_plane = optional_number(lua, 1, "far", camera.far_plane);
        camera.view_projection(float(state.width) / float(state.height));
        if (const auto active = state.world.active_camera()) {
            state.world.set_camera(*active, {camera, true});
        } else {
            state.frame.camera = camera;
        }
    } catch (const std::invalid_argument& error) {
        return luaL_error(lua, "%s", error.what());
    }
    return 0;
}

int LuaProject::Impl::entity_create(lua_State* lua) {
    if (lua_gettop(lua) > 1) return luaL_error(lua, "entity_create() accepts one optional name");
    const char* name = luaL_optstring(lua, 1, "");
    try {
        lua_pushinteger(lua, lua_Integer(context(lua).world.create(name)));
    } catch (const std::exception& error) {
        return luaL_error(lua, "%s", error.what());
    }
    return 1;
}

int LuaProject::Impl::entity_destroy(lua_State* lua) {
    if (lua_gettop(lua) != 1) return luaL_error(lua, "entity_destroy() expects an ID");
    const EntityId id = checked_id(lua, 1);
    try { context(lua).world.destroy(id); }
    catch (const std::invalid_argument& error) { return luaL_error(lua, "%s", error.what()); }
    return 0;
}

int LuaProject::Impl::entity_exists(lua_State* lua) {
    if (lua_gettop(lua) != 1) return luaL_error(lua, "entity_exists() expects an ID");
    lua_pushboolean(lua, context(lua).world.exists(checked_id(lua, 1)));
    return 1;
}

int LuaProject::Impl::entities(lua_State* lua) {
    if (lua_gettop(lua) != 0) return luaL_error(lua, "entities() expects no arguments");
    const auto ids = context(lua).world.entities();
    lua_createtable(lua, int(ids.size()), 0);
    for (size_t i = 0; i < ids.size(); ++i) {
        lua_pushinteger(lua, lua_Integer(ids[i]));
        lua_rawseti(lua, -2, lua_Integer(i + 1));
    }
    return 1;
}

int LuaProject::Impl::entity_name(lua_State* lua) {
    if (lua_gettop(lua) != 1) return luaL_error(lua, "entity_name() expects an ID");
    try { lua_pushstring(lua, context(lua).world.name(checked_id(lua, 1)).c_str()); }
    catch (const std::invalid_argument& error) { return luaL_error(lua, "%s", error.what()); }
    return 1;
}

int LuaProject::Impl::component_set(lua_State* lua) {
    if (lua_gettop(lua) != 3) return luaL_error(lua, "component_set() expects ID, kind, table");
    const EntityId id = checked_id(lua, 1);
    const char* kind = luaL_checkstring(lua, 2);
    luaL_checktype(lua, 3, LUA_TTABLE);
    Impl& state = context(lua);
    try {
        if (std::strcmp(kind, "transform") == 0) {
            TransformComponent value = state.world.transform(id).value_or(TransformComponent{});
            value.position = vector_field(lua, 3, "position", value.position, false);
            value.rotation = vector_field(lua, 3, "rotation", value.rotation, false);
            value.scale = vector_field(lua, 3, "scale", value.scale, true);
            lua_getfield(lua, 3, "parent");
            if (!lua_isnil(lua, -1)) {
                if (!lua_isinteger(lua, -1) || lua_tointeger(lua, -1) < 0)
                    throw std::invalid_argument("transform.parent must be an entity ID or zero");
                const lua_Integer parent = lua_tointeger(lua, -1);
                value.parent = parent == 0 ? std::nullopt
                    : std::optional<EntityId>(EntityId(parent));
            }
            lua_pop(lua, 1);
            state.world.set_transform(id, value);
        } else if (std::strcmp(kind, "camera") == 0) {
            CameraComponent value = state.world.camera(id).value_or(CameraComponent{});
            value.camera.eye = vector_field(lua, 3, "eye", value.camera.eye, false);
            value.camera.target = vector_field(lua, 3, "target", value.camera.target, false);
            value.camera.up = vector_field(lua, 3, "up", value.camera.up, false);
            value.camera.vertical_fov_degrees = optional_number(lua, 3, "fov", value.camera.vertical_fov_degrees);
            value.camera.near_plane = optional_number(lua, 3, "near", value.camera.near_plane);
            value.camera.far_plane = optional_number(lua, 3, "far", value.camera.far_plane);
            value.active = optional_boolean(lua, 3, "active", value.active);
            state.world.set_camera(id, value);
        } else if (std::strcmp(kind, "light") == 0) {
            LightComponent value = state.world.light(id).value_or(LightComponent{});
            lua_getfield(lua, 3, "type");
            if (!lua_isnil(lua, -1)) {
                if (lua_type(lua, -1) != LUA_TSTRING)
                    throw std::invalid_argument("light.type must be a string");
                const char* type = lua_tostring(lua, -1);
                if (std::strcmp(type, "point") == 0) value.type = LightComponent::Type::Point;
                else if (std::strcmp(type, "directional") == 0) value.type = LightComponent::Type::Directional;
                else throw std::invalid_argument("light.type must be point or directional");
            }
            lua_pop(lua, 1);
            value.color = vector_field(lua, 3, "color", value.color, false);
            value.direction = vector_field(lua, 3, "direction", value.direction, false);
            value.intensity = optional_number(lua, 3, "intensity", value.intensity, "light");
            state.world.set_light(id, value);
        } else if (std::strcmp(kind, "mesh") == 0) {
            MeshComponent value = state.world.mesh(id).value_or(MeshComponent{});
            lua_getfield(lua, 3, "mesh");
            if (!lua_isnil(lua, -1)) {
                if (lua_type(lua, -1) != LUA_TSTRING)
                    throw std::invalid_argument("mesh.mesh must be a string");
                value.mesh = lua_tostring(lua, -1);
            }
            lua_pop(lua, 1);
            value.visible = optional_boolean(lua, 3, "visible", value.visible);
            state.world.set_mesh(id, value);
        } else if (std::strcmp(kind, "script") == 0) {
            ScriptComponent value = state.world.script(id).value_or(ScriptComponent{});
            lua_getfield(lua, 3, "path");
            if (!lua_isnil(lua, -1)) {
                if (lua_type(lua, -1) != LUA_TSTRING)
                    throw std::invalid_argument("script.path must be a string");
                value.path = lua_tostring(lua, -1);
            }
            lua_pop(lua, 1);
            value.enabled = optional_boolean(lua, 3, "enabled", value.enabled);
            const std::string file = state.project_file(value.path).string();
            if (luaL_loadfilex(lua, file.c_str(), "t") != LUA_OK) {
                const std::string error = lua_tostring(lua, -1);
                lua_pop(lua, 1);
                throw std::runtime_error("script component: " + error);
            }
            lua_pop(lua, 1);
            state.world.set_script(id, value);
        } else {
            throw std::invalid_argument(std::string("unknown component kind: ") + kind);
        }
    } catch (const std::exception& error) {
        return luaL_error(lua, "%s", error.what());
    }
    return 0;
}

int LuaProject::Impl::component_get(lua_State* lua) {
    if (lua_gettop(lua) != 2) return luaL_error(lua, "component_get() expects ID and kind");
    const EntityId id = checked_id(lua, 1);
    const char* kind = luaL_checkstring(lua, 2);
    const World& world = context(lua).world;
    try {
        if (std::strcmp(kind, "transform") == 0) {
            const auto value = world.transform(id);
            if (!value) { lua_pushnil(lua); return 1; }
            lua_createtable(lua, 0, 4);
            push_vec3(lua, value->position); lua_setfield(lua, -2, "position");
            push_vec3(lua, value->rotation); lua_setfield(lua, -2, "rotation");
            push_vec3(lua, value->scale); lua_setfield(lua, -2, "scale");
            if (value->parent) {
                lua_pushinteger(lua, lua_Integer(*value->parent));
                lua_setfield(lua, -2, "parent");
            }
        } else if (std::strcmp(kind, "camera") == 0) {
            const auto value = world.camera(id);
            if (!value) { lua_pushnil(lua); return 1; }
            lua_createtable(lua, 0, 7);
            push_vec3(lua, value->camera.eye); lua_setfield(lua, -2, "eye");
            push_vec3(lua, value->camera.target); lua_setfield(lua, -2, "target");
            push_vec3(lua, value->camera.up); lua_setfield(lua, -2, "up");
            lua_pushnumber(lua, value->camera.vertical_fov_degrees); lua_setfield(lua, -2, "fov");
            lua_pushnumber(lua, value->camera.near_plane); lua_setfield(lua, -2, "near");
            lua_pushnumber(lua, value->camera.far_plane); lua_setfield(lua, -2, "far");
            lua_pushboolean(lua, value->active); lua_setfield(lua, -2, "active");
        } else if (std::strcmp(kind, "light") == 0) {
            const auto value = world.light(id);
            if (!value) { lua_pushnil(lua); return 1; }
            lua_createtable(lua, 0, 4);
            lua_pushstring(lua, value->type == LightComponent::Type::Point ? "point" : "directional");
            lua_setfield(lua, -2, "type");
            push_vec3(lua, value->color); lua_setfield(lua, -2, "color");
            push_vec3(lua, value->direction); lua_setfield(lua, -2, "direction");
            lua_pushnumber(lua, value->intensity); lua_setfield(lua, -2, "intensity");
        } else if (std::strcmp(kind, "mesh") == 0) {
            const auto value = world.mesh(id);
            if (!value) { lua_pushnil(lua); return 1; }
            lua_createtable(lua, 0, 2);
            lua_pushstring(lua, value->mesh.c_str()); lua_setfield(lua, -2, "mesh");
            lua_pushboolean(lua, value->visible); lua_setfield(lua, -2, "visible");
        } else if (std::strcmp(kind, "script") == 0) {
            const auto value = world.script(id);
            if (!value) { lua_pushnil(lua); return 1; }
            lua_createtable(lua, 0, 2);
            lua_pushstring(lua, value->path.c_str()); lua_setfield(lua, -2, "path");
            lua_pushboolean(lua, value->enabled); lua_setfield(lua, -2, "enabled");
        } else {
            return luaL_error(lua, "unknown component kind: %s", kind);
        }
    } catch (const std::invalid_argument& error) {
        return luaL_error(lua, "%s", error.what());
    }
    return 1;
}

int LuaProject::Impl::component_remove(lua_State* lua) {
    if (lua_gettop(lua) != 2) return luaL_error(lua, "component_remove() expects ID and kind");
    const EntityId id = checked_id(lua, 1);
    const char* kind = luaL_checkstring(lua, 2);
    World& world = context(lua).world;
    bool removed = false;
    try {
        if (std::strcmp(kind, "transform") == 0) removed = world.remove_transform(id);
        else if (std::strcmp(kind, "camera") == 0) removed = world.remove_camera(id);
        else if (std::strcmp(kind, "light") == 0) removed = world.remove_light(id);
        else if (std::strcmp(kind, "mesh") == 0) removed = world.remove_mesh(id);
        else if (std::strcmp(kind, "script") == 0) removed = world.remove_script(id);
        else return luaL_error(lua, "unknown component kind: %s", kind);
    } catch (const std::invalid_argument& error) {
        return luaL_error(lua, "%s", error.what());
    }
    lua_pushboolean(lua, removed);
    return 1;
}

SceneFrame LuaProject::Impl::step(double dt) {
    if (!std::isfinite(dt) || dt < 0.0 || dt > 0.25) {
        throw std::invalid_argument("frame delta must be finite and between 0 and 0.25 seconds");
    }
    if (wants_quit) {
        SceneFrame scene = world.snapshot(frame.camera);
        scene.quit = true;
        return scene;
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
    SceneFrame scene = world.snapshot(frame.camera);
    scene.quit = wants_quit;
    return scene;
}

LuaProject::LuaProject(const fs::path& root) : impl_(std::make_unique<Impl>(root)) {}
LuaProject::~LuaProject() = default;
uint32_t LuaProject::width() const { return impl_->width; }
uint32_t LuaProject::height() const { return impl_->height; }
void LuaProject::inject(const InputEvent& event) { impl_->input.inject(event); }
void LuaProject::set_pointer(double x, double y) { impl_->input.set_pointer(x, y); }
void LuaProject::set_viewport(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0 || width > 8192 || height > 8192) {
        throw std::invalid_argument("viewport size must be from 1 to 8192");
    }
    impl_->width = width;
    impl_->height = height;
}
SceneFrame LuaProject::step(double dt) { return impl_->step(dt); }

}  // namespace threepoint
