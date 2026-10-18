#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct lua_State;

namespace threepoint {

struct EditorEntity {
    std::string name;
    std::string parent;
    std::vector<std::string> components;
};

// Edits declarative Lua scenes and writes the same file read by the runtime.
class SceneDocument {
public:
    SceneDocument(const std::filesystem::path& project_root, const std::string& scene);
    ~SceneDocument();
    SceneDocument(const SceneDocument&) = delete;
    SceneDocument& operator=(const SceneDocument&) = delete;

    const std::filesystem::path& path() const { return path_; }
    std::vector<EditorEntity> tree() const;
    std::string inspect(const std::string& entity) const;
    void add_entity(const std::string& name);
    void set_field(const std::string& entity, const std::string& component,
                   const std::string& field, const std::string& lua_literal);
    void save();

private:
    int find_entity(const std::string& name) const; // leaves entity table on Lua stack
    std::filesystem::path root_;
    std::filesystem::path path_;
    std::string original_;
    lua_State* lua_ = nullptr;
};

}  // namespace threepoint
