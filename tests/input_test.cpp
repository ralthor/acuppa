#include "acuppa/input.hpp"

#include <cmath>
#include <stdexcept>

namespace {
void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    acuppa::InputState input;
    input.bind("turn", acuppa::InputDevice::Keyboard, "space");
    input.bind("turn", acuppa::InputDevice::GamepadButton, "a");
    input.bind("move", acuppa::InputDevice::GamepadAxis, "left_x");
    input.bind("select", acuppa::InputDevice::MouseButton, "left");
    expect(input.action("turn") == 0.0f, "new action should be released");
    input.inject({acuppa::InputDevice::Keyboard, "space", 1.0f});
    expect(input.action("turn") == 1.0f, "key press did not activate action");
    input.inject({acuppa::InputDevice::GamepadButton, "a", 1.0f});
    input.inject({acuppa::InputDevice::Keyboard, "space", 0.0f});
    expect(input.action("turn") == 1.0f, "second binding did not keep action active");
    input.inject({acuppa::InputDevice::GamepadButton, "a", 0.0f});
    expect(input.action("turn") == 0.0f, "release did not reset action");
    input.inject({acuppa::InputDevice::GamepadAxis, "left_x", -0.5f});
    expect(input.action("move") == -0.5f, "analog axis value was lost");
    input.inject({acuppa::InputDevice::MouseButton, "left", 1.0f});
    expect(input.action("select") == 1.0f, "mouse button was not read");
    input.set_pointer(320.0, 240.0);
    expect(input.pointer().first == 320.0 && input.pointer().second == 240.0,
           "pointer position was not retained");

    bool rejected = false;
    try { input.bind("bad", acuppa::InputDevice::Keyboard, "not_a_key"); }
    catch (const std::invalid_argument&) { rejected = true; }
    expect(rejected, "unsupported key was accepted");
    rejected = false;
    try { input.inject({acuppa::InputDevice::GamepadAxis, "left_x", 2.0f}); }
    catch (const std::invalid_argument&) { rejected = true; }
    expect(rejected, "out-of-range axis was accepted");
    rejected = false;
    try { input.action("not_declared"); }
    catch (const std::invalid_argument&) { rejected = true; }
    expect(rejected, "unknown action was accepted");

    const acuppa::PerspectiveCamera camera;
    const auto center = acuppa::screen_ray(camera, 1280, 720, 640, 360);
    expect(std::abs(center.direction.x) < 0.0001f &&
           std::abs(center.direction.y) < 0.0001f && center.direction.z < -0.99f,
           "center screen ray does not point along camera forward");
    const auto right = acuppa::screen_ray(camera, 1280, 720, 1000, 360);
    expect(right.direction.x > 0.2f && right.direction.z < 0.0f,
           "right screen ray points the wrong way");
    rejected = false;
    try { acuppa::screen_ray(camera, 0, 720, 0, 0); }
    catch (const std::invalid_argument&) { rejected = true; }
    expect(rejected, "zero-size ray viewport was accepted");
    rejected = false;
    try { acuppa::screen_ray(camera, 1280, 720, 1e300, 0); }
    catch (const std::invalid_argument&) { rejected = true; }
    expect(rejected, "unrepresentable pointer coordinate was accepted");
}
