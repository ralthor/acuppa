#pragma once

#include "threepoint/input.hpp"
#include "threepoint/scene_frame.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace threepoint {

class LuaProject {
public:
    enum class Mode { Game, EditorPreview };
    explicit LuaProject(const std::filesystem::path& root,
                        std::string initial_scene = {}, Mode mode = Mode::Game,
                        bool audio_output = true);
    ~LuaProject();
    LuaProject(const LuaProject&) = delete;
    LuaProject& operator=(const LuaProject&) = delete;

    uint32_t width() const;
    uint32_t height() const;
    void inject(const InputEvent& event);
    void set_pointer(double x, double y);
    void set_viewport(uint32_t width, uint32_t height);
    SceneFrame step(double dt);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace threepoint
