#include "acuppa/physics_world.hpp"

#include <glm/geometric.hpp>

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

acuppa::PhysicsBodyDesc ground(uint64_t id) {
    acuppa::PhysicsBodyDesc desc;
    desc.id = id;
    desc.position = {0, -0.5f, 0};
    desc.half_extents = {40, 0.5f, 40};
    return desc;
}

// Box `pusher` slides into a resting box; returns how far the target moved.
float pushed_distance(float pusher_density, float target_density) {
    using namespace acuppa;
    PhysicsWorld world;
    world.add(ground(1));
    PhysicsBodyDesc pusher;
    pusher.id = 2;
    pusher.motion = PhysicsMotion::Dynamic;
    pusher.position = {0, 0.5f, 0};
    pusher.velocity = {6, 0, 0};
    pusher.density = pusher_density;
    world.add(pusher);
    PhysicsBodyDesc target = pusher;
    target.id = 3;
    target.position = {1.5f, 0.5f, 0};
    target.velocity = {0, 0, 0};
    target.density = target_density;
    world.add(target);
    for (int i = 0; i < 120; ++i) world.step();
    return world.state(3)->position.x - 1.5f;
}

// Highest sphere center reached after the first floor contact.
float bounce_peak(float restitution) {
    using namespace acuppa;
    PhysicsWorld world;
    world.add(ground(1));
    PhysicsBodyDesc ball;
    ball.id = 2;
    ball.motion = PhysicsMotion::Dynamic;
    ball.shape = ColliderShape::Sphere;
    ball.radius = 0.25f;
    ball.position = {0, 3, 0};
    ball.restitution = restitution;
    world.add(ball);
    bool landed = false;
    float peak = 0;
    for (int i = 0; i < 180; ++i) {
        world.step();
        for (const auto& contact : world.contacts()) landed |= contact.began;
        if (landed) peak = std::max(peak, world.state(2)->position.y);
    }
    return peak;
}

float slide_distance(float friction) {
    using namespace acuppa;
    PhysicsWorld world;
    world.add(ground(1));
    PhysicsBodyDesc box;
    box.id = 2;
    box.motion = PhysicsMotion::Dynamic;
    box.position = {0, 0.5f, 0};
    box.velocity = {4, 0, 0};
    box.friction = friction;
    world.add(box);
    for (int i = 0; i < 240; ++i) world.step();
    return world.state(2)->position.x;
}

float free_speed(float linear_damping) {
    using namespace acuppa;
    PhysicsWorld world;
    PhysicsBodyDesc ball;
    ball.id = 2;
    ball.motion = PhysicsMotion::Dynamic;
    ball.shape = ColliderShape::Sphere;
    ball.velocity = {10, 0, 0};
    ball.linear_damping = linear_damping;
    world.add(ball);
    for (int i = 0; i < 60; ++i) world.step();
    return world.state(2)->velocity.x;
}

// A rolling sphere keeps its spin unless angular damping removes it.
float roll_distance(float angular_damping) {
    using namespace acuppa;
    PhysicsWorld world;
    world.add(ground(1));
    PhysicsBodyDesc ball;
    ball.id = 2;
    ball.motion = PhysicsMotion::Dynamic;
    ball.shape = ColliderShape::Sphere;
    ball.position = {0, 0.5f, 0};
    ball.velocity = {5, 0, 0};
    ball.friction = 1.0f;
    ball.linear_damping = 0.0f;
    ball.angular_damping = angular_damping;
    world.add(ball);
    for (int i = 0; i < 180; ++i) world.step();
    return world.state(2)->position.x;
}

// A 0.1 m sphere at 120 m/s moves 2 m per step toward a 0.05 m thick wall.
acuppa::PhysicsBodyState fast_shot(bool ccd) {
    using namespace acuppa;
    PhysicsWorld world;
    PhysicsBodyDesc wall;
    wall.id = 1;
    wall.position = {0, 0, 0};
    wall.half_extents = {0.025f, 5, 5};
    world.add(wall);
    PhysicsBodyDesc shot;
    shot.id = 2;
    shot.motion = PhysicsMotion::Dynamic;
    shot.shape = ColliderShape::Sphere;
    shot.radius = 0.05f;
    shot.position = {-3.1f, 0, 0};
    shot.velocity = {120, 0, 0};
    shot.ccd = ccd;
    world.add(shot);
    for (int i = 0; i < 30; ++i) world.step();
    return *world.state(2);
}

