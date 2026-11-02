#include "acuppa/data_file.hpp"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cstring>
#include <memory>
#include <fcntl.h>
#include <limits>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace acuppa {
namespace {

constexpr size_t max_bytes = 1024 * 1024;
constexpr unsigned max_depth = 16;
constexpr size_t max_entries = 10000;
constexpr size_t max_parse_memory = 32 * 1024 * 1024;
constexpr int hook_interval = 1000;
constexpr int max_hook_calls = 10000;  // about ten million VM instructions

struct FileDescriptor {
    int fd = -1;
    explicit FileDescriptor(int value = -1) : fd(value) {}
    ~FileDescriptor() { reset(); }
    void reset(int value = -1) { if (fd >= 0) close(fd); fd = value; }
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;
};

SaveValue convert(lua_State* lua, int index, unsigned depth, size_t& entries,
                  std::set<const void*>& active) {
    if (depth > max_depth) throw std::invalid_argument("save table nesting exceeds 16 levels");
    if (!lua_checkstack(lua, 4)) throw std::invalid_argument("save table nesting is too deep");
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
        if (size > max_bytes) throw std::invalid_argument("save string exceeds 1 MiB");
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
            if (++entries > max_entries) {
                lua_pop(lua, 2);
                throw std::invalid_argument("save table exceeds 10000 entries");
            }
            SaveKey key;
            if (lua_isinteger(lua, -2)) {
                key.integer = true;
                key.number = lua_tointeger(lua, -2);
            } else if (lua_type(lua, -2) == LUA_TSTRING) {
                size_t size = 0;
                const char* bytes = lua_tolstring(lua, -2, &size);
                if (size > max_bytes) {
                    lua_pop(lua, 2);
                    throw std::invalid_argument("save key exceeds 1 MiB");
                }
                key.text.assign(bytes, size);
            } else {
                lua_pop(lua, 2);
                throw std::invalid_argument("save table keys must be strings or integers");
            }
            try {
                value.entries.emplace_back(std::move(key),
                                           convert(lua, -1, depth + 1, entries, active));
            } catch (...) {
                lua_pop(lua, 2);
                throw;
            }
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

class Writer {
public:
    void put(std::string_view text) {
        if (out.size() + text.size() > max_bytes)
            throw std::invalid_argument("data file exceeds 1 MiB");
        out.append(text);
    }
    void indent(unsigned level) { put(std::string(level * 2, ' ')); }
    std::string out;
};

bool is_identifier(const std::string& key) {
    static const std::set<std::string> keywords = {
        "and", "break", "do", "else", "elseif", "end", "false", "for", "function", "goto",
        "if", "in", "local", "nil", "not", "or", "repeat", "return", "then", "true",
        "until", "while"};
    if (key.empty() || !(std::isalpha(static_cast<unsigned char>(key[0])) || key[0] == '_'))
        return false;
    for (unsigned char c : key)
        if (!(std::isalnum(c) || c == '_')) return false;
    return keywords.count(key) == 0;
}

std::string quote(const std::string& text) {
    std::string result = "\"";
    for (unsigned char c : text) {
        switch (c) {
        case '"': result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (c < 0x20 || c == 0x7f) {
                char escaped[5];
                std::snprintf(escaped, sizeof(escaped), "\\%03u", unsigned(c));
                result += escaped;
            } else result.push_back(char(c));
        }
    }
    return result + "\"";
}

std::string integer_text(int64_t value) {
    if (value == std::numeric_limits<int64_t>::min()) return "(-9223372036854775807 - 1)";
    return std::to_string(value);
}

std::string number_text(double value) {
    if (!std::isfinite(value)) throw std::invalid_argument("save number must be finite");
    char buffer[40];
    for (int precision = 1; precision <= 17; ++precision) {
        std::snprintf(buffer, sizeof(buffer), "%.*g", precision, value);
        if (std::strtod(buffer, nullptr) == value) break;
    }
    std::string text = buffer;
    if (text.find_first_of(".e") == std::string::npos) text += ".0";
    return text;
}

bool scalar(const SaveValue& value) { return value.type != SaveValue::Type::Table; }

void emit(Writer& out, const SaveValue& value, unsigned depth, size_t& entries) {
    if (depth > max_depth) throw std::invalid_argument("save table nesting exceeds 16 levels");
    switch (value.type) {
    case SaveValue::Type::Boolean: out.put(value.boolean ? "true" : "false"); return;
    case SaveValue::Type::Integer: out.put(integer_text(value.integer)); return;
    case SaveValue::Type::Number: out.put(number_text(value.number)); return;
    case SaveValue::Type::String:
        if (value.text.size() > max_bytes) throw std::invalid_argument("data file exceeds 1 MiB");
        out.put(quote(value.text));
        return;
    case SaveValue::Type::Table: break;
    default: throw std::invalid_argument("unsupported save value type");
    }
    entries += value.entries.size();
    if (entries > max_entries) throw std::invalid_argument("save table exceeds 10000 entries");
    std::vector<const std::pair<SaveKey, SaveValue>*> sorted;
    for (const auto& entry : value.entries) sorted.push_back(&entry);
    std::sort(sorted.begin(), sorted.end(), [](const auto* a, const auto* b) {
        if (a->first.integer != b->first.integer) return a->first.integer;
        if (a->first.integer) return a->first.number < b->first.number;
        return a->first.text < b->first.text;
    });
    for (size_t i = 1; i < sorted.size(); ++i) {
        const auto& a = sorted[i - 1]->first;
        const auto& b = sorted[i]->first;
        if (a.integer == b.integer && (a.integer ? a.number == b.number : a.text == b.text))
            throw std::invalid_argument("duplicate save table key");
    }
    if (sorted.empty()) { out.put("{}"); return; }
    size_t array = 0;
    while (array < sorted.size() && sorted[array]->first.integer &&
           sorted[array]->first.number == int64_t(array + 1))
        ++array;
    const bool inline_array = array == sorted.size() && array <= 16 &&
        std::all_of(sorted.begin(), sorted.end(), [](const auto* e) { return scalar(e->second); });
    if (inline_array) {
        out.put("{");
        for (size_t i = 0; i < sorted.size(); ++i) {
            if (i) out.put(", ");
            emit(out, sorted[i]->second, depth + 1, entries);
        }
        out.put("}");
        return;
    }
    out.put("{\n");
    for (size_t i = 0; i < sorted.size(); ++i) {
        const auto& [key, child] = *sorted[i];
        out.indent(depth + 1);
        if (i >= array) {
            if (key.integer) out.put("[" + integer_text(key.number) + "] = ");
            else if (is_identifier(key.text)) out.put(key.text + " = ");
            else out.put("[" + quote(key.text) + "] = ");
        }
        emit(out, child, depth + 1, entries);
        out.put(",\n");
    }
    out.indent(depth);
    out.put("}");
}

struct ParseLimits {
    size_t used = 0;
    int hooks = 0;
};

void* limited_alloc(void* user, void* pointer, size_t old_size, size_t new_size) {
    auto* limits = static_cast<ParseLimits*>(user);
    if (!pointer) old_size = 0;
    if (new_size == 0) {
        limits->used -= old_size;
        std::free(pointer);
        return nullptr;
    }
    if (new_size > old_size && limits->used + (new_size - old_size) > max_parse_memory)
        return nullptr;
    void* result = std::realloc(pointer, new_size);
    if (result) limits->used = limits->used - old_size + new_size;
    return result;
}

void instruction_hook(lua_State* lua, lua_Debug*) {
    void* user = nullptr;
    lua_getallocf(lua, &user);
    if (++static_cast<ParseLimits*>(user)->hooks > max_hook_calls)
        luaL_error(lua, "data file exceeds the instruction limit");
}

std::vector<std::string> components(const std::string& relative, const char* what) {
    if (relative.empty() || relative.size() > 4096 || relative.find('\0') != std::string::npos)
        throw std::invalid_argument(std::string(what) + " must be a non-empty relative path");
    if (relative[0] == '/' || relative.find('\\') != std::string::npos)
        throw std::invalid_argument(std::string(what) + " must be relative: " + relative);
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= relative.size()) {
        const size_t end = std::min(relative.find('/', start), relative.size());
        const std::string part = relative.substr(start, end - start);
        if (part.empty() || part == "." || part == "..")
            throw std::invalid_argument(std::string(what) + " has an empty, '.', or '..' component: " + relative);
        if (part[0] == '.')
            throw std::invalid_argument(std::string(what) + " has a hidden component: " + relative);
        parts.push_back(part);
        start = end + 1;
    }
    return parts;
}

std::string join(const std::vector<std::string>& parts, size_t count) {
    std::string result;
    for (size_t i = 0; i < count; ++i) result += (i ? "/" : "") + parts[i];
    return result;
}

std::vector<std::string> data_path(const std::string& relative) {
    auto parts = components(relative, "data file path");
    const std::string& file = parts.back();
    if (file.size() <= 4 || file.compare(file.size() - 4, 4, ".lua") != 0)
        throw std::invalid_argument("data file must be a .lua file: " + relative);
    return parts;
}

// Opens every directory component without following symlinks. Returns -1 when
// a component is missing and create is false.
int open_directories(const std::filesystem::path& root, const std::vector<std::string>& parts,
                     bool create, const std::string& relative) {
    FileDescriptor directory(open(root.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    if (directory.fd < 0) throw std::runtime_error("cannot open project directory");
    for (size_t i = 0; i + 1 < parts.size(); ++i) {
        int next = openat(directory.fd, parts[i].c_str(),
                          O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (next < 0 && errno == ENOENT && create) {
            if (mkdirat(directory.fd, parts[i].c_str(), 0755) != 0 && errno != EEXIST)
                throw std::runtime_error("cannot create data directory for " + relative);
            next = openat(directory.fd, parts[i].c_str(),
                          O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        }
        if (next < 0) {
            if (errno == ENOENT && !create) return -1;
            throw std::runtime_error("data file directory is missing, not a directory, or a symlink: " + relative);
        }
        directory.reset(next);
    }
    const int result = directory.fd;
    directory.fd = -1;
    return result;
}

std::optional<std::string> read_at(int directory, const std::string& file,
                                   const std::string& relative) {
    const int fd = openat(directory, file.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) {
        if (errno == ENOENT) return std::nullopt;
        throw std::runtime_error("cannot read data file (missing permission or symlink): " + relative);
    }
    FileDescriptor input(fd);
    struct stat info {};
    if (fstat(input.fd, &info) != 0 || !S_ISREG(info.st_mode))
        throw std::runtime_error("data file is not a regular file: " + relative);
    if (info.st_size > off_t(max_bytes))
        throw std::runtime_error("data file exceeds 1 MiB: " + relative);
    std::string bytes(size_t(info.st_size), '\0');
    size_t offset = 0;
    while (offset < bytes.size()) {
        const ssize_t count = read(input.fd, bytes.data() + offset, bytes.size() - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) throw std::runtime_error("truncated data file: " + relative);
        offset += size_t(count);
    }
    return bytes;
}

}  // namespace

SaveValue save_value_from_lua(lua_State* lua, int index) {
    size_t entries = 0;
    std::set<const void*> active;
    return convert(lua, index, 0, entries, active);
}

std::string serialize_data(const SaveValue& value) {
    if (value.type != SaveValue::Type::Table)
        throw std::invalid_argument("data file value must be a table");
    Writer out;
    out.put("return ");
    size_t entries = 0;
    emit(out, value, 0, entries);
    out.put("\n");
    return out.out;
}

SaveValue parse_data(std::string_view text, const std::string& name) {
    if (text.size() > max_bytes) throw std::runtime_error("data file " + name + " exceeds 1 MiB");
    ParseLimits limits;
    std::unique_ptr<lua_State, decltype(&lua_close)> state(
        lua_newstate(limited_alloc, &limits), lua_close);
    if (!state) throw std::runtime_error("could not create data Lua state");
    lua_State* lua = state.get();
    lua_sethook(lua, instruction_hook, LUA_MASKCOUNT, hook_interval);
    const std::string chunk = "@" + name;
    int status = luaL_loadbufferx(lua, text.data(), text.size(), chunk.c_str(), "t");
    if (status == LUA_OK) {
        lua_newtable(lua);  // empty environment: no globals, require, or libraries
        lua_setupvalue(lua, -2, 1);
        status = lua_pcall(lua, 0, 1, 0);
    }
    if (status != LUA_OK) {
        const char* message = lua_tostring(lua, -1);
        throw std::runtime_error("data file " + name + ": " +
                                 (message ? message : "unknown Lua error"));
    }
    if (!lua_istable(lua, -1)) throw std::runtime_error("data file " + name + " must return a table");
    try {
        return save_value_from_lua(lua, -1);
    } catch (const std::exception& error) {
        throw std::runtime_error("data file " + name + ": " + error.what());
    }
}

std::string normalize_writable_directory(const std::string& directory) {
    const auto parts = components(directory, "writable directory");
    if (parts[0] == "saves" || parts[0] == "tests" || parts[0] == "build")
        throw std::invalid_argument("writable directory cannot be inside saves, tests, or build: " + directory);
    return join(parts, parts.size());
}

void write_data_file(const std::filesystem::path& project_root, const std::string& relative,
                     const SaveValue& value, const DataWritePolicy& policy) {
    const auto parts = data_path(relative);
    const std::string normalized = join(parts, parts.size());
    if (normalized == "game.lua") throw std::invalid_argument("data_write cannot replace game.lua");
    if (policy.protected_files.count(normalized))
        throw std::invalid_argument("data_write cannot replace a scene, prefab, or script: " + normalized);
    const bool allowed = std::any_of(policy.writable.begin(), policy.writable.end(),
        [&](const std::string& directory) {
            return normalized.size() > directory.size() + 1 &&
                   normalized.compare(0, directory.size(), directory) == 0 &&
                   normalized[directory.size()] == '/';
        });
    if (!allowed)
        throw std::invalid_argument("data_write path is not inside a game.lua writable directory: " + normalized);
    const std::string bytes = serialize_data(value);
    FileDescriptor directory(open_directories(project_root, parts, true, normalized));
    const std::string& file = parts.back();
    mode_t mode = 0644;
    struct stat info {};
    if (fstatat(directory.fd, file.c_str(), &info, AT_SYMLINK_NOFOLLOW) == 0) {
        if (!S_ISREG(info.st_mode))
            throw std::runtime_error("data file target is not a regular file: " + normalized);
        const auto existing = read_at(directory.fd, file, normalized);
        if (existing) {
            try { parse_data(*existing, normalized); }
            catch (const std::exception&) {
                throw std::runtime_error("data_write refuses to replace a non-data file: " + normalized);
            }
        }
        mode = info.st_mode & 07777;
    } else if (errno != ENOENT) {
        throw std::runtime_error("cannot inspect data file: " + normalized);
    }
    static std::atomic<uint64_t> next_temp{0};
    std::string temp;
    int fd = -1;
    for (int attempt = 0; attempt < 100 && fd < 0; ++attempt) {
        temp = "." + file + ".tmp." + std::to_string(getpid()) + "." + std::to_string(next_temp++);
        fd = openat(directory.fd, temp.c_str(),
                    O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0644);
        if (fd < 0 && errno != EEXIST) break;
    }
    if (fd < 0) throw std::runtime_error("cannot create temporary data file for " + normalized);
    FileDescriptor output(fd);
    try {
        if (S_ISREG(info.st_mode) && fchmod(output.fd, mode) != 0)
            throw std::runtime_error("cannot preserve data file permissions: " + normalized);
        size_t offset = 0;
        while (offset < bytes.size()) {
            const ssize_t written = write(output.fd, bytes.data() + offset, bytes.size() - offset);
            if (written < 0 && errno == EINTR) continue;
            if (written <= 0) throw std::runtime_error("cannot write data file: " + normalized);
            offset += size_t(written);
        }
        if (fsync(output.fd) != 0 ||
            renameat(directory.fd, temp.c_str(), directory.fd, file.c_str()) != 0)
            throw std::runtime_error("cannot finish data file: " + normalized);
        fsync(directory.fd);
    } catch (...) {
        unlinkat(directory.fd, temp.c_str(), 0);
        throw;
    }
}

std::optional<SaveValue> read_data_file(const std::filesystem::path& project_root,
                                        const std::string& relative) {
    const auto parts = data_path(relative);
    const std::string normalized = join(parts, parts.size());
    FileDescriptor directory(open_directories(project_root, parts, false, normalized));
    if (directory.fd < 0) return std::nullopt;
    const auto bytes = read_at(directory.fd, parts.back(), normalized);
    if (!bytes) return std::nullopt;
    return parse_data(*bytes, normalized);
}

}  // namespace acuppa
