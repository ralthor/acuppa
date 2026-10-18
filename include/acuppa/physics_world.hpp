#pragma once

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace acuppa {

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
    bool began = true;
    // Unit normal from first to second; closing speed along that normal at entry.
    glm::vec3 normal{0.0f};
    float closing_speed = 0.0f;
};

struct PhysicsRayHit {
    uint64_t id = 0;
    glm::vec3 position{0.0f};
    float distance = 0.0f;
};

struct VehicleConfig {
    float half_track = 0.55f;
    float half_wheelbase = 0.72f;
    float wheel_radius = 0.28f;
    float engine_torque = 800.0f;
    float rear_grip = 0.75f;
};

struct VehicleWheelState {
    bool grounded = false;
    float steer = 0.0f;
    float rotation = 0.0f;
    float lateral_slip = 0.0f;
};

struct VehicleState {
    float engine_rpm = 0.0f;
    VehicleWheelState wheels[4]; // Front left/right, rear left/right.
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
    void set_velocity(uint64_t id, glm::vec3 velocity);
    void move(uint64_t id, glm::vec3 position);
    void set_pose(uint64_t id, glm::vec3 position, glm::quat rotation);
    void apply_force(uint64_t id, glm::vec3 force);
    void apply_impulse(uint64_t id, glm::vec3 impulse);
    void add_vehicle(uint64_t id, const VehicleConfig& config = {});
    void set_vehicle_input(uint64_t id, float throttle, float steer, float brake,
                           float handbrake);
    std::optional<VehicleState> vehicle_state(uint64_t id) const;
    std::optional<PhysicsRayHit> raycast(glm::vec3 origin, glm::vec3 direction,
                                         float distance) const;
    std::vector<uint64_t> overlap_sphere(glm::vec3 center, float radius) const;
    void step();
    const std::vector<PhysicsContact>& contacts() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace acuppa
