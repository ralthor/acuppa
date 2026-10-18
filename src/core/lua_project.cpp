#include "threepoint/lua_project.hpp"

#include "threepoint/project.hpp"
#include "threepoint/audio_world.hpp"
#include "threepoint/physics_world.hpp"
#include "threepoint/save_data.hpp"
#include "threepoint/world.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace threepoint {
namespace fs = std::filesystem;

struct LuaProject::Impl {
    struct ScriptRefs;
    explicit Impl(const fs::path& directory, const std::string& initial_scene,
                  Mode selected_mode, bool audio_output);
    ~Impl() { if (lua) lua_close(lua); }

    SceneFrame step(double dt);
    fs::path project_file(const std::string& relative) const;
    void load_table(const fs::path& file);
    void call(int reference, const char* name, double dt, bool pass_dt);
    void install_module();
    void load_scene(const std::string& name);
    EntityId instantiate(int table, const std::map<std::string, EntityId>& names = {});
    void apply_components(EntityId id, int table, const std::map<std::string, EntityId>& names);
    void sync_scripts();
    void call_entity(int reference, const char* name, EntityId id, double dt, bool pass_dt);
    void call_entity_scripts(int ScriptRefs::*reference, const char* name, double dt, bool pass_dt);
    void clear_callbacks();
    std::shared_ptr<const GltfAsset> ensure_asset(const std::string& source);
    SceneFrame snapshot();

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
    static int set_exposure(lua_State* lua);
    static int set_ssao(lua_State* lua);
    static int set_environment(lua_State* lua);
    static int entity_create(lua_State* lua);
    static int entity_destroy(lua_State* lua);
    static int entity_exists(lua_State* lua);
    static int entities(lua_State* lua);
    static int entity_name(lua_State* lua);
    static int component_set(lua_State* lua);
    static int component_get(lua_State* lua);
    static int component_remove(lua_State* lua);
    static int entity_find(lua_State* lua);
    static int prefab_spawn(lua_State* lua);
    static int scene_name(lua_State* lua);
    static int scene_switch(lua_State* lua);
    static int scene_reload(lua_State* lua);
    static int physics_state(lua_State* lua);
    static int physics_set_velocity(lua_State* lua);
    static int physics_move(lua_State* lua);
    static int physics_set_pose(lua_State* lua);
    static int physics_apply_force(lua_State* lua);
    static int physics_apply_impulse(lua_State* lua);
    static int physics_raycast(lua_State* lua);
    static int physics_overlap_sphere(lua_State* lua);
    static int physics_contacts(lua_State* lua);
    static int timer_start(lua_State* lua);
    static int timer_cancel(lua_State* lua);
    static int timer_remaining(lua_State* lua);
    static int timer_events(lua_State* lua);
    static int save(lua_State* lua);
    static int load(lua_State* lua);
    static int ui_rect(lua_State* lua);
    static int ui_text(lua_State* lua);
    static int ui_button(lua_State* lua);
    static int ui_focus(lua_State* lua);
    static int viewport_size(lua_State* lua);
    static int sound_play(lua_State* lua);
    static int sound_stop(lua_State* lua);
    static int sound_set_volume(lua_State* lua);
    static int sound_set_position(lua_State* lua);
    static int sound_set_listener(lua_State* lua);
    static int sound_state(lua_State* lua);
    AudioWorld& ensure_audio();
    void sync_physics();
    void add_ui_element(UiElement element);
    static glm::vec3 vector_field(lua_State* lua, int table, const char* field,
                                  glm::vec3 fallback, bool positive);
    static glm::vec3 vector_arg(lua_State* lua, int index, bool positive);

    fs::path root;
    Mode mode = Mode::Game;
    bool audio_output = true;
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
    int component_set_ref = LUA_NOREF;
    struct ScriptRefs {
        std::string path;
        int start = LUA_NOREF;
        int fixed = LUA_NOREF;
        int update = LUA_NOREF;
        int draw = LUA_NOREF;
    };
    std::map<EntityId, ScriptRefs> scripts;
    std::map<std::string, std::string> scenes;
    std::map<std::string, std::string> prefabs;
    std::map<std::string, std::shared_ptr<const GltfAsset>> assets;
    std::map<std::string, std::shared_ptr<const HdrImage>> hdr_assets;
    std::string current_scene;
    std::optional<std::string> pending_scene;
    struct Timer { double remaining; double interval; bool repeat; };
    std::map<std::string, Timer> timers;
    std::vector<std::string> expired_timers;
    std::vector<std::string> previous_ui_buttons;
    std::vector<std::string> current_ui_buttons;
    std::string focused_ui_button;
    bool ui_click_edge = false;
    bool ui_enter_edge = false;
    bool ui_tab_edge = false;
    bool ui_activated = false;
    bool ui_mouse_held = false;
    bool ui_enter_held = false;
    bool ui_tab_held = false;
    bool drawing_ui = false;
    SceneFrame frame;
    World world;
    std::unique_ptr<PhysicsWorld> physics = std::make_unique<PhysicsWorld>();
    std::unique_ptr<AudioWorld> audio;
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

void validate_entity_fields(lua_State* lua, int table) {
    table = lua_absindex(lua, table);
    lua_pushnil(lua);
    while (lua_next(lua, table) != 0) {
        if (lua_type(lua, -2) != LUA_TSTRING)
            throw std::runtime_error("entity fields must be names");
        const char* field = lua_tostring(lua, -2);
        bool known = false;
        for (const char* candidate : {"name", "prefab", "transform", "camera",
                                      "light", "mesh", "script", "physics"}) {
            if (std::strcmp(field, candidate) == 0) { known = true; break; }
        }
        if (!known) throw std::runtime_error("unknown entity field: " + std::string(field));
        lua_pop(lua, 1);
    }
}

glm::quat physics_rotation(glm::vec3 angles) {
    return glm::angleAxis(angles.y, glm::vec3(0, 1, 0)) *
           glm::angleAxis(angles.x, glm::vec3(1, 0, 0)) *
           glm::angleAxis(angles.z, glm::vec3(0, 0, 1));
}

PhysicsBodyDesc physics_desc(EntityId id, const PhysicsComponent& component,
                             const TransformComponent& transform, glm::vec3 velocity) {
    PhysicsBodyDesc desc;
    desc.id = id;
    desc.motion = component.motion;
    desc.shape = component.shape;
    desc.position = transform.position;
    desc.rotation = physics_rotation(transform.rotation);
    desc.half_extents = component.half_extents;
    desc.radius = component.radius;
    desc.trigger = component.trigger;
    desc.velocity = velocity;
    return desc;
}

std::string timer_name(lua_State* lua, int index) {
    if (lua_type(lua, index) != LUA_TSTRING)
        throw std::invalid_argument("timer name must be a string");
    std::string name = lua_tostring(lua, index);
    if (name.empty() || name.size() > 64 ||
        !std::all_of(name.begin(), name.end(), [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '_';
        }))
        throw std::invalid_argument("timer name must use 1-64 letters, digits, or underscores");
    return name;
}

SaveValue from_lua(lua_State* lua, int index, unsigned depth,
                   size_t& entries, std::set<const void*>& active) {
    if (depth > 16) throw std::invalid_argument("save table nesting exceeds 16 levels");
    index = lua_absindex(lua, index);
    SaveValue value;
    switch (lua_type(lua, index)) {
    case LUA_TBOOLEAN:
        value.type = SaveValue::Type::Boolean;
        value.boolean = lua_toboolean(lua, index) != 0;
        break;
    case LUA_TNUMBER:
        if (lua_isinteger(lua, index)) {
            value.type = SaveValue::Type::Integer;
            value.integer = lua_tointeger(lua, index);
        } else {
            value.type = SaveValue::Type::Number;
            value.number = lua_tonumber(lua, index);
            if (!std::isfinite(value.number))
                throw std::invalid_argument("save number must be finite");
        }
        break;
    case LUA_TSTRING: {
        value.type = SaveValue::Type::String;
        size_t size = 0;
        const char* bytes = lua_tolstring(lua, index, &size);
        if (size > 1024 * 1024) throw std::invalid_argument("save string exceeds 1 MiB");
        value.text.assign(bytes, size);
        break;
    }
    case LUA_TTABLE: {
        value.type = SaveValue::Type::Table;
        const void* pointer = lua_topointer(lua, index);
        if (!active.insert(pointer).second)
            throw std::invalid_argument("save table contains a cycle");
        lua_pushnil(lua);
        while (lua_next(lua, index) != 0) {
            if (++entries > 10000) throw std::invalid_argument("save table exceeds 10000 entries");
            SaveKey key;
            if (lua_isinteger(lua, -2)) {
                key.integer = true;
                key.number = lua_tointeger(lua, -2);
            } else if (lua_type(lua, -2) == LUA_TSTRING) {
                size_t size = 0;
                const char* bytes = lua_tolstring(lua, -2, &size);
                if (size > 1024 * 1024)
                    throw std::invalid_argument("save key exceeds 1 MiB");
                key.text.assign(bytes, size);
            } else throw std::invalid_argument("save table keys must be strings or integers");
            value.entries.emplace_back(std::move(key), from_lua(lua, -1, depth + 1, entries, active));
            lua_pop(lua, 1);
        }
        std::sort(value.entries.begin(), value.entries.end(), [](const auto& a, const auto& b) {
            if (a.first.integer != b.first.integer) return a.first.integer;
            if (a.first.integer) return a.first.number < b.first.number;
            return a.first.text < b.first.text;
        });
        active.erase(pointer);
        break;
    }
    default: throw std::invalid_argument("save value must be a boolean, number, string, or table");
    }
    return value;
}

void push_saved(lua_State* lua, const SaveValue& value) {
    switch (value.type) {
    case SaveValue::Type::Boolean: lua_pushboolean(lua, value.boolean); break;
    case SaveValue::Type::Integer: lua_pushinteger(lua, lua_Integer(value.integer)); break;
    case SaveValue::Type::Number: lua_pushnumber(lua, value.number); break;
    case SaveValue::Type::String:
        lua_pushlstring(lua, value.text.data(), value.text.size()); break;
    case SaveValue::Type::Table:
        lua_createtable(lua, 0, int(value.entries.size()));
        for (const auto& [key, child] : value.entries) {
            if (key.integer) lua_pushinteger(lua, lua_Integer(key.number));
            else lua_pushlstring(lua, key.text.data(), key.text.size());
            push_saved(lua, child);
            lua_rawset(lua, -3);
        }
        break;
    }
}

}  // namespace

