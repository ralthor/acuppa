#include "acuppa/project.hpp"

#include <stdexcept>
#include <string>
#include <system_error>

namespace acuppa {
namespace {

bool project_directory_exists(const std::filesystem::path& path) {
    std::error_code error;
    const bool result = std::filesystem::is_directory(path, error);
    if (error && error != std::errc::no_such_file_or_directory) {
        throw std::runtime_error("cannot inspect '" + path.string() + "': " + error.message());
    }
    return result;
}

void require_file(const std::filesystem::path& path) {
    std::error_code error;
    const bool result = std::filesystem::is_regular_file(path, error);
    if (error && error != std::errc::no_such_file_or_directory) {
        throw std::runtime_error("cannot inspect '" + path.string() + "': " + error.message());
    }
    if (!result) {
        throw std::runtime_error("required project file is missing: " + path.string());
    }
}

}  // namespace

ProjectPaths require_project(const std::filesystem::path& root) {
    if (root.empty() || !project_directory_exists(root)) {
        throw std::runtime_error("project directory does not exist: " + root.string());
    }

    ProjectPaths paths{root, root / "game.lua"};
    require_file(paths.manifest);
    return paths;
}

}  // namespace acuppa
