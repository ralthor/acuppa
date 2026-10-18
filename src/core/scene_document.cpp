#include "threepoint/scene_document.hpp"

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace threepoint {
namespace fs = std::filesystem;
namespace {

void budget_hook(lua_State* lua, lua_Debug*) {
    auto* budget = static_cast<int*>(lua_getextraspace(lua));
    if (--*budget <= 0) luaL_error(lua, "scene editor instruction limit reached");
}

void run_table(lua_State* lua, const fs::path& path) {
    *static_cast<int*>(lua_getextraspace(lua)) = 100;
    lua_sethook(lua, budget_hook, LUA_MASKCOUNT, 10000);
    if (luaL_loadfilex(lua, path.c_str(), "t") != LUA_OK ||
        lua_pcall(lua, 0, 1, 0) != LUA_OK) {
        const std::string error = lua_tostring(lua, -1);
        lua_sethook(lua, nullptr, 0, 0);
        lua_pop(lua, 1);
        throw std::runtime_error(path.string() + ": " + error);
    }
    lua_sethook(lua, nullptr, 0, 0);
    if (!lua_istable(lua, -1)) throw std::runtime_error("scene source must return a table");
}

fs::path project_file(const fs::path& root, const std::string& relative) {
    const fs::path named(relative);
    if (relative.empty() || named.is_absolute())
        throw std::invalid_argument("project path must be relative");
    std::error_code error;
    const fs::path resolved = fs::weakly_canonical(root / named, error);
    if (error) throw std::runtime_error("cannot resolve project path");
    const fs::path inside = fs::relative(resolved, root, error);
    if (error || inside.empty() || *inside.begin() == ".." ||
        !fs::is_regular_file(resolved))
        throw std::runtime_error("scene path is missing or outside project");
    return resolved;
}

std::string read_text(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("cannot read scene source");
    std::string content((std::istreambuf_iterator<char>(stream)),
                        std::istreambuf_iterator<char>());
    if (stream.bad() || content.size() > 1024 * 1024)
        throw std::runtime_error("scene source exceeds 1 MiB or cannot be read");
    return content;
}

bool declarative_source(const std::string& content) {
    size_t offset = 0;
    while (offset < content.size()) {
        while (offset < content.size() && std::isspace(static_cast<unsigned char>(content[offset])))
            ++offset;
        if (content.compare(offset, 2, "--") != 0) break;
        const size_t newline = content.find('\n', offset);
        offset = newline == std::string::npos ? content.size() : newline + 1;
    }
    if (content.compare(offset, 6, "return") != 0) return false;
    offset += 6;
    while (offset < content.size() && std::isspace(static_cast<unsigned char>(content[offset])))
        ++offset;
    return offset < content.size() && content[offset] == '{';
}

std::string quoted(const std::string& value) {
    std::string result = "\"";
    for (unsigned char c : value) {
        if (c == '\\' || c == '"') { result += '\\'; result += char(c); }
        else if (c >= 32 && c <= 126) result += char(c);
        else {
            char digits[5];
            std::snprintf(digits, sizeof(digits), "\\%03u", unsigned(c));
            result += digits;
        }
    }
    return result + '"';
}

std::string serialize(lua_State* lua, int index, unsigned depth,
                      std::set<const void*>& active, size_t& entries) {
    if (depth > 16) throw std::runtime_error("scene table nesting exceeds 16 levels");
    index = lua_absindex(lua, index);
    switch (lua_type(lua, index)) {
    case LUA_TBOOLEAN: return lua_toboolean(lua, index) ? "true" : "false";
    case LUA_TNUMBER:
        if (lua_isinteger(lua, index)) return std::to_string(lua_tointeger(lua, index));
        else {
            if (!std::isfinite(lua_tonumber(lua, index)))
                throw std::runtime_error("scene number must be finite");
            std::ostringstream output;
            output << std::setprecision(17) << lua_tonumber(lua, index);
            return output.str();
        }
    case LUA_TSTRING: {
        size_t length = 0;
        const char* bytes = lua_tolstring(lua, index, &length);
        return quoted({bytes, length});
    }
    case LUA_TTABLE: {
        const void* identity = lua_topointer(lua, index);
        if (!active.insert(identity).second) throw std::runtime_error("cyclic scene table");
        std::vector<std::pair<std::string, std::string>> fields;
        lua_pushnil(lua);
        while (lua_next(lua, index) != 0) {
            if (++entries > 10000) throw std::runtime_error("scene table entry limit reached");
            std::string key;
            if (lua_isinteger(lua, -2)) key = "[" + std::to_string(lua_tointeger(lua, -2)) + "]";
            else if (lua_type(lua, -2) == LUA_TSTRING) {
                size_t length = 0;
                const char* bytes = lua_tolstring(lua, -2, &length);
                key = "[" + quoted({bytes, length}) + "]";
            } else throw std::runtime_error("scene table has unsupported key type");
            fields.emplace_back(std::move(key), serialize(lua, -1, depth + 1, active, entries));
            lua_pop(lua, 1);
        }
        active.erase(identity);
        std::sort(fields.begin(), fields.end());
        if (fields.empty()) return "{}";
        std::string result = "{\n";
        for (const auto& [key, value] : fields)
            result += std::string((depth + 1) * 2, ' ') + key + " = " + value + ",\n";
        result += std::string(depth * 2, ' ') + "}";
        return result;
    }
    default: throw std::runtime_error("scene has unsupported value type");
    }
}

void validate_name(const std::string& name) {
    if (name.empty() || name.size() > 128 || name.find('\0') != std::string::npos)
        throw std::invalid_argument("entity name must contain 1-128 non-NUL characters");
}

void atomic_write(const fs::path& path, const std::string& content) {
    static std::atomic<uint64_t> serial{0};
    const fs::path temp = path.string() + ".tmp." + std::to_string(getpid()) + "." +
                          std::to_string(serial++);
    const int fd = open(temp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) throw std::runtime_error("cannot create temporary scene file");
    try {
        size_t offset = 0;
        while (offset < content.size()) {
            const ssize_t n = write(fd, content.data() + offset, content.size() - offset);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) throw std::runtime_error("cannot write scene file");
            offset += size_t(n);
        }
        if (fsync(fd) != 0) throw std::runtime_error("cannot sync scene file");
        if (close(fd) != 0) throw std::runtime_error("cannot close scene file");
        if (rename(temp.c_str(), path.c_str()) != 0)
            throw std::runtime_error("cannot replace scene file");
        const int dir = open(path.parent_path().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (dir >= 0) { fsync(dir); close(dir); }
    } catch (...) {
        close(fd);
        unlink(temp.c_str());
        throw;
    }
}

}  // namespace

SceneDocument::SceneDocument(const fs::path& project_root, const std::string& scene) {
    root_ = fs::canonical(project_root);
    lua_ = luaL_newstate();
    if (!lua_) throw std::runtime_error("cannot create editor Lua state");
    try {
        run_table(lua_, project_file(root_, "game.lua"));
        lua_getfield(lua_, -1, "scenes");
        if (!lua_istable(lua_, -1)) throw std::runtime_error("project scenes must be a table");
        lua_getfield(lua_, -1, scene.c_str());
        if (lua_type(lua_, -1) != LUA_TSTRING) throw std::runtime_error("unknown scene: " + scene);
        const std::string relative = lua_tostring(lua_, -1);
        lua_settop(lua_, 0);
        path_ = project_file(root_, relative);
        original_ = read_text(path_);
        if (!declarative_source(original_))
            throw std::runtime_error("editor can change only declarative 'return {...}' scene files");
        run_table(lua_, path_);
        lua_getfield(lua_, -1, "entities");
        if (!lua_istable(lua_, -1)) throw std::runtime_error("scene entities must be a table");
        lua_pop(lua_, 1);
    } catch (...) { lua_close(lua_); lua_ = nullptr; throw; }
}

SceneDocument::~SceneDocument() { if (lua_) lua_close(lua_); }

int SceneDocument::find_entity(const std::string& name) const {
    lua_getfield(lua_, 1, "entities");
    const size_t count = lua_rawlen(lua_, -1);
    for (size_t i = 1; i <= count; ++i) {
        lua_rawgeti(lua_, -1, lua_Integer(i));
        lua_getfield(lua_, -1, "name");
        const bool same = lua_type(lua_, -1) == LUA_TSTRING && name == lua_tostring(lua_, -1);
        lua_pop(lua_, 1);
        if (same) { lua_remove(lua_, -2); return int(i); }
        lua_pop(lua_, 1);
    }
    lua_pop(lua_, 1);
    throw std::runtime_error("unknown entity: " + name);
}

std::vector<EditorEntity> SceneDocument::tree() const {
    std::vector<EditorEntity> result;
    lua_getfield(lua_, 1, "entities");
    const size_t count = lua_rawlen(lua_, -1);
    for (size_t i = 1; i <= count; ++i) {
        lua_rawgeti(lua_, -1, lua_Integer(i));
        if (!lua_istable(lua_, -1)) throw std::runtime_error("scene entity is not a table");
        EditorEntity entity;
        lua_getfield(lua_, -1, "name");
        if (lua_type(lua_, -1) == LUA_TSTRING) entity.name = lua_tostring(lua_, -1);
        else entity.name = "#" + std::to_string(i);
        lua_pop(lua_, 1);
        lua_getfield(lua_, -1, "transform");
        if (lua_istable(lua_, -1)) {
            lua_getfield(lua_, -1, "parent");
            if (lua_type(lua_, -1) == LUA_TSTRING) entity.parent = lua_tostring(lua_, -1);
            lua_pop(lua_, 1);
        }
        lua_pop(lua_, 1);
        lua_pushnil(lua_);
        while (lua_next(lua_, -2) != 0) {
            if (lua_type(lua_, -2) == LUA_TSTRING) {
                const std::string key = lua_tostring(lua_, -2);
                if (key != "name") entity.components.push_back(key);
            }
            lua_pop(lua_, 1);
        }
        std::sort(entity.components.begin(), entity.components.end());
        result.push_back(std::move(entity));
        lua_pop(lua_, 1);
    }
    lua_pop(lua_, 1);
    return result;
}

std::string SceneDocument::inspect(const std::string& entity) const {
    find_entity(entity);
    std::set<const void*> active;
    size_t entries = 0;
    const std::string result = serialize(lua_, -1, 0, active, entries);
    lua_pop(lua_, 1);
    return result;
}

void SceneDocument::add_entity(const std::string& name) {
    validate_name(name);
    const auto entities = tree();
    if (std::any_of(entities.begin(), entities.end(), [&](const EditorEntity& entry) {
        return entry.name == name;
    })) throw std::invalid_argument("duplicate entity name");
    lua_getfield(lua_, 1, "entities");
    const size_t count = lua_rawlen(lua_, -1);
    lua_createtable(lua_, 0, 1);
    lua_pushlstring(lua_, name.data(), name.size());
    lua_setfield(lua_, -2, "name");
    lua_rawseti(lua_, -2, lua_Integer(count + 1));
    lua_pop(lua_, 1);
}

void SceneDocument::set_field(const std::string& entity, const std::string& component,
                              const std::string& field, const std::string& lua_literal) {
    if (component != "transform" && component != "mesh" && component != "camera" &&
        component != "light" && component != "script" && component != "physics")
        throw std::invalid_argument("unknown component name");
    if (field.empty() || field.size() > 64 || !std::all_of(field.begin(), field.end(),
        [](unsigned char c) { return std::isalnum(c) || c == '_'; }))
        throw std::invalid_argument("invalid component field name");
    if (lua_literal.empty() || lua_literal.size() > 4096)
        throw std::invalid_argument("component value literal must use 1-4096 bytes");
    const int initial = lua_gettop(lua_);
    try {
        find_entity(entity);
        lua_getfield(lua_, -1, component.c_str());
        if (lua_isnil(lua_, -1)) { lua_pop(lua_, 1); lua_newtable(lua_); }
        if (!lua_istable(lua_, -1)) throw std::runtime_error("component is not a table");
        const std::string code = "return " + lua_literal;
        *static_cast<int*>(lua_getextraspace(lua_)) = 100;
        lua_sethook(lua_, budget_hook, LUA_MASKCOUNT, 10000);
        if (luaL_loadstring(lua_, code.c_str()) != LUA_OK ||
            lua_pcall(lua_, 0, 1, 0) != LUA_OK) {
            const std::string error = lua_tostring(lua_, -1);
            throw std::invalid_argument("invalid component value: " + error);
        }
        lua_sethook(lua_, nullptr, 0, 0);
        if (!lua_isnil(lua_, -1)) {
            std::set<const void*> active;
            size_t entries = 0;
            serialize(lua_, -1, 0, active, entries);
        }
        lua_setfield(lua_, -2, field.c_str());
        lua_setfield(lua_, -2, component.c_str());
        lua_settop(lua_, initial);
    } catch (...) {
        lua_sethook(lua_, nullptr, 0, 0);
        lua_settop(lua_, initial);
        throw;
    }
}

void SceneDocument::save() {
    if (read_text(path_) != original_)
        throw std::runtime_error("scene changed on disk since editor opened it");
    std::set<const void*> active;
    size_t entries = 0;
    const std::string content = "return " + serialize(lua_, 1, 0, active, entries) + "\n";
    if (content.size() > 1024 * 1024) throw std::runtime_error("scene exceeds 1 MiB");
    atomic_write(path_, content);
    original_ = content;
}

}  // namespace threepoint
