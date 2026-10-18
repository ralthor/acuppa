#include "acuppa/lua_project.hpp"
#include "acuppa/project.hpp"
#include "acuppa/preview.hpp"
#include "export_game.hpp"

#include <charconv>
#include <exception>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << "ACuppa Engine " << ACUPPA_VERSION << " — Brew games, one idea at a time.\n";
        return 0;
    }
    const std::string_view command = argc > 1 ? argv[1] : "";
    if (command == "build" && argc == 4) {
        try {
            const std::filesystem::path root = argv[2];
            const acuppa::LuaProject manifest(root, {},
                acuppa::LuaProject::Mode::EditorPreview, false);
            for (const auto& name : manifest.scene_names()) {
                acuppa::LuaProject project(root, name,
                    acuppa::LuaProject::Mode::Game, false);
                project.step(0);
                project.step(1.0 / 60.0);
            }
            const auto self = std::filesystem::read_symlink("/proc/self/exe");
            export_game(root, argv[3], self.parent_path() / "acuppa-player");
            std::cout << "Exported standalone game to " << argv[3] << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "ACuppa build: " << error.what() << '\n';
            return 1;
        }
    }
    if (command == "test" && argc == 3) {
        try {
            const std::filesystem::path root = argv[2];
            const acuppa::LuaProject manifest(root, {},
                acuppa::LuaProject::Mode::EditorPreview, false);
            const auto scenes = manifest.scene_names();
            for (const auto& name : scenes) {
                try {
                    acuppa::LuaProject project(root, name,
                        acuppa::LuaProject::Mode::Game, false);
                    auto frame = project.step(0);
                    for (int tick = 0; tick < 1 && !frame.quit; ++tick)
                        frame = project.step(1.0 / 60.0);
                    std::cout << "PASS " << name << '\n';
                } catch (const std::exception& error) {
                    throw std::runtime_error("scene '" + name + "': " + error.what());
                }
            }
            std::cout << "Tested " << scenes.size() << " source scenes headlessly\n";
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "ACuppa test: " << error.what() << '\n';
            return 1;
        }
    }
    if (command == "run" && argc >= 3) {
        try {
            uint32_t frames = 0;
            std::filesystem::path capture;
            std::string held_key;
            std::string initial_scene;
            for (int i = 3; i < argc; ++i) {
                const std::string_view option = argv[i];
                if (option == "--frames" && i + 1 < argc && frames == 0) {
                    const std::string number = argv[++i];
                    unsigned parsed = 0;
                    const auto result = std::from_chars(number.data(),
                        number.data() + number.size(), parsed);
                    if (result.ec != std::errc{} ||
                        result.ptr != number.data() + number.size() ||
                        parsed == 0 || parsed > 100000) {
                        throw std::invalid_argument("--frames must be an integer from 1 to 100000");
                    }
                    frames = uint32_t(parsed);
                } else if (option == "--capture" && i + 1 < argc && capture.empty()) {
                    capture = argv[++i];
                    if (capture.empty()) throw std::invalid_argument("--capture needs a path");
                } else if (option == "--hold-key" && i + 1 < argc && held_key.empty()) {
                    held_key = argv[++i];
                    if (held_key.empty()) throw std::invalid_argument("--hold-key needs a key name");
                } else if (option == "--scene" && i + 1 < argc && initial_scene.empty()) {
                    initial_scene = argv[++i];
                    if (initial_scene.empty()) throw std::invalid_argument("--scene needs a name");
                } else {
                    throw std::invalid_argument("unknown or repeated run option: " +
                                                std::string(option));
                }
            }
            if (!capture.empty() && frames == 0) {
                throw std::invalid_argument("--capture requires --frames");
            }
            acuppa::LuaProject project(argv[2], initial_scene);
            if (!held_key.empty()) {
                project.inject({acuppa::InputDevice::Keyboard, held_key, 1.0f});
            }
            acuppa::PreviewOptions options;
            options.scene = acuppa::PreviewOptions::Scene::Project;
            options.width = project.width();
            options.height = project.height();
            options.frame_limit = frames;
            options.capture_path = capture;
            options.capture_frame = frames == 0 ? 1 : frames;
            options.fixed_delta = frames == 0 ? 0.0 : 1.0 / 60.0;
            options.on_frame = [&project](double dt) { return project.step(dt); };
            options.on_input = [&project](const acuppa::InputEvent& event) {
                project.inject(event);
            };
            options.on_pointer = [&project](double x, double y) {
                project.set_pointer(x, y);
            };
            options.on_resize = [&project](uint32_t width, uint32_t height) {
                project.set_viewport(width, height);
            };
            const auto result = acuppa::run_preview(options);
            std::cout << "Rendered " << result.frames << " project frames\n";
            if (!capture.empty() && result.frames != frames) {
                std::cerr << "Game stopped before the requested capture frame\n";
                return 1;
            }
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "ACuppa: " << error.what() << '\n';
            return 1;
        }
    }
    if ((argc == 2 && (command == "--preview" || command == "--smoke-vulkan" ||
                       command == "--smoke-cube")) ||
        (argc == 3 && (command == "--capture-clear" || command == "--capture-cube")) ||
        (argc == 4 && command == "--capture-cube")) {
        const bool smoke = command == "--smoke-vulkan" || command == "--smoke-cube";
        const bool cube = command == "--preview" || command == "--smoke-cube" ||
                          command == "--capture-cube";
        try {
            acuppa::PreviewOptions options;
            if (cube) options.scene = acuppa::PreviewOptions::Scene::Cube;
            if (smoke) {
                options.frame_limit = 6;
                options.resize_after = 2;
            } else if (argc >= 3) {
                options.frame_limit = 1;
                options.capture_path = argv[2];
                options.cube_angle = 0.35f;
                if (argc == 4) {
                    size_t end = 0;
                    options.cube_angle = std::stof(argv[3], &end);
                    if (end != std::string(argv[3]).size() ||
                        !std::isfinite(options.cube_angle)) {
                        throw std::invalid_argument("cube angle must be a finite number of radians");
                    }
                }
            }
            const auto result = acuppa::run_preview(options);
            std::cout << "Rendered " << result.frames << (cube ? " cube" : " clear")
                      << " frames; swapchain rebuilds: "
                      << result.swapchain_rebuilds << '\n';
            if (smoke && (result.frames != options.frame_limit || result.swapchain_rebuilds == 0)) {
                std::cerr << "Vulkan smoke check did not complete its frame and resize checks\n";
                return 1;
            }
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "ACuppa: " << error.what() << '\n';
            return 1;
        }
    }
    if (argc == 3 && std::string_view(argv[1]) == "--check-project") {
        try {
            const auto project = acuppa::require_project(argv[2]);
            std::cout << "Project files found: " << project.root.string() << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "ACuppa: " << error.what() << '\n';
            return 1;
        }
    }
    std::cerr << "Usage: acuppa build <project> <output-directory> | acuppa test <project> | acuppa run <project> [--scene name] [--frames N] [--capture file.ppm] [--hold-key name] | --version | --check-project <directory> | --preview | --smoke-vulkan | --smoke-cube | --capture-clear <file.ppm> | --capture-cube <file.ppm> [angle-radians]\n";
    return 2;
}
