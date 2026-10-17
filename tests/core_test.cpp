#include "threepoint/project.hpp"

#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void expect_failure(const std::filesystem::path& path, const std::string& message) {
    try {
        threepoint::require_project(path);
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()).find(message) != std::string::npos) {
            return;
        }
        throw std::runtime_error("unexpected diagnostic: " + std::string(error.what()));
    }
    throw std::runtime_error("invalid project accepted: " + path.string());
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "expected feature-scene directory\n";
        return 2;
    }
    try {
        const std::filesystem::path root(argv[1]);
        const auto project = threepoint::require_project(root);
        if (project.root != root || project.manifest != root / "game.lua") {
            throw std::runtime_error("project paths do not match source layout");
        }
        if (!std::filesystem::is_regular_file(root / "scenes/main.scene.lua") ||
            !std::filesystem::is_regular_file(root / "scripts/main.lua")) {
            throw std::runtime_error("feature scene source files are missing");
        }
        expect_failure({}, "project directory does not exist");
        expect_failure(root / "not-a-project", "project directory does not exist");
        expect_failure(root / "scenes", "game.lua");
        const auto fixture = std::filesystem::current_path() / "core-project-layout-test";
        std::filesystem::remove_all(fixture);
        std::filesystem::create_directory(fixture);
        expect_failure(fixture, "game.lua");
        std::filesystem::create_directory(fixture / "game.lua");
        expect_failure(fixture, "game.lua");
        std::filesystem::remove(fixture / "game.lua");
        {
            std::ofstream manifest(fixture / "game.lua");
            manifest << "return {}\n";
        }
        threepoint::require_project(fixture);
        std::filesystem::remove_all(fixture);
        std::cout << "Core project checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
