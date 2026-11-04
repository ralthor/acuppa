#pragma once

#include "rocket.hpp"
#include "network.hpp"

#include <iosfwd>
#include <vector>

namespace neat_rocket {

// Text batch protocol between trainer/train.py and rocket_runner; see README.md.
struct GenomeSpec {
    long long key = 0;
    bool trace = false;
    std::vector<NodeSpec> nodes;
};

struct Batch {
    int threads = 0;  // 0 uses every hardware thread.
    std::vector<Scenario> scenarios;
    int trace_scenario = 0;  // Index into scenarios.
    std::vector<GenomeSpec> genomes;
};

struct GenomeResult {
    double fitness = 0;  // Mean over all scenarios.
    int landed = 0;
    std::vector<FlightResult> episodes;
    // Filled for traced genomes: the trace scenario replayed with per-frame
    // node values in `node_keys` order.
    FlightResult traced;
    std::vector<Frame> frames;
    std::vector<int> node_keys;
    std::vector<std::vector<double>> activations;
};

Batch read_batch(std::istream& in);  // Throws std::invalid_argument naming the problem.
GenomeResult evaluate(const GenomeSpec& genome, const std::vector<Scenario>& scenarios,
                      int trace_scenario);
std::vector<GenomeResult> evaluate_batch(const Batch& batch);
void write_results(std::ostream& out, const Batch& batch, const std::vector<GenomeResult>& results);

}  // namespace neat_rocket
