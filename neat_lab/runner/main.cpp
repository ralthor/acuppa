// Reads one genome batch on stdin, evaluates every genome on all lander
// scenarios in parallel, and writes fitness results and traces to stdout.
#include "protocol.hpp"

#include <cstdio>
#include <exception>
#include <iostream>
#include <sstream>

int main(int argc, char**) {
    if (argc != 1) {
        std::fprintf(stderr, "usage: lander_runner < batch.txt\n");
        return 2;
    }
    try {
        std::ios::sync_with_stdio(false);
        const auto batch = neat_lab::read_batch(std::cin);
        const auto results = neat_lab::evaluate_batch(batch);
        std::ostringstream out;
        neat_lab::write_results(out, batch, results);
        std::cout << out.str() << std::flush;
        return std::cout ? 0 : 1;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "lander_runner: %s\n", error.what());
        return 2;
    }
}
