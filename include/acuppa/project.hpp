#pragma once

#include <filesystem>

namespace acuppa {

struct ProjectPaths {
    std::filesystem::path root;
    std::filesystem::path manifest;
};

// Checks the entry point only. LuaProject validates scene and script paths.
ProjectPaths require_project(const std::filesystem::path& root);

}  // namespace acuppa
