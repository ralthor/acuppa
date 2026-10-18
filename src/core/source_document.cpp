#include "acuppa/source_document.hpp"

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

#include <atomic>
#include <cerrno>
#include <fcntl.h>
#include <fstream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace acuppa {
namespace fs = std::filesystem;
namespace {

constexpr size_t source_limit = 1024 * 1024;

std::string read_source(const fs::path& path) {
    std::error_code error;
    if (fs::file_size(path, error) > source_limit || error)
        throw std::runtime_error("Lua source exceeds 1 MiB or cannot be read");
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot read Lua source");
    std::string text((std::istreambuf_iterator<char>(input)),
                     std::istreambuf_iterator<char>());
    if (input.bad() || text.size() > source_limit)
        throw std::runtime_error("Lua source exceeds 1 MiB or cannot be read");
    return text;
}

void validate_lua(const std::string& source) {
    lua_State* lua = luaL_newstate();
    if (!lua) throw std::runtime_error("cannot create Lua parser");
    const int result = luaL_loadbufferx(lua, source.data(), source.size(), "editor source", "t");
    if (result != LUA_OK) {
        const std::string error = lua_tostring(lua, -1);
        lua_close(lua);
        throw std::invalid_argument("invalid Lua source: " + error);
    }
    lua_close(lua);
}

void atomic_write(const fs::path& path, const std::string& source) {
    struct stat metadata {};
    if (stat(path.c_str(), &metadata) != 0)
        throw std::runtime_error("cannot stat Lua source");
    static std::atomic<uint64_t> serial{0};
    const fs::path temp = path.string() + ".tmp." + std::to_string(getpid()) + "." +
                          std::to_string(serial++);
    const int fd = open(temp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) throw std::runtime_error("cannot create temporary Lua source");
    bool open_fd = true;
    try {
        if (fchmod(fd, metadata.st_mode & 0777) != 0)
            throw std::runtime_error("cannot preserve Lua source permissions");
        size_t offset = 0;
        while (offset < source.size()) {
            const ssize_t count = write(fd, source.data() + offset, source.size() - offset);
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) throw std::runtime_error("cannot write Lua source");
            offset += size_t(count);
        }
        if (fsync(fd) != 0) throw std::runtime_error("cannot sync Lua source");
        if (close(fd) != 0) { open_fd = false; throw std::runtime_error("cannot close Lua source"); }
        open_fd = false;
        if (rename(temp.c_str(), path.c_str()) != 0)
            throw std::runtime_error("cannot replace Lua source");
        const int dir = open(path.parent_path().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (dir >= 0) { fsync(dir); close(dir); }
    } catch (...) {
        if (open_fd) close(fd);
        unlink(temp.c_str());
        throw;
    }
}

}  // namespace

SourceDocument::SourceDocument(const fs::path& project_root,
                               const fs::path& project_relative_file) {
    const fs::path root = fs::canonical(project_root);
    if (project_relative_file.empty() || project_relative_file.is_absolute() ||
        project_relative_file.extension() != ".lua")
        throw std::invalid_argument("source path must be a project-relative .lua file");
    const fs::path candidate = root / project_relative_file;
    std::error_code error;
    path_ = fs::weakly_canonical(candidate, error);
    if (error) throw std::invalid_argument("cannot resolve Lua source path");
    const fs::path relative = path_.lexically_relative(root);
    if (relative.empty() || *relative.begin() == ".." || !fs::is_regular_file(path_))
        throw std::invalid_argument("source path is outside project or not a regular file");
    if (fs::is_symlink(candidate))
        throw std::invalid_argument("source file must not be a symlink");
    original_ = read_source(path_);
}

void SourceDocument::replace(const std::string& source) {
    if (source.size() > source_limit)
        throw std::invalid_argument("Lua source exceeds 1 MiB");
    validate_lua(source);
    if (read_source(path_) != original_)
        throw std::runtime_error("Lua source changed on disk since editor opened it");
    atomic_write(path_, source);
    original_ = source;
}

}  // namespace acuppa
