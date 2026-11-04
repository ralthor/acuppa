#include "protocol.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace neat_rocket;

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message.c_str());
        ++failures;
    }
}

template <typename F>
void check_throws(F&& action, const std::string& fragment, const std::string& message) {
    try {
        action();
    } catch (const std::exception& error) {
        const std::string text = error.what();
        check(text.find(fragment) != std::string::npos,
              message + " (wrong error: " + text + ")");
        return;
    }
    check(false, message + " (no error)");
}

Controller constant(double throttle, double gimbal_x, double gimbal_z) {
    return [=](const std::array<double, kInputCount>&) {
        return Command{throttle, gimbal_x, gimbal_z};
    };
}

// Hand-tuned cascaded controller proving the task can be flown: position
// error picks a tilt, tilt error picks a spin rate, and the gimbal tracks that
// rate; the throttle follows a braking-distance descent profile.
Command oracle(const std::array<double, kInputCount>& in) {
    const double px = in[0] * 10, pz = in[1] * 10, h = in[2] * 10;
    const double vx = in[3] * 5, vy = in[4] * 5, vz = in[5] * 5;
    const double ux = in[6], uz = in[7], wx = in[8] * 2, wz = in[9] * 2;
    const double uy = std::sqrt(std::max(0.2, 1 - ux * ux - uz * uz));
    // Close to the ground, stand up straight for the touchdown.
    const double lean = std::clamp((h - 0.15) / 0.8, 0.0, 1.0);
    auto tilt_for = [&](double p, double v) {
        return lean * std::clamp((-2.0 * p - 2.5 * v) / kGravity, -0.5, 0.5);
    };
    const double want_wz = -4.0 * (tilt_for(px, vx) - ux);
    const double want_wx = 4.0 * (tilt_for(pz, vz) - uz);
    const bool over_pad = std::hypot(px, pz) < 0.8 && std::hypot(vx, vz) < 0.8;
    const double brake = over_pad ? 2.0 : 0.5;
    const double want_vy = -std::clamp(std::sqrt(2 * brake * std::max(0.0, h - 0.1)), 0.6, 7.0);
    const double accel = kGravity + 5.0 * (want_vy - vy);
    return {std::clamp(accel / (kMaxThrust * kGravity * uy), 0.0, 1.0),
            std::clamp(0.6 * (want_wz - wz), -1.0, 1.0),
            std::clamp(-0.6 * (want_wx - wx), -1.0, 1.0)};
}

Scenario random_scenario(std::mt19937& rng) {
    auto pick = [&](float low, float high) {
        return std::uniform_real_distribution<float>(low, high)(rng);
    };
    return {pick(-8, 8),       pick(12, 18),  pick(-8, 8),  pick(-2, 2),
            pick(-2, 0),       pick(-2, 2),   pick(-0.1f, 0.1f), pick(-0.1f, 0.1f),
            pick(0, 2.0f),     pick(0, 6.2831853f), pick(0, 1.0f), pick(1.5f, 5)};
}

std::string scenario_text() {
    std::ostringstream out;
    out.precision(9);
    out << "scenarios " << sample_scenarios().size() << "\n";
    for (const auto& s : sample_scenarios())
        out << "scenario " << s.x << ' ' << s.y << ' ' << s.z << ' ' << s.vx << ' ' << s.vy
            << ' ' << s.vz << ' ' << s.tilt_x << ' ' << s.tilt_z << ' ' << s.wind << ' '
            << s.wind_heading << ' ' << s.gust << ' ' << s.gust_period << "\n";
    return out.str();
}

std::string batch_text(const std::string& genomes, int count, int threads = 1) {
    return "acuppa-rocket 1 threads " + std::to_string(threads) + "\n" + scenario_text() +
           "trace_scenario 2 genomes " + std::to_string(count) + "\n" + genomes + "end\n";
}

Batch parse(const std::string& text) {
    std::istringstream in(text);
    return read_batch(in);
}

