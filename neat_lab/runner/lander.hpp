#pragma once

#include <array>
#include <functional>
#include <string>
#include <vector>

namespace neat_lab {

// World layout shared with viewer/scenes/main.scene.lua. Y is up, the lander
// moves in the X/Y plane, and the pad is centered at x = 0 on the ground.
constexpr float kPadHalfWidth = 1.0f;
constexpr float kLanderHalfWidth = 0.45f;
constexpr float kLanderHalfHeight = 0.3f;
constexpr float kLanderDensity = 400.0f;
constexpr float kGravity = 9.81f;
constexpr float kMainThrust = 2.0f;   // Multiple of the lander's weight at full throttle.
constexpr float kSideThrust = 0.5f;   // Multiple of weight for the sideways jets.
constexpr float kFuelSeconds = 6.0f;  // Seconds of full main thrust in a full tank.
constexpr float kSafeImpact = 1.5f;   // Touchdown closing speed (m/s) a landing survives.
constexpr float kBoundsX = 30.0f;
constexpr float kBoundsY = 40.0f;
constexpr int kMaxTicks = 600;        // 10 seconds at the engine's 60 Hz physics step.
constexpr int kSettleTicks = 60;      // Engines cut at touchdown; the body settles for 1 s.
constexpr int kTraceStride = 2;       // Trace frames are recorded at 30 Hz.

// Start state and weather for one episode. The trainer draws new scenarios
// every generation, so networks cannot memorize them.
struct Scenario {
    float x = 0, y = 12, vx = 0, vy = 0;
    float wind = 0;         // Steady sideways acceleration (m/s^2), unseen by the network.
    float gust = 0;         // Gust amplitude (m/s^2), added as a sine wave.
    float gust_period = 3;  // Seconds per gust cycle, > 0.
    float wind_at(float seconds) const;
};
constexpr int kMaxScenarios = 64;
// A fixed, varied set for tests and for runs without trainer-supplied scenarios.
const std::vector<Scenario>& sample_scenarios();

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
    float main, side, fuel, wind;
};

// Fitness is the sum of these parts; README.md lists the rules.
struct Score {
    double center = 0;   // Up to 60 for ending close to the pad.
    double landing = 0;  // Landed 100, off pad 40, tipped 20, crash 0..20.
    double soft = 0;     // Up to 30 for a gentle touchdown that did not crash.
    double fuel = 0;     // Up to 40 for fuel left after a landing.
    double speed = 0;    // Up to 30 for landing sooner.
    double total() const { return center + landing + soft + fuel + speed; }
};

struct EpisodeResult {
    Outcome outcome = Outcome::Timeout;
    double fitness = 0;
    Score score;
    float impact = 0;      // Touchdown closing speed, 0 without touchdown.
    float touch_time = 0;  // Seconds until touchdown, 0 without touchdown.
    float end_x = 0;
    float fuel = 1;
    int ticks = 0;
};

using Controller = std::function<Thrust(const std::array<double, kInputCount>& inputs)>;

// Runs one deterministic episode in a fresh physics world. When `trace` is
// non-null, records a frame every kTraceStride ticks plus the final state;
// `on_frame` is called right after each recorded frame is appended.
EpisodeResult run_episode(const Scenario& scenario, const Controller& controller,
                          std::vector<Frame>* trace = nullptr,
                          const std::function<void()>& on_frame = {});

}  // namespace neat_lab
