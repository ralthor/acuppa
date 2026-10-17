#pragma once

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace threepoint {

enum class PhysicsMotion { Static, Dynamic, Kinematic };
enum class ColliderShape { Box, Sphere };

struct PhysicsBodyDesc {
    uint64_t id = 0;
    PhysicsMotion motion = PhysicsMotion::Static;
    ColliderShape shape = ColliderShape::Box;
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 half_extents{0.5f};
    float radius = 0.5f;
    glm::vec3 velocity{0.0f};
    bool trigger = false;
};

struct PhysicsBodyState {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 velocity{0.0f};
    bool active = false;
};

struct PhysicsContact {
    uint64_t first = 0;
    uint64_t second = 0;
    bool trigger = false;
};

// Y is up; each step advances exactly 1/60 second. Contacts are new pairs from
// the most recent step. One world is driven from one thread at a time.
class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();
    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;
    PhysicsWorld(PhysicsWorld&&) = delete;
    PhysicsWorld& operator=(PhysicsWorld&&) = delete;

    void add(const PhysicsBodyDesc& body);
    bool remove(uint64_t id);
    std::optional<PhysicsBodyState> state(uint64_t id) const;
    void step();
    const std::vector<PhysicsContact>& contacts() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace threepoint
