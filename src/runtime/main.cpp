#include "threepoint/project.hpp"
#include "threepoint/preview.hpp"

#include <exception>
#include <iostream>
#include <string_view>

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << "ThreePoint runtime " << THREEPOINT_VERSION << " (Vulkan preview)\n";
        return 0;
    }
    if ((argc == 2 && (std::string_view(argv[1]) == "--preview" ||
                       std::string_view(argv[1]) == "--smoke-vulkan")) ||
        (argc == 3 && std::string_view(argv[1]) == "--capture-clear")) {
        const bool smoke = std::string_view(argv[1]) == "--smoke-vulkan";
        try {
            threepoint::PreviewOptions options;
            if (smoke) {
                options.frame_limit = 6;
                options.resize_after = 2;
            } else if (argc == 3) {
                options.frame_limit = 1;
                options.capture_path = argv[2];
            }
            const auto result = threepoint::run_preview(options);
            std::cout << "Rendered " << result.frames << " clear frames; swapchain rebuilds: "
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
    std::cerr << "Usage: threepoint --version | --check-project <directory> | --preview | --smoke-vulkan | --capture-clear <file.ppm>\n";
    std::cerr << "3D game execution is planned for Phase 4.\n";
    return 2;
}
