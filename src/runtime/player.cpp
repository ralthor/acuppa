#include "threepoint/lua_project.hpp"
#include "threepoint/preview.hpp"

#include <charconv>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

int main(int argc, char* argv[]) {
    try {
        const auto self = std::filesystem::read_symlink("/proc/self/exe");
        const auto root = self.parent_path() / "game";
        if (argc == 2 && std::string_view(argv[1]) == "--check") {
            const threepoint::LuaProject manifest(root, {},
                threepoint::LuaProject::Mode::EditorPreview, false);
            for (const auto& name : manifest.scene_names()) {
                threepoint::LuaProject scene(root, name,
                    threepoint::LuaProject::Mode::Game, false);
                scene.step(0);
                scene.step(1.0 / 60.0);
            }
            std::cout << "Bundled game scenes passed\n";
            return 0;
        }
        uint32_t frames = 0;
        std::filesystem::path capture;
        std::string held_key;
        for (int i = 1; i < argc; ++i) {
            const std::string_view option = argv[i];
            if (option == "--frames" && i + 1 < argc && frames == 0) {
                const std::string number = argv[++i];
                unsigned parsed = 0;
                const auto result = std::from_chars(number.data(),
                    number.data() + number.size(), parsed);
                if (result.ec != std::errc{} ||
                    result.ptr != number.data() + number.size() || parsed == 0 || parsed > 100000)
                    throw std::invalid_argument("--frames must be from 1 to 100000");
                frames = parsed;
            } else if (option == "--capture" && i + 1 < argc && capture.empty()) {
                capture = argv[++i];
                if (capture.empty()) throw std::invalid_argument("--capture needs a path");
            } else if (option == "--hold-key" && i + 1 < argc && held_key.empty()) {
                held_key = argv[++i];
                if (held_key.empty()) throw std::invalid_argument("--hold-key needs a name");
            } else throw std::invalid_argument("unknown or repeated player option: " + std::string(option));
        }
        if (!capture.empty() && frames == 0)
            throw std::invalid_argument("--capture requires --frames");
        threepoint::LuaProject project(root);
        if (!held_key.empty())
            project.inject({threepoint::InputDevice::Keyboard, held_key, 1.0f});
        threepoint::PreviewOptions options;
        options.scene = threepoint::PreviewOptions::Scene::Project;
        options.width = project.width();
        options.height = project.height();
        options.frame_limit = frames;
        options.capture_path = capture;
        options.capture_frame = frames == 0 ? 1 : frames;
        options.fixed_delta = frames == 0 ? 0.0 : 1.0 / 60.0;
        options.on_frame = [&project](double dt) { return project.step(dt); };
        options.on_input = [&project](const threepoint::InputEvent& event) {
            project.inject(event);
        };
        options.on_pointer = [&project](double x, double y) { project.set_pointer(x, y); };
        options.on_resize = [&project](uint32_t width, uint32_t height) {
            project.set_viewport(width, height);
        };
        const auto result = threepoint::run_preview(options);
        if (!capture.empty() && result.frames != frames)
            throw std::runtime_error("game stopped before the requested capture frame");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ThreePoint game: " << error.what() << '\n';
        return 1;
    }
}
