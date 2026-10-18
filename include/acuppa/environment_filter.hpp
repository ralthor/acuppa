#pragma once

#include "acuppa/hdr_image.hpp"

#include <vector>

namespace acuppa {

struct FilteredEnvironment {
    HdrImage diffuse;                 // Cosine-weighted irradiance, 64x32.
    std::vector<HdrImage> specular;   // GGX roughness levels, 256x128 down to 4x2.
};

// Deterministic CPU prefilter. Call once per HDRI, not once per rendered frame.
FilteredEnvironment filter_environment(const HdrImage& source);

}  // namespace acuppa
