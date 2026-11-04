"""Evolves lander controllers with neat-python and publishes each generation
to the ACuppa viewer.

    .tools/neat-venv/bin/python neat_lab/trainer/train.py --generations 150

Every generation is scored by one lander_runner call. Afterwards the champion
and the best member of each species are replayed on one scenario (rotating
through all eight), and the result is written to <out>/gen_NNNN.lua with
<out>/status.lua listing the available generations.
"""

import argparse
import glob
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lab  # noqa: E402

import neat  # noqa: E402

LAB = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_RUNNER = os.path.join(os.path.dirname(LAB), "build-neat", "lander_runner")


class ViewerPublisher(neat.reporting.BaseReporter):
    def __init__(self, args, config, population):
        self.args, self.config, self.population = args, config, population
        self.generation = 0
        self.history = []
        self.published = []
        self.started = time.monotonic()

    def start_generation(self, generation):
        self.generation = generation + 1
        self.started = time.monotonic()

    def post_evaluate(self, config, population, species_set, best):
        self.history.append(best.fitness)
        scenario = (self.generation - 1) % lab.SCENARIO_COUNT
        picks = lab.view_landers(population, species_set, best)
        text = lab.encode_batch(((g.key, g, True) for _role, g, _sid in picks), config,
                                trace_scenario=scenario, threads=self.args.threads)
        results, traces = lab.run_batch(self.args.runner, text)
        landed = results[best.key][1]
        run = lab.build_run(self.generation, scenario, config, population, species_set,
                            best, picks, traces, landed, self.history)
        name = f"gen_{self.generation:04d}.lua"
        lab.write_lua(os.path.join(self.args.out, name), run)
        self.published.append(self.generation)
        lab.write_lua(os.path.join(self.args.out, "status.lua"),
                      {"format": 1, "latest": self.generation,
                       "generations": self.published, "running": True})
        print(f"gen {self.generation:4d}  best {best.fitness:7.2f}  landed {landed}/8  "
              f"species {len(species_set.species):2d}  nodes {len(best.nodes):2d}  "
              f"conns {sum(c.enabled for c in best.connections.values()):3d}", flush=True)
        if self.args.pace > 0:
            remaining = self.args.pace - (time.monotonic() - self.started)
            if remaining > 0:
                time.sleep(remaining)

    def finish(self):
        if self.published:
            lab.write_lua(os.path.join(self.args.out, "status.lua"),
                          {"format": 1, "latest": self.published[-1],
                           "generations": self.published, "running": False})


def evaluator(args):
    def evaluate(genomes, config):
        text = lab.encode_batch(((key, genome, False) for key, genome in genomes), config,
                                threads=args.threads)
        results, _ = lab.run_batch(args.runner, text)
        for key, genome in genomes:
            genome.fitness = results[key][0]
    return evaluate


def parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--generations", type=int, default=150)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--config", default=os.path.join(LAB, "trainer", "neat.cfg"))
    parser.add_argument("--runner", default=DEFAULT_RUNNER)
    parser.add_argument("--out", default=os.path.join(LAB, "viewer", "runs"))
    parser.add_argument("--threads", type=int, default=0, help="0 uses every CPU thread")
    parser.add_argument("--pace", type=float, default=0.0,
                        help="minimum seconds per generation, to watch each one live")
    parser.add_argument("--pop-size", type=int, help="override the config population size")
    args = parser.parse_args(argv)
    if args.generations < 1:
        parser.error("--generations must be at least 1")
    if not os.path.isfile(args.runner):
        parser.error(f"runner not found: {args.runner} (run neat_lab/configure.sh)")
    return args


def main(argv=None):
    args = parse_args(argv)
    config = lab.load_config(args.config)
    if args.pop_size:
        config.pop_size = args.pop_size
    os.makedirs(args.out, exist_ok=True)
    # Clear only files this trainer writes, so the viewer never mixes runs.
    for path in glob.glob(os.path.join(args.out, "gen_*.lua")):
        os.remove(path)
    population = neat.Population(config, seed=args.seed)
    publisher = ViewerPublisher(args, config, population)
    population.add_reporter(publisher)
    try:
        winner = population.run(evaluator(args), args.generations)
    finally:
        publisher.finish()
    print(f"best genome {winner.key}: fitness {winner.fitness:.2f}, "
          f"{len(winner.nodes)} nodes, "
          f"{sum(c.enabled for c in winner.connections.values())} enabled connections")
    return 0


if __name__ == "__main__":
    sys.exit(main())
