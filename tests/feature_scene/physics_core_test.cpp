#include "threepoint/physics_world.hpp"

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

bool touched(const threepoint::PhysicsWorld& world, uint64_t a, uint64_t b, bool trigger) {
    for (const auto& contact : world.contacts())
        if (((contact.first == a && contact.second == b) ||
             (contact.first == b && contact.second == a)) && contact.trigger == trigger)
            return true;
    return false;
}
}

int main() {
    using namespace threepoint;
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
    for (int i = 0; i < 180; ++i) {
        world.step();
        collided |= touched(world, 1, 2, false);
    }
    const auto settled = world.state(2);
    expect(start > settled->position.y, "gravity did not make the ball fall");
    expect(collided, "solid floor collision was not reported");
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
    for (int i = 0; i < 90; ++i) {
        trigger_world.step();
        entered |= touched(trigger_world, 2, 3, true);
    }
    expect(entered, "trigger entry was not reported");
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
}
