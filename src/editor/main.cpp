#include "threepoint/project.hpp"
#include "threepoint/scene_document.hpp"

#include <algorithm>
#include <exception>
#include <iostream>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace {

void print_tree(const std::vector<threepoint::EditorEntity>& entities) {
    std::set<std::string> printed;
    const auto visit = [&](const auto& self, const threepoint::EditorEntity& entity,
                           unsigned depth) -> void {
        if (!printed.insert(entity.name).second) return;
        std::cout << std::string(depth * 2, ' ') << entity.name;
        if (!entity.components.empty()) {
            std::cout << " [";
            for (size_t i = 0; i < entity.components.size(); ++i) {
                if (i) std::cout << ", ";
                std::cout << entity.components[i];
            }
            std::cout << ']';
        }
        std::cout << '\n';
        for (const auto& child : entities)
            if (child.parent == entity.name) self(self, child, depth + 1);
    };
    for (const auto& entity : entities) {
        if (entity.parent.empty() || std::none_of(entities.begin(), entities.end(),
            [&](const auto& candidate) { return candidate.name == entity.parent; }))
            visit(visit, entity, 0);
    }
    for (const auto& entity : entities) if (!printed.count(entity.name)) visit(visit, entity, 0);
}

void usage() {
    std::cerr << "Usage:\n"
              << "  threepoint-editor --check-project <directory>\n"
              << "  threepoint-editor tree <project> <scene>\n"
              << "  threepoint-editor inspect <project> <scene> <entity>\n"
              << "  threepoint-editor add <project> <scene> <entity>\n"
              << "  threepoint-editor set <project> <scene> <entity> <component> <field> <Lua value>\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << "ThreePoint editor " << THREEPOINT_VERSION << " (source scene editor)\n";
        return 0;
    }
    try {
        if (argc == 3 && std::string_view(argv[1]) == "--check-project") {
            const auto project = threepoint::require_project(argv[2]);
            std::cout << "Project files found: " << project.root.string() << '\n';
            return 0;
        }
        if (argc >= 4) {
            const std::string_view command = argv[1];
            if ((command == "tree" && argc == 4) ||
                ((command == "inspect" || command == "add") && argc == 5) ||
                (command == "set" && argc == 8)) {
                threepoint::SceneDocument document(argv[2], argv[3]);
                if (command == "tree") {
                    std::cout << document.path().string() << '\n';
                    print_tree(document.tree());
                } else if (command == "inspect") {
                    std::cout << document.inspect(argv[4]) << '\n';
                } else if (command == "add") {
                    document.add_entity(argv[4]);
                    document.save();
                    std::cout << "Added " << argv[4] << " to " << document.path().string() << '\n';
                } else {
                    document.set_field(argv[4], argv[5], argv[6], argv[7]);
                    document.save();
                    std::cout << "Updated " << argv[4] << '.' << argv[5] << '.' << argv[6]
                              << " in " << document.path().string() << '\n';
                }
                return 0;
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "ThreePoint editor: " << error.what() << '\n';
        return 1;
    }
    usage();
    return 2;
}
