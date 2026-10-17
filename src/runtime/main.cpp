#include "threepoint/project.hpp"

#include <exception>
#include <iostream>
#include <string_view>

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << "ThreePoint runtime " << THREEPOINT_VERSION << " (3D foundation)\n";
        return 0;
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
    std::cerr << "Usage: threepoint --version | --check-project <directory>\n";
    std::cerr << "3D game execution is planned for Phase 4.\n";
    return 2;
}
