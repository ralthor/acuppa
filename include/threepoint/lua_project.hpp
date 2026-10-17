#pragma once

#include "threepoint/scene_frame.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>

namespace threepoint {

class LuaProject {
public:
    explicit LuaProject(const std::filesystem::path& root);
    ~LuaProject();
    LuaProject(const LuaProject&) = delete;
    LuaProject& operator=(const LuaProject&) = delete;

    uint32_t width() const;
    uint32_t height() const;
    SceneFrame step(double dt);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace threepoint
