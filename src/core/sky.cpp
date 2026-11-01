#include "acuppa/sky.hpp"

#include <glm/common.hpp>
#include <glm/exponential.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <cmath>
#include <stdexcept>
#include <string>

namespace acuppa {
namespace {

void check_color(glm::vec3 color, const char* field) {
    for (int i = 0; i < 3; ++i)
        if (!std::isfinite(color[i]) || color[i] < 0.0f || color[i] > kSkyMaxColor)
            throw std::invalid_argument(std::string("sky.") + field +
                                        " must be three finite numbers from 0 to 64");
}

float smooth(float edge0, float edge1, float x) {
    const float t = glm::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

}  // namespace

SkyGradient validated_sky(const SkyGradient& sky) {
    check_color(sky.zenith, "zenith");
    check_color(sky.horizon, "horizon");
    check_color(sky.ground, "ground");
    check_color(sky.sun_color, "sun_color");
    const glm::vec3 sun = sky.sun_direction;
    if (!std::isfinite(sun.x) || !std::isfinite(sun.y) || !std::isfinite(sun.z))
        throw std::invalid_argument("sky.sun_direction must be finite");
    const float length = glm::length(sun);
    if (!std::isfinite(length) || length < 1e-6f)
        throw std::invalid_argument("sky.sun_direction must be finite and nonzero");
    if (!std::isfinite(sky.sun_size) || sky.sun_size < 0.0f || sky.sun_size > kSkyMaxSunSize)
        throw std::invalid_argument("sky.sun_size must be from 0 to 20 degrees");
    SkyGradient result = sky;
    result.sun_direction = sun / length;
    return result;
}

SkySunShape sky_sun_shape(float sun_size_degrees) {
    SkySunShape shape;
    if (!(sun_size_degrees > 0.0f)) return shape;
    const float radius = glm::radians(sun_size_degrees);
    shape.outer_cos = std::cos(radius);
    shape.inner_cos = std::cos(radius * 0.7f);
    // A faint glow that fades over about three disc radii.
    shape.halo_sharpness = 1.0f / (4.5f * radius * radius);
    shape.halo_strength = 0.03f;
    return shape;
}

glm::vec3 sky_radiance(const SkyGradient& sky, glm::vec3 direction) {
    const float height = direction.y;
    const float up = std::pow(glm::max(height, 0.0f), 0.5f);
    glm::vec3 color = glm::mix(sky.horizon, sky.zenith, smooth(0.0f, 1.0f, up));
    color = glm::mix(color, sky.ground, smooth(0.0f, 0.12f, -height));
    const SkySunShape sun = sky_sun_shape(sky.sun_size);
    if (sky.sun_size > 0.0f) {
        const float facing = glm::dot(direction, sky.sun_direction);
        const float disc = smooth(sun.outer_cos, sun.inner_cos, facing);
        const float halo = sun.halo_strength *
                           std::exp((facing - 1.0f) * sun.halo_sharpness);
        color += sky.sun_color * (disc + halo);
    }
    return color;
}

}  // namespace acuppa
