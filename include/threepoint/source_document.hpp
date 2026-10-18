#pragma once

#include <filesystem>
#include <string>

namespace threepoint {

// Raw Lua source editing. Unlike SceneDocument, this preserves comments and
// computed scene code, and can also open behavior scripts and game.lua.
class SourceDocument {
public:
    SourceDocument(const std::filesystem::path& project_root,
                   const std::filesystem::path& project_relative_file);

    const std::filesystem::path& path() const { return path_; }
    const std::string& text() const { return original_; }
    void replace(const std::string& source);

private:
    std::filesystem::path path_;
    std::string original_;
};

}  // namespace threepoint
