#include "acuppa/physics_world.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
void expect(bool good, const char* message) {
    if (!good) throw std::runtime_error(message);
}

template <typename F> void rejects(F&& run, const char* message) {
    try { run(); } catch (const std::invalid_argument&) { return; }
    throw std::runtime_error(message);
}

bool touched(const acuppa::PhysicsWorld& world, uint64_t a, uint64_t b, bool trigger) {
    for (const auto& contact : world.contacts())
        if (((contact.first == a && contact.second == b) ||
             (contact.first == b && contact.second == a)) && contact.trigger == trigger)
            return true;
    return false;
}
}

int main() {
    using namespace acuppa;
    PhysicsWorld world;
    PhysicsBodyDesc floor;
    floor.id = 1;
    floor.position = {0, -0.5f, 0};
    floor.half_extents = {10, 0.5f, 10};
    world.add(floor);

    PhysicsBodyDesc ball;
    ball.id = 2;
    ball.motion = PhysicsMotion::Dynamic;
    ball.shape = ColliderShape::Sphere;
    ball.radius = 0.5f;
    ball.position = {0, 4, 0};
    world.add(ball);

    const float start = world.state(2)->position.y;
    bool collided = false;
    float impact_speed = 0;
    for (int i = 0; i < 180; ++i) {
        world.step();
        for (const auto& contact : world.contacts()) {
            if (contact.first == 1 && contact.second == 2 && !contact.trigger && contact.began) {
                collided = true;
                impact_speed = contact.closing_speed;
                expect(contact.normal.y > 0.9f, "floor contact normal has wrong direction");
            }
        }
    }
    const auto settled = world.state(2);
    expect(start > settled->position.y, "gravity did not make the ball fall");
    expect(collided, "solid floor collision was not reported");
    expect(impact_speed > 1.0f, "solid contact did not report closing speed");
    expect(std::abs(settled->position.y - 0.5f) < 0.03f,
           "ball did not rest on the floor without penetrating it");
    expect(std::abs(settled->velocity.y) < 0.05f, "resting ball kept falling");

    PhysicsWorld copy;
    copy.add(floor);
    copy.add(ball);
    for (int i = 0; i < 180; ++i) copy.step();
    expect(std::abs(copy.state(2)->position.y - settled->position.y) < 0.00001f,
           "identical fixed-step worlds diverged");

    PhysicsWorld trigger_world;
    PhysicsBodyDesc sensor;
    sensor.id = 3;
    sensor.trigger = true;
    sensor.position = {0, 2, 0};
    sensor.half_extents = {1, 0.25f, 1};
    trigger_world.add(sensor);
    trigger_world.add(ball);
    bool entered = false;
    bool exited = false;
    for (int i = 0; i < 90; ++i) {
        trigger_world.step();
        for (const auto& contact : trigger_world.contacts()) {
            if (contact.trigger && contact.first == 2 && contact.second == 3) {
                entered |= contact.began;
                exited |= !contact.began;
            }
        }
    }
    expect(entered, "trigger entry was not reported");
    expect(exited, "trigger exit was not reported");
    expect(trigger_world.state(2)->position.y < -4.0f,
           "trigger incorrectly stopped the falling ball");

    PhysicsBodyDesc box;
    box.id = 4;
    box.motion = PhysicsMotion::Dynamic;
    box.position = {2, 2, 0};
    world.add(box);
    for (int i = 0; i < 150; ++i) world.step();
    expect(std::abs(world.state(4)->position.y - 0.5f) < 0.04f,
           "dynamic box did not collide and rest");
    expect(world.remove(4) && !world.remove(4) && !world.state(4),
           "body removal did not clear state");

    PhysicsWorld kinematic_world;
    PhysicsBodyDesc platform;
    platform.id = 7;
    platform.motion = PhysicsMotion::Kinematic;
    platform.velocity = {1, 0, 0};
    kinematic_world.add(platform);
    for (int i = 0; i < 60; ++i) kinematic_world.step();
    expect(std::abs(kinematic_world.state(7)->position.x - 1.0f) < 0.03f &&
           std::abs(kinematic_world.state(7)->position.y) < 0.01f,
           "kinematic body did not follow its velocity without gravity");

    const auto ray = world.raycast({2, 5, 0}, {0, -1, 0}, 10);
    expect(!ray || ray->id != 4, "raycast found a removed body");
    const auto floor_ray = world.raycast({3, 5, 0}, {0, -1, 0}, 10);
    expect(floor_ray && floor_ray->id == 1 &&
           std::abs(floor_ray->position.y) < 0.05f,
           "raycast did not find the floor surface");
    expect(world.overlap_sphere({0, 0.5f, 0}, 0.6f).size() >= 1,
           "sphere overlap did not find the resting body");
    expect(!world.raycast({0, 5, 0}, {0, 1, 0}, 2),
           "raycast reported a hit in empty space");

    PhysicsWorld driven;
    ball.id = 8;
    ball.position = {0, 1, 0};
    driven.add(ball);
    driven.set_velocity(8, {1, 0, 0});
    driven.apply_force(8, {0, 20, 0});
    driven.apply_impulse(8, {0, 1000, 0});
    driven.step();
    expect(driven.state(8)->position.x > 0 && driven.state(8)->velocity.y > 0,
           "velocity, force, or impulse did not affect a dynamic body");
    driven.move(8, {2, 3, 0});
    expect(std::abs(driven.state(8)->position.x - 2) < 0.001f,
           "moving a body did not update its position");
    rejects([&] { driven.apply_force(999, {0, 1, 0}); }, "unknown force target accepted");
    rejects([&] { driven.raycast({0, 0, 0}, {0, 0, 0}, 1); }, "zero ray accepted");
    rejects([&] { driven.overlap_sphere({0, 0, 0}, 0); }, "zero overlap radius accepted");

    ball.id = 2;
    rejects([&] { world.add(ball); }, "duplicate body ID accepted");
    ball.id = 0;
    rejects([&] { world.add(ball); }, "zero body ID accepted");
    ball.id = 5;
    ball.radius = 0;
    rejects([&] { world.add(ball); }, "zero sphere radius accepted");
    ball.radius = 0.5f;
    ball.position.x = std::numeric_limits<float>::quiet_NaN();
    rejects([&] { world.add(ball); }, "nonfinite position accepted");
    box.id = 6;
    box.half_extents.x = -1;
    rejects([&] { world.add(box); }, "negative box extent accepted");

    PhysicsWorld vehicle_world;
    floor.id = 20;
    floor.half_extents = {30, 0.5f, 30};
    vehicle_world.add(floor);
    PhysicsBodyDesc chassis;
    chassis.id = 21;
    chassis.motion = PhysicsMotion::Dynamic;
    chassis.half_extents = {0.65f, 0.2f, 1.05f};
    chassis.position = {0, 0.75f, 0};
    vehicle_world.add(chassis);
    VehicleConfig invalid_center;
    invalid_center.center_of_mass_y = 0.1f;
    rejects([&] { vehicle_world.add_vehicle(21, invalid_center); },
            "raised vehicle center of mass accepted");
    invalid_center.center_of_mass_y = -1.1f;
    rejects([&] { vehicle_world.add_vehicle(21, invalid_center); },
            "out-of-range vehicle center of mass accepted");
    vehicle_world.add_vehicle(21);
    rejects([&] { vehicle_world.add_vehicle(21); }, "duplicate vehicle accepted");
    rejects([&] { vehicle_world.add_vehicle(20); }, "static body accepted as vehicle");
    rejects([&] { vehicle_world.set_vehicle_input(21, 2, 0, 0, 0); },
            "out-of-range throttle accepted");
    for (int i = 0; i < 100; ++i) {
        vehicle_world.set_vehicle_input(21, 0, 1, 0, 0);
        vehicle_world.step();
    }
    const auto still = vehicle_world.state(21);
    expect(std::abs(still->position.x) < 0.08f && std::abs(still->position.z) < 0.08f &&
           std::abs(still->rotation.y) < 0.08f,
           "front steering rotated or translated a stationary vehicle");
    expect(vehicle_world.vehicle_state(21)->wheels[0].grounded &&
           std::abs(vehicle_world.vehicle_state(21)->wheels[0].steer) > 0.1f,
           "front wheel did not steer on the ground");
    for (int i = 0; i < 180; ++i) {
        vehicle_world.set_vehicle_input(21, 1, 0, 0, 0);
        vehicle_world.step();
    }
    expect(vehicle_world.state(21)->position.z > 1.0f,
           "rear driven wheels did not move the chassis forward");
    expect(vehicle_world.vehicle_state(21)->engine_rpm > 0,
           "vehicle engine did not advance");
    const float straight_x = vehicle_world.state(21)->position.x;
    float max_rear_slip = 0.0f;
    for (int i = 0; i < 80; ++i) {
        vehicle_world.set_vehicle_input(21, 0.8f, 0.6f, 0, 1);
        vehicle_world.step();
        const auto wheels = vehicle_world.vehicle_state(21);
        max_rear_slip = std::max({max_rear_slip,
                                  std::abs(wheels->wheels[2].lateral_slip),
                                  std::abs(wheels->wheels[3].lateral_slip)});
    }
    expect(std::abs(vehicle_world.state(21)->position.x - straight_x) > 0.2f,
           "front steering did not turn the moving chassis");
    expect(max_rear_slip > 0.01f,
           "rear wheels did not develop lateral slip under steering and handbrake");
    expect(vehicle_world.remove(21) && !vehicle_world.vehicle_state(21),
           "vehicle removal left an active constraint");

    PhysicsWorld lowered_world;
    lowered_world.add(floor);
    chassis.id = 22;
    lowered_world.add(chassis);
    VehicleConfig lowered_center;
    lowered_center.center_of_mass_y = -0.2f;
    lowered_world.add_vehicle(22, lowered_center);
    expect(std::abs(lowered_world.state(22)->position.y - 0.75f) < 0.001f,
           "lowering vehicle center of mass moved the chassis shape");
    bool lowered_wheel_grounded = false;
    for (int i = 0; i < 180; ++i) {
        lowered_world.set_vehicle_input(22, 1, 0.3f, 0, 0);
        lowered_world.step();
        lowered_wheel_grounded |= lowered_world.vehicle_state(22)->wheels[0].grounded;
    }
    expect(lowered_wheel_grounded &&
           std::abs(lowered_world.state(22)->position.x) > 0.2f,
           "lower center of mass broke wheel contact or steering");
}
