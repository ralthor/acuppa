#pragma once

#include <cstdint>
#include <filesystem>

namespace threepoint {

struct PreviewOptions {
    uint32_t width = 960;
    uint32_t height = 540;
    uint32_t frame_limit = 0;       // Zero runs until the window is closed.
    uint32_t resize_after = 0;      // Zero disables the automated resize check.
    std::filesystem::path capture_path; // Optional PPM readback of the first frame.
};

struct PreviewResult {
    uint32_t frames = 0;
    uint32_t swapchain_rebuilds = 0;
};

PreviewResult run_preview(const PreviewOptions& options);

}  // namespace threepoint