void material_checks() {
    using namespace acuppa;
    const float heavy_push = pushed_distance(4000.0f, 250.0f);
    const float light_push = pushed_distance(250.0f, 4000.0f);
    expect(heavy_push > light_push * 2.0f && heavy_push > 0.5f,
           "dense box did not push a light box further than the reverse");
    expect(std::abs(pushed_distance(1000.0f, 1000.0f) - pushed_distance(1000.0f, 1000.0f)) == 0,
           "material push runs diverged");

    const float bouncy = bounce_peak(0.8f);
    const float dead = bounce_peak(0.0f);
    expect(bouncy > 1.2f && dead < 0.5f, "restitution did not change bounce height");
    expect(bounce_peak(0.8f) == bouncy, "restitution runs diverged");

    const float slippery = slide_distance(0.05f);
    const float grippy = slide_distance(1.0f);
    expect(slippery > grippy * 2.0f && grippy > 0.5f,
           "higher friction did not shorten the slide");

    const float undamped = free_speed(0.0f);
    const float damped = free_speed(2.0f);
    expect(std::abs(undamped - 10.0f) < 0.01f && damped < 3.0f && damped > 0.5f,
           "linear damping did not slow a free body");
    const float rolling = roll_distance(0.0f);
    const float braked = roll_distance(5.0f);
    expect(rolling > braked * 1.5f, "angular damping did not slow a rolling sphere");

    const auto tunneled = fast_shot(false);
    const auto stopped = fast_shot(true);
    expect(tunneled.position.x > 1.0f, "non-CCD test shot no longer tunnels; retune it");
    expect(stopped.position.x < 0.0f, "CCD shot passed through the thin wall");
    const auto repeat = fast_shot(true);
    expect(repeat.position.x == stopped.position.x && repeat.position.y == stopped.position.y,
           "CCD runs diverged");

    PhysicsWorld world;
    PhysicsBodyDesc body;
    body.id = 9;
    body.motion = PhysicsMotion::Dynamic;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    auto invalid = [&](auto change, const char* message) {
        PhysicsBodyDesc copy = body;
        change(copy);
        rejects([&] { world.add(copy); }, message);
    };
    invalid([&](PhysicsBodyDesc& d) { d.density = 0; }, "zero density accepted");
    invalid([&](PhysicsBodyDesc& d) { d.density = -5; }, "negative density accepted");
    invalid([&](PhysicsBodyDesc& d) { d.density = nan; }, "NaN density accepted");
    invalid([&](PhysicsBodyDesc& d) { d.friction = -0.1f; }, "negative friction accepted");
    invalid([&](PhysicsBodyDesc& d) { d.friction = inf; }, "infinite friction accepted");
    invalid([&](PhysicsBodyDesc& d) { d.restitution = 1.1f; }, "restitution above 1 accepted");
    invalid([&](PhysicsBodyDesc& d) { d.restitution = -0.1f; }, "negative restitution accepted");
    invalid([&](PhysicsBodyDesc& d) { d.restitution = nan; }, "NaN restitution accepted");
    invalid([&](PhysicsBodyDesc& d) { d.linear_damping = -1; }, "negative linear damping accepted");
    invalid([&](PhysicsBodyDesc& d) { d.angular_damping = nan; }, "NaN angular damping accepted");
    expect(!world.state(9), "rejected material body was added");
    body.restitution = 1.0f;
    body.friction = 0.0f;
    body.linear_damping = 0.0f;
    body.angular_damping = 0.0f;
    world.add(body);
    expect(world.state(9).has_value(), "boundary material values were rejected");
}
}

