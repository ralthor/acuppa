#pragma once

#include "acuppa/scene_frame.hpp"
#include "acuppa/physics_world.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace acuppa {

using EntityId = uint64_t;

struct TransformComponent {
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 scale{1.0f};
    std::optional<EntityId> parent;
};

struct CameraComponent {
    PerspectiveCamera camera;
    bool active = false;
};

struct LightComponent {
    enum class Type { Directional, Point };
    Type type = Type::Point;
    glm::vec3 color{1.0f};
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    float intensity = 1.0f;
    bool shadows = false;
    float shadow_range = 25.0f;
};

struct MeshComponent {
    std::string mesh = "builtin:cube";
    bool visible = true;
    bool casts_shadows = true;
};

struct ScriptComponent {
    std::string path;
    bool enabled = true;
};

struct PhysicsComponent {
    PhysicsMotion motion = PhysicsMotion::Static;
    ColliderShape shape = ColliderShape::Box;
    glm::vec3 half_extents{0.5f};
    float radius = 0.5f;
    glm::vec3 velocity{0.0f};
    bool trigger = false;
    float density = 1000.0f;
    float friction = 0.2f;
    float restitution = 0.0f;
    float linear_damping = 0.05f;
    float angular_damping = 0.05f;
    bool ccd = false;
};

class World {
public:
    EntityId create(std::string name = {});
    void destroy(EntityId id);
    bool exists(EntityId id) const;
    std::vector<EntityId> entities() const;
    const std::string& name(EntityId id) const;
    void set_name(EntityId id, std::string name);

    void set_transform(EntityId id, const TransformComponent& value);
    void set_camera(EntityId id, const CameraComponent& value);
    void set_light(EntityId id, const LightComponent& value);
    void set_mesh(EntityId id, const MeshComponent& value);
    void set_script(EntityId id, const ScriptComponent& value);
    void set_physics(EntityId id, const PhysicsComponent& value);

    std::optional<TransformComponent> transform(EntityId id) const;
    std::optional<CameraComponent> camera(EntityId id) const;
    std::optional<LightComponent> light(EntityId id) const;
    std::optional<MeshComponent> mesh(EntityId id) const;
    std::optional<ScriptComponent> script(EntityId id) const;
    std::optional<PhysicsComponent> physics(EntityId id) const;

    bool remove_transform(EntityId id);
    bool remove_camera(EntityId id);
    bool remove_light(EntityId id);
    bool remove_mesh(EntityId id);
    bool remove_script(EntityId id);
    bool remove_physics(EntityId id);

    std::optional<EntityId> active_camera() const;
    SceneFrame snapshot(const PerspectiveCamera& fallback_camera) const;

private:
    struct Entity {
        std::string name;
        std::optional<TransformComponent> transform;
        std::optional<CameraComponent> camera;
        std::optional<LightComponent> light;
        std::optional<MeshComponent> mesh;
        std::optional<ScriptComponent> script;
        std::optional<PhysicsComponent> physics;
    };
    Entity& require(EntityId id);
    const Entity& require(EntityId id) const;
    std::map<EntityId, Entity> entities_;
    EntityId next_id_ = 1;
};

}  // namespace acuppa
