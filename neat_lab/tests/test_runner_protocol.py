"""Checks the lander_runner text protocol against neat-python itself.

usage: test_runner_protocol.py <lander_runner>
"""

import os
import random
import subprocess
import sys
import unittest

LAB = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(LAB, "trainer"))
import lab  # noqa: E402

import neat  # noqa: E402

RUNNER = None
SCENARIOS = lab.random_scenarios(seed=5, generation=1)


def mutated_population(config, count, rounds=12, seed=3):
    """Genomes with hidden nodes and mixed activations, like mid-training ones."""
    # Population sets up the innovation tracker that structural mutation needs.
    population = neat.Population(config, seed=seed)
    random.seed(seed)
    genomes = sorted(population.population.values(), key=lambda g: g.key)[:count]
    for genome in genomes:
        for _ in range(rounds):
            genome.mutate(config.genome_config)
    return genomes


class RunnerProtocolTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.config = lab.load_config(os.path.join(LAB, "trainer", "neat.cfg"))
        cls.genomes = mutated_population(cls.config, 20)

    def run_raw(self, text):
        return subprocess.run([RUNNER], input=text, capture_output=True, text=True, timeout=120)

    def test_results_for_every_genome_and_traces_when_asked(self):
        entries = [(g.key, g, g.key in (2, 5)) for g in self.genomes]
        results, traces = lab.run_batch(RUNNER, lab.encode_batch(entries, self.config,
                                                                 SCENARIOS, 2, 4))
        self.assertEqual(sorted(results), [g.key for g in self.genomes])
        self.assertEqual(sorted(traces), [2, 5])
        for fitness, landed in results.values():
            self.assertGreaterEqual(fitness, 0.0)
            self.assertTrue(0 <= landed <= 8)
        trace = traces[5]
        self.assertIn(trace["outcome"], {"landed", "off_pad", "crashed", "tipped",
                                         "timeout", "out_of_bounds"})
        self.assertEqual(trace["node_keys"][:7], [-1, -2, -3, -4, -5, 0, 1])
        self.assertEqual(len(trace["frames"]), len(trace["acts"]))
        self.assertEqual(len(trace["frames"][0]), lab.FRAME_FIELDS)
        self.assertAlmostEqual(sum(trace["score"].values()), trace["fitness"], delta=0.01)
        self.assertEqual(set(trace["score"]), set(lab.SCORE_PARTS))

    def test_weather_changes_the_result(self):
        genome = self.genomes[0]
        calm = [dict(s, wind=0.0, gust=0.0) for s in SCENARIOS]
        stormy = [dict(s, wind=3.5, gust=2.0) for s in SCENARIOS]
        first, _ = lab.run_batch(RUNNER, lab.encode_batch([(1, genome, False)], self.config, calm))
        second, _ = lab.run_batch(RUNNER, lab.encode_batch([(1, genome, False)], self.config,
                                                           stormy))
        self.assertNotEqual(first[1][0], second[1][0])

    def test_runner_network_matches_neat_python(self):
        entries = [(g.key, g, True) for g in self.genomes]
        _, traces = lab.run_batch(RUNNER, lab.encode_batch(entries, self.config, SCENARIOS, 2, 2))
        start = SCENARIOS[2]
        x, y, vx, vy = start["x"], start["y"], start["vx"], start["vy"]
        inputs = [x / 10, (y - 0.3) / 10, vx / 5, vy / 5, 1.0]
        hidden_seen = 0
        for genome in self.genomes:
            net = neat.nn.FeedForwardNetwork.create(genome, self.config)
            expected = net.activate(inputs)
            trace = traces[genome.key]
            first = dict(zip(trace["node_keys"], map(float, trace["acts"][0])))
            for output, value in zip((0, 1), expected):
                self.assertAlmostEqual(first[output], value, delta=6e-4,
                                       msg=f"genome {genome.key} output {output}")
            hidden_seen += len(trace["node_keys"]) - 7
        self.assertGreater(hidden_seen, 0, "fixture genomes should include hidden nodes")

    def test_same_batch_gives_same_fitness(self):
        text = lab.encode_batch([(g.key, g, False) for g in self.genomes], self.config, SCENARIOS)
        first, _ = lab.run_batch(RUNNER, text)
        second, _ = lab.run_batch(RUNNER, text.replace("threads 0", "threads 1"))
        self.assertEqual(first, second)

    def test_malformed_input_is_rejected_with_a_message(self):
        good = lab.encode_batch([(1, self.genomes[0], False)], self.config, SCENARIOS)
        cases = {
            "": "missing",
            "acuppa-lander 2 threads x": "thread count",
            "acuppa-lander 1 threads 1": "version",
            good.replace("scenarios 8", "scenarios 0", 1): "scenario count",
            good.replace("end\n", ""): "missing end",
            good.replace(" tanh ", " softmax ", 1): "activation",
            good + "junk": "after 'end'",
        }
        for text, fragment in cases.items():
            process = self.run_raw(text)
            self.assertEqual(process.returncode, 2, text[:40])
            self.assertIn(fragment, process.stderr, text[:40])
        self.assertEqual(self.run_raw("").stdout, "")
        args = subprocess.run([RUNNER, "extra"], capture_output=True, text=True)
        self.assertEqual(args.returncode, 2)
        with self.assertRaises(lab.RunnerError):
            lab.run_batch(RUNNER, "acuppa-lander 2")

    def test_parse_results_rejects_damaged_output(self):
        with self.assertRaises(lab.RunnerError):
            lab.parse_results("hello\nend")
        with self.assertRaises(lab.RunnerError):
            lab.parse_results("acuppa-lander 2\nresult 1 2.0 0")
        with self.assertRaises(lab.RunnerError):
            lab.parse_results("acuppa-lander 2\ntrace 1 landed 0 0 0 0 0 0 0 0 2 7 "
                              "-1 -2 -3 -4 -5 0 1\nf 0 0 0 0 0 0 0 0 0\nend")
        with self.assertRaises(lab.RunnerError):
            lab.parse_results("acuppa-lander 2\nbogus\nend")

    def test_encode_rejects_bad_values(self):
        with self.assertRaises(ValueError):
            lab.encode_batch([], self.config, SCENARIOS, trace_scenario=8)
        with self.assertRaises(ValueError):
            lab.encode_batch([], self.config, [])
        genome = mutated_population(self.config, 1, rounds=0)[0]
        genome.nodes[0].bias = float("nan")
        with self.assertRaises(ValueError):
            lab.encode_batch([(1, genome, False)], self.config, SCENARIOS)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    RUNNER = sys.argv.pop(1)
    unittest.main()