// A sleeping plank must fall once both of its supports are removed.
void removal_wakes_resting_bodies() {
    using namespace acuppa;
    PhysicsWorld world;
    PhysicsBodyDesc ground;
    ground.id = 1;
    ground.position = {0, -0.5f, 0};
    ground.half_extents = {10, 0.5f, 10};
    world.add(ground);
    for (uint64_t id : {2u, 3u}) {
        PhysicsBodyDesc pillar;
        pillar.id = id;
        pillar.motion = PhysicsMotion::Dynamic;
        pillar.position = {id == 2 ? -1.0f : 1.0f, 0.5f, 0};
        pillar.half_extents = {0.2f, 0.5f, 0.2f};
        world.add(pillar);
    }
    PhysicsBodyDesc plank;
    plank.id = 4;
    plank.motion = PhysicsMotion::Dynamic;
    plank.position = {0, 1.1f, 0};
    plank.half_extents = {1.5f, 0.1f, 0.3f};
    world.add(plank);
    for (int i = 0; i < 240; ++i) world.step();
    expect(!world.state(4)->active, "resting plank never went to sleep");
    const float rest = world.state(4)->position.y;
    expect(world.remove(2) && world.remove(3), "support removal failed");
    for (int i = 0; i < 60; ++i) world.step();
    expect(world.state(4)->position.y < rest - 0.5f,
           "plank stayed in mid-air after its supports were removed");
}

int main() {
    using namespace acuppa;
    material_checks();
    removal_wakes_resting_bodies();
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

    // An off-center force turns a body; the same force through the center only moves it.
    PhysicsWorld turning;
    PhysicsBodyDesc rod;
    rod.motion = PhysicsMotion::Dynamic;
    rod.half_extents = {0.2f, 1.0f, 0.2f};
    rod.id = 20;
    rod.position = {0, 10, 0};
    turning.add(rod);
    rod.id = 21;
    rod.position = {5, 10, 0};
    turning.add(rod);
    rod.id = 22;
    rod.position = {-5, 10, 0};
    turning.add(rod);
    expect(glm::length(turning.state(20)->angular_velocity) == 0.0f,
           "a new body should not be turning");
    turning.apply_force_at(20, {2000, 0, 0}, {0, 9, 0});  // At the bottom end.
    turning.apply_force(21, {2000, 0, 0});
    // A couple: equal and opposite pushes at both ends turn without moving.
    turning.apply_force_at(22, {1500, 0, 0}, {-5, 9, 0});
    turning.apply_force_at(22, {-1500, 0, 0}, {-5, 11, 0});
    turning.step();
    const auto pushed_low = *turning.state(20), pushed_center = *turning.state(21),
               coupled = *turning.state(22);
    // r x F = (0, -1, 0) x (2000, 0, 0) points along +Z.
    expect(pushed_low.angular_velocity.z > 0.1f &&
               std::abs(pushed_low.angular_velocity.x) < 1e-4f &&
               std::abs(pushed_low.angular_velocity.y) < 1e-4f,
           "a force below the center did not turn the body about +Z");
    expect(glm::length(pushed_center.angular_velocity) < 1e-5f,
           "a force through the center turned the body");
    expect(std::abs(pushed_low.velocity.x - pushed_center.velocity.x) < 1e-4f,
           "an off-center force changed the linear push");
    expect(coupled.angular_velocity.z > 0.1f && std::abs(coupled.velocity.x) < 1e-4f,
           "a force couple should turn the body without moving it sideways");
    turning.apply_torque(21, {0, -300, 0});
    turning.step();
    expect(turning.state(21)->angular_velocity.y < -0.1f &&
               std::abs(turning.state(21)->velocity.x - pushed_center.velocity.x) < 1e-4f,
           "a torque should turn the body without pushing it");
    rejects([&] { turning.apply_force_at(999, {1, 0, 0}, {0, 0, 0}); },
            "unknown off-center force target accepted");
    rejects([&] { turning.apply_force_at(20, {1, 0, 0}, {NAN, 0, 0}); },
            "nonfinite force point accepted");
    rejects([&] { turning.apply_force_at(20, {INFINITY, 0, 0}, {0, 0, 0}); },
            "nonfinite off-center force accepted");
    rejects([&] { turning.apply_torque(20, {0, NAN, 0}); }, "nonfinite torque accepted");
    rejects([&] { turning.apply_torque(999, {0, 1, 0}); }, "unknown torque target accepted");
    rod.id = 23;
    rod.motion = PhysicsMotion::Kinematic;
    turning.add(rod);
    rejects([&] { turning.apply_torque(23, {0, 1, 0}); }, "torque on a kinematic body accepted");
    rejects([&] { turning.apply_force_at(23, {1, 0, 0}, {0, 0, 0}); },
            "off-center force on a kinematic body accepted");
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
