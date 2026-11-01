#pragma once

#include <glm/vec3.hpp>

namespace acuppa {

// A source-authored background drawn behind 3D geometry. Colors are linear HDR
// radiance (0..64) and pass through exposure and tonemapping like the scene.
struct SkyGradient {
    glm::vec3 zenith{0.05f, 0.22f, 0.95f};
    glm::vec3 horizon{0.75f, 0.88f, 1.0f};
    glm::vec3 ground{0.22f, 0.26f, 0.16f};
    glm::vec3 sun_direction{0.4f, 0.6f, -0.7f};  // From the scene toward the sun.
    glm::vec3 sun_color{14.0f, 12.0f, 9.0f};
    float sun_size = 0.0f;  // Angular radius in degrees, 0..20; 0 draws no disc.
};

constexpr float kSkyMaxColor = 64.0f;
constexpr float kSkyMaxSunSize = 20.0f;

// Validates every field and returns a copy with a unit sun direction. Throws
// std::invalid_argument naming the bad field.
SkyGradient validated_sky(const SkyGradient& sky);

// Shape terms shared by the CPU reference and the Vulkan sky shader.
struct SkySunShape {
    float outer_cos = 1.0f;  // Disc edge.
    float inner_cos = 1.0f;  // Fully bright core.
    float halo_sharpness = 0.0f;
    float halo_strength = 0.0f;
};
SkySunShape sky_sun_shape(float sun_size_degrees);

// Radiance seen along a unit view direction (CPU reference of the shader).
glm::vec3 sky_radiance(const SkyGradient& sky, glm::vec3 direction);

}  // namespace acuppa
