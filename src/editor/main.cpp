#include "threepoint/project.hpp"

#include <exception>
#include <iostream>
#include <string_view>

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << "ThreePoint editor " << THREEPOINT_VERSION << " (3D foundation)\n";
        return 0;
    }
    if (argc == 3 && std::string_view(argv[1]) == "--check-project") {
        try {
            const auto project = threepoint::require_project(argv[2]);
            std::cout << "Project files found: " << project.root.string() << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "ThreePoint editor: " << error.what() << '\n';
            return 1;
        }
    }
    std::cerr << "Usage: threepoint-editor --version | --check-project <directory>\n";
    std::cerr << "Interactive editing is planned for Phase 18.\n";
    return 2;
}
