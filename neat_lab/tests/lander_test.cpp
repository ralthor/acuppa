#include "protocol.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace neat_lab;

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

Controller constant(double main, double side) {
    return [=](const std::array<double, kInputCount>&) { return Thrust{main, side}; };
}

// Hand-tuned controller proving the task can be landed with the fuel given:
// a braking-distance descent profile plus a velocity loop toward the pad.
Thrust oracle(const std::array<double, kInputCount>& in) {
    const double x = in[0] * 10, h = in[1] * 10, vx = in[2] * 5, vy = in[3] * 5;
    const double want_vx = std::clamp(-0.9 * x, -4.0, 4.0);
    const double brake = std::abs(x) < 0.6 && std::abs(vx) < 0.6 ? 3.0 : 0.6;
    const double want_vy = -std::clamp(std::sqrt(2.0 * brake * std::max(0.0, h - 0.2)), 0.6, 7.0);
    return {std::clamp(0.5 + 0.6 * (want_vy - vy), 0.0, 1.0),
            std::clamp(1.2 * (want_vx - vx), -1.0, 1.0)};
}

std::string scenario_text() {
    std::ostringstream out;
    out.precision(9);
    out << "scenarios " << sample_scenarios().size() << "\n";
    for (const auto& s : sample_scenarios())
        out << "scenario " << s.x << ' ' << s.y << ' ' << s.vx << ' ' << s.vy << ' ' << s.wind
            << ' ' << s.gust << ' ' << s.gust_period << "\n";
    return out.str();
}

std::string batch_text(const std::string& genomes, int count, int threads = 1) {
    return "acuppa-lander 2 threads " + std::to_string(threads) + "\n" + scenario_text() +
           "trace_scenario 2 genomes " + std::to_string(count) + "\n" + genomes + "end\n";
}

Batch parse(const std::string& text) {
    std::istringstream in(text);
    return read_batch(in);
}

std::string random_genome(std::mt19937& rng, long long key, bool trace) {
    std::normal_distribution<double> weight(0.0, 1.2);
    std::ostringstream out;
    out.precision(17);
    // One hidden node fed by every input, plus direct input links to both outputs.
    out << "genome " << key << ' ' << (trace ? 1 : 0) << " 3\n";
    out << "node 7 tanh sum " << weight(rng) << " 1 5";
    for (int i = 1; i <= 5; ++i) out << ' ' << -i << ' ' << weight(rng);
    out << "\n";
    for (int o = 0; o < 2; ++o) {
        out << "node " << o << " tanh sum " << weight(rng) << " 1 6 7 " << weight(rng);
        for (int i = 1; i <= 5; ++i) out << ' ' << -i << ' ' << weight(rng);
        out << "\n";
    }
    return out.str();
}

}  // namespace

