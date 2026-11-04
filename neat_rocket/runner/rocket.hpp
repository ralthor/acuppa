#pragma once

#include <array>
#include <functional>
#include <string>
#include <vector>

namespace neat_rocket {

// World layout shared with viewer/scenes/main.scene.lua. Y is up; the square
// pad is centered at the origin on the ground, and the rocket flies in full 3D.
constexpr float kPadHalfSize = 1.5f;
constexpr float kBodyHalfWidth = 0.5f;
constexpr float kBodyHalfHeight = 0.9f;
constexpr float kBodyDensity = 300.0f;
constexpr float kGravity = 9.81f;
constexpr float kMaxThrust = 2.0f;        // Multiple of the rocket's weight at full throttle.
constexpr float kMaxGimbal = 0.5235988f;  // 30 degrees each way, on each body axis.
constexpr float kFuelSeconds = 7.0f;      // Seconds of full throttle in a full tank.
constexpr float kSafeImpact = 2.0f;       // Touchdown closing speed (m/s) a landing survives.
constexpr float kUprightCos = 0.966f;     // Within 15 degrees of vertical counts as upright.
constexpr float kBoundsXZ = 30.0f;
constexpr float kBoundsY = 45.0f;
constexpr int kMaxTicks = 720;            // 12 seconds at the engine's 60 Hz physics step.
constexpr int kSettleTicks = 60;          // Engine cuts at touchdown; the body settles for 1 s.
constexpr int kTraceStride = 2;           // Trace frames are recorded at 30 Hz.

// Start state and weather for one flight. The trainer draws new scenarios
// every generation, so networks cannot memorize them.
struct Scenario {
    float x = 0, y = 15, z = 0;
    float vx = 0, vy = 0, vz = 0;
    float tilt_x = 0, tilt_z = 0;  // Start tilt (radians) about the world Z and X axes.
    float wind = 0;                // Steady wind acceleration (m/s^2), unseen by the network.
    float wind_heading = 0;        // Wind direction in the ground plane (radians from +X).
    float gust = 0;                // Gust amplitude (m/s^2), a sine wave along the wind.
    float gust_period = 3;         // Seconds per gust cycle, > 0.
    // Wind acceleration on the ground plane at a time.
    std::array<float, 2> wind_at(float seconds) const;
};
constexpr int kMaxScenarios = 64;
const std::vector<Scenario>& sample_scenarios();

// Network inputs in neat-python key order -1..-11: pad offset x and z,
// height, velocity x/y/z, the rocket's up vector x and z (its tilt), spin
// rate about world x and z, and fuel.
constexpr int kInputCount = 11;
constexpr int kOutputCount = 3;

struct Command {
    double throttle = 0;  // 0..1
    double gimbal_x = 0;  // -1..1 of kMaxGimbal; positive tilts the thrust toward body +X.
    double gimbal_z = 0;  // -1..1 of kMaxGimbal; positive tilts the thrust toward body +Z.
};
// Maps raw network outputs (tanh range) to a clamped engine command.
Command command_from_outputs(double throttle, double gimbal_x, double gimbal_z);

enum class Outcome { Landed, OffPad, Crashed, Tipped, Tumbled, Timeout, OutOfBounds };
const char* outcome_name(Outcome outcome);

struct Frame {
    float x, y, z, rx, ry, rz;  // Position and engine YXZ Euler rotation (radians).
    float throttle, gimbal_x, gimbal_z, fuel, wind_x, wind_z;
};

// Fitness is the sum of these parts; README.md lists the rules.
struct Score {
    double center = 0;   // Up to 60 for ending close to the pad.
    double landing = 0;  // Landed 100, off pad 40, crash 0..40, tipped/tumbled/timeout up to 20.
    double soft = 0;     // Up to 30 for a gentle touchdown that did not crash.
    double fuel = 0;     // Up to 40 for fuel left after a landing.
    double speed = 0;    // Up to 30 for landing sooner.
    double total() const { return center + landing + soft + fuel + speed; }
};

struct FlightResult {
    Outcome outcome = Outcome::Timeout;
    double fitness = 0;
    Score score;
    float impact = 0;
    float touch_time = 0;
    float end_x = 0, end_z = 0;
    float fuel = 1;
    int ticks = 0;
};

using Controller = std::function<Command(const std::array<double, kInputCount>& inputs)>;

// Runs one deterministic flight in a fresh physics world. The engine pushes at
// the bottom of the rocket through ACuppa's off-center force, so a swiveled
// engine both pushes and turns it. With `trace`, records a frame every
// kTraceStride ticks plus the final state; `on_frame` runs after each one.
FlightResult run_flight(const Scenario& scenario, const Controller& controller,
                        std::vector<Frame>* trace = nullptr,
                        const std::function<void()>& on_frame = {});

}  // namespace neat_rocket
