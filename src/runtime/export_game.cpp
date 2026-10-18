#include "export_game.hpp"

#include "threepoint/project.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unistd.h>

namespace fs = std::filesystem;
namespace {

bool is_within(const fs::path& child, const fs::path& parent) {
    auto left = child.begin();
    auto right = parent.begin();
    for (; right != parent.end(); ++right, ++left)
        if (left == child.end() || *left != *right) return false;
    return true;
}

bool ship(const fs::path& file) {
    static constexpr std::array<const char*, 11> extensions = {
        ".lua", ".gltf", ".glb", ".bin", ".png", ".jpg", ".jpeg",
        ".hdr", ".wav", ".mp3", ".flac"};
    const auto extension = file.extension().string();
    return std::find_if(extensions.begin(), extensions.end(),
                        [&](const char* value) { return extension == value; }) != extensions.end();
}

}  // namespace

void export_game(const fs::path& project, const fs::path& destination,
                 const fs::path& player_binary) {
    const fs::path root = fs::canonical(threepoint::require_project(project).root);
    if (destination.empty()) throw std::invalid_argument("build destination is empty");
    if (!fs::is_regular_file(player_binary))
        throw std::runtime_error("threepoint-player was not built beside the CLI");
    const fs::path output = fs::weakly_canonical(destination);
    if (is_within(output, root) || is_within(root, output))
        throw std::invalid_argument("build destination must be separate from the project");
    if (fs::exists(output)) throw std::invalid_argument("build destination already exists");
    const fs::path staging = output.string() + ".tmp-" + std::to_string(getpid());
    if (fs::exists(staging)) throw std::invalid_argument("staging destination already exists");
    fs::create_directories(output.parent_path());
    try {
        fs::create_directories(staging / "game");
        uintmax_t bytes = 0;
        for (fs::recursive_directory_iterator it(root), end; it != end; ++it) {
            const fs::path relative = it->path().lexically_relative(root);
            const std::string name = it->path().filename().string();
            if (it->is_symlink())
                throw std::invalid_argument("project symlinks cannot be exported: " + relative.string());
            if (it->is_directory()) {
                if (name.empty() || name[0] == '.' || name == "saves" || name == "tests" ||
                    name == "build") it.disable_recursion_pending();
                continue;
            }
            if (!it->is_regular_file())
                throw std::invalid_argument("unsupported project entry: " + relative.string());
            if (name.empty() || name[0] == '.' || !ship(it->path())) continue;
            const uintmax_t size = it->file_size();
            if (size > 64 * 1024 * 1024 || bytes + size > 512 * 1024 * 1024)
                throw std::invalid_argument("game bundle exceeds file or total size limit");
            bytes += size;
            fs::create_directories((staging / "game" / relative).parent_path());
            fs::copy_file(it->path(), staging / "game" / relative);
        }
        if (!fs::is_regular_file(staging / "game/game.lua"))
            throw std::runtime_error("game.lua was not copied into the bundle");
        fs::copy_file(player_binary, staging / "threepoint-game");
        fs::permissions(staging / "threepoint-game",
                        fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                        fs::perm_options::add);
        fs::rename(staging, output);
    } catch (...) {
        std::error_code error;
        fs::remove_all(staging, error);
        throw;
    }
}
