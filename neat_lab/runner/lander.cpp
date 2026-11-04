#include "lander.hpp"

#include "acuppa/physics_world.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <algorithm>
#include <cmath>

namespace neat_lab {
namespace {

constexpr uint64_t kGroundId = 1;
constexpr uint64_t kLanderId = 2;

double clamp(double value, double low, double high) {
    if (!std::isfinite(value)) return 0;
    return std::max(low, std::min(high, value));
}

double proximity(float x) { return 60.0 * std::max(0.0, 1.0 - std::abs(x) / 15.0); }

Frame frame_of(const acuppa::PhysicsBodyState& state, const Thrust& thrust, float fuel) {
    float y = 0, x = 0, z = 0;
    glm::extractEulerAngleYXZ(glm::mat4_cast(state.rotation), y, x, z);
    return {state.position.x, state.position.y, x, y, z,
            float(thrust.main), float(thrust.side), fuel};
}

}  // namespace

const std::array<Scenario, kScenarioCount>& scenarios() {
    static const std::array<Scenario, kScenarioCount> values{{
        {-10.0f, 12.0f, 1.5f, 0.0f, 0.0f},  {8.0f, 13.0f, -1.0f, -0.5f, 1.2f},
        {-4.0f, 10.0f, 0.0f, -1.0f, -1.5f}, {3.0f, 14.0f, 0.8f, 0.0f, 0.8f},
        {0.0f, 11.0f, -1.5f, 0.0f, -0.6f},  {-7.0f, 14.0f, -0.5f, -0.5f, 1.8f},
        {10.0f, 10.0f, 0.0f, 0.0f, -1.0f},  {5.0f, 12.0f, 2.0f, -1.0f, 0.0f},
    }};
    return values;
}

std::array<double, kInputCount> observe(float x, float bottom, float vx, float vy, float fuel) {
    return {x / 10.0, bottom / 10.0, vx / 5.0, vy / 5.0, fuel};
}

Thrust thrust_from_outputs(double main_output, double side_output) {
    return {clamp((main_output + 1.0) * 0.5, 0.0, 1.0), clamp(side_output, -1.0, 1.0)};
}

const char* outcome_name(Outcome outcome) {
    switch (outcome) {
    case Outcome::Landed: return "landed";
    case Outcome::OffPad: return "off_pad";
    case Outcome::Crashed: return "crashed";
    case Outcome::Tipped: return "tipped";
    case Outcome::Timeout: return "timeout";
    case Outcome::OutOfBounds: return "out_of_bounds";
    }
    return "unknown";
}

EpisodeResult run_episode(const Scenario& scenario, const Controller& controller,
                          std::vector<Frame>* trace, const std::function<void()>& on_frame) {
    acuppa::PhysicsWorld world;
    acuppa::PhysicsBodyDesc ground;
    ground.id = kGroundId;
    ground.position = {0.0f, -0.5f, 0.0f};
    ground.half_extents = {kBoundsX + 10.0f, 0.5f, 4.0f};
    ground.friction = 0.8f;
    world.add(ground);

    acuppa::PhysicsBodyDesc lander;
    lander.id = kLanderId;
    lander.motion = acuppa::PhysicsMotion::Dynamic;
    lander.position = {scenario.x, scenario.y, 0.0f};
    lander.velocity = {scenario.vx, scenario.vy, 0.0f};
    lander.half_extents = {kLanderHalfWidth, kLanderHalfHeight, kLanderHalfWidth};
    lander.density = kLanderDensity;
    lander.friction = 0.8f;
    lander.linear_damping = 0.0f;
    world.add(lander);

    const float mass = 8.0f * kLanderHalfWidth * kLanderHalfHeight * kLanderHalfWidth *
                       kLanderDensity;
    const float weight = mass * kGravity;
    const float burn_per_tick = 1.0f / (kFuelSeconds * 60.0f);

    EpisodeResult result;
    float fuel = 1.0f;
    bool touched = false;
    int settle = 0;
    Thrust thrust;
    auto state = *world.state(kLanderId);
    auto record = [&](bool force) {
        if (!trace || (!force && result.ticks % kTraceStride != 0)) return;
        trace->push_back(frame_of(state, thrust, fuel));
        if (on_frame) on_frame();
    };

    for (;;) {
        const auto inputs = observe(state.position.x, state.position.y - kLanderHalfHeight,
                                    state.velocity.x, state.velocity.y, fuel);
        thrust = touched ? Thrust{} : controller(inputs);
        thrust.main = clamp(thrust.main, 0.0, 1.0);
        thrust.side = clamp(thrust.side, -1.0, 1.0);
        if (fuel <= 0.0f) thrust = {};
        record(false);
        fuel = std::max(0.0f, fuel - float(thrust.main + 0.5 * std::abs(thrust.side)) *
                                         burn_per_tick);
        // Wind keeps blowing after touchdown; friction must hold the lander.
        world.apply_force(kLanderId, {float(thrust.side) * kSideThrust * weight +
                                          scenario.wind * mass,
                                      float(thrust.main) * kMainThrust * weight, 0.0f});
        world.step();
        ++result.ticks;
        state = *world.state(kLanderId);

        if (!touched) {
            for (const auto& contact : world.contacts()) {
                const bool pair = (contact.first == kLanderId && contact.second == kGroundId) ||
                                  (contact.first == kGroundId && contact.second == kLanderId);
                if (!pair || !contact.began || contact.trigger) continue;
                touched = true;
                result.impact = contact.closing_speed;
            }
            if (touched && result.impact > kSafeImpact) {
                result.outcome = Outcome::Crashed;
                break;
            }
        } else if (++settle >= kSettleTicks) {
            const glm::vec3 up = state.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
            if (up.y < 0.95f) result.outcome = Outcome::Tipped;
            else if (std::abs(state.position.x) <= kPadHalfWidth) result.outcome = Outcome::Landed;
            else result.outcome = Outcome::OffPad;
            break;
        }
        if (std::abs(state.position.x) > kBoundsX || state.position.y > kBoundsY) {
            result.outcome = Outcome::OutOfBounds;
            break;
        }
        if (!touched && result.ticks >= kMaxTicks) {
            result.outcome = Outcome::Timeout;
            break;
        }
    }
    thrust = {};
    record(true);

    result.end_x = state.position.x;
    result.fuel = fuel;
    const double near = proximity(result.end_x);
    switch (result.outcome) {
    case Outcome::Landed:
        result.fitness = 100.0 + near + 40.0 * fuel +
                         30.0 * std::max(0.0, 1.0 - double(result.impact) / kSafeImpact);
        break;
    case Outcome::OffPad: result.fitness = near + 40.0; break;
    case Outcome::Tipped: result.fitness = near + 20.0; break;
    case Outcome::Crashed:
        result.fitness = near + 20.0 * std::max(0.0, 1.0 - (result.impact - kSafeImpact) / 10.0);
        break;
    case Outcome::Timeout: {
        const double bottom = state.position.y - kLanderHalfHeight;
        result.fitness = 0.5 * near + 10.0 * std::max(0.0, 1.0 - bottom / 15.0);
        break;
    }
    case Outcome::OutOfBounds: result.fitness = 0.0; break;
    }
    return result;
}

}  // namespace neat_lab
