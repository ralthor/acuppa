#include "acuppa/lua_project.hpp"

#include <cmath>
#include <stdexcept>

namespace {
void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int count_blocks(const acuppa::SceneFrame& frame, float z) {
    int count = 0;
    for (const auto& cube : frame.cubes)
        if (std::abs(cube.position.z - z) < 0.01f) ++count;
    return count;
}

float active_x(const acuppa::SceneFrame& frame) {
    float sum = 0.0f;
    for (const auto& cube : frame.cubes)
        if (std::abs(cube.position.z - 0.25f) < 0.01f) sum += cube.position.x;
    return sum / 4.0f;
}

float active_y(const acuppa::SceneFrame& frame) {
    float sum = 0.0f;
    for (const auto& cube : frame.cubes)
        if (std::abs(cube.position.z - 0.25f) < 0.01f) sum += cube.position.y;
    return sum / 4.0f;
}

int score_pixels(const acuppa::SceneFrame& frame) {
    int count = 0;
    for (const auto& cube : frame.cubes)
        if (std::abs(cube.position.z - 0.8f) < 0.01f &&
            cube.position.x >= 7.0f && cube.position.y > 0.9f &&
            cube.position.y < 2.7f) ++count;
    return count;
}
}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) throw std::runtime_error("falling-block game path required");
    acuppa::LuaProject game(argv[1]);
    auto frame = game.step(0.0);
    expect(frame.cubes.size() > 70 && count_blocks(frame, 0.25f) == 4 &&
           count_blocks(frame, -0.1f) == 16,
           "the source-only scene did not create its board, piece, ghost, and display");
    const float initial_x = active_x(frame);
    const float initial_y = active_y(frame);
    const int initial_score_pixels = score_pixels(frame);

    game.inject({acuppa::InputDevice::Keyboard, "left", 1.0f});
    frame = game.step(0.0);
    expect(std::abs(active_x(frame) - initial_x + 1.0f) < 0.01f,
           "left action did not move the active piece one cell");
    game.inject({acuppa::InputDevice::Keyboard, "left", 0.0f});
    game.step(0.0);
    game.inject({acuppa::InputDevice::Keyboard, "up", 1.0f});
    frame = game.step(0.0);
    expect(count_blocks(frame, 0.25f) == 4 &&
           std::abs(active_y(frame) - initial_y) > 0.1f,
           "rotate action did not change the active piece orientation");
    game.inject({acuppa::InputDevice::Keyboard, "up", 0.0f});
    game.step(0.0);
    game.inject({acuppa::InputDevice::Keyboard, "space", 1.0f});
    frame = game.step(0.0);
    expect(count_blocks(frame, 0.0f) == 4 && count_blocks(frame, 0.25f) == 4,
           "hard drop did not lock the piece and spawn the next one");
    expect(score_pixels(frame) != initial_score_pixels,
           "hard-drop points did not update the visible score");
    game.inject({acuppa::InputDevice::Keyboard, "space", 0.0f});
    game.step(0.0);
    game.inject({acuppa::InputDevice::Keyboard, "r", 1.0f});
    frame = game.step(0.0);
    expect(count_blocks(frame, 0.0f) == 0 && count_blocks(frame, 0.25f) == 4 &&
           score_pixels(frame) == initial_score_pixels,
           "restart did not clear the board");
    game.inject({acuppa::InputDevice::Keyboard, "r", 0.0f});
    for (int i = 0; i < 43; ++i) frame = game.step(1.0 / 60.0);
    expect(std::abs(active_y(frame) - initial_y + 1.0f) < 0.01f,
           "fixed-step gravity did not lower the active piece");

    acuppa::LuaProject complete(argv[1]);
    frame = complete.step(0.0);
    expect(count_blocks(frame, 0.2f) == 4,
           "the next-piece preview did not render");
    complete.inject({acuppa::InputDevice::Keyboard, "c", 1.0f});
    frame = complete.step(0.0);
    expect(count_blocks(frame, 0.2f) == 8 && count_blocks(frame, 0.25f) == 4,
           "hold did not swap the piece and render both previews");
    complete.inject({acuppa::InputDevice::Keyboard, "c", 0.0f});
    complete.step(0.0);

    const int display_before_pause = count_blocks(frame, 0.8f);
    const float paused_y = active_y(frame);
    complete.inject({acuppa::InputDevice::Keyboard, "p", 1.0f});
    frame = complete.step(0.0);
    expect(count_blocks(frame, 0.8f) > display_before_pause,
           "pause state was not visible in the 3D display");
    complete.inject({acuppa::InputDevice::Keyboard, "p", 0.0f});
    for (int i = 0; i < 84; ++i) frame = complete.step(1.0 / 120.0);
    expect(std::abs(active_y(frame) - paused_y) < 0.01f,
           "paused game continued to advance under variable frame timing");
    complete.inject({acuppa::InputDevice::Keyboard, "p", 1.0f});
    frame = complete.step(0.0);
    expect(count_blocks(frame, 0.8f) == display_before_pause,
           "pause message remained after resume");
    complete.inject({acuppa::InputDevice::Keyboard, "p", 0.0f});
    complete.step(0.0);

    bool topped_out = false;
    for (int i = 0; i < 100; ++i) {
        complete.inject({acuppa::InputDevice::Keyboard, "space", 1.0f});
        frame = complete.step(0.0);
        complete.inject({acuppa::InputDevice::Keyboard, "space", 0.0f});
        complete.step(0.0);
        if (count_blocks(frame, 0.25f) == 0) { topped_out = true; break; }
    }
    expect(topped_out && count_blocks(frame, 0.8f) > display_before_pause,
           "a full match did not reach visible game over");
    complete.inject({acuppa::InputDevice::Keyboard, "r", 1.0f});
    frame = complete.step(0.0);
    expect(count_blocks(frame, 0.0f) == 0 && count_blocks(frame, 0.25f) == 4,
           "game over could not restart a clean match");

    acuppa::LuaProject sixty(argv[1]), one_twenty(argv[1]);
    auto sixty_frame = sixty.step(0.0), one_twenty_frame = one_twenty.step(0.0);
    for (int i = 0; i < 42; ++i) sixty_frame = sixty.step(1.0 / 60.0);
    for (int i = 0; i < 84; ++i) one_twenty_frame = one_twenty.step(1.0 / 120.0);
    expect(std::abs(active_y(sixty_frame) - active_y(one_twenty_frame)) < 0.01f,
           "gravity changed with render frame timing");

    acuppa::LuaProject controller(argv[1]);
    const float controller_x = active_x(controller.step(0.0));
    controller.inject({acuppa::InputDevice::GamepadButton, "dpad_left", 1.0f});
    expect(std::abs(active_x(controller.step(0.0)) - controller_x + 1.0f) < 0.01f,
           "controller action did not move the game piece");
}
