#include "acuppa/lua_project.hpp"
#include "acuppa/ui_canvas.hpp"

#include <filesystem>
#include <stdexcept>

namespace fs = std::filesystem;

void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main(int argc, char* argv[]) {
    if (argc != 3) throw std::runtime_error("source and temporary project paths required");
    acuppa::UiElement colored;
    colored.width = colored.height = 2;
    colored.red = 255;
    colored.green = colored.blue = 0;
    colored.alpha = 128;
    auto raster = acuppa::rasterize_ui(2, 2, {colored});
    expect(raster[0] == 255 && raster[1] == 0 && raster[3] == 128,
           "UI alpha rasterization failed");
    bool rejected = false;
    try { acuppa::rasterize_ui(0, 2, {colored}); }
    catch (const std::invalid_argument&) { rejected = true; }
    expect(rejected, "zero-size UI canvas accepted");
    const fs::path work = argv[2];
    fs::remove_all(work);
    fs::copy(argv[1], work, fs::copy_options::recursive);
    fs::remove_all(work / "saves"); // local play must not leak into the fixture
    {
        acuppa::LuaProject project(work, "menu");
        auto frame = project.step(0);
        expect(frame.ui.size() >= 4 && frame.cubes.empty(), "menu UI did not draw");
        auto pixels = acuppa::rasterize_ui(1280, 720, frame.ui);
        expect(pixels[(300 * 1280 + 500) * 4 + 3] != 0,
               "button was absent from the rasterized UI");
        expect(frame.ui[2].focused, "first button did not receive focus");
        project.inject({acuppa::InputDevice::Keyboard, "tab", 1});
        frame = project.step(0);
        expect(frame.ui[3].focused, "Tab did not advance focus");
        project.inject({acuppa::InputDevice::Keyboard, "enter", 1});
        frame = project.step(0);
        expect(frame.ui.size() >= 5, "focused Help button did not activate");
        project.inject({acuppa::InputDevice::Keyboard, "tab", 0});
        project.inject({acuppa::InputDevice::Keyboard, "enter", 0});
        project.step(0);
        project.set_pointer(550, 290);
        project.inject({acuppa::InputDevice::MouseButton, "left", 1});
        frame = project.step(0);
        expect(frame.cubes.size() == 1 && frame.cubes[0].position.x == 1,
               "mouse click did not start the source-only game");
    }
    {
        acuppa::LuaProject project(work, "menu");
        project.inject({acuppa::InputDevice::Keyboard, "enter", 1});
        auto frame = project.step(0);
        expect(frame.cubes.size() == 1 && frame.cubes[0].position.x == 2,
               "keyboard start did not restore and update the saved game state");
    }
    {
        acuppa::LuaProject project(work, "menu");
        project.set_viewport(640, 480);
        const auto frame = project.step(0);
        expect(frame.ui.size() >= 4 && frame.ui[0].width == 640 &&
               frame.ui[0].height == 480 && frame.ui[2].x == 170,
               "menu layout did not respond to a viewport resize");
    }
}
