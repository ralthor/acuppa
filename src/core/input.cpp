#include "threepoint/input.hpp"

#include <glm/geometric.hpp>
#include <glm/matrix.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace threepoint {
namespace {

bool contains(std::string_view list, std::string_view control) {
    size_t begin = 0;
    while (begin < list.size()) {
        const size_t end = list.find(' ', begin);
        const size_t length = (end == std::string_view::npos ? list.size() : end) - begin;
        if (list.substr(begin, length) == control) return true;
        if (end == std::string_view::npos) break;
        begin = end + 1;
    }
    return false;
}

bool valid_action(std::string_view action) {
    if (action.empty() || action.size() > 64) return false;
    return std::all_of(action.begin(), action.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '_';
    });
}

}  // namespace

bool InputState::valid_control(InputDevice device, std::string_view control) {
    switch (device) {
    case InputDevice::Keyboard:
        if (control.size() == 1 && ((control[0] >= 'a' && control[0] <= 'z') ||
                                    (control[0] >= '0' && control[0] <= '9'))) return true;
        return contains("left right up down space enter escape tab backspace "
                        "left_shift right_shift left_control right_control", control);
    case InputDevice::MouseButton:
        return contains("left right middle", control);
    case InputDevice::GamepadButton:
        return contains("a b x y left_bumper right_bumper back start guide left_thumb "
                        "right_thumb dpad_up dpad_right dpad_down dpad_left", control);
    case InputDevice::GamepadAxis:
        return contains("left_x left_y right_x right_y left_trigger right_trigger", control);
    }
    return false;
}

std::string InputState::key(InputDevice device, std::string_view control) {
    return std::to_string(int(device)) + ":" + std::string(control);
}

void InputState::bind(std::string action, InputDevice device, std::string control) {
    if (!valid_action(action) || !valid_control(device, control)) {
        throw std::invalid_argument("invalid input action or control: " + action + "/" + control);
    }
    auto& list = bindings_[std::move(action)];
    const auto same = [&](const Binding& binding) {
        return binding.device == device && binding.control == control;
    };
    if (std::any_of(list.begin(), list.end(), same)) {
        throw std::invalid_argument("duplicate input binding");
    }
    list.push_back({device, std::move(control)});
}

void InputState::inject(const InputEvent& event) {
    if (!valid_control(event.device, event.control) || !std::isfinite(event.value) ||
        event.value < (event.device == InputDevice::GamepadAxis ? -1.0f : 0.0f) ||
        event.value > 1.0f ||
        (event.device != InputDevice::GamepadAxis && event.value != 0.0f &&
         event.value != 1.0f)) {
        throw std::invalid_argument("invalid input event");
    }
    values_[key(event.device, event.control)] = event.value;
}

float InputState::action(std::string_view name) const {
    const auto binding = bindings_.find(std::string(name));
    if (binding == bindings_.end()) {
        throw std::invalid_argument("unknown input action: " + std::string(name));
    }
    float result = 0.0f;
    for (const Binding& control : binding->second) {
        const auto value = values_.find(key(control.device, control.control));
        if (value != values_.end() && std::abs(value->second) > std::abs(result)) {
            result = value->second;
        }
    }
    return result;
}

void InputState::set_pointer(double x, double y) {
    if (!std::isfinite(x) || !std::isfinite(y)) {
        throw std::invalid_argument("pointer coordinates must be finite");
    }
    pointer_x_ = x;
    pointer_y_ = y;
}

WorldRay screen_ray(const PerspectiveCamera& camera, uint32_t width, uint32_t height,
                    double pixel_x, double pixel_y) {
    if (width == 0 || height == 0 || !std::isfinite(pixel_x) || !std::isfinite(pixel_y) ||
        std::abs(pixel_x) > 1e9 || std::abs(pixel_y) > 1e9) {
        throw std::invalid_argument("screen ray needs a valid viewport and finite coordinates");
    }
    const glm::mat4 inverse = glm::inverse(camera.view_projection(float(width) / float(height)));
    const float x = float(2.0 * pixel_x / width - 1.0);
    const float y = float(1.0 - 2.0 * pixel_y / height);
    const glm::vec4 near = inverse * glm::vec4(x, y, 0.0f, 1.0f);
    const glm::vec4 far = inverse * glm::vec4(x, y, 1.0f, 1.0f);
    const glm::vec3 near_world = glm::vec3(near) / near.w;
    const glm::vec3 far_world = glm::vec3(far) / far.w;
    return {camera.eye, glm::normalize(far_world - near_world)};
}

}  // namespace threepoint
