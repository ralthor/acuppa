#pragma once

#include <filesystem>

namespace threepoint {

struct ProjectPaths {
    std::filesystem::path root;
    std::filesystem::path manifest;
};

// Checks the entry point only. Scene and script paths come from game.lua in Phase 4.
ProjectPaths require_project(const std::filesystem::path& root);

}  // namespace threepoint
