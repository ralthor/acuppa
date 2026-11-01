#include "acuppa/sky.hpp"

#include <glm/geometric.hpp>

#include <cmath>
#include <functional>
#include <limits>
#include <stdexcept>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool near(glm::vec3 a, glm::vec3 b, float tolerance = 1e-4f) {
    return glm::length(a - b) < tolerance;
}

void rejects(const std::function<void(acuppa::SkyGradient&)>& change, const char* message) {
    acuppa::SkyGradient sky;
    change(sky);
    try { acuppa::validated_sky(sky); }
    catch (const std::invalid_argument&) { return; }
    throw std::runtime_error(message);
}

}  // namespace

int main() {
    using acuppa::SkyGradient;
    SkyGradient sky;
    sky.zenith = {0.1f, 0.3f, 0.9f};
    sky.horizon = {0.8f, 0.9f, 1.0f};
    sky.ground = {0.3f, 0.25f, 0.2f};
    sky.sun_direction = {0, 2, 0};
    sky.sun_size = 0;
    const SkyGradient valid = acuppa::validated_sky(sky);
    expect(near(valid.sun_direction, {0, 1, 0}), "sun direction was not normalized");

    expect(near(acuppa::sky_radiance(valid, {0, 1, 0}), sky.zenith), "zenith color wrong");
    expect(near(acuppa::sky_radiance(valid, {1, 0, 0}), sky.horizon), "horizon color wrong");
    expect(near(acuppa::sky_radiance(valid, {0, -1, 0}), sky.ground), "ground color wrong");
    const glm::vec3 mid = acuppa::sky_radiance(valid, glm::normalize(glm::vec3(1, 1, 0)));
    expect(mid.r < sky.horizon.r && mid.r > sky.zenith.r && mid.b > 0.9f,
           "upper sky did not blend between horizon and zenith");
    const glm::vec3 low = acuppa::sky_radiance(valid, glm::normalize(glm::vec3(1, -0.06f, 0)));
    expect(low.r < sky.horizon.r && low.r > sky.ground.r,
           "horizon did not fade smoothly into the ground");
    float previous = acuppa::sky_radiance(valid, {1, 0, 0}).b;
    for (int i = 1; i <= 20; ++i) {
        const float angle = 1.5707963f * float(i) / 20.0f;
        const float blue = acuppa::sky_radiance(
            valid, {std::cos(angle), std::sin(angle), 0}).b;
        expect(blue <= previous + 1e-6f, "sky gradient was not monotonic toward the zenith");
        previous = blue;
    }

    // No disc when sun_size is zero; with a disc the center is much brighter
    // than the sky, the edge is soft, and the glow is faint further out.
    sky.sun_direction = {0, 1, 1};
    const SkyGradient no_sun = acuppa::validated_sky(sky);
    const glm::vec3 toward = glm::normalize(glm::vec3(0, 1, 1));
    sky.sun_size = 3;
    sky.sun_color = {10, 9, 7};
    const SkyGradient sunny = acuppa::validated_sky(sky);
    const glm::vec3 base = acuppa::sky_radiance(no_sun, toward);
    const glm::vec3 center = acuppa::sky_radiance(sunny, toward);
    expect(center.r > base.r + 9.0f, "sun disc was not brighter than the sky");
    const auto at_angle = [&](float degrees) {
        const float radians = degrees * 3.14159265f / 180.0f;
        const glm::vec3 direction = glm::normalize(glm::vec3(
            0, std::cos(0.785398f + radians), std::sin(0.785398f + radians)));
        return acuppa::sky_radiance(sunny, direction).r -
               acuppa::sky_radiance(no_sun, direction).r;
    };
    const float edge = at_angle(2.6f);
    expect(edge > 0.5f && edge < 9.0f, "sun disc edge was not soft");
    expect(at_angle(3.5f) < 0.5f && at_angle(20.0f) < 0.05f &&
           at_angle(40.0f) < 0.001f,
           "sun glow spread too far from the disc");

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    rejects([&](SkyGradient& s) { s.zenith.r = -0.1f; }, "negative zenith accepted");
    rejects([&](SkyGradient& s) { s.horizon.g = 65.0f; }, "horizon above 64 accepted");
    rejects([&](SkyGradient& s) { s.ground.b = nan; }, "NaN ground accepted");
    rejects([&](SkyGradient& s) { s.sun_color.r = inf; }, "infinite sun color accepted");
    rejects([&](SkyGradient& s) { s.sun_direction = {0, 0, 0}; }, "zero sun direction accepted");
    rejects([&](SkyGradient& s) { s.sun_direction.x = nan; }, "NaN sun direction accepted");
    rejects([&](SkyGradient& s) { s.sun_direction = {3e38f, 3e38f, 0}; },
            "overflowing sun direction accepted");
    rejects([&](SkyGradient& s) { s.sun_size = -1; }, "negative sun size accepted");
    rejects([&](SkyGradient& s) { s.sun_size = 21; }, "sun size above 20 accepted");
    rejects([&](SkyGradient& s) { s.sun_size = nan; }, "NaN sun size accepted");
    SkyGradient bounds;
    bounds.zenith = {64, 64, 64};
    bounds.ground = {0, 0, 0};
    bounds.sun_size = 20;
    acuppa::validated_sky(bounds);
    acuppa::validated_sky(SkyGradient{});
}
