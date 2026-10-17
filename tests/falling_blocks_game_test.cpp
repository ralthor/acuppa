#include "threepoint/lua_project.hpp"

#include <cmath>
#include <stdexcept>

namespace {
void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int count_blocks(const threepoint::SceneFrame& frame, float z) {
    int count = 0;
    for (const auto& cube : frame.cubes)
        if (std::abs(cube.position.z - z) < 0.01f) ++count;
    return count;
}

float active_x(const threepoint::SceneFrame& frame) {
    float sum = 0.0f;
    for (const auto& cube : frame.cubes)
        if (std::abs(cube.position.z - 0.25f) < 0.01f) sum += cube.position.x;
    return sum / 4.0f;
}

float active_y(const threepoint::SceneFrame& frame) {
    float sum = 0.0f;
    for (const auto& cube : frame.cubes)
        if (std::abs(cube.position.z - 0.25f) < 0.01f) sum += cube.position.y;
    return sum / 4.0f;
}
}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) throw std::runtime_error("falling-block game path required");
    threepoint::LuaProject game(argv[1]);
    auto frame = game.step(0.0);
    expect(frame.cubes.size() == 54 && count_blocks(frame, 0.25f) == 4,
           "the source-only scene did not create its board and active piece");
    const float initial_x = active_x(frame);
    const float initial_y = active_y(frame);

    game.inject({threepoint::InputDevice::Keyboard, "left", 1.0f});
    frame = game.step(0.0);
    expect(std::abs(active_x(frame) - initial_x + 1.0f) < 0.01f,
           "left action did not move the active piece one cell");
    game.inject({threepoint::InputDevice::Keyboard, "left", 0.0f});
    game.step(0.0);
    game.inject({threepoint::InputDevice::Keyboard, "up", 1.0f});
    frame = game.step(0.0);
    expect(count_blocks(frame, 0.25f) == 4 &&
           std::abs(active_y(frame) - initial_y) > 0.1f,
           "rotate action did not change the active piece orientation");
    game.inject({threepoint::InputDevice::Keyboard, "up", 0.0f});
    game.step(0.0);
    game.inject({threepoint::InputDevice::Keyboard, "space", 1.0f});
    frame = game.step(0.0);
    expect(count_blocks(frame, 0.0f) == 4 && count_blocks(frame, 0.25f) == 4,
           "hard drop did not lock the piece and spawn the next one");
    game.inject({threepoint::InputDevice::Keyboard, "space", 0.0f});
    game.step(0.0);
    game.inject({threepoint::InputDevice::Keyboard, "r", 1.0f});
    frame = game.step(0.0);
    expect(count_blocks(frame, 0.0f) == 0 && count_blocks(frame, 0.25f) == 4,
           "restart did not clear the board");
    game.inject({threepoint::InputDevice::Keyboard, "r", 0.0f});
    for (int i = 0; i < 43; ++i) frame = game.step(1.0 / 60.0);
    expect(std::abs(active_y(frame) - initial_y + 1.0f) < 0.01f,
           "fixed-step gravity did not lower the active piece");
}
