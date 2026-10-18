#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace acuppa {

struct HdrImage {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<float> rgba;
};

// Decodes a Radiance equirectangular HDR file to linear RGBA pixels.
HdrImage load_hdr_image(const std::filesystem::path& file);

}  // namespace acuppa
