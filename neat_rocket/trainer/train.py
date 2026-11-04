"""Evolves gimbal-rocket controllers with neat-python and publishes each generation
to the ACuppa viewer.

    .tools/neat-venv/bin/python neat_rocket/trainer/train.py --generations 150

Every generation flies 8 new random scenarios (start state, steady wind, and
gusts drawn from --seed and the generation number) in one rocket_runner call.
Scenarios start easy, above the pad in calm air, and grow toward full size
whenever the champion lands at least 6 of its 8 flights (see --ramp-step).
Afterwards the champion and the best member of each species are replayed on
3 of those flights, from different starts, and the result is written to
<out>/gen_NNNN.lua with <out>/status.lua listing the available generations.
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
DEFAULT_RUNNER = os.path.join(os.path.dirname(LAB), "build-rocket", "rocket_runner")


class ViewerPublisher(neat.reporting.BaseReporter):
    def __init__(self, args, config, population):
        self.args, self.config, self.population = args, config, population
        self.generation = 0
        self.scenarios = []
        self.level = 0.0 if args.ramp_step > 0 else 1.0
        self.landing_rate = 0.0
        self.history = []
        self.rate_history = []
        self.published = []
        self.started = time.monotonic()

    def start_generation(self, generation):
        self.generation = generation + 1
        self.scenarios = lab.random_scenarios(self.args.seed, self.generation,
                                              level=self.level)
        self.started = time.monotonic()

    def post_evaluate(self, config, population, species_set, best):
        self.history.append(best.fitness)
        self.rate_history.append(self.landing_rate)
        picks = lab.view_landers(population, species_set, best)
        flights, landed, node_keys = [], 0, None
        for index in lab.view_flight_indices(self.generation):
            text = lab.encode_batch(((g.key, g, True) for _role, g, _sid in picks), config,
                                    self.scenarios, trace_scenario=index,
                                    threads=self.args.threads)
            results, traces = lab.run_batch(self.args.runner, text)
            landed = results[best.key][1]
            flights.append(lab.build_flight(index, self.scenarios[index], picks, traces,
                                            best.key))
            node_keys = traces[best.key]["node_keys"]
        run = lab.build_run(self.generation, flights, node_keys, config, population,
                            species_set, best, landed, self.landing_rate, self.history,
                            self.rate_history, self.level)
        name = f"gen_{self.generation:04d}.lua"
        lab.write_lua(os.path.join(self.args.out, name), run)
        self.published.append(self.generation)
        lab.write_lua(os.path.join(self.args.out, "status.lua"),
                      {"format": 1, "latest": self.generation,
                       "generations": self.published, "running": True})
        print(f"gen {self.generation:4d}  best {best.fitness:7.2f}  landed {landed:2d}/{lab.SCENARIO_COUNT}  "
              f"population {100 * self.landing_rate:5.1f}%  difficulty {self.level:4.2f}  "
              f"species {len(species_set.species):2d}  nodes {len(best.nodes):2d}  "
              f"conns {sum(c.enabled for c in best.connections.values()):3d}", flush=True)
        if self.args.ramp_step > 0:
            self.level = lab.next_difficulty(self.level, landed / lab.SCENARIO_COUNT,
                                             self.args.ramp_step)
        if self.args.pace > 0:
            remaining = self.args.pace - (time.monotonic() - self.started)
            if remaining > 0:
                time.sleep(remaining)

    def finish(self):
        if self.published:
            lab.write_lua(os.path.join(self.args.out, "status.lua"),
                          {"format": 1, "latest": self.published[-1],
                           "generations": self.published, "running": False})


def evaluator(args, publisher):
    def evaluate(genomes, config):
        text = lab.encode_batch(((key, genome, False) for key, genome in genomes), config,
                                publisher.scenarios, threads=args.threads)
        results, _ = lab.run_batch(args.runner, text)
        landed = 0
        for key, genome in genomes:
            genome.fitness, count = results[key]
            landed += count
        publisher.landing_rate = landed / max(1, len(genomes) * lab.SCENARIO_COUNT)
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
    parser.add_argument("--no-reflex", action="store_true",
                        help="start from fully random networks without the innate balance "
                             "and descent links")
    parser.add_argument("--ramp-step", type=float, default=0.05,
                        help="difficulty added after a generation whose champion lands 6 of 8 "
                             "(0 starts at full difficulty)")
    args = parser.parse_args(argv)
    if args.generations < 1:
        parser.error("--generations must be at least 1")
    if not os.path.isfile(args.runner):
        parser.error(f"runner not found: {args.runner} (run neat_rocket/configure.sh)")
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
    if not args.no_reflex:
        lab.seed_reflex(population.population)
    publisher = ViewerPublisher(args, config, population)
    population.add_reporter(publisher)
    try:
        winner = population.run(evaluator(args, publisher), args.generations)
    finally:
        publisher.finish()
    print(f"best genome {winner.key}: fitness {winner.fitness:.2f}, "
          f"{len(winner.nodes)} nodes, "
          f"{sum(c.enabled for c in winner.connections.values())} enabled connections")
    return 0


if __name__ == "__main__":
    sys.exit(main())
