#include "threepoint/hdr_image.hpp"

#include <stb_image.h>

#include <cmath>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>

namespace threepoint {

HdrImage load_hdr_image(const std::filesystem::path& file) {
    if (file.extension() != ".hdr")
        throw std::runtime_error(file.string() + ": environment image must end in .hdr");
    std::error_code error;
    const auto size = std::filesystem::file_size(file, error);
    if (error || size == 0)
        throw std::runtime_error(file.string() + ": environment image is missing or empty");
    if (size > 64 * 1024 * 1024)
        throw std::runtime_error(file.string() + ": environment image exceeds 64 MiB");
    std::ifstream input(file, std::ios::binary);
    const std::vector<unsigned char> bytes(std::istreambuf_iterator<char>{input}, {});
    if (!input && !input.eof())
        throw std::runtime_error(file.string() + ": cannot read environment image");
    if (!stbi_is_hdr_from_memory(bytes.data(), int(bytes.size())))
        throw std::runtime_error(file.string() + ": invalid Radiance HDR image");
    int width = 0, height = 0, channels = 0;
    if (!stbi_info_from_memory(bytes.data(), int(bytes.size()),
                               &width, &height, &channels) ||
        width < 2 || width > 4096 || height < 1 || height > 2048 || width != 2 * height)
        throw std::runtime_error(file.string() + ": HDRI must be 2:1 and at most 4096x2048");
    float* decoded = stbi_loadf_from_memory(bytes.data(), int(bytes.size()),
                                            &width, &height, &channels, 4);
    if (!decoded) throw std::runtime_error(file.string() + ": cannot decode HDRI");
    std::unique_ptr<float, decltype(&stbi_image_free)> guard(decoded, stbi_image_free);
    const size_t count = size_t(width) * size_t(height) * 4;
    HdrImage image{uint32_t(width), uint32_t(height),
                   std::vector<float>(decoded, decoded + count)};
    for (float value : image.rgba) {
        if (!std::isfinite(value) || value < 0)
            throw std::runtime_error(file.string() + ": invalid HDR radiance value");
    }
    return image;
}

}  // namespace threepoint
