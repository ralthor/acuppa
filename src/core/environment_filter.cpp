#include "threepoint/environment_filter.hpp"

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace threepoint {
namespace {

constexpr float pi = 3.14159265358979323846f;
constexpr int samples = 64;

float radical_inverse(uint32_t bits) {
    bits = (bits << 16) | (bits >> 16);
    bits = ((bits & 0x55555555u) << 1) | ((bits & 0xaaaaaaaau) >> 1);
    bits = ((bits & 0x33333333u) << 2) | ((bits & 0xccccccccu) >> 2);
    bits = ((bits & 0x0f0f0f0fu) << 4) | ((bits & 0xf0f0f0f0u) >> 4);
    bits = ((bits & 0x00ff00ffu) << 8) | ((bits & 0xff00ff00u) >> 8);
    return float(bits) * 2.3283064365386963e-10f;
}

glm::vec3 direction(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    const float azimuth = 2 * pi * ((float(x) + 0.5f) / float(width) - 0.5f);
    const float polar = pi * (float(y) + 0.5f) / float(height);
    return {std::cos(azimuth) * std::sin(polar), std::cos(polar),
            std::sin(azimuth) * std::sin(polar)};
}

glm::vec3 sample(const HdrImage& image, glm::vec3 ray) {
    const float u = std::atan2(ray.z, ray.x) / (2 * pi) + 0.5f;
    const float v = std::acos(std::clamp(ray.y, -1.0f, 1.0f)) / pi;
    const float fx = u * float(image.width) - 0.5f;
    const float fy = v * float(image.height) - 0.5f;
    const int x0 = int(std::floor(fx));
    const int y0 = std::clamp(int(std::floor(fy)), 0, int(image.height) - 1);
    const int y1 = std::clamp(y0 + 1, 0, int(image.height) - 1);
    const float tx = fx - std::floor(fx);
    const float ty = std::clamp(fy - std::floor(fy), 0.0f, 1.0f);
    const auto texel = [&](int x, int y) {
        x = (x % int(image.width) + int(image.width)) % int(image.width);
        const size_t at = (size_t(y) * image.width + size_t(x)) * 4;
        return glm::vec3(image.rgba[at], image.rgba[at + 1], image.rgba[at + 2]);
    };
    return glm::mix(glm::mix(texel(x0, y0), texel(x0 + 1, y0), tx),
                    glm::mix(texel(x0, y1), texel(x0 + 1, y1), tx), ty);
}

void store(HdrImage& image, uint32_t x, uint32_t y, glm::vec3 value) {
    const size_t at = (size_t(y) * image.width + x) * 4;
    image.rgba[at] = value.x;
    image.rgba[at + 1] = value.y;
    image.rgba[at + 2] = value.z;
    image.rgba[at + 3] = 1.0f;
}

HdrImage blank(uint32_t width, uint32_t height) {
    return {width, height, std::vector<float>(size_t(width) * height * 4, 0.0f)};
}

void basis(glm::vec3 normal, glm::vec3& tangent, glm::vec3& bitangent) {
    tangent = glm::normalize(glm::cross(std::abs(normal.y) < 0.99f
        ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0), normal));
    bitangent = glm::cross(normal, tangent);
}

}  // namespace

FilteredEnvironment filter_environment(const HdrImage& source) {
    if (source.width < 2 || source.width > 4096 || source.height < 1 ||
        source.height > 2048 || source.width != source.height * 2 ||
        source.rgba.size() != size_t(source.width) * source.height * 4)
        throw std::invalid_argument("HDRI prefilter needs bounded 2:1 RGBA data");
    for (float value : source.rgba)
        if (!std::isfinite(value) || value < 0)
            throw std::invalid_argument("HDRI prefilter needs finite nonnegative radiance");

    FilteredEnvironment result;
    result.diffuse = blank(64, 32);
    for (uint32_t y = 0; y < result.diffuse.height; ++y) {
        for (uint32_t x = 0; x < result.diffuse.width; ++x) {
            const glm::vec3 normal = direction(x, y, 64, 32);
            glm::vec3 tangent, bitangent;
            basis(normal, tangent, bitangent);
            glm::vec3 sum(0);
            for (int i = 0; i < samples; ++i) {
                const float angle = 2 * pi * float(i) / float(samples);
                const float z = std::sqrt(1.0f - radical_inverse(uint32_t(i)));
                const float r = std::sqrt(radical_inverse(uint32_t(i)));
                const glm::vec3 ray = tangent * (std::cos(angle) * r) +
                                      bitangent * (std::sin(angle) * r) + normal * z;
                sum += sample(source, ray);
            }
            store(result.diffuse, x, y, sum / float(samples));
        }
    }

    for (uint32_t width = 256, height = 128; width >= 4; width /= 2, height /= 2) {
        result.specular.push_back(blank(width, height));
        if (width == 4) break;
    }
    for (size_t level = 0; level < result.specular.size(); ++level) {
        HdrImage& image = result.specular[level];
        const float roughness = float(level) / float(result.specular.size() - 1);
        const float a = roughness * roughness;
        for (uint32_t y = 0; y < image.height; ++y) {
            for (uint32_t x = 0; x < image.width; ++x) {
                const glm::vec3 normal = direction(x, y, image.width, image.height);
                if (level == 0) {
                    store(image, x, y, sample(source, normal));
                    continue;
                }
                glm::vec3 tangent, bitangent;
                basis(normal, tangent, bitangent);
                glm::vec3 sum(0);
                float weight = 0;
                for (int i = 0; i < samples; ++i) {
                    const float angle = 2 * pi * float(i) / float(samples);
                    const float xi = radical_inverse(uint32_t(i));
                    const float z = std::sqrt((1.0f - xi) / (1.0f + (a * a - 1.0f) * xi));
                    const float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
                    const glm::vec3 half = tangent * (std::cos(angle) * r) +
                                           bitangent * (std::sin(angle) * r) + normal * z;
                    const glm::vec3 ray = glm::reflect(-normal, half);
                    const float ndotl = std::max(glm::dot(normal, ray), 0.0f);
                    if (ndotl > 0) {
                        sum += sample(source, ray) * ndotl;
                        weight += ndotl;
                    }
                }
                store(image, x, y, weight > 0 ? sum / weight : glm::vec3(0));
            }
        }
    }
    return result;
}

}  // namespace threepoint
