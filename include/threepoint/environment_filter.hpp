#pragma once

#include "threepoint/hdr_image.hpp"

#include <vector>

namespace threepoint {

struct FilteredEnvironment {
    HdrImage diffuse;                 // Cosine-weighted irradiance, 64x32.
    std::vector<HdrImage> specular;   // GGX roughness levels, 256x128 down to 4x2.
};

// Deterministic CPU prefilter. Call once per HDRI, not once per rendered frame.
FilteredEnvironment filter_environment(const HdrImage& source);

}  // namespace threepoint
