#pragma once

#include "acuppa/scene_frame.hpp"

#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace acuppa {

struct ShadowAtlasLayout {
    static constexpr uint32_t tile = 1024;
    static constexpr uint32_t columns = 6;
    std::array<int, 8> row_for_light{};
    uint32_t rows = 0;
    uint64_t bytes() const { return uint64_t(tile) * columns * tile * rows * 4; }
};

inline ShadowAtlasLayout plan_shadow_atlas(const std::vector<LightInstance>& lights) {
    if (lights.size() > 8) throw std::invalid_argument("shadow atlas supports at most eight lights");
    ShadowAtlasLayout layout;
    layout.row_for_light.fill(-1);
    for (size_t i = 0; i < lights.size(); ++i)
        if (lights[i].shadows) layout.row_for_light[i] = int(layout.rows++);
    return layout;
}

}  // namespace acuppa