LuaProject::Impl::Impl(const fs::path& directory, const std::string& initial_scene,
                       Mode selected_mode, bool use_audio_output)
    : mode(selected_mode), audio_output(use_audio_output) {
    const auto paths = require_project(directory);
    root = fs::canonical(paths.root);
    lua = luaL_newstate();
    if (!lua) throw std::runtime_error("could not create Lua state");
    std::unique_ptr<lua_State, decltype(&lua_close)> guard(lua, lua_close);
    luaL_openlibs(lua);
    lua_getglobal(lua, "package");
    const std::string module_path = (root / "?.lua").string() + ";" +
                                    (root / "?/init.lua").string();
    lua_pushlstring(lua, module_path.data(), module_path.size());
    lua_setfield(lua, -2, "path");
    lua_pushliteral(lua, "");
    lua_setfield(lua, -2, "cpath");
    lua_pop(lua, 1);
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
    lua_pushnil(lua);
    while (lua_next(lua, -2) != 0) {
        if (lua_type(lua, -2) != LUA_TSTRING || lua_type(lua, -1) != LUA_TSTRING)
            throw std::runtime_error("game.lua scenes must map names to paths");
        scenes.emplace(lua_tostring(lua, -2), lua_tostring(lua, -1));
        lua_pop(lua, 1);
    }
    lua_pop(lua, 1);
    if (scenes.find(first_scene) == scenes.end())
        throw std::runtime_error("unknown first scene: " + first_scene);
    lua_getfield(lua, -1, "prefabs");
    if (!lua_isnil(lua, -1)) {
        if (!lua_istable(lua, -1)) throw std::runtime_error("game.lua prefabs must be a table");
        lua_pushnil(lua);
        while (lua_next(lua, -2) != 0) {
            if (lua_type(lua, -2) != LUA_TSTRING || lua_type(lua, -1) != LUA_TSTRING)
                throw std::runtime_error("game.lua prefabs must map names to paths");
            prefabs.emplace(lua_tostring(lua, -2), lua_tostring(lua, -1));
            lua_pop(lua, 1);
        }
    }
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

    load_scene(initial_scene.empty() ? first_scene : initial_scene);
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

std::shared_ptr<const GltfAsset> LuaProject::Impl::ensure_asset(const std::string& source) {
    if (source == "builtin:cube") return {};
    const auto found = assets.find(source);
    if (found != assets.end()) return found->second;
    if (assets.size() >= 128) throw std::runtime_error("project glTF asset limit reached");
    auto asset = std::make_shared<GltfAsset>(load_gltf(root, source));
    assets.emplace(source, asset);
    return asset;
}

SceneFrame LuaProject::Impl::snapshot() {
    SceneFrame scene = world.snapshot(frame.camera);
    scene.exposure = frame.exposure;
    scene.ssao_quality = frame.ssao_quality;
    scene.ssao_radius = frame.ssao_radius;
    scene.ssao_strength = frame.ssao_strength;
    scene.environment = frame.environment;
    scene.environment_intensity = frame.environment_intensity;
    scene.ui = frame.ui;
    for (auto& instance : scene.imported) instance.asset = ensure_asset(instance.source);
    return scene;
}

void LuaProject::Impl::clear_callbacks() {
    for (int* reference : {&start_ref, &fixed_ref, &update_ref, &draw_ref}) {
        if (*reference != LUA_NOREF) luaL_unref(lua, LUA_REGISTRYINDEX, *reference);
        *reference = LUA_NOREF;
    }
    for (auto& [_, refs] : scripts) {
        for (int reference : {refs.start, refs.fixed, refs.update, refs.draw}) {
            if (reference != LUA_NOREF) luaL_unref(lua, LUA_REGISTRYINDEX, reference);
        }
    }
    scripts.clear();
}

void LuaProject::Impl::apply_components(EntityId id, int table,
                                         const std::map<std::string, EntityId>& names) {
    table = lua_absindex(lua, table);
    validate_entity_fields(lua, table);
    lua_getfield(lua, table, "prefab");
    if (!lua_isnil(lua, -1)) {
        if (lua_type(lua, -1) != LUA_TSTRING)
            throw std::runtime_error("entity prefab must be a name");
        const std::string name = lua_tostring(lua, -1);
        const auto found = prefabs.find(name);
        if (found == prefabs.end()) throw std::runtime_error("unknown prefab: " + name);
        lua_pop(lua, 1);
        load_table(project_file(found->second));
        lua_getfield(lua, -1, "prefab");
        const bool nested = !lua_isnil(lua, -1);
        lua_pop(lua, 1);
        if (nested) throw std::runtime_error("nested prefabs are not supported: " + name);
        apply_components(id, -1, names);
        lua_pop(lua, 1);
    } else {
        lua_pop(lua, 1);
    }
    lua_getfield(lua, table, "name");
    if (!lua_isnil(lua, -1)) {
        if (lua_type(lua, -1) != LUA_TSTRING)
            throw std::runtime_error("entity name must be a string");
        world.set_name(id, lua_tostring(lua, -1));
    }
    lua_pop(lua, 1);
    for (const char* kind : {"transform", "camera", "light", "mesh", "script", "physics"}) {
        lua_getfield(lua, table, kind);
        if (lua_isnil(lua, -1)) { lua_pop(lua, 1); continue; }
        if (!lua_istable(lua, -1))
            throw std::runtime_error(std::string("entity ") + kind + " must be a table");
        if (std::strcmp(kind, "transform") == 0) {
            lua_getfield(lua, -1, "parent");
            if (lua_type(lua, -1) == LUA_TSTRING) {
                const std::string parent_name = lua_tostring(lua, -1);
                const auto parent = names.find(parent_name);
                if (parent == names.end())
                    throw std::runtime_error("unknown parent entity: " + parent_name);
                lua_pop(lua, 1);
                lua_pushinteger(lua, lua_Integer(parent->second));
                lua_setfield(lua, -2, "parent");
            } else {
                lua_pop(lua, 1);
            }
        }
        lua_rawgeti(lua, LUA_REGISTRYINDEX, component_set_ref);
        lua_pushinteger(lua, lua_Integer(id));
        lua_pushstring(lua, kind);
        lua_pushvalue(lua, -4);
        if (lua_pcall(lua, 3, 0, 0) != LUA_OK) {
            const std::string error = lua_tostring(lua, -1);
            lua_pop(lua, 2);
            throw std::runtime_error(std::string("entity ") + kind + ": " + error);
        }
        lua_pop(lua, 1);
    }
}

EntityId LuaProject::Impl::instantiate(int table,
                                        const std::map<std::string, EntityId>& names) {
    table = lua_absindex(lua, table);
    lua_getfield(lua, table, "name");
    if (!lua_isnil(lua, -1) && lua_type(lua, -1) != LUA_TSTRING)
        throw std::runtime_error("entity name must be a string");
    const std::string name = lua_isnil(lua, -1) ? "" : lua_tostring(lua, -1);
    lua_pop(lua, 1);
    const EntityId id = world.create(name);
    try {
        apply_components(id, table, names);
    } catch (...) {
        physics->remove(id);
        world.destroy(id);
        throw;
    }
    return id;
}

void LuaProject::Impl::load_scene(const std::string& name) {
    const auto found = scenes.find(name);
    if (found == scenes.end()) throw std::runtime_error("unknown scene: " + name);
    clear_callbacks();
    world = World{};
    physics = std::make_unique<PhysicsWorld>();
    audio.reset();
    frame.exposure = 1.0f;
    frame.ssao_quality = 0;
    frame.ssao_radius = 0.35f;
    frame.ssao_strength = 2.0f;
    frame.environment.reset();
    frame.environment_intensity = 0.0f;
    frame.ui.clear();
    previous_ui_buttons.clear();
    current_ui_buttons.clear();
    focused_ui_button.clear();
    current_scene = name;
    timers.clear();
    expired_timers.clear();
    time_seconds = 0.0;
    fixed_accumulator = 0.0;
    load_table(project_file(found->second));
    lua_getfield(lua, -1, "entities");
    if (!lua_istable(lua, -1)) throw std::runtime_error("scene entities must be a table");
    const size_t count = lua_rawlen(lua, -1);
    size_t entries = 0;
    lua_pushnil(lua);
    while (lua_next(lua, -2) != 0) {
        if (!lua_isinteger(lua, -2) || lua_tointeger(lua, -2) < 1 ||
            size_t(lua_tointeger(lua, -2)) > count)
            throw std::runtime_error("scene entities must be a dense array");
        ++entries;
        lua_pop(lua, 1);
    }
    if (entries != count) throw std::runtime_error("scene entities must be a dense array");
    std::map<std::string, EntityId> names;
    std::vector<EntityId> ids;
    ids.reserve(count);
    for (size_t i = 1; i <= count; ++i) {
        lua_rawgeti(lua, -1, lua_Integer(i));
        if (!lua_istable(lua, -1)) throw std::runtime_error("scene entity must be a table");
        lua_getfield(lua, -1, "name");
        if (!lua_isnil(lua, -1) && lua_type(lua, -1) != LUA_TSTRING)
            throw std::runtime_error("scene entity name must be a string");
        const std::string entity_name = lua_isnil(lua, -1) ? "" : lua_tostring(lua, -1);
        lua_pop(lua, 1);
        const EntityId id = world.create(entity_name);
        if (!entity_name.empty() && !names.emplace(entity_name, id).second)
            throw std::runtime_error("duplicate scene entity name: " + entity_name);
        ids.push_back(id);
        lua_pop(lua, 1);
    }
    for (size_t i = 1; i <= count; ++i) {
        lua_rawgeti(lua, -1, lua_Integer(i));
        apply_components(ids[i - 1], -1, names);
        lua_pop(lua, 1);
    }
    lua_pop(lua, 1);
    const std::string script_path = checked_string(lua, -1, "script");
    lua_pop(lua, 1);
    if (mode == Mode::EditorPreview) {
        project_file(script_path); // Validate the declared script path without executing it.
        return;
    }
    load_table(project_file(script_path));
    start_ref = callback_ref(lua, "start", true);
    fixed_ref = callback_ref(lua, "fixed_update", true);
    update_ref = callback_ref(lua, "update", true);
    draw_ref = callback_ref(lua, "draw", false);
    lua_pop(lua, 1);
    call(start_ref, "start", 0.0, false);
    sync_scripts();
}

void LuaProject::Impl::call_entity(int reference, const char* name, EntityId id,
                                    double dt, bool pass_dt) {
    if (reference == LUA_NOREF) return;
    lua_rawgeti(lua, LUA_REGISTRYINDEX, reference);
    lua_pushinteger(lua, lua_Integer(id));
    if (pass_dt) lua_pushnumber(lua, dt);
    if (lua_pcall(lua, pass_dt ? 2 : 1, 0, 0) != LUA_OK) {
        const std::string error = lua_tostring(lua, -1);
        lua_pop(lua, 1);
        throw std::runtime_error("entity " + std::to_string(id) + " Lua " + name + "(): " + error);
    }
}

void LuaProject::Impl::sync_scripts() {
    for (auto it = scripts.begin(); it != scripts.end();) {
        const auto component = world.exists(it->first) ? world.script(it->first) : std::nullopt;
        if (component && component->enabled && component->path == it->second.path) {
            ++it;
            continue;
        }
        for (int reference : {it->second.start, it->second.fixed,
                              it->second.update, it->second.draw}) {
            if (reference != LUA_NOREF) luaL_unref(lua, LUA_REGISTRYINDEX, reference);
        }
        it = scripts.erase(it);
    }
    for (const EntityId id : world.entities()) {
        if (!world.exists(id)) continue;
        const auto component = world.script(id);
        if (!component || !component->enabled || scripts.count(id)) continue;
        load_table(project_file(component->path));
        ScriptRefs refs;
        refs.path = component->path;
        refs.start = callback_ref(lua, "start", false);
        refs.fixed = callback_ref(lua, "fixed_update", false);
        refs.update = callback_ref(lua, "update", false);
        refs.draw = callback_ref(lua, "draw", false);
        lua_pop(lua, 1);
        scripts.emplace(id, refs);
        call_entity(refs.start, "start", id, 0.0, false);
    }
}

void LuaProject::Impl::call_entity_scripts(int ScriptRefs::*reference, const char* name,
                                            double dt, bool pass_dt) {
    for (const EntityId id : world.entities()) {
        if (!world.exists(id)) continue;
        const auto component = world.script(id);
        const auto it = scripts.find(id);
        if (component && component->enabled && it != scripts.end())
            call_entity(it->second.*reference, name, id, dt, pass_dt);
    }
}

void LuaProject::Impl::install_module() {
    lua_getglobal(lua, "package");
    lua_getfield(lua, -1, "preload");
    lua_pushlightuserdata(lua, this);
    lua_pushcclosure(lua, &open_module, 1);
    lua_setfield(lua, -2, "threepoint");
    lua_pop(lua, 2);
    lua_pushlightuserdata(lua, this);
    lua_pushcclosure(lua, &component_set, 1);
    component_set_ref = luaL_ref(lua, LUA_REGISTRYINDEX);
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
    add_function("set_exposure", &set_exposure);
    add_function("set_ssao", &set_ssao);
    add_function("set_environment", &set_environment);
    add_function("entity_create", &entity_create);
    add_function("entity_destroy", &entity_destroy);
    add_function("entity_exists", &entity_exists);
    add_function("entities", &entities);
    add_function("entity_name", &entity_name);
    add_function("component_set", &component_set);
    add_function("component_get", &component_get);
    add_function("component_remove", &component_remove);
    add_function("entity_find", &entity_find);
    add_function("prefab_spawn", &prefab_spawn);
    add_function("scene_name", &scene_name);
    add_function("scene_switch", &scene_switch);
    add_function("scene_reload", &scene_reload);
    add_function("physics_state", &physics_state);
    add_function("physics_set_velocity", &physics_set_velocity);
    add_function("physics_move", &physics_move);
    add_function("physics_set_pose", &physics_set_pose);
    add_function("physics_apply_force", &physics_apply_force);
    add_function("physics_apply_impulse", &physics_apply_impulse);
    add_function("physics_raycast", &physics_raycast);
    add_function("physics_overlap_sphere", &physics_overlap_sphere);
    add_function("physics_contacts", &physics_contacts);
    add_function("sound_play", &sound_play);
    add_function("sound_stop", &sound_stop);
    add_function("sound_set_volume", &sound_set_volume);
    add_function("sound_set_position", &sound_set_position);
    add_function("sound_set_listener", &sound_set_listener);
    add_function("sound_state", &sound_state);
    add_function("timer_start", &timer_start);
    add_function("timer_cancel", &timer_cancel);
    add_function("timer_remaining", &timer_remaining);
    add_function("timer_events", &timer_events);
    add_function("save", &save);
    add_function("load", &load);
    add_function("ui_rect", &ui_rect);
    add_function("ui_text", &ui_text);
    add_function("ui_button", &ui_button);
    add_function("ui_focus", &ui_focus);
    add_function("viewport_size", &viewport_size);
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
    lua_pop(lua, 1);
    try {
        state.ensure_asset(mesh);
        TransformComponent transform;
        transform.position = vector_field(lua, 1, "position", glm::vec3(0), false);
        transform.rotation = vector_field(lua, 1, "rotation", glm::vec3(0), false);
        transform.scale = vector_field(lua, 1, "scale", glm::vec3(1), true);
        const bool casts_shadows = optional_boolean(lua, 1, "casts_shadows", true);
        const EntityId id = state.world.create();
        state.world.set_transform(id, transform);
        state.world.set_mesh(id, {mesh, true, casts_shadows});
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

int LuaProject::Impl::set_exposure(lua_State* lua) {
    if (lua_gettop(lua) != 1 || !lua_isnumber(lua, 1))
        return luaL_error(lua, "set_exposure() expects one number");
    const double exposure = lua_tonumber(lua, 1);
    if (!std::isfinite(exposure) || exposure < 0.01 || exposure > 32.0)
        return luaL_error(lua, "exposure must be finite and between 0.01 and 32");
    context(lua).frame.exposure = float(exposure);
    return 0;
}

int LuaProject::Impl::set_ssao(lua_State* lua) {
    const int count = lua_gettop(lua);
    if (count < 1 || count > 3 || !lua_isinteger(lua, 1))
        return luaL_error(lua, "set_ssao() expects quality 0..3 and optional radius/strength");
    const lua_Integer quality = lua_tointeger(lua, 1);
    if (quality < 0 || quality > 3)
        return luaL_error(lua, "SSAO quality must be an integer from 0 to 3");
    auto& frame = context(lua).frame;
    double radius = frame.ssao_radius;
    double strength = frame.ssao_strength;
    if (count >= 2) {
        if (lua_type(lua, 2) != LUA_TNUMBER)
            return luaL_error(lua, "SSAO radius must be a number");
        radius = lua_tonumber(lua, 2);
    }
    if (count == 3) {
        if (lua_type(lua, 3) != LUA_TNUMBER)
            return luaL_error(lua, "SSAO strength must be a number");
        strength = lua_tonumber(lua, 3);
    }
    if (!std::isfinite(radius) || radius < 0.05 || radius > 5.0 ||
        !std::isfinite(strength) || strength < 0.0 || strength > 4.0)
        return luaL_error(lua, "SSAO radius must be 0.05..5 and strength 0..4");
    frame.ssao_quality = int(quality);
    frame.ssao_radius = float(radius);
    frame.ssao_strength = float(strength);
    return 0;
}

int LuaProject::Impl::set_environment(lua_State* lua) {
    if (lua_gettop(lua) < 1 || lua_gettop(lua) > 2)
        return luaL_error(lua, "set_environment() expects a project HDRI path and optional intensity");
    Impl& state = context(lua);
    if (lua_isnil(lua, 1)) {
        if (lua_gettop(lua) != 1) return luaL_error(lua, "clearing environment takes no intensity");
        state.frame.environment.reset();
        state.frame.environment_intensity = 0.0f;
        return 0;
    }
    if (lua_type(lua, 1) != LUA_TSTRING)
        return luaL_error(lua, "environment path must be a string");
    const std::string path = lua_tostring(lua, 1);
    const double intensity = lua_gettop(lua) == 2 ? luaL_checknumber(lua, 2) : 1.0;
    if (!std::isfinite(intensity) || intensity < 0.0 || intensity > 32.0)
        return luaL_error(lua, "environment intensity must be finite and between 0 and 32");
    try {
        const fs::path file = state.project_file(path);
        auto found = state.hdr_assets.find(path);
        if (found == state.hdr_assets.end()) {
            if (state.hdr_assets.size() >= 8)
                throw std::runtime_error("project HDRI asset limit reached");
            auto image = std::make_shared<HdrImage>(load_hdr_image(file));
            found = state.hdr_assets.emplace(path, std::move(image)).first;
        }
        state.frame.environment = found->second;
        state.frame.environment_intensity = float(intensity);
    } catch (const std::exception& error) {
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
    try {
        Impl& state = context(lua);
        state.world.destroy(id);
        state.physics->remove(id);
    }
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
            if (state.world.physics(id))
                state.physics->set_pose(id, value.position, physics_rotation(value.rotation));
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
            value.shadows = optional_boolean(lua, 3, "shadows", value.shadows);
            value.shadow_range = optional_number(lua, 3, "shadow_range", value.shadow_range, "light");
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
            value.casts_shadows = optional_boolean(lua, 3, "casts_shadows", value.casts_shadows);
            state.ensure_asset(value.mesh);
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
        } else if (std::strcmp(kind, "physics") == 0) {
            PhysicsComponent value = state.world.physics(id).value_or(PhysicsComponent{});
            lua_getfield(lua, 3, "motion");
            if (!lua_isnil(lua, -1)) {
                if (lua_type(lua, -1) != LUA_TSTRING)
                    throw std::invalid_argument("physics.motion must be a string");
                const char* motion = lua_tostring(lua, -1);
                if (std::strcmp(motion, "static") == 0) value.motion = PhysicsMotion::Static;
                else if (std::strcmp(motion, "dynamic") == 0) value.motion = PhysicsMotion::Dynamic;
                else if (std::strcmp(motion, "kinematic") == 0) value.motion = PhysicsMotion::Kinematic;
                else throw std::invalid_argument("physics.motion must be static, dynamic, or kinematic");
            }
            lua_pop(lua, 1);
            lua_getfield(lua, 3, "shape");
            if (!lua_isnil(lua, -1)) {
                if (lua_type(lua, -1) != LUA_TSTRING)
                    throw std::invalid_argument("physics.shape must be a string");
                const char* shape = lua_tostring(lua, -1);
                if (std::strcmp(shape, "box") == 0) value.shape = ColliderShape::Box;
                else if (std::strcmp(shape, "sphere") == 0) value.shape = ColliderShape::Sphere;
                else throw std::invalid_argument("physics.shape must be box or sphere");
            }
            lua_pop(lua, 1);
            value.half_extents = vector_field(lua, 3, "half_extents", value.half_extents, true);
            value.radius = optional_number(lua, 3, "radius", value.radius, "physics");
            value.velocity = vector_field(lua, 3, "velocity", value.velocity, false);
            value.trigger = optional_boolean(lua, 3, "trigger", value.trigger);
            const auto transform = state.world.transform(id);
            if (!transform || transform->parent)
                throw std::invalid_argument("physics component requires an unparented transform");
            const auto previous = state.world.physics(id);
            const auto old_state = state.physics->state(id);
            lua_getfield(lua, 3, "velocity");
            const bool supplied_velocity = !lua_isnil(lua, -1);
            lua_pop(lua, 1);
            const glm::vec3 velocity = supplied_velocity ? value.velocity :
                old_state && value.motion != PhysicsMotion::Static ? old_state->velocity :
                value.velocity;
            state.world.set_physics(id, value);
            state.physics->remove(id);
            try {
                state.physics->add(physics_desc(id, value, *transform, velocity));
            } catch (...) {
                state.world.remove_physics(id);
                if (previous) {
                    state.world.set_physics(id, *previous);
                    state.physics->add(physics_desc(id, *previous, *transform,
                        old_state ? old_state->velocity : glm::vec3(0.0f)));
                }
                throw;
            }
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
            lua_createtable(lua, 0, 6);
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
            lua_pushboolean(lua, value->shadows); lua_setfield(lua, -2, "shadows");
            lua_pushnumber(lua, value->shadow_range); lua_setfield(lua, -2, "shadow_range");
        } else if (std::strcmp(kind, "mesh") == 0) {
            const auto value = world.mesh(id);
            if (!value) { lua_pushnil(lua); return 1; }
            lua_createtable(lua, 0, 3);
            lua_pushstring(lua, value->mesh.c_str()); lua_setfield(lua, -2, "mesh");
            lua_pushboolean(lua, value->visible); lua_setfield(lua, -2, "visible");
            lua_pushboolean(lua, value->casts_shadows); lua_setfield(lua, -2, "casts_shadows");
        } else if (std::strcmp(kind, "script") == 0) {
            const auto value = world.script(id);
            if (!value) { lua_pushnil(lua); return 1; }
            lua_createtable(lua, 0, 2);
            lua_pushstring(lua, value->path.c_str()); lua_setfield(lua, -2, "path");
            lua_pushboolean(lua, value->enabled); lua_setfield(lua, -2, "enabled");
        } else if (std::strcmp(kind, "physics") == 0) {
            const auto value = world.physics(id);
            if (!value) { lua_pushnil(lua); return 1; }
            lua_createtable(lua, 0, 6);
            const char* motion = value->motion == PhysicsMotion::Static ? "static" :
                value->motion == PhysicsMotion::Dynamic ? "dynamic" : "kinematic";
            lua_pushstring(lua, motion); lua_setfield(lua, -2, "motion");
            lua_pushstring(lua, value->shape == ColliderShape::Box ? "box" : "sphere");
            lua_setfield(lua, -2, "shape");
            push_vec3(lua, value->half_extents); lua_setfield(lua, -2, "half_extents");
            lua_pushnumber(lua, value->radius); lua_setfield(lua, -2, "radius");
            const auto body = context(lua).physics->state(id);
            push_vec3(lua, body ? body->velocity : value->velocity);
            lua_setfield(lua, -2, "velocity");
            lua_pushboolean(lua, value->trigger); lua_setfield(lua, -2, "trigger");
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
    Impl& state = context(lua);
    World& world = state.world;
    bool removed = false;
    try {
        if (std::strcmp(kind, "transform") == 0) removed = world.remove_transform(id);
        else if (std::strcmp(kind, "camera") == 0) removed = world.remove_camera(id);
        else if (std::strcmp(kind, "light") == 0) removed = world.remove_light(id);
        else if (std::strcmp(kind, "mesh") == 0) removed = world.remove_mesh(id);
        else if (std::strcmp(kind, "script") == 0) removed = world.remove_script(id);
        else if (std::strcmp(kind, "physics") == 0) {
            removed = world.remove_physics(id);
            state.physics->remove(id);
        }
        else return luaL_error(lua, "unknown component kind: %s", kind);
    } catch (const std::invalid_argument& error) {
        return luaL_error(lua, "%s", error.what());
    }
    lua_pushboolean(lua, removed);
    return 1;
}

int LuaProject::Impl::entity_find(lua_State* lua) {
    if (lua_gettop(lua) != 1) return luaL_error(lua, "entity_find() expects a name");
    const char* name = luaL_checkstring(lua, 1);
    const World& world = context(lua).world;
    for (const EntityId id : world.entities()) {
        if (world.name(id) == name) {
            lua_pushinteger(lua, lua_Integer(id));
            return 1;
        }
    }
    lua_pushnil(lua);
    return 1;
}

int LuaProject::Impl::prefab_spawn(lua_State* lua) {
    if (lua_gettop(lua) < 1 || lua_gettop(lua) > 2)
        return luaL_error(lua, "prefab_spawn() expects a name and optional overrides");
    const char* name = luaL_checkstring(lua, 1);
    if (lua_gettop(lua) == 2) luaL_checktype(lua, 2, LUA_TTABLE);
    Impl& state = context(lua);
    EntityId id = 0;
    try {
        const auto found = state.prefabs.find(name);
        if (found == state.prefabs.end())
            throw std::invalid_argument(std::string("unknown prefab: ") + name);
        state.load_table(state.project_file(found->second));
        id = state.instantiate(-1);
        lua_pop(lua, 1);
        if (lua_gettop(lua) == 2) state.apply_components(id, 2, {});
        lua_pushinteger(lua, lua_Integer(id));
        return 1;
    } catch (const std::exception& error) {
        if (id && state.world.exists(id)) {
            state.physics->remove(id);
            state.world.destroy(id);
        }
        lua_pushstring(lua, error.what());
    }
    return lua_error(lua);
}

int LuaProject::Impl::scene_name(lua_State* lua) {
    if (lua_gettop(lua) != 0) return luaL_error(lua, "scene_name() expects no arguments");
    lua_pushstring(lua, context(lua).current_scene.c_str());
    return 1;
}

int LuaProject::Impl::scene_switch(lua_State* lua) {
    if (lua_gettop(lua) != 1) return luaL_error(lua, "scene_switch() expects a name");
    const char* name = luaL_checkstring(lua, 1);
    Impl& state = context(lua);
    if (state.scenes.find(name) == state.scenes.end())
        return luaL_error(lua, "unknown scene: %s", name);
    state.pending_scene = name;
    return 0;
}

int LuaProject::Impl::scene_reload(lua_State* lua) {
    if (lua_gettop(lua) != 0) return luaL_error(lua, "scene_reload() expects no arguments");
    Impl& state = context(lua);
    state.pending_scene = state.current_scene;
    return 0;
}

int LuaProject::Impl::physics_state(lua_State* lua) {
    if (lua_gettop(lua) != 1) return luaL_error(lua, "physics_state() expects an ID");
    const auto body = context(lua).physics->state(checked_id(lua, 1));
    if (!body) { lua_pushnil(lua); return 1; }
    lua_createtable(lua, 0, 4);
    push_vec3(lua, body->position); lua_setfield(lua, -2, "position");
    push_vec3(lua, context(lua).world.transform(checked_id(lua, 1))->rotation);
    lua_setfield(lua, -2, "rotation");
    push_vec3(lua, body->velocity); lua_setfield(lua, -2, "velocity");
    lua_pushboolean(lua, body->active); lua_setfield(lua, -2, "active");
    return 1;
}

int LuaProject::Impl::physics_set_velocity(lua_State* lua) {
    if (lua_gettop(lua) != 2) return luaL_error(lua, "physics_set_velocity() expects ID and vector");
    try { context(lua).physics->set_velocity(checked_id(lua, 1), vector_arg(lua, 2, false)); }
    catch (const std::exception& error) { return luaL_error(lua, "%s", error.what()); }
    return 0;
}

int LuaProject::Impl::physics_move(lua_State* lua) {
    if (lua_gettop(lua) != 2) return luaL_error(lua, "physics_move() expects ID and position");
    try {
        Impl& state = context(lua);
        const EntityId id = checked_id(lua, 1);
        const glm::vec3 position = vector_arg(lua, 2, false);
        state.physics->move(id, position);
        auto transform = state.world.transform(id).value();
        transform.position = position;
        state.world.set_transform(id, transform);
    } catch (const std::exception& error) { return luaL_error(lua, "%s", error.what()); }
    return 0;
}

int LuaProject::Impl::physics_set_pose(lua_State* lua) {
    if (lua_gettop(lua) != 3)
        return luaL_error(lua, "physics_set_pose() expects ID, position, Euler rotation");
    try {
        Impl& state = context(lua);
        const EntityId id = checked_id(lua, 1);
        const glm::vec3 position = vector_arg(lua, 2, false);
        const glm::vec3 rotation = vector_arg(lua, 3, false);
        state.physics->set_pose(id, position, physics_rotation(rotation));
        auto transform = state.world.transform(id).value();
        transform.position = position;
        transform.rotation = rotation;
        state.world.set_transform(id, transform);
    } catch (const std::exception& error) { return luaL_error(lua, "%s", error.what()); }
    return 0;
}

int LuaProject::Impl::physics_apply_force(lua_State* lua) {
    if (lua_gettop(lua) != 2) return luaL_error(lua, "physics_apply_force() expects ID and vector");
    try { context(lua).physics->apply_force(checked_id(lua, 1), vector_arg(lua, 2, false)); }
    catch (const std::exception& error) { return luaL_error(lua, "%s", error.what()); }
    return 0;
}

int LuaProject::Impl::physics_apply_impulse(lua_State* lua) {
    if (lua_gettop(lua) != 2) return luaL_error(lua, "physics_apply_impulse() expects ID and vector");
    try { context(lua).physics->apply_impulse(checked_id(lua, 1), vector_arg(lua, 2, false)); }
    catch (const std::exception& error) { return luaL_error(lua, "%s", error.what()); }
    return 0;
}

int LuaProject::Impl::physics_raycast(lua_State* lua) {
    if (lua_gettop(lua) != 3) return luaL_error(lua, "physics_raycast() expects origin, direction, distance");
    try {
        const glm::vec3 origin = vector_arg(lua, 1, false);
        const glm::vec3 direction = vector_arg(lua, 2, false);
        const float distance = float(luaL_checknumber(lua, 3));
        const auto hit = context(lua).physics->raycast(origin, direction, distance);
        if (!hit) { lua_pushnil(lua); return 1; }
        lua_createtable(lua, 0, 3);
        lua_pushinteger(lua, lua_Integer(hit->id)); lua_setfield(lua, -2, "id");
        push_vec3(lua, hit->position); lua_setfield(lua, -2, "position");
        lua_pushnumber(lua, hit->distance); lua_setfield(lua, -2, "distance");
    } catch (const std::exception& error) { return luaL_error(lua, "%s", error.what()); }
    return 1;
}

int LuaProject::Impl::physics_overlap_sphere(lua_State* lua) {
    if (lua_gettop(lua) != 2) return luaL_error(lua, "physics_overlap_sphere() expects center and radius");
    try {
        const auto ids = context(lua).physics->overlap_sphere(
            vector_arg(lua, 1, false), float(luaL_checknumber(lua, 2)));
        lua_createtable(lua, int(ids.size()), 0);
        for (size_t i = 0; i < ids.size(); ++i) {
            lua_pushinteger(lua, lua_Integer(ids[i]));
            lua_rawseti(lua, -2, lua_Integer(i + 1));
        }
    } catch (const std::exception& error) { return luaL_error(lua, "%s", error.what()); }
    return 1;
}

int LuaProject::Impl::physics_contacts(lua_State* lua) {
    if (lua_gettop(lua) != 0) return luaL_error(lua, "physics_contacts() expects no arguments");
    const auto& events = context(lua).physics->contacts();
    lua_createtable(lua, int(events.size()), 0);
    for (size_t i = 0; i < events.size(); ++i) {
        lua_createtable(lua, 0, 4);
        lua_pushinteger(lua, lua_Integer(events[i].first)); lua_setfield(lua, -2, "first");
        lua_pushinteger(lua, lua_Integer(events[i].second)); lua_setfield(lua, -2, "second");
        lua_pushboolean(lua, events[i].trigger); lua_setfield(lua, -2, "trigger");
        lua_pushboolean(lua, events[i].began); lua_setfield(lua, -2, "began");
        lua_rawseti(lua, -2, lua_Integer(i + 1));
    }
    return 1;
}

AudioWorld& LuaProject::Impl::ensure_audio() {
    if (!audio) audio = std::make_unique<AudioWorld>(audio_output);
    return *audio;
}

int LuaProject::Impl::sound_play(lua_State* lua) {
    if (lua_gettop(lua) < 1 || lua_gettop(lua) > 2 || lua_type(lua, 1) != LUA_TSTRING ||
        (lua_gettop(lua) == 2 && !lua_istable(lua, 2)))
        return luaL_error(lua, "sound_play() expects a project audio path and optional settings table");
    try {
        Impl& state = context(lua);
        SoundState settings;
        if (lua_gettop(lua) == 2) {
            lua_getfield(lua, 2, "position");
            const bool positioned = !lua_isnil(lua, -1);
            if (positioned) settings.position = vector_arg(lua, -1, false);
            lua_pop(lua, 1);
            settings.spatial = optional_boolean(lua, 2, "spatial", positioned);
            settings.looping = optional_boolean(lua, 2, "loop", false);
            settings.volume = optional_number(lua, 2, "volume", 1.0f, "sound");
            lua_pushnil(lua);
            while (lua_next(lua, 2) != 0) {
                if (lua_type(lua, -2) != LUA_TSTRING)
                    throw std::invalid_argument("sound setting names must be strings");
                const std::string key = lua_tostring(lua, -2);
                if (key != "position" && key != "volume" && key != "spatial" && key != "loop")
                    throw std::invalid_argument("unknown sound setting: " + key);
                lua_pop(lua, 1);
            }
        }
        const fs::path file = state.project_file(lua_tostring(lua, 1));
        lua_pushinteger(lua, lua_Integer(state.ensure_audio().play(file, settings)));
    } catch (const std::exception& error) { return luaL_error(lua, "%s", error.what()); }
    return 1;
}

int LuaProject::Impl::sound_stop(lua_State* lua) {
    if (lua_gettop(lua) != 1) return luaL_error(lua, "sound_stop() expects a sound ID");
    try {
        const auto id = checked_id(lua, 1);
        context(lua).ensure_audio().stop(id);
    } catch (const std::exception& error) { return luaL_error(lua, "%s", error.what()); }
    return 0;
}

int LuaProject::Impl::sound_set_volume(lua_State* lua) {
    if (lua_gettop(lua) != 2) return luaL_error(lua, "sound_set_volume() expects ID and volume");
    try {
        const auto id = checked_id(lua, 1);
        if (lua_type(lua, 2) != LUA_TNUMBER)
            throw std::invalid_argument("sound volume must be a number");
        context(lua).ensure_audio().set_volume(id, float(lua_tonumber(lua, 2)));
    } catch (const std::exception& error) { return luaL_error(lua, "%s", error.what()); }
    return 0;
}

int LuaProject::Impl::sound_set_position(lua_State* lua) {
    if (lua_gettop(lua) != 2) return luaL_error(lua, "sound_set_position() expects ID and position");
    try {
        const auto id = checked_id(lua, 1);
        context(lua).ensure_audio().set_position(id, vector_arg(lua, 2, false));
    } catch (const std::exception& error) { return luaL_error(lua, "%s", error.what()); }
    return 0;
}

int LuaProject::Impl::sound_set_listener(lua_State* lua) {
    if (lua_gettop(lua) != 3)
        return luaL_error(lua, "sound_set_listener() expects position, forward, up");
    try {
        context(lua).ensure_audio().set_listener(vector_arg(lua, 1, false),
            vector_arg(lua, 2, false), vector_arg(lua, 3, false));
    } catch (const std::exception& error) { return luaL_error(lua, "%s", error.what()); }
    return 0;
}

int LuaProject::Impl::sound_state(lua_State* lua) {
    if (lua_gettop(lua) != 1) return luaL_error(lua, "sound_state() expects a sound ID");
    try {
        Impl& state = context(lua);
        const auto id = checked_id(lua, 1);
        const auto result = state.audio ? state.audio->state(id) : std::nullopt;
        if (!result) { lua_pushnil(lua); return 1; }
        lua_createtable(lua, 0, 5);
        lua_pushboolean(lua, result->playing); lua_setfield(lua, -2, "playing");
        lua_pushboolean(lua, result->spatial); lua_setfield(lua, -2, "spatial");
        lua_pushboolean(lua, result->looping); lua_setfield(lua, -2, "loop");
        lua_pushnumber(lua, result->volume); lua_setfield(lua, -2, "volume");
        push_vec3(lua, result->position); lua_setfield(lua, -2, "position");
    } catch (const std::exception& error) { return luaL_error(lua, "%s", error.what()); }
    return 1;
}

int LuaProject::Impl::timer_start(lua_State* lua) {
    if (lua_gettop(lua) < 2 || lua_gettop(lua) > 3)
        return luaL_error(lua, "timer_start() expects name, seconds, optional repeat");
    try {
        Impl& state = context(lua);
        const std::string name = timer_name(lua, 1);
        if (!lua_isnumber(lua, 2)) throw std::invalid_argument("timer duration must be a number");
        const double seconds = lua_tonumber(lua, 2);
        if (!std::isfinite(seconds) || seconds < 1.0 / 60.0 || seconds > 3600.0)
            throw std::invalid_argument("timer duration must be from 1/60 to 3600 seconds");
        bool loop = false;
        if (lua_gettop(lua) == 3) {
            if (!lua_isboolean(lua, 3)) throw std::invalid_argument("timer repeat must be boolean");
            loop = lua_toboolean(lua, 3) != 0;
        }
        if (!state.timers.count(name) && state.timers.size() >= 128)
            throw std::runtime_error("timer limit reached");
        state.timers[name] = {seconds, seconds, loop};
    } catch (const std::exception& error) { return luaL_error(lua, "%s", error.what()); }
    return 0;
}

int LuaProject::Impl::timer_cancel(lua_State* lua) {
    if (lua_gettop(lua) != 1) return luaL_error(lua, "timer_cancel() expects a name");
    try {
        const auto name = timer_name(lua, 1);
        lua_pushboolean(lua, context(lua).timers.erase(name) != 0);
    } catch (const std::exception& error) { return luaL_error(lua, "%s", error.what()); }
    return 1;
}

int LuaProject::Impl::timer_remaining(lua_State* lua) {
    if (lua_gettop(lua) != 1) return luaL_error(lua, "timer_remaining() expects a name");
    try {
        const auto name = timer_name(lua, 1);
        const auto found = context(lua).timers.find(name);
        if (found == context(lua).timers.end()) lua_pushnil(lua);
        else lua_pushnumber(lua, found->second.remaining);
    } catch (const std::exception& error) { return luaL_error(lua, "%s", error.what()); }
    return 1;
}

int LuaProject::Impl::timer_events(lua_State* lua) {
    if (lua_gettop(lua) != 0) return luaL_error(lua, "timer_events() expects no arguments");
    const auto& events = context(lua).expired_timers;
    lua_createtable(lua, int(events.size()), 0);
    for (size_t i = 0; i < events.size(); ++i) {
        lua_pushstring(lua, events[i].c_str());
        lua_rawseti(lua, -2, lua_Integer(i + 1));
    }
    return 1;
}

namespace {

float ui_number(lua_State* lua, int index, float low, float high) {
    if (!lua_isnumber(lua, index)) throw std::invalid_argument("UI coordinate must be a number");
    const double value = lua_tonumber(lua, index);
    if (!std::isfinite(value) || value < low || value > high)
        throw std::invalid_argument("UI number is outside its valid range");
    return float(value);
}

std::string ui_label(lua_State* lua, int index, size_t maximum) {
    if (lua_type(lua, index) != LUA_TSTRING)
        throw std::invalid_argument("UI label must be a string");
    size_t length = 0;
    const char* chars = lua_tolstring(lua, index, &length);
    if (length == 0 || length > maximum ||
        !std::all_of(chars, chars + length, [](unsigned char c) {
            return c >= 32 && c <= 126;
        })) throw std::invalid_argument("UI label must contain printable ASCII within length limit");
    return {chars, length};
}

}  // namespace

void LuaProject::Impl::add_ui_element(UiElement element) {
    if (!drawing_ui) throw std::invalid_argument("UI drawing is available only in draw()");
    if (frame.ui.size() >= 256) throw std::runtime_error("UI element limit reached");
    frame.ui.push_back(std::move(element));
}

int LuaProject::Impl::ui_rect(lua_State* lua) {
    if (lua_gettop(lua) != 8)
        return luaL_error(lua, "ui_rect() expects x, y, width, height, r, g, b, a");
    try {
        UiElement element;
        element.x = ui_number(lua, 1, -8192, 8192);
        element.y = ui_number(lua, 2, -8192, 8192);
        element.width = ui_number(lua, 3, 1, 8192);
        element.height = ui_number(lua, 4, 1, 8192);
        element.red = uint8_t(std::round(ui_number(lua, 5, 0, 1) * 255));
        element.green = uint8_t(std::round(ui_number(lua, 6, 0, 1) * 255));
        element.blue = uint8_t(std::round(ui_number(lua, 7, 0, 1) * 255));
        element.alpha = uint8_t(std::round(ui_number(lua, 8, 0, 1) * 255));
        context(lua).add_ui_element(std::move(element));
    } catch (const std::exception& error) { return luaL_error(lua, "%s", error.what()); }
    return 0;
}

int LuaProject::Impl::ui_text(lua_State* lua) {
    if (lua_gettop(lua) != 4)
        return luaL_error(lua, "ui_text() expects x, y, text, scale");
    try {
        UiElement element;
        element.type = UiElement::Type::Text;
        element.x = ui_number(lua, 1, -8192, 8192);
        element.y = ui_number(lua, 2, -8192, 8192);
        element.text = ui_label(lua, 3, 256);
        element.scale = ui_number(lua, 4, 1, 8);
        context(lua).add_ui_element(std::move(element));
    } catch (const std::exception& error) { return luaL_error(lua, "%s", error.what()); }
    return 0;
}

int LuaProject::Impl::ui_button(lua_State* lua) {
    if (lua_gettop(lua) != 6)
        return luaL_error(lua, "ui_button() expects id, x, y, width, height, label");
    try {
        auto& state = context(lua);
        const std::string id = ui_label(lua, 1, 64);
        if (std::find(state.current_ui_buttons.begin(), state.current_ui_buttons.end(), id) !=
            state.current_ui_buttons.end()) throw std::invalid_argument("duplicate UI button id");
        UiElement element;
        element.type = UiElement::Type::Button;
        element.x = ui_number(lua, 2, -8192, 8192);
        element.y = ui_number(lua, 3, -8192, 8192);
        element.width = ui_number(lua, 4, 1, 8192);
        element.height = ui_number(lua, 5, 1, 8192);
        element.text = ui_label(lua, 6, 256);
        element.scale = 2;
        if (state.focused_ui_button.empty()) state.focused_ui_button = id;
        const auto [px, py] = state.input.pointer();
        const bool clicked = !state.ui_activated && state.ui_click_edge && px >= element.x &&
            py >= element.y && px < element.x + element.width &&
            py < element.y + element.height;
        if (clicked) state.focused_ui_button = id;
        const bool activated = clicked || (!state.ui_activated && state.ui_enter_edge &&
                                         state.focused_ui_button == id);
        state.ui_activated |= activated;
        element.focused = state.focused_ui_button == id;
        state.add_ui_element(std::move(element));
        state.current_ui_buttons.push_back(id);
        lua_pushboolean(lua, activated);
    } catch (const std::exception& error) { return luaL_error(lua, "%s", error.what()); }
    return 1;
}

int LuaProject::Impl::ui_focus(lua_State* lua) {
    if (lua_gettop(lua) > 1) return luaL_error(lua, "ui_focus() expects zero or one button id");
    try {
        auto& state = context(lua);
        if (lua_gettop(lua) == 1) state.focused_ui_button = ui_label(lua, 1, 64);
        lua_pushstring(lua, state.focused_ui_button.c_str());
    } catch (const std::exception& error) { return luaL_error(lua, "%s", error.what()); }
    return 1;
}

int LuaProject::Impl::viewport_size(lua_State* lua) {
    if (lua_gettop(lua) != 0) return luaL_error(lua, "viewport_size() expects no arguments");
    lua_pushinteger(lua, context(lua).width);
    lua_pushinteger(lua, context(lua).height);
    return 2;
}

int LuaProject::Impl::save(lua_State* lua) {
    if (lua_gettop(lua) != 2) return luaL_error(lua, "save() expects slot and value");
    if (lua_type(lua, 1) != LUA_TSTRING) return luaL_error(lua, "save slot must be a string");
    try {
        size_t entries = 0;
        std::set<const void*> active;
        const SaveValue value = from_lua(lua, 2, 0, entries, active);
        write_save(context(lua).root, lua_tostring(lua, 1), value);
    } catch (const std::exception& error) { return luaL_error(lua, "%s", error.what()); }
    return 0;
}

int LuaProject::Impl::load(lua_State* lua) {
    if (lua_gettop(lua) != 1) return luaL_error(lua, "load() expects a slot");
    if (lua_type(lua, 1) != LUA_TSTRING) return luaL_error(lua, "save slot must be a string");
    try {
        const auto value = read_save(context(lua).root, lua_tostring(lua, 1));
        if (value) push_saved(lua, *value);
        else lua_pushnil(lua);
    } catch (const std::exception& error) { return luaL_error(lua, "%s", error.what()); }
    return 1;
}

void LuaProject::Impl::sync_physics() {
    for (EntityId id : world.entities()) {
        const auto component = world.physics(id);
        if (!component || component->motion == PhysicsMotion::Static) continue;
        const auto body = physics->state(id);
        if (!body) continue;
        auto transform = world.transform(id).value();
        transform.position = body->position;
        float y = 0, x = 0, z = 0;
        glm::extractEulerAngleYXZ(glm::mat4_cast(body->rotation), y, x, z);
        transform.rotation = {x, y, z};
        world.set_transform(id, transform);
    }
}

SceneFrame LuaProject::Impl::step(double dt) {
    if (!std::isfinite(dt) || dt < 0.0 || dt > 0.25) {
        throw std::invalid_argument("frame delta must be finite and between 0 and 0.25 seconds");
    }
    if (mode == Mode::EditorPreview) return snapshot();
    if (wants_quit) {
        SceneFrame scene = snapshot();
        scene.quit = true;
        return scene;
    }
    constexpr double fixed_dt = 1.0 / 60.0;
    time_seconds += dt;
    fixed_accumulator += dt;
    unsigned fixed_steps = 0;
    sync_scripts();
    while (fixed_accumulator + 1e-12 >= fixed_dt && fixed_steps < 8 &&
           !wants_quit && !pending_scene) {
        expired_timers.clear();
        for (auto it = timers.begin(); it != timers.end();) {
            it->second.remaining -= fixed_dt;
            if (it->second.remaining <= 1e-12) {
                expired_timers.push_back(it->first);
                if (it->second.repeat) {
                    it->second.remaining += it->second.interval;
                    ++it;
                } else it = timers.erase(it);
            } else ++it;
        }
        physics->step();
        sync_physics();
        call(fixed_ref, "fixed_update", fixed_dt, true);
        sync_scripts();
        if (!pending_scene && !wants_quit)
            call_entity_scripts(&ScriptRefs::fixed, "fixed_update", fixed_dt, true);
        fixed_accumulator -= fixed_dt;
        ++fixed_steps;
    }
    if (fixed_steps == 8) fixed_accumulator = std::min(fixed_accumulator, fixed_dt);
    if (!wants_quit && !pending_scene) call(update_ref, "update", dt, true);
    if (!wants_quit && !pending_scene) {
        sync_scripts();
        call_entity_scripts(&ScriptRefs::update, "update", dt, true);
    }
    frame.ui.clear();
    current_ui_buttons.clear();
    const bool mouse = input.value(InputDevice::MouseButton, "left") > 0;
    const bool enter = input.value(InputDevice::Keyboard, "enter") > 0;
    const bool tab = input.value(InputDevice::Keyboard, "tab") > 0;
    ui_click_edge = mouse && !ui_mouse_held;
    ui_enter_edge = enter && !ui_enter_held;
    ui_tab_edge = tab && !ui_tab_held;
    ui_activated = false;
    ui_mouse_held = mouse;
    ui_enter_held = enter;
    ui_tab_held = tab;
    if (ui_tab_edge && !previous_ui_buttons.empty()) {
        const auto found = std::find(previous_ui_buttons.begin(), previous_ui_buttons.end(),
                                     focused_ui_button);
        const size_t next = found == previous_ui_buttons.end() ? 0 :
            (size_t(found - previous_ui_buttons.begin()) + 1) % previous_ui_buttons.size();
        focused_ui_button = previous_ui_buttons[next];
    }
    drawing_ui = true;
    if (!wants_quit && !pending_scene) call(draw_ref, "draw", 0.0, false);
    if (!wants_quit && !pending_scene) {
        sync_scripts();
        call_entity_scripts(&ScriptRefs::draw, "draw", 0.0, false);
    }
    drawing_ui = false;
    if (!current_ui_buttons.empty() &&
        std::find(current_ui_buttons.begin(), current_ui_buttons.end(), focused_ui_button) ==
            current_ui_buttons.end()) focused_ui_button = current_ui_buttons.front();
    previous_ui_buttons = current_ui_buttons;
    if (pending_scene && !wants_quit) {
        const std::string next = *pending_scene;
        pending_scene.reset();
        load_scene(next);
    }
    if (audio && !audio->output_available() && dt > 0)
        audio->render_offline(uint64_t(std::llround(dt * 48000.0)));
    SceneFrame scene = snapshot();
    scene.quit = wants_quit;
    return scene;
}

LuaProject::LuaProject(const fs::path& root, std::string initial_scene, Mode mode,
                       bool audio_output)
    : impl_(std::make_unique<Impl>(root, initial_scene, mode, audio_output)) {}
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
