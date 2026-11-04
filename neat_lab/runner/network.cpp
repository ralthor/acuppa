#include "network.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_map>

namespace neat_lab {
namespace {

enum Activation { Tanh, Sigmoid, Relu, Identity, Clamped };

int activation_id(const std::string& name) {
    if (name == "tanh") return Tanh;
    if (name == "sigmoid") return Sigmoid;
    if (name == "relu") return Relu;
    if (name == "identity") return Identity;
    if (name == "clamped") return Clamped;
    throw std::invalid_argument("unsupported activation: " + name);
}

// Same formulas and clamps as neat/activations.py.
double activate_value(int activation, double z) {
    switch (activation) {
    case Tanh: return std::tanh(std::max(-60.0, std::min(60.0, 2.5 * z)));
    case Sigmoid: return 1.0 / (1.0 + std::exp(-std::max(-60.0, std::min(60.0, 5.0 * z))));
    case Relu: return z > 0.0 ? z : 0.0;
    case Identity: return z;
    default: return std::max(-1.0, std::min(1.0, z));
    }
}

}  // namespace

Network::Network(int input_count, int output_count, const std::vector<NodeSpec>& nodes)
    : inputs_(input_count), outputs_(output_count) {
    if (input_count <= 0 || output_count <= 0)
        throw std::invalid_argument("network needs inputs and outputs");
    std::unordered_map<int, int> slot;
    for (int i = 1; i <= input_count; ++i) {
        slot[-i] = int(keys_.size());
        keys_.push_back(-i);
    }
    for (int i = 0; i < output_count; ++i) {
        slot[i] = int(keys_.size());
        keys_.push_back(i);
    }
    for (const auto& node : nodes) {
        if (node.key < 0) throw std::invalid_argument("input nodes cannot be evaluated");
        if (node.aggregation != "sum")
            throw std::invalid_argument("unsupported aggregation: " + node.aggregation);
        if (!std::isfinite(node.bias) || !std::isfinite(node.response))
            throw std::invalid_argument("node bias and response must be finite");
        if (!slot.count(node.key)) {
            slot[node.key] = int(keys_.size());
            keys_.push_back(node.key);
        }
    }
    std::vector<bool> evaluated(keys_.size(), false);
    for (int i = 0; i < input_count; ++i) evaluated[i] = true;
    for (const auto& node : nodes) {
        Eval eval{slot.at(node.key), activation_id(node.activation), node.bias, node.response, {}};
        if (evaluated[eval.slot])
            throw std::invalid_argument("node evaluated twice: " + std::to_string(node.key));
        for (const auto& [from, weight] : node.links) {
            const auto it = slot.find(from);
            if (it == slot.end() || !evaluated[it->second])
                throw std::invalid_argument("link from unevaluated node " + std::to_string(from) +
                                            " to " + std::to_string(node.key));
            if (!std::isfinite(weight)) throw std::invalid_argument("link weight must be finite");
            eval.links.emplace_back(it->second, weight);
        }
        evaluated[eval.slot] = true;
        evals_.push_back(std::move(eval));
    }
    values_.assign(keys_.size(), 0.0);
}

std::vector<double> Network::activate(const std::vector<double>& inputs) {
    if (int(inputs.size()) != inputs_) throw std::invalid_argument("wrong input count");
    std::copy(inputs.begin(), inputs.end(), values_.begin());
    for (const auto& eval : evals_) {
        double sum = 0;
        for (const auto& [from, weight] : eval.links) sum += values_[from] * weight;
        values_[eval.slot] = activate_value(eval.activation, eval.bias + eval.response * sum);
    }
    return {values_.begin() + inputs_, values_.begin() + inputs_ + outputs_};
}

}  // namespace neat_lab
