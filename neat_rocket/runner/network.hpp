#pragma once

#include <string>
#include <vector>

namespace neat_rocket {

// One neat-python FeedForwardNetwork node evaluation, in evaluation order.
struct NodeSpec {
    int key = 0;
    std::string activation = "tanh";
    std::string aggregation = "sum";
    double bias = 0;
    double response = 1;
    std::vector<std::pair<int, double>> links;  // (input node key, weight)
};

// Mirrors neat.nn.FeedForwardNetwork.activate: inputs use keys -1..-n, outputs
// keys 0..m-1, and outputs without an evaluation stay 0.
class Network {
public:
    Network(int input_count, int output_count, const std::vector<NodeSpec>& nodes);
    std::vector<double> activate(const std::vector<double>& inputs);
    // Node keys in a stable order (inputs, outputs, hidden by evaluation) and
    // their values after the last activate().
    const std::vector<int>& keys() const { return keys_; }
    const std::vector<double>& values() const { return values_; }

private:
    struct Eval {
        int slot;
        int activation;
        double bias, response;
        std::vector<std::pair<int, double>> links;  // (slot, weight)
    };
    int inputs_, outputs_;
    std::vector<int> keys_;
    std::vector<double> values_;
    std::vector<Eval> evals_;
};

}  // namespace neat_rocket
