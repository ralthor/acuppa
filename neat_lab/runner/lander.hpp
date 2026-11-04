#pragma once

#include <array>
#include <functional>
#include <string>
#include <vector>

namespace neat_lab {

// World layout shared with viewer/scenes/main.scene.lua. Y is up, the lander
// moves in the X/Y plane, and the pad is centered at x = 0 on the ground.
constexpr float kPadHalfWidth = 1.5f;
constexpr float kLanderHalfWidth = 0.45f;
constexpr float kLanderHalfHeight = 0.3f;
constexpr float kLanderDensity = 400.0f;
constexpr float kGravity = 9.81f;
constexpr float kMainThrust = 2.0f;   // Multiple of the lander's weight at full throttle.
constexpr float kSideThrust = 0.5f;   // Multiple of weight for the sideways jets.
constexpr float kFuelSeconds = 6.0f;  // Seconds of full main thrust in a full tank.
constexpr float kSafeImpact = 2.5f;   // Touchdown closing speed (m/s) a landing survives.
constexpr float kBoundsX = 30.0f;
constexpr float kBoundsY = 40.0f;
constexpr int kMaxTicks = 600;        // 10 seconds at the engine's 60 Hz physics step.
constexpr int kSettleTicks = 60;      // Engines cut at touchdown; the body settles for 1 s.
constexpr int kTraceStride = 2;       // Trace frames are recorded at 30 Hz.

struct Scenario {
    float x, y, vx, vy;
    float wind = 0;  // Steady sideways acceleration (m/s^2) the network must infer.
};
constexpr int kScenarioCount = 8;
const std::array<Scenario, kScenarioCount>& scenarios();

// Network inputs, in neat-python input-key order -1..-5.
constexpr int kInputCount = 5;
constexpr int kOutputCount = 2;
std::array<double, kInputCount> observe(float x, float bottom, float vx, float vy, float fuel);

struct Thrust {
    double main = 0;  // 0..1
    double side = 0;  // -1..1, positive pushes toward +X.
};
// Maps raw network outputs (tanh range) to clamped thrust commands.
Thrust thrust_from_outputs(double main_output, double side_output);

enum class Outcome { Landed, OffPad, Crashed, Tipped, Timeout, OutOfBounds };
const char* outcome_name(Outcome outcome);

struct Frame {
    float x, y, rx, ry, rz;  // Position and engine YXZ Euler rotation (radians).
    float main, side, fuel;
};

struct EpisodeResult {
    Outcome outcome = Outcome::Timeout;
    double fitness = 0;
    float impact = 0;    // Touchdown closing speed, 0 without touchdown.
    float end_x = 0;
    float fuel = 1;
    int ticks = 0;
};

// Called once per tick before the physics step with the current observation;
// fills `activations` (optional) with per-node values for tracing.
using Controller = std::function<Thrust(const std::array<double, kInputCount>& inputs)>;

// Runs one deterministic episode in a fresh physics world. When `trace` is
// non-null, records a frame every kTraceStride ticks plus the final state;
// `on_frame` is called right after each recorded frame is appended.
EpisodeResult run_episode(const Scenario& scenario, const Controller& controller,
                          std::vector<Frame>* trace = nullptr,
                          const std::function<void()>& on_frame = {});

}  // namespace neat_lab
