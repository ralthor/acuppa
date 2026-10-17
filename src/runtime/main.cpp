#include "threepoint/project.hpp"
#include "threepoint/preview.hpp"

#include <exception>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << "ThreePoint runtime " << THREEPOINT_VERSION << " (3D cube preview)\n";
        return 0;
    }
    const std::string_view command = argc > 1 ? argv[1] : "";
    if ((argc == 2 && (command == "--preview" || command == "--smoke-vulkan" ||
                       command == "--smoke-cube")) ||
        (argc == 3 && (command == "--capture-clear" || command == "--capture-cube")) ||
        (argc == 4 && command == "--capture-cube")) {
        const bool smoke = command == "--smoke-vulkan" || command == "--smoke-cube";
        const bool cube = command == "--preview" || command == "--smoke-cube" ||
                          command == "--capture-cube";
        try {
            threepoint::PreviewOptions options;
            if (cube) options.scene = threepoint::PreviewOptions::Scene::Cube;
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
            const auto result = threepoint::run_preview(options);
            std::cout << "Rendered " << result.frames << (cube ? " cube" : " clear")
                      << " frames; swapchain rebuilds: "
                      << result.swapchain_rebuilds << '\n';
            if (smoke && (result.frames != options.frame_limit || result.swapchain_rebuilds == 0)) {
                std::cerr << "Vulkan smoke check did not complete its frame and resize checks\n";
                return 1;
            }
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "ThreePoint: " << error.what() << '\n';
            return 1;
        }
    }
    if (argc == 3 && std::string_view(argv[1]) == "--check-project") {
        try {
            const auto project = threepoint::require_project(argv[2]);
            std::cout << "Project files found: " << project.root.string() << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "ThreePoint: " << error.what() << '\n';
            return 1;
        }
    }
    std::cerr << "Usage: threepoint --version | --check-project <directory> | --preview | --smoke-vulkan | --smoke-cube | --capture-clear <file.ppm> | --capture-cube <file.ppm> [angle-radians]\n";
    std::cerr << "3D game execution is planned for Phase 4.\n";
    return 2;
}
