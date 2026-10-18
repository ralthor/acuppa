#pragma once

#include "threepoint/scene_frame.hpp"

#include <cstdint>
#include <vector>

namespace threepoint {

// RGBA8, top-to-bottom rows, alpha composited in source order.
std::vector<uint8_t> rasterize_ui(uint32_t width, uint32_t height,
                                  const std::vector<UiElement>& elements);

}  // namespace threepoint
