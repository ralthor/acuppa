#pragma once

#include "acuppa/input.hpp"
#include "acuppa/scene_frame.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>

namespace acuppa {

struct PreviewOptions {
    enum class Scene { Clear, Cube, Project };
    Scene scene = Scene::Clear;
    uint32_t width = 960;
    uint32_t height = 540;
    uint32_t frame_limit = 0;       // Zero runs until the window is closed.
    uint32_t resize_after = 0;      // Zero disables the automated resize check.
    std::filesystem::path capture_path; // Optional PPM readback.
    uint32_t capture_frame = 1;
    float cube_angle = 0.0f;     // Capture angle in radians; live preview rotates over time.
    double fixed_delta = 0.0;    // Positive value drives deterministic scripted frames.
    std::function<SceneFrame(double)> on_frame;
    std::function<void(const InputEvent&)> on_input;
    std::function<void(double, double)> on_pointer;
    std::function<void(uint32_t, uint32_t)> on_resize;
};

struct PreviewResult {
    uint32_t frames = 0;
    uint32_t swapchain_rebuilds = 0;
};

PreviewResult run_preview(const PreviewOptions& options);

}  // namespace acuppa
