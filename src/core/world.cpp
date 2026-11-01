#include "acuppa/world.hpp"

#include <glm/geometric.hpp>
#include <glm/ext/matrix_transform.hpp>

#include <cmath>
#include <filesystem>
#include <limits>
#include <stdexcept>

namespace acuppa {
namespace {

bool finite(glm::vec3 vector) {
    return std::isfinite(vector.x) && std::isfinite(vector.y) &&
           std::isfinite(vector.z);
}

}  // namespace

EntityId World::create(std::string name) {
    if (entities_.size() >= 10000 || next_id_ == std::numeric_limits<EntityId>::max()) {
        throw std::runtime_error("entity limit reached");
    }
    const EntityId id = next_id_++;
    entities_.emplace(id, Entity{std::move(name)});
    return id;
}

void World::destroy(EntityId id) {
    if (entities_.erase(id) == 0) throw std::invalid_argument("unknown entity ID");
    for (auto& [_, entity] : entities_) {
        if (entity.transform && entity.transform->parent == id) {
            entity.transform->parent.reset();
        }
    }
}

bool World::exists(EntityId id) const { return entities_.find(id) != entities_.end(); }

std::vector<EntityId> World::entities() const {
    std::vector<EntityId> result;
    result.reserve(entities_.size());
    for (const auto& [id, _] : entities_) result.push_back(id);
    return result;
}

World::Entity& World::require(EntityId id) {
    const auto found = entities_.find(id);
    if (found == entities_.end()) throw std::invalid_argument("unknown entity ID");
    return found->second;
}

const World::Entity& World::require(EntityId id) const {
    const auto found = entities_.find(id);
    if (found == entities_.end()) throw std::invalid_argument("unknown entity ID");
    return found->second;
}

const std::string& World::name(EntityId id) const { return require(id).name; }
void World::set_name(EntityId id, std::string name) { require(id).name = std::move(name); }

void World::set_transform(EntityId id, const TransformComponent& value) {
    if (!finite(value.position) || !finite(value.rotation) || !finite(value.scale) ||
        value.scale.x <= 0.0f || value.scale.y <= 0.0f || value.scale.z <= 0.0f) {
        throw std::invalid_argument("transform position/rotation must be finite and scale positive");
    }
    require(id);
    if (require(id).physics && value.parent)
        throw std::invalid_argument("physics transform cannot have a parent");
    if (value.parent) {
        if (!exists(*value.parent) || *value.parent == id) {
            throw std::invalid_argument("transform parent must be another live entity");
        }
        EntityId ancestor = *value.parent;
        while (true) {
            if (ancestor == id) throw std::invalid_argument("transform parenting cycle");
            const auto parent_transform = require(ancestor).transform;
            if (!parent_transform || !parent_transform->parent) break;
            ancestor = *parent_transform->parent;
        }
    }
    require(id).transform = value;
}

void World::set_camera(EntityId id, const CameraComponent& value) {
    value.camera.view_projection(1.0f);
    Entity& entity = require(id);
    if (value.active) {
        for (auto& [other_id, other] : entities_) {
            if (other_id != id && other.camera) other.camera->active = false;
        }
    }
    entity.camera = value;
}

void World::set_light(EntityId id, const LightComponent& value) {
    if (!finite(value.color) || !finite(value.direction) ||
        value.color.x < 0.0f || value.color.y < 0.0f || value.color.z < 0.0f ||
        glm::length(value.direction) <= 0.0f || !std::isfinite(value.intensity) ||
        value.intensity < 0.0f || !std::isfinite(value.shadow_range) ||
        value.shadow_range < 0.1f || value.shadow_range > 1000.0f) {
        throw std::invalid_argument("invalid light color, direction, intensity, or shadow range");
    }
    Entity& entity = require(id);
    if (!entity.light) {
        size_t count = 0;
        for (const auto& [_, other] : entities_) count += other.light.has_value();
        if (count >= 8) throw std::invalid_argument("scene supports at most eight lights");
    }
    entity.light = value;
}

void World::set_mesh(EntityId id, const MeshComponent& value) {
    const std::filesystem::path path(value.mesh);
    if (value.mesh != "builtin:cube" &&
        (value.mesh.empty() || path.is_absolute() ||
         (path.extension() != ".gltf" && path.extension() != ".glb") ||
         value.mesh.find("..") != std::string::npos)) {
        throw std::invalid_argument("unsupported mesh: " + value.mesh);
    }
    require(id).mesh = value;
}

void World::set_script(EntityId id, const ScriptComponent& value) {
    const std::filesystem::path path(value.path);
    if (value.path.empty() || path.is_absolute()) {
        throw std::invalid_argument("script path must be relative and nonempty");
    }
    for (const auto& part : path) {
        if (part == "..") throw std::invalid_argument("script path must stay inside project");
    }
    require(id).script = value;
}

void World::set_physics(EntityId id, const PhysicsComponent& value) {
    if (!finite(value.half_extents) || value.half_extents.x <= 0.0f ||
        value.half_extents.y <= 0.0f || value.half_extents.z <= 0.0f ||
        !std::isfinite(value.radius) || value.radius <= 0.0f || !finite(value.velocity))
        throw std::invalid_argument("physics collider dimensions must be finite and positive");
    if (!std::isfinite(value.density) || value.density <= 0.0f ||
        !std::isfinite(value.friction) || value.friction < 0.0f ||
        !std::isfinite(value.restitution) || value.restitution < 0.0f ||
        value.restitution > 1.0f || !std::isfinite(value.linear_damping) ||
        value.linear_damping < 0.0f || !std::isfinite(value.angular_damping) ||
        value.angular_damping < 0.0f)
        throw std::invalid_argument("physics material values are out of range");
    Entity& entity = require(id);
    if (!entity.transform || entity.transform->parent)
        throw std::invalid_argument("physics component requires an unparented transform");
    entity.physics = value;
}

std::optional<TransformComponent> World::transform(EntityId id) const { return require(id).transform; }
std::optional<CameraComponent> World::camera(EntityId id) const { return require(id).camera; }
std::optional<LightComponent> World::light(EntityId id) const { return require(id).light; }
std::optional<MeshComponent> World::mesh(EntityId id) const { return require(id).mesh; }
std::optional<ScriptComponent> World::script(EntityId id) const { return require(id).script; }
std::optional<PhysicsComponent> World::physics(EntityId id) const { return require(id).physics; }

bool World::remove_transform(EntityId id) {
    if (require(id).physics)
        throw std::invalid_argument("remove physics before removing its transform");
    auto& component = require(id).transform;
    const bool removed = component.has_value();
    component.reset();
    if (removed) {
        for (auto& [_, entity] : entities_) {
            if (entity.transform && entity.transform->parent == id) {
                entity.transform->parent.reset();
            }
        }
    }
    return removed;
}
bool World::remove_camera(EntityId id) {
    auto& component = require(id).camera;
    const bool removed = component.has_value();
    component.reset();
    return removed;
}
bool World::remove_light(EntityId id) {
    auto& component = require(id).light;
    const bool removed = component.has_value();
    component.reset();
    return removed;
}
bool World::remove_mesh(EntityId id) {
    auto& component = require(id).mesh;
    const bool removed = component.has_value();
    component.reset();
    return removed;
}
bool World::remove_script(EntityId id) {
    auto& component = require(id).script;
    const bool removed = component.has_value();
    component.reset();
    return removed;
}
bool World::remove_physics(EntityId id) {
    auto& component = require(id).physics;
    const bool removed = component.has_value();
    component.reset();
    return removed;
}

std::optional<EntityId> World::active_camera() const {
    for (const auto& [id, entity] : entities_) {
        if (entity.camera && entity.camera->active) return id;
    }
    return std::nullopt;
}

SceneFrame World::snapshot(const PerspectiveCamera& fallback_camera) const {
    SceneFrame frame;
    frame.camera = fallback_camera;
    if (const auto id = active_camera()) frame.camera = camera(*id)->camera;
    const auto local_matrix = [](const TransformComponent& transform) {
        glm::mat4 model = glm::translate(glm::mat4(1.0f), transform.position);
        model = glm::rotate(model, transform.rotation.y, glm::vec3(0, 1, 0));
        model = glm::rotate(model, transform.rotation.x, glm::vec3(1, 0, 0));
        model = glm::rotate(model, transform.rotation.z, glm::vec3(0, 0, 1));
        return glm::scale(model, transform.scale);
    };
    std::map<EntityId, glm::mat4> cached;
    const auto world_matrix = [&](EntityId id) -> glm::mat4 {
        std::vector<EntityId> chain;
        glm::mat4 matrix(1.0f);
        while (true) {
            if (const auto found = cached.find(id); found != cached.end()) {
                matrix = found->second;
                break;
            }
            const auto& transform = require(id).transform;
            if (!transform) break;
            chain.push_back(id);
            if (!transform->parent) break;
            id = *transform->parent;
        }
        for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
            matrix *= local_matrix(*require(*it).transform);
            cached.emplace(*it, matrix);
        }
        return matrix;
    };
    for (const auto& [id, entity] : entities_) {
        if (entity.light) {
            const glm::mat4 transform = world_matrix(id);
            const glm::vec3 direction = glm::normalize(glm::vec3(
                transform * glm::vec4(entity.light->direction, 0.0f)));
            frame.lights.push_back({entity.light->type == LightComponent::Type::Directional,
                                    glm::vec3(transform[3]), direction, entity.light->color,
                                    entity.light->intensity, entity.light->shadows,
                                    entity.light->shadow_range});
        }
        if (entity.mesh && entity.mesh->visible && entity.mesh->mesh == "builtin:cube") {
            const TransformComponent transform = entity.transform.value_or(TransformComponent{});
            const glm::mat4 parent = transform.parent
                ? world_matrix(*transform.parent) : glm::mat4(1.0f);
            frame.cubes.push_back({id, transform.position, transform.rotation, transform.scale,
                                   parent, entity.mesh->casts_shadows});
        } else if (entity.mesh && entity.mesh->visible) {
            frame.imported.push_back({id, entity.mesh->mesh, world_matrix(id), {},
                                      entity.mesh->casts_shadows});
        }
    }
    return frame;
}

}  // namespace acuppa
