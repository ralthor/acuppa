#include "threepoint/environment_filter.hpp"

#include <cmath>
#include <filesystem>
#include <limits>
#include <stdexcept>

void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main(int argc, char* argv[]) {
    if (argc != 2) throw std::runtime_error("HDRI fixture path required");
    const auto source = threepoint::load_hdr_image(std::filesystem::path(argv[1]));
    const auto filtered = threepoint::filter_environment(source);
    expect(filtered.diffuse.width == 64 && filtered.diffuse.height == 32,
           "diffuse prefilter dimensions changed");
    expect(filtered.specular.size() == 7 && filtered.specular.front().width == 256 &&
           filtered.specular.back().width == 4 && filtered.specular.back().height == 2,
           "specular roughness pyramid is incomplete");
    for (const auto* image : {&filtered.diffuse, &filtered.specular.front(),
                              &filtered.specular.back()}) {
        expect(image->rgba.size() == size_t(image->width) * image->height * 4,
               "prefilter RGBA size is wrong");
        for (float value : image->rgba)
            expect(std::isfinite(value) && value >= 0, "prefilter output has invalid radiance");
    }
    float sharp_max = 0, rough_max = 0;
    for (size_t i = 2; i < filtered.specular.front().rgba.size(); i += 4)
        sharp_max = std::max(sharp_max, filtered.specular.front().rgba[i]);
    for (size_t i = 2; i < filtered.specular.back().rgba.size(); i += 4)
        rough_max = std::max(rough_max, filtered.specular.back().rgba[i]);
    expect(sharp_max > rough_max * 1.5f, "rough GGX level did not blur the HDRI highlight");

    auto invalid = source;
    invalid.rgba[0] = std::numeric_limits<float>::quiet_NaN();
    bool rejected = false;
    try { (void)threepoint::filter_environment(invalid); }
    catch (const std::invalid_argument&) { rejected = true; }
    expect(rejected, "nonfinite HDR radiance passed prefilter validation");
    invalid = source;
    invalid.rgba.pop_back();
    rejected = false;
    try { (void)threepoint::filter_environment(invalid); }
    catch (const std::invalid_argument&) { rejected = true; }
    expect(rejected, "truncated HDR radiance passed prefilter validation");
}