std::string random_genome(std::mt19937& rng, long long key, bool trace) {
    std::normal_distribution<double> weight(0.0, 1.0);
    std::ostringstream out;
    out.precision(17);
    // One hidden node fed by every input, plus direct input links to every output.
    out << "genome " << key << ' ' << (trace ? 1 : 0) << " 4\n";
    out << "node 20 tanh sum " << weight(rng) << " 1 " << kInputCount;
    for (int i = 1; i <= kInputCount; ++i) out << ' ' << -i << ' ' << weight(rng);
    out << "\n";
    for (int o = 0; o < kOutputCount; ++o) {
        out << "node " << o << " tanh sum " << weight(rng) << " 1 " << kInputCount + 1 << " 20 "
            << weight(rng);
        for (int i = 1; i <= kInputCount; ++i) out << ' ' << -i << ' ' << weight(rng);
        out << "\n";
    }
    return out.str();
}

}  // namespace

int main() {
    const auto& all = sample_scenarios();

    // Free fall from 15 m hits at about 17 m/s and crashes upright.
    auto fall = run_flight(all[0], constant(0, 0, 0));
    check(fall.outcome == Outcome::Crashed && fall.impact > 14 && fall.fuel == 1.0f,
          "free fall should crash fast without burning fuel, impact " +
              std::to_string(fall.impact));

    // Full straight thrust is twice the weight, so the rocket climbs out of bounds.
    auto climb = run_flight(all[0], constant(1, 0, 0));
    check(climb.outcome == Outcome::OutOfBounds && climb.fitness == 0,
          "full thrust should leave the bounds with zero fitness");

    // Straight thrust through the bottom never turns an upright rocket.
    std::vector<Frame> trace;
    run_flight(all[0], constant(0.5, 0, 0), &trace);
    check(trace.size() > 60 && std::abs(trace[60].rx) < 1e-4f && std::abs(trace[60].rz) < 1e-4f,
          "centered thrust should keep the rocket upright");

    // Swiveling the engine toward +X pushes the bottom toward +X, so the top
    // tips toward -X: rotation about +Z, and the drift then follows the nose.
    trace.clear();
    auto tipped = run_flight(all[0], constant(0.5, 0.3, 0), &trace);
    check(trace.size() > 20 && trace[20].rz > 0.05f && std::abs(trace[20].rx) < 0.02f,
          "a +X gimbal should turn the rocket about +Z, got rz " +
              std::to_string(trace.size() > 20 ? trace[20].rz : 0));
    check(tipped.outcome == Outcome::Tumbled || tipped.outcome == Outcome::Crashed ||
              tipped.outcome == Outcome::OutOfBounds,
          std::string("a held gimbal should end badly, got ") + outcome_name(tipped.outcome));
    trace.clear();
    run_flight(all[0], constant(0.5, 0, 0.3), &trace);
    check(trace.size() > 20 && trace[20].rx < -0.05f && std::abs(trace[20].rz) < 0.02f,
          "a +Z gimbal should turn the rocket about -X");
    check(trace[20].gimbal_z > 0.29f && trace[20].throttle > 0.49f, "trace should record the engine");

    // Steady wind along +Z pushes the rocket that way.
    trace.clear();
    run_flight({0, 15, 0, 0, 0, 0, 0, 0, 2.0f, 1.5707963f, 0, 3}, constant(0.5, 0, 0), &trace);
    check(trace.size() > 60 && trace[60].z > 0.5f && std::abs(trace[60].x) < 0.01f &&
              std::abs(trace[60].wind_z - 2.0f) < 1e-3f,
          "wind heading 90 degrees should push toward +Z and be recorded");

    // The task is flyable: the oracle lands every sample and most random flights.
    for (size_t i = 0; i < all.size(); ++i) {
        auto result = run_flight(all[i], oracle);
        check(result.outcome == Outcome::Landed,
              "oracle failed sample " + std::to_string(i) + ": " + outcome_name(result.outcome) +
                  " impact " + std::to_string(result.impact) + " at " +
                  std::to_string(result.end_x) + "," + std::to_string(result.end_z));
    }
    std::mt19937 weather(11);
    std::map<std::string, int> outcomes;
    const int tried = 300;
    for (int i = 0; i < tried; ++i) outcomes[outcome_name(run_flight(random_scenario(weather), oracle).outcome)]++;
    if (outcomes["landed"] < tried * 7 / 10) {
        for (const auto& [name, count] : outcomes) std::fprintf(stderr, "  %s %d\n", name.c_str(), count);
        check(false, "oracle landed only " + std::to_string(outcomes["landed"]) + " of 300");
    }

    auto good = run_flight(all[0], oracle);
    check(std::abs(good.score.total() - good.fitness) < 1e-9 && good.score.landing == 100 &&
              good.score.speed > 0 && good.score.fuel > 0,
          "a landing should earn landing, fuel, and speed parts that sum to the fitness");
    auto again = run_flight(all[1], oracle);
    check(again.fitness == run_flight(all[1], oracle).fitness, "flights are not deterministic");

    auto command = command_from_outputs(-1, 2, -3);
    check(command.throttle == 0 && command.gimbal_x == 1 && command.gimbal_z == -1,
          "outputs should clamp to engine ranges");

    // Batch protocol: parallel evaluation equals serial evaluation exactly.
    std::mt19937 rng(42);
    std::string genomes;
    for (int i = 0; i < 16; ++i) genomes += random_genome(rng, 100 + i, i == 3);
    auto serial = evaluate_batch(parse(batch_text(genomes, 16, 1)));
    auto parallel = evaluate_batch(parse(batch_text(genomes, 16, 8)));
    bool same = true;
    for (size_t i = 0; i < serial.size(); ++i)
        same = same && serial[i].fitness == parallel[i].fitness;
    check(same, "parallel evaluation differs from serial");
    const auto& traced = serial[3];
    check(!traced.frames.empty() && traced.frames.size() == traced.activations.size() &&
              traced.node_keys.size() == size_t(kInputCount + kOutputCount + 1),
          "trace should hold inputs, outputs, and the hidden node per frame");
    check(traced.traced.fitness == traced.episodes[2].fitness,
          "traced replay should reproduce the scored trace scenario");

    std::ostringstream written;
    write_results(written, parse(batch_text(genomes, 16, 2)), serial);
    const auto text = written.str();
    check(text.rfind("acuppa-rocket 1\n", 0) == 0 && text.find("trace 103 ") != std::string::npos &&
              text.substr(text.size() - 4) == "end\n",
          "written results have the wrong layout");

    check_throws([] { parse("acuppa-lander 2"); }, "acuppa-rocket", "lander batch accepted");
    check_throws([] { parse("acuppa-rocket 2 threads 1"); }, "version", "bad version accepted");
    check_throws([] { parse("acuppa-rocket 1 threads 1 scenarios 1 scenario "
                            "40 15 0 0 0 0 0 0 0 0 0 3"); },
                 "out of bounds", "a start outside the bounds accepted");
    check_throws([] { parse("acuppa-rocket 1 threads 1 scenarios 1 scenario "
                            "0 15 0 0 0 0 1.5 0 0 0 0 3"); },
                 "tilted", "a start tilted over 1 rad accepted");
    check_throws([] { parse("acuppa-rocket 1 threads 1 scenarios 1 scenario "
                            "0 15 0 0 0 0 0 0 0 0 0 0"); },
                 "gust period", "a zero gust period accepted");
    check_throws([] { parse("acuppa-rocket 1 threads 1 scenarios 1 scenario 0 15 0"); },
                 "scenario vx", "a short scenario accepted");
    check_throws([] { parse(batch_text("genome 1 0 1\nnode 0 tanh sum 0 1 1 -12 0.5\n", 1)); },
                 "genome 1", "a link from a missing input accepted");

    if (failures) {
        std::fprintf(stderr, "%d rocket check(s) failed\n", failures);
        return 1;
    }
    std::puts("rocket checks passed");
    return 0;
}
