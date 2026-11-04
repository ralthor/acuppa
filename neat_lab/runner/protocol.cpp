#include "protocol.hpp"

#include <atomic>
#include <cmath>
#include <cstdio>
#include <exception>
#include <istream>
#include <mutex>
#include <ostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace neat_lab {
namespace {

constexpr int kMaxGenomes = 100000;
constexpr int kMaxNodes = 10000;
constexpr int kMaxLinks = 100000;

class Reader {
public:
    explicit Reader(std::istream& in) : in_(in) {}
    std::string word(const char* what) {
        std::string value;
        if (!(in_ >> value)) throw std::invalid_argument(std::string("missing ") + what);
        return value;
    }
    void expect(const char* keyword) {
        const auto value = word(keyword);
        if (value != keyword)
            throw std::invalid_argument(std::string("expected '") + keyword + "', got '" +
                                        value + "'");
    }
    long long integer(const char* what, long long low, long long high) {
        const auto text = word(what);
        size_t used = 0;
        long long value = 0;
        try { value = std::stoll(text, &used); } catch (const std::exception&) { used = 0; }
        if (used != text.size() || value < low || value > high)
            throw std::invalid_argument(std::string("invalid ") + what + ": " + text);
        return value;
    }
    double number(const char* what) {
        const auto text = word(what);
        size_t used = 0;
        double value = 0;
        try { value = std::stod(text, &used); } catch (const std::exception&) { used = 0; }
        if (used != text.size() || !std::isfinite(value))
            throw std::invalid_argument(std::string("invalid ") + what + ": " + text);
        return value;
    }

private:
    std::istream& in_;
};

}  // namespace

Batch read_batch(std::istream& in) {
    Reader read(in);
    read.expect("acuppa-lander");
    if (read.integer("protocol version", 0, 1000) != 1)
        throw std::invalid_argument("unsupported protocol version");
    Batch batch;
    read.expect("threads");
    batch.threads = int(read.integer("thread count", 0, 1024));
    read.expect("trace_scenario");
    batch.trace_scenario = int(read.integer("trace scenario", 0, kScenarioCount - 1));
    read.expect("genomes");
    const auto count = read.integer("genome count", 0, kMaxGenomes);
    for (long long g = 0; g < count; ++g) {
        GenomeSpec genome;
        read.expect("genome");
        genome.key = read.integer("genome key", -(1LL << 50), 1LL << 50);
        genome.trace = read.integer("trace flag", 0, 1) == 1;
        const auto nodes = read.integer("node count", 0, kMaxNodes);
        for (long long n = 0; n < nodes; ++n) {
            NodeSpec node;
            read.expect("node");
            node.key = int(read.integer("node key", -kMaxNodes, 1LL << 30));
            node.activation = read.word("activation");
            node.aggregation = read.word("aggregation");
            node.bias = read.number("bias");
            node.response = read.number("response");
            const auto links = read.integer("link count", 0, kMaxLinks);
            for (long long l = 0; l < links; ++l) {
                const int from = int(read.integer("link input", -kMaxNodes, 1LL << 30));
                node.links.emplace_back(from, read.number("link weight"));
            }
            genome.nodes.push_back(std::move(node));
        }
        // Validate structure now so errors name the genome instead of a worker.
        try {
            Network(kInputCount, kOutputCount, genome.nodes);
        } catch (const std::exception& error) {
            throw std::invalid_argument("genome " + std::to_string(genome.key) + ": " +
                                        error.what());
        }
        batch.genomes.push_back(std::move(genome));
    }
    read.expect("end");
    std::string extra;
    if (in >> extra) throw std::invalid_argument("unexpected data after 'end': " + extra);
    return batch;
}

GenomeResult evaluate(const GenomeSpec& genome, int trace_scenario) {
    Network network(kInputCount, kOutputCount, genome.nodes);
    const Controller controller = [&](const std::array<double, kInputCount>& inputs) {
        const auto out = network.activate({inputs.begin(), inputs.end()});
        return thrust_from_outputs(out[0], out[1]);
    };
    GenomeResult result;
    for (const auto& scenario : scenarios()) {
        result.episodes.push_back(run_episode(scenario, controller));
        result.fitness += result.episodes.back().fitness;
        if (result.episodes.back().outcome == Outcome::Landed) ++result.landed;
    }
    result.fitness /= kScenarioCount;
    if (genome.trace) {
        Network replay(kInputCount, kOutputCount, genome.nodes);
        const Controller traced = [&](const std::array<double, kInputCount>& inputs) {
            const auto out = replay.activate({inputs.begin(), inputs.end()});
            return thrust_from_outputs(out[0], out[1]);
        };
        result.node_keys = replay.keys();
        result.traced = run_episode(scenarios()[trace_scenario], traced, &result.frames,
                                    [&] { result.activations.push_back(replay.values()); });
    }
    return result;
}

std::vector<GenomeResult> evaluate_batch(const Batch& batch) {
    std::vector<GenomeResult> results(batch.genomes.size());
    unsigned threads = batch.threads > 0 ? unsigned(batch.threads)
                                         : std::max(1u, std::thread::hardware_concurrency());
    threads = std::min<unsigned>(threads, std::max<size_t>(1, batch.genomes.size()));
    std::atomic<size_t> next{0};
    std::exception_ptr failure;
    std::mutex failure_lock;
    auto work = [&] {
        for (size_t i = next++; i < batch.genomes.size(); i = next++) {
            try {
                results[i] = evaluate(batch.genomes[i], batch.trace_scenario);
            } catch (...) {
                std::lock_guard<std::mutex> guard(failure_lock);
                if (!failure) failure = std::current_exception();
            }
        }
    };
    std::vector<std::thread> pool;
    for (unsigned t = 1; t < threads; ++t) pool.emplace_back(work);
    work();
    for (auto& thread : pool) thread.join();
    if (failure) std::rethrow_exception(failure);
    return results;
}

void write_results(std::ostream& out, const Batch& batch, const std::vector<GenomeResult>& results) {
    char line[128];
    out << "acuppa-lander 1\n";
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        std::snprintf(line, sizeof line, "%.9g %d", r.fitness, r.landed);
        out << "result " << batch.genomes[i].key << ' ' << line << '\n';
        if (!batch.genomes[i].trace) continue;
        std::snprintf(line, sizeof line, "%s %.4f %.4f %zu %zu", outcome_name(r.traced.outcome),
                      r.traced.impact, r.traced.fitness, r.frames.size(), r.node_keys.size());
        out << "trace " << batch.genomes[i].key << ' ' << line;
        for (int key : r.node_keys) out << ' ' << key;
        out << '\n';
        for (size_t f = 0; f < r.frames.size(); ++f) {
            const auto& fr = r.frames[f];
            std::snprintf(line, sizeof line, "f %.4f %.4f %.4f %.4f %.4f %.3f %.3f %.3f", fr.x,
                          fr.y, fr.rx, fr.ry, fr.rz, fr.main, fr.side, fr.fuel);
            out << line;
            for (double value : r.activations[f]) {
                std::snprintf(line, sizeof line, " %.3f", value);
                out << line;
            }
            out << '\n';
        }
    }
    out << "end\n";
}

}  // namespace neat_lab
