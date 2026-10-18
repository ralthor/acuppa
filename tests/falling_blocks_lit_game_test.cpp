#include "acuppa/lua_project.hpp"

#include <cmath>
#include <set>
#include <stdexcept>
#include <string>

namespace {
void expect(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

int count_at(const acuppa::SceneFrame& frame, float z) {
    int count = 0;
    for (const auto& mesh : frame.imported)
        if (std::abs(mesh.transform[3].z - z) < 0.01f) ++count;
    return count;
}

float active_x(const acuppa::SceneFrame& frame) {
    float total = 0;
    for (const auto& mesh : frame.imported)
        if (std::abs(mesh.transform[3].z - 0.25f) < 0.01f)
            total += mesh.transform[3].x;
    return total / 4;
}
}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) throw std::runtime_error("lit game path required");
    acuppa::LuaProject game(argv[1]);
    auto frame = game.step(0);
    expect(frame.environment && frame.lights.size() == 1 && frame.lights[0].shadows &&
           std::abs(frame.lights[0].shadow_range - 11.0f) < 0.001f &&
           std::abs(frame.exposure - 0.85f) < 0.001f &&
           frame.ssao_quality == 3 && std::abs(frame.ssao_radius - 0.65f) < 0.001f &&
           std::abs(frame.ssao_strength - 0.25f) < 0.001f,
           "copied game did not configure the HDRI, exposure, shadows, and SSAO");
    for (const auto& cube : frame.cubes)
        expect(!cube.casts_shadows, "HUD, border, or ghost cube casts a board shadow");
    for (const auto& mesh : frame.imported)
        if (mesh.source != "assets/panel.gltf")
            expect(mesh.casts_shadows, "colored tile stopped casting shadows");
    acuppa::LuaProject unoccluded(argv[1], "ssao_off");
    expect(unoccluded.step(0).ssao_quality == 0,
           "game's SSAO comparison scene did not disable the effect");
    expect(count_at(frame, -0.45f) == 1 && count_at(frame, 0.25f) == 4 &&
           count_at(frame, 0.2f) == 4 && frame.cubes.size() > 70,
           "panel, colored pieces, or source-authored display did not appear");

    const float start_x = active_x(frame);
    game.inject({acuppa::InputDevice::Keyboard, "left", 1});
    frame = game.step(0);
    expect(std::abs(active_x(frame) - start_x + 1) < 0.01f,
           "left input did not move the lit piece");
    game.inject({acuppa::InputDevice::Keyboard, "left", 0});
    game.step(0);

    std::set<std::string> piece_assets;
    for (int i = 0; i < 7; ++i) {
        for (const auto& mesh : frame.imported)
            if (std::abs(mesh.transform[3].z - 0.25f) < 0.01f)
                piece_assets.insert(mesh.source);
        if (i == 6) break;
        game.inject({acuppa::InputDevice::Keyboard, "space", 1});
        frame = game.step(0);
        game.inject({acuppa::InputDevice::Keyboard, "space", 0});
        frame = game.step(0);
    }
    expect(piece_assets.size() == 7, "the full seven-piece bag did not load distinct PBR assets");
    expect(count_at(frame, 0) >= 4, "hard drop did not lock colored pieces");

    game.inject({acuppa::InputDevice::Keyboard, "r", 1});
    frame = game.step(0);
    expect(count_at(frame, 0) == 0 && count_at(frame, 0.25f) == 4,
           "restart did not reset the copied game");
}
