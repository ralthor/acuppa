#include "threepoint/project.hpp"
#include "threepoint/scene_document.hpp"
#include "threepoint/source_document.hpp"
#include "threepoint/editor_viewport.hpp"
#include "threepoint/preview.hpp"

#include <algorithm>
#include <charconv>
#include <exception>
#include <filesystem>
#include <iostream>
#include <iterator>
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
    std::cerr << "  threepoint-editor source <project> <relative.lua>\n"
              << "  threepoint-editor replace <project> <relative.lua>  (read Lua from stdin)\n";
    std::cerr << "  threepoint-editor viewport <project> <scene> [--select <entity>]"
                 " [--frames <count> --capture <ppm-file>]\n";
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
            if (argc == 4 && (command == "source" || command == "replace")) {
                threepoint::SourceDocument document(argv[2], argv[3]);
                if (command == "source") {
                    std::cout.write(document.text().data(),
                                    static_cast<std::streamsize>(document.text().size()));
                } else {
                    std::string source;
                    for (char block[4096]; std::cin;) {
                        std::cin.read(block, sizeof(block));
                        const auto count = std::cin.gcount();
                        if (count > 0) source.append(block, static_cast<size_t>(count));
                        if (source.size() > 1024 * 1024)
                            throw std::invalid_argument("Lua source exceeds 1 MiB");
                    }
                    if (std::cin.bad()) throw std::runtime_error("cannot read Lua source from stdin");
                    document.replace(source);
                    std::cout << "Updated " << document.path().string() << '\n';
                }
                return 0;
            }
            if (command == "viewport") {
                threepoint::EditorViewport viewport(argv[2], argv[3]);
                uint32_t frames = 0;
                std::filesystem::path capture;
                std::string selected;
                for (int i = 4; i < argc; ++i) {
                    const std::string_view option = argv[i];
                    if (option == "--select" && i + 1 < argc && selected.empty()) {
                        selected = argv[++i];
                    } else if (option == "--frames" && i + 1 < argc && frames == 0) {
                        const std::string value = argv[++i];
                        unsigned parsed = 0;
                        const auto result = std::from_chars(value.data(), value.data() + value.size(),
                                                             parsed);
                        if (result.ec != std::errc{} ||
                            result.ptr != value.data() + value.size() ||
                            parsed == 0 || parsed > 100000)
                            throw std::invalid_argument("--frames must be from 1 to 100000");
                        frames = parsed;
                    } else if (option == "--capture" && i + 1 < argc && capture.empty()) {
                        capture = argv[++i];
                    } else throw std::invalid_argument("unknown or repeated viewport option");
                }
                if (!capture.empty() && frames == 0)
                    throw std::invalid_argument("--capture requires --frames");
                if (!selected.empty()) viewport.select(selected);
                threepoint::PreviewOptions options;
                options.scene = threepoint::PreviewOptions::Scene::Project;
                options.width = viewport.width();
                options.height = viewport.height();
                options.frame_limit = frames;
                options.capture_frame = frames == 0 ? 1 : frames;
                options.capture_path = capture;
                options.on_frame = [&](double) { return viewport.frame(); };
                options.on_input = [&](const threepoint::InputEvent& event) { viewport.input(event); };
                options.on_pointer = [&](double x, double y) { viewport.pointer(x, y); };
                options.on_resize = [&](uint32_t width, uint32_t height) {
                    viewport.resize(width, height);
                };
                const auto result = threepoint::run_preview(options);
                std::cout << "Editor viewport rendered " << result.frames << " frames\n";
                return result.frames == frames || frames == 0 ? 0 : 1;
            }
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