int main() {
    const auto& all = sample_scenarios();

    // Free fall from 12 m hits at about sqrt(2 * 9.81 * 11.7) = 15 m/s.
    auto fall = run_episode(all[0], constant(0, 0));
    check(fall.outcome == Outcome::Crashed && fall.impact > 12 && fall.impact < 18,
          "free fall should crash near 15 m/s, got " + std::to_string(fall.impact));
    check(fall.fuel == 1.0f && fall.fitness < 60, "free fall used fuel or scored high");

    // Full thrust is twice the weight, so the lander climbs out of bounds.
    auto climb = run_episode(all[2], constant(1, 0));
    check(climb.outcome == Outcome::OutOfBounds && climb.fitness == 0,
          "full thrust should leave the bounds with zero fitness");

    // Quarter thrust cannot hover; fuel falls at 0.25 / (kFuelSeconds * 60) per tick.
    auto weak = run_episode(all[2], constant(0.25, 0));
    const float expected = 1.0f - 0.25f * float(weak.ticks - 1) / (kFuelSeconds * 60.0f);
    check(weak.outcome == Outcome::Crashed && std::abs(weak.fuel - expected) < 0.01f,
          "quarter thrust fuel use is wrong: " + std::to_string(weak.fuel) + " vs " +
              std::to_string(expected));

    // Exhausted fuel cuts the engines even when the controller asks for thrust.
    // Hovering with alternating side jets burns a full tank in kFuelSeconds.
    int tick = 0;
    auto empty = run_episode({0, 12, 0, 0}, [&](const std::array<double, kInputCount>&) {
        return Thrust{0.5, (tick++ % 2) ? 1.0 : -1.0};
    });
    check(empty.fuel == 0.0f && empty.outcome == Outcome::Crashed,
          "hover until empty should end in a crash once fuel is gone");

    // Side jets push toward +X when positive.
    std::vector<Frame> trace;
    auto drift = run_episode({0, 12, 0, 0}, constant(0.5, 1), &trace);
    check(trace.size() > 10 && trace[10].x > 0.1f, "positive side thrust should move +X");
    check(trace.back().main == 0 && trace.back().side == 0, "final frame keeps engine output");
    check(int(trace.size()) == (drift.ticks + kTraceStride - 1) / kTraceStride + 1,
          "trace should hold one frame per stride plus the final state");

    // The task is solvable: a simple controller lands every scenario.
    for (size_t i = 0; i < all.size(); ++i) {
        auto result = run_episode(all[i], oracle);
        check(result.outcome == Outcome::Landed && result.fitness > 180,
              "oracle failed scenario " + std::to_string(i) + ": " +
                  outcome_name(result.outcome) + " impact " + std::to_string(result.impact) +
                  " x " + std::to_string(result.end_x) + " fuel " + std::to_string(result.fuel));
    }

    // The trainer's random ranges stay landable but not trivial: the oracle
    // lands at least 80% of random scenarios (wind up to 4 m/s^2, gusts to 2.5).
    std::mt19937 weather(11);
    auto pick = [&](float low, float high) {
        return std::uniform_real_distribution<float>(low, high)(weather);
    };
    int landed = 0, tried = 300;
    for (int i = 0; i < tried; ++i) {
        Scenario s{pick(-12, 12), pick(9, 15), pick(-3, 3), pick(-2, 0.5f),
                   pick(-4, 4),   pick(0, 2.5f), pick(1.5f, 5)};
        if (run_episode(s, oracle).outcome == Outcome::Landed) ++landed;
    }
    check(landed >= tried * 8 / 10,
          "oracle landed only " + std::to_string(landed) + " of 300 random scenarios");

    // Wind alone pushes a lander sideways, and the score parts add up to the fitness.
    trace.clear();
    auto windy = run_episode({0, 12, 0, 0, 2.0f, 0.0f, 3.0f}, constant(0.5, 0), &trace);
    check(trace.size() > 30 && trace[30].x > 0.5f && std::abs(trace[30].wind - 2.0f) < 1e-4f,
          "steady wind should drift the lander toward +X and be recorded");
    std::vector<Frame> gusty;
    run_episode({0, 12, 0, 0, 0.0f, 1.0f, 2.0f}, constant(0.5, 0), &gusty);
    check(gusty.size() > 16 && std::abs(gusty[15].wind - 1.0f) < 0.02f &&
              std::abs(gusty[30].wind) < 0.02f,
          "gusts should follow a sine wave with the given period");
    check(std::abs(windy.score.total() - windy.fitness) < 1e-9, "score parts must sum to fitness");
    const auto good = run_episode(all[0], oracle);
    check(good.score.landing == 100 && good.score.speed > 0 && good.score.fuel > 0 &&
              good.touch_time > 1 && good.touch_time < 10,
          "a landing should earn landing, fuel, and speed parts");
    auto slow = [](const std::array<double, kInputCount>& in) {
        auto t = oracle(in);
        if (in[1] > 0.05) t.main = std::max(t.main, 0.52);  // Hover-heavy descent.
        return t;
    };
    const auto lazy = run_episode(all[0], slow);
    check(lazy.outcome != Outcome::Landed || lazy.score.speed < good.score.speed,
          "a slower landing should earn less for speed");

    // Repeating an episode gives bit-identical results.
    auto again = run_episode(all[0], constant(0, 0));
    check(again.fitness == fall.fitness && again.ticks == fall.ticks, "episode not deterministic");

    // Network evaluation matches neat-python: tanh(2.5 * (bias + response * sum)).
    Network net(5, 2, {{0, "tanh", "sum", 0.1, 2.0, {{-1, 0.5}, {-3, -1.0}}},
                       {1, "sigmoid", "sum", -0.2, 1.0, {{-2, 1.0}}}});
    auto out = net.activate({0.4, 0.2, 0.3, 0, 0});
    check(std::abs(out[0] - std::tanh(2.5 * (0.1 + 2.0 * (0.2 - 0.3)))) < 1e-12,
          "tanh output differs from neat-python");
    check(std::abs(out[1] - 1.0 / (1.0 + std::exp(-5.0 * (-0.2 + 0.2)))) < 1e-12,
          "sigmoid output differs from neat-python");
    Network bare(5, 2, {});
    check(bare.activate({1, 1, 1, 1, 1}) == std::vector<double>{0, 0},
          "unconnected outputs should stay 0");
    auto thrust = thrust_from_outputs(-1, 2);
    check(thrust.main == 0 && thrust.side == 1, "outputs should clamp to thrust ranges");

    check_throws([] { Network(5, 2, {{0, "softmax", "sum", 0, 1, {}}}); }, "activation",
                 "unknown activation accepted");
    check_throws([] { Network(5, 2, {{0, "tanh", "max", 0, 1, {}}}); }, "aggregation",
                 "unknown aggregation accepted");
    check_throws([] { Network(5, 2, {{0, "tanh", "sum", 0, 1, {{9, 1}}}}); }, "unevaluated",
                 "link from a missing node accepted");
    check_throws([] { Network(5, 2, {{0, "tanh", "sum", NAN, 1, {}}}); }, "finite",
                 "NaN bias accepted");
    check_throws([] { Network(5, 2, {{0, "tanh", "sum", 0, 1, {}}, {0, "tanh", "sum", 0, 1, {}}}); },
                 "twice", "duplicate node accepted");
    check_throws([] { Network(5, 2, {{-1, "tanh", "sum", 0, 1, {}}}); }, "input",
                 "evaluating an input accepted");

    // Batch protocol: parallel evaluation equals serial evaluation exactly.
    std::mt19937 rng(42);
    std::string genomes;
    for (int i = 0; i < 24; ++i) genomes += random_genome(rng, 100 + i, i == 3);
    auto serial = evaluate_batch(parse(batch_text(genomes, 24, 1)));
    auto parallel = evaluate_batch(parse(batch_text(genomes, 24, 8)));
    bool same = true;
    for (size_t i = 0; i < serial.size(); ++i)
        same = same && serial[i].fitness == parallel[i].fitness &&
               serial[i].landed == parallel[i].landed;
    check(same, "parallel evaluation differs from serial");
    const auto& traced = serial[3];
    check(!traced.frames.empty() && traced.frames.size() == traced.activations.size() &&
              traced.node_keys.size() == 8 && traced.activations[0].size() == 8,
          "trace should hold a frame and 8 node values (5 in, 2 out, 1 hidden) per frame");
    check(traced.traced.fitness == traced.episodes[2].fitness,
          "traced replay should reproduce the scored trace scenario");
    check(serial[0].frames.empty(), "untraced genomes should carry no frames");

    std::ostringstream written;
    auto batch = parse(batch_text(genomes, 24, 2));
    write_results(written, batch, serial);
    const auto text = written.str();
    check(text.rfind("acuppa-lander 2\n", 0) == 0 && text.find("result 100 ") != std::string::npos &&
              text.find("trace 103 ") != std::string::npos && text.size() > 5 &&
              text.substr(text.size() - 4) == "end\n",
          "written results have the wrong layout");

    check_throws([] { parse("hello 1"); }, "acuppa-lander", "bad header accepted");
    check_throws([] { parse("acuppa-lander 1 threads 1"); }, "version", "old version accepted");
    check_throws([] { parse(batch_text("", 1)); }, "genome", "missing genome accepted");
    check_throws([] { parse(batch_text("", 0) + "extra"); }, "after", "trailing data accepted");
    check_throws([] { parse("acuppa-lander 2 threads 1 scenarios 1 scenario 0 9 0 0 0 0 3 "
                            "trace_scenario 1 genomes 0 end"); },
                 "trace scenario", "out-of-range trace scenario accepted");
    check_throws([] { parse(batch_text("genome 1 0 1\nnode 0 tanh sum 0 1 1 -9 0.5\n", 1)); },
                 "genome 1", "error should name the genome");
    check_throws([] { parse(batch_text("genome 1 0 1\nnode 0 tanh sum nan 1 0\n", 1)); }, "bias",
                 "nan in text accepted");
    check_throws([] { parse("acuppa-lander 2 threads 1 scenarios 0"); }, "scenario count",
                 "an empty scenario list accepted");
    check_throws([] { parse("acuppa-lander 2 threads 1 scenarios 1 scenario 40 9 0 0 0 0 3"); },
                 "out of bounds", "a start outside the bounds accepted");
    check_throws([] { parse("acuppa-lander 2 threads 1 scenarios 1 scenario 0 9 0 0 0 0 0"); },
                 "gust period", "a zero gust period accepted");
    check_throws([] { parse("acuppa-lander 2 threads 1 scenarios 1 scenario 0 9 0 0 inf 0 3"); },
                 "wind", "an infinite wind accepted");
    check_throws([] { parse(batch_text("genome 1 0 1\nnode 0 tanh sum 0 1 2 -1 0.5\n", 1)); },
                 "link input", "truncated links accepted");

    if (failures) {
        std::fprintf(stderr, "%d lander check(s) failed\n", failures);
        return 1;
    }
    std::puts("lander checks passed");
    return 0;
}
