#include "acuppa/ui_canvas.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

#include <stb_easy_font.h>

namespace acuppa {
namespace {

struct Vertex { float x, y, z; uint8_t color[4]; };

void fill(std::vector<uint8_t>& pixels, uint32_t width, uint32_t height,
          float x, float y, float w, float h, const UiElement& element) {
    if (w <= 0 || h <= 0 || element.alpha == 0) return;
    const int left = std::max(0, int(std::floor(x)));
    const int top = std::max(0, int(std::floor(y)));
    const int right = std::min(int(width), int(std::ceil(x + w)));
    const int bottom = std::min(int(height), int(std::ceil(y + h)));
    if (left >= right || top >= bottom) return;
    const uint32_t source_alpha = element.alpha;
    for (int row = top; row < bottom; ++row) {
        for (int col = left; col < right; ++col) {
            uint8_t* pixel = pixels.data() + (size_t(row) * width + size_t(col)) * 4;
            const uint32_t old_alpha = pixel[3];
            const uint32_t combined = source_alpha * 255 + old_alpha * (255 - source_alpha);
            for (int channel = 0; channel < 3; ++channel) {
                const uint32_t source = channel == 0 ? element.red :
                                        channel == 1 ? element.green : element.blue;
                pixel[channel] = uint8_t((source * source_alpha * 255 +
                    pixel[channel] * old_alpha * (255 - source_alpha)) /
                    std::max(1u, combined));
            }
            pixel[3] = uint8_t(combined / 255);
        }
    }
}

void text(std::vector<uint8_t>& pixels, uint32_t width, uint32_t height,
          const UiElement& element) {
    if (element.text.empty()) return;
    if (element.text.size() > 256) throw std::invalid_argument("UI text exceeds 256 bytes");
    std::vector<char> source(element.text.begin(), element.text.end());
    source.push_back('\0');
    // stb_easy_font emits at most about 270 bytes per character.
    std::vector<char> buffer(source.size() * 512);
    const int count = stb_easy_font_print(0, 0, source.data(), nullptr,
                                          buffer.data(), int(buffer.size()));
    float origin_x = element.x;
    float origin_y = element.y;
    if (element.type == UiElement::Type::Button) {
        origin_x += (element.width - float(stb_easy_font_width(source.data())) *
                    element.scale) * 0.5f;
        origin_y += (element.height - 12.0f * element.scale) * 0.5f;
    }
    for (int quad = 0; quad < count; ++quad) {
        Vertex first, third;
        std::memcpy(&first, buffer.data() + size_t(quad) * 4 * sizeof(Vertex), sizeof(Vertex));
        std::memcpy(&third, buffer.data() + (size_t(quad) * 4 + 2) * sizeof(Vertex), sizeof(Vertex));
        fill(pixels, width, height, origin_x + first.x * element.scale,
             origin_y + first.y * element.scale, (third.x - first.x) * element.scale,
             (third.y - first.y) * element.scale, element);
    }
}

}  // namespace

std::vector<uint8_t> rasterize_ui(uint32_t width, uint32_t height,
                                  const std::vector<UiElement>& elements) {
    if (width == 0 || height == 0 || width > 8192 || height > 8192 ||
        elements.size() > 256)
        throw std::invalid_argument("invalid UI canvas dimensions or element count");
    std::vector<uint8_t> pixels(size_t(width) * height * 4, 0);
    for (const auto& element : elements) {
        if (!std::isfinite(element.x) || !std::isfinite(element.y) ||
            !std::isfinite(element.width) || !std::isfinite(element.height) ||
            !std::isfinite(element.scale) || element.scale <= 0)
            throw std::invalid_argument("invalid UI element geometry");
        if (element.type == UiElement::Type::Rectangle) {
            fill(pixels, width, height, element.x, element.y,
                 element.width, element.height, element);
        } else if (element.type == UiElement::Type::Text) {
            text(pixels, width, height, element);
        } else {
            UiElement background = element;
            background.red = element.focused ? 58 : 33;
            background.green = element.focused ? 112 : 55;
            background.blue = element.focused ? 173 : 84;
            background.alpha = 235;
            fill(pixels, width, height, element.x, element.y,
                 element.width, element.height, background);
            text(pixels, width, height, element);
        }
    }
    return pixels;
}

}  // namespace acuppa
