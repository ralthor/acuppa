#include "rocket.hpp"

#include "acuppa/physics_world.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <algorithm>
#include <cmath>

namespace neat_rocket {
namespace {

constexpr uint64_t kGroundId = 1;
constexpr uint64_t kRocketId = 2;

double clamp(double value, double low, double high) {
    if (!std::isfinite(value)) return 0;
    return std::max(low, std::min(high, value));
}

// Steep near the pad, so every meter closer pays: 60 on the centre, 22 at 3 m.
double proximity(float x, float z) {
    return 60.0 * std::exp(-std::sqrt(double(x) * x + double(z) * z) / 3.0);
}

// Thrust direction in the rocket's frame: each gimbal axis swivels up to 30 degrees.
glm::vec3 thrust_direction(const Command& command) {
    return glm::normalize(glm::vec3(std::tan(float(command.gimbal_x) * kMaxGimbal), 1.0f,
                                    std::tan(float(command.gimbal_z) * kMaxGimbal)));
}

Frame frame_of(const acuppa::PhysicsBodyState& state, const Command& command, float fuel,
               const std::array<float, 2>& wind) {
    float y = 0, x = 0, z = 0;
    glm::extractEulerAngleYXZ(glm::mat4_cast(state.rotation), y, x, z);
    return {state.position.x, state.position.y, state.position.z, x, y, z,
            float(command.throttle), float(command.gimbal_x), float(command.gimbal_z), fuel,
            wind[0], wind[1]};
}

Score score_of(const FlightResult& result) {
    Score score;
    const double near = proximity(result.end_x, result.end_z);
    const double gentle = 30.0 * std::max(0.0, 1.0 - double(result.impact) / kSafeImpact);
    switch (result.outcome) {
    case Outcome::Landed:
        score.center = near;
        score.landing = 100.0;
        score.soft = gentle;
        score.fuel = 40.0 * result.fuel;
        score.speed = 30.0 * std::max(0.0, 1.0 - result.touch_time * 60.0 / kMaxTicks);
        break;
    case Outcome::OffPad: score = {near, 20.0, gentle}; break;
    case Outcome::Tipped: score = {near, 20.0, gentle}; break;
    case Outcome::Crashed:
        // Partial credit falls from 40 near the safe speed to 0 at a 14 m/s hit,
        // so slowing down pays before landing is ever learned.
        score.center = near;
        score.landing = 40.0 * std::max(0.0, 1.0 - (result.impact - kSafeImpact) / 12.0);
        break;
    case Outcome::Tumbled:
        // Staying upright longer before turning over earns up to 20.
        score.center = near;
        score.landing = 20.0 * double(result.ticks) / kMaxTicks;
        break;
    case Outcome::Timeout:
        score.center = 0.5 * near;
        score.landing = 20.0;  // Stayed upright and aloft for the whole flight.
        break;
    case Outcome::OutOfBounds: break;
    }
    return score;
}

}  // namespace

std::array<float, 2> Scenario::wind_at(float seconds) const {
    const float strength = wind + gust * std::sin(6.2831853f * seconds / gust_period);
    return {strength * std::cos(wind_heading), strength * std::sin(wind_heading)};
}

const std::vector<Scenario>& sample_scenarios() {
    static const std::vector<Scenario> values{
        {0, 15, 0, 0, -1, 0, 0, 0, 0, 0, 0, 3},
        {-6, 14, 2, 1.5f, -0.5f, 0, 0.05f, 0, 1.0f, 0.3f, 0.5f, 2.5f},
        {5, 16, -4, -1, 0, 1, 0, -0.08f, 1.5f, 2.0f, 0.8f, 4},
        {3, 13, 6, 0, -1.5f, -1.5f, -0.06f, 0.06f, 2.0f, 4.0f, 0, 3},
        {-7, 17, -5, 0.5f, 0, 0.5f, 0, 0, 0.5f, 5.5f, 1.2f, 2},
    };
    return values;
}

Command command_from_outputs(double throttle, double gimbal_x, double gimbal_z) {
    return {clamp((throttle + 1.0) * 0.5, 0.0, 1.0), clamp(gimbal_x, -1.0, 1.0),
            clamp(gimbal_z, -1.0, 1.0)};
}

const char* outcome_name(Outcome outcome) {
    switch (outcome) {
    case Outcome::Landed: return "landed";
    case Outcome::OffPad: return "off_pad";
    case Outcome::Crashed: return "crashed";
    case Outcome::Tipped: return "tipped";
    case Outcome::Tumbled: return "tumbled";
    case Outcome::Timeout: return "timeout";
    case Outcome::OutOfBounds: return "out_of_bounds";
    }
    return "unknown";
}

FlightResult run_flight(const Scenario& scenario, const Controller& controller,
                        std::vector<Frame>* trace, const std::function<void()>& on_frame) {
    acuppa::PhysicsWorld world;
    acuppa::PhysicsBodyDesc ground;
    ground.id = kGroundId;
    ground.position = {0.0f, -0.5f, 0.0f};
    ground.half_extents = {kBoundsXZ + 10.0f, 0.5f, kBoundsXZ + 10.0f};
    ground.friction = 0.8f;
    world.add(ground);

    acuppa::PhysicsBodyDesc rocket;
    rocket.id = kRocketId;
    rocket.motion = acuppa::PhysicsMotion::Dynamic;
    rocket.position = {scenario.x, scenario.y, scenario.z};
    rocket.velocity = {scenario.vx, scenario.vy, scenario.vz};
    rocket.rotation = glm::angleAxis(scenario.tilt_z, glm::vec3(1, 0, 0)) *
                      glm::angleAxis(scenario.tilt_x, glm::vec3(0, 0, 1));
    rocket.half_extents = {kBodyHalfWidth, kBodyHalfHeight, kBodyHalfWidth};
    rocket.density = kBodyDensity;
    rocket.friction = 0.8f;
    rocket.linear_damping = 0.0f;
    world.add(rocket);

    const float mass = 8.0f * kBodyHalfWidth * kBodyHalfHeight * kBodyHalfWidth * kBodyDensity;
    const float weight = mass * kGravity;
    const float burn_per_tick = 1.0f / (kFuelSeconds * 60.0f);

    FlightResult result;
    float fuel = 1.0f;
    bool touched = false;
    int settle = 0;
    Command command;
    auto state = *world.state(kRocketId);
    auto record = [&](bool force) {
        if (!trace || (!force && result.ticks % kTraceStride != 0)) return;
        trace->push_back(frame_of(state, command, fuel, scenario.wind_at(result.ticks / 60.0f)));
        if (on_frame) on_frame();
    };

    for (;;) {
        const glm::vec3 up = state.rotation * glm::vec3(0, 1, 0);
        const std::array<double, kInputCount> inputs{
            state.position.x / 10.0, state.position.z / 10.0,
            (state.position.y - kBodyHalfHeight) / 10.0,
            state.velocity.x / 5.0, state.velocity.y / 5.0, state.velocity.z / 5.0,
            up.x, up.z, state.angular_velocity.x / 2.0, state.angular_velocity.z / 2.0, fuel};
        command = touched || fuel <= 0.0f ? Command{} : controller(inputs);
        command.throttle = clamp(command.throttle, 0.0, 1.0);
        command.gimbal_x = clamp(command.gimbal_x, -1.0, 1.0);
        command.gimbal_z = clamp(command.gimbal_z, -1.0, 1.0);
        record(false);
        fuel = std::max(0.0f, fuel - float(command.throttle) * burn_per_tick);
        if (command.throttle > 0.0) {
            // The engine sits at the bottom of the body, so a swiveled thrust also turns it.
            const glm::vec3 force = state.rotation * thrust_direction(command) *
                                    float(command.throttle) * kMaxThrust * weight;
            const glm::vec3 nozzle =
                state.position + state.rotation * glm::vec3(0, -kBodyHalfHeight, 0);
            world.apply_force_at(kRocketId, force, nozzle);
        }
        // Wind keeps blowing after touchdown; friction must hold the rocket.
        const auto wind = scenario.wind_at(result.ticks / 60.0f);
        world.apply_force(kRocketId, {wind[0] * mass, 0.0f, wind[1] * mass});
        world.step();
        ++result.ticks;
        state = *world.state(kRocketId);
        const glm::vec3 now_up = state.rotation * glm::vec3(0, 1, 0);

        if (!touched) {
            for (const auto& contact : world.contacts()) {
                const bool pair = (contact.first == kRocketId && contact.second == kGroundId) ||
                                  (contact.first == kGroundId && contact.second == kRocketId);
                if (!pair || !contact.began || contact.trigger) continue;
                touched = true;
                result.impact = contact.closing_speed;
                result.touch_time = result.ticks / 60.0f;
            }
            if (touched && result.impact > kSafeImpact) {
                result.outcome = Outcome::Crashed;
                break;
            }
            if (!touched && now_up.y < 0.0f) {
                result.outcome = Outcome::Tumbled;  // Turned past horizontal in the air.
                break;
            }
        } else if (++settle >= kSettleTicks) {
            if (now_up.y < kUprightCos) result.outcome = Outcome::Tipped;
            else if (std::abs(state.position.x) <= kPadHalfSize &&
                     std::abs(state.position.z) <= kPadHalfSize)
                result.outcome = Outcome::Landed;
            else result.outcome = Outcome::OffPad;
            break;
        }
        if (std::abs(state.position.x) > kBoundsXZ || std::abs(state.position.z) > kBoundsXZ ||
            state.position.y > kBoundsY) {
            result.outcome = Outcome::OutOfBounds;
            break;
        }
        if (!touched && result.ticks >= kMaxTicks) {
            result.outcome = Outcome::Timeout;
            break;
        }
    }
    command = {};
    record(true);

    result.end_x = state.position.x;
    result.end_z = state.position.z;
    result.fuel = fuel;
    result.score = score_of(result);
    result.fitness = result.score.total();
    return result;
}

}  // namespace neat_rocket
