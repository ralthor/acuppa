#pragma once

#include "threepoint/camera.hpp"

#include <glm/vec3.hpp>

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace threepoint {

enum class InputDevice { Keyboard, MouseButton, GamepadButton, GamepadAxis };

struct InputEvent {
    InputDevice device;
    std::string control;
    float value;
};

struct WorldRay {
    glm::vec3 origin;
    glm::vec3 direction;
};

class InputState {
public:
    void bind(std::string action, InputDevice device, std::string control);
    void inject(const InputEvent& event);
    float action(std::string_view name) const;
    float value(InputDevice device, std::string_view control) const;
    void set_pointer(double x, double y);
    std::pair<double, double> pointer() const { return {pointer_x_, pointer_y_}; }

    static bool valid_control(InputDevice device, std::string_view control);

private:
    struct Binding {
        InputDevice device;
        std::string control;
    };
    static std::string key(InputDevice device, std::string_view control);
    std::map<std::string, std::vector<Binding>> bindings_;
    std::map<std::string, float> values_;
    double pointer_x_ = 0.0;
    double pointer_y_ = 0.0;
};

WorldRay screen_ray(const PerspectiveCamera& camera, uint32_t width, uint32_t height,
                    double pixel_x, double pixel_y);

}  // namespace threepoint
