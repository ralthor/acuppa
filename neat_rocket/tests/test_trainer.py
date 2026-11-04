"""Checks viewer data helpers and a short end-to-end training run.

usage: test_trainer.py <lander_runner> <work directory>
"""

import os
import re
import shutil
import sys
import unittest

LAB = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(LAB, "trainer"))
import lab  # noqa: E402
import train  # noqa: E402

RUNNER = WORK = None


def read(path):
    with open(path, encoding="ascii") as handle:
        return handle.read()


def write(path, text):
    with open(path, "w", encoding="ascii") as handle:
        handle.write(text)


class LuaWriterTest(unittest.TestCase):
    def test_values(self):
        self.assertEqual(lab.to_lua({"a": 1, "b": [1, 2.5], "c": True, "d": 'x"y'}),
                         '{\n  a = 1,\n  b = {1, 2.5},\n  c = true,\n  d = "x\\"y"\n}')
        self.assertEqual(lab.to_lua([]), "{}")
        self.assertEqual(lab.to_lua({"n": [{"k": -1}]}), "{\n  n = {\n    {\n      k = -1\n    }\n  }\n}")

    def test_rejects_values_lua_data_cannot_hold(self):
        for bad in ({"not a name": 1}, {1: 2}, float("inf"), "line\nbreak", None, {"x": object()}):
            with self.assertRaises(ValueError, msg=repr(bad)):
                lab.to_lua(bad)

    def test_write_is_atomic_and_leaves_no_temp(self):
        os.makedirs(WORK, exist_ok=True)
        path = os.path.join(WORK, "data.lua")
        lab.write_lua(path, {"a": 1})
        lab.write_lua(path, {"a": 2})
        self.assertEqual(read(path), "return {\n  a = 2\n}\n")
        self.assertFalse(os.path.exists(path + ".tmp"))


class LayoutTest(unittest.TestCase):
    def test_columns_follow_longest_path(self):
        config = lab.load_config(os.path.join(LAB, "trainer", "neat.cfg"))
        genome = config.genome_type(1)
        genome.nodes = {}
        for key in (0, 1, 2, 10, 11, 12, 13):
            genome.nodes[key] = config.genome_config.node_gene_type(key)
            genome.nodes[key].init_attributes(config.genome_config)
        # Node 12 only has a disabled link and node 13 is a dead end, so the panel
        # leaves both out but still counts them.
        links = {(-1, 10): True, (10, 11): True, (11, 0): True, (-2, 1): False, (12, 1): False,
                 (-3, 13): True}
        for innovation, ((source, target), enabled) in enumerate(links.items(), 1):
            gene = config.genome_config.connection_gene_type((source, target), innovation)
            gene.init_attributes(config.genome_config)
            gene.enabled = enabled
            genome.connections[(source, target)] = gene
        view = lab.layout_network(genome, config)
        nodes = {n["key"]: n for n in view["nodes"]}
        self.assertEqual([n["key"] for n in view["nodes"]],
                         list(range(-1, -12, -1)) + [0, 1, 2, 10, 11])
        self.assertEqual((nodes[-1]["x"], nodes[0]["x"]), (0.0, 1.0))
        self.assertLess(nodes[10]["x"], nodes[11]["x"])
        self.assertTrue(0 < nodes[10]["x"] < 1 and 0 < nodes[11]["x"] < 1)
        self.assertEqual(nodes[-11]["label"], "FUEL")
        self.assertEqual(nodes[-7]["label"], "TILT X")
        self.assertEqual(nodes[2]["label"], "GIMBAL Z")
        self.assertNotIn(12, nodes)
        self.assertNotIn(13, nodes)
        self.assertEqual((view["genome_nodes"], view["genome_links"]), (18, 4))
        self.assertNotIn((-3, 13), [(c["from"], c["to"]) for c in view["connections"]])
        disabled = [c for c in view["connections"] if not c["enabled"]]
        self.assertEqual([(c["from"], c["to"]) for c in disabled], [(-2, 1)])


class ScenarioTest(unittest.TestCase):
    def test_random_scenarios_are_reproducible_per_generation(self):
        first = lab.random_scenarios(3, 10)
        self.assertEqual(first, lab.random_scenarios(3, 10))
        self.assertNotEqual(first, lab.random_scenarios(3, 11))
        self.assertNotEqual(first, lab.random_scenarios(4, 10))
        self.assertEqual(len(first), lab.SCENARIO_COUNT)
        many = [s for g in range(1, 60) for s in lab.random_scenarios(1, g)]
        for name, (low, high) in lab.SCENARIO_RANGES.items():
            values = [s[name] for s in many]
            self.assertTrue(all(low <= v <= high for v in values), name)
            self.assertGreater(max(values) - min(values), 0.8 * (high - low), name)


class DifficultyTest(unittest.TestCase):
    def test_level_zero_starts_above_the_pad_in_calm_air(self):
        for scenario in lab.random_scenarios(2, 4, level=0.0):
            for name in lab.SCENARIO_RANGES:
                if name in lab.FIXED_RANGES:
                    continue
                self.assertEqual(scenario[name], 0.0, name)
            self.assertTrue(12 <= scenario["y"] <= 18)
        half = lab.random_scenarios(2, 4, level=0.5)
        full = lab.random_scenarios(2, 4, level=1.0)
        for a, b in zip(half, full):
            self.assertAlmostEqual(a["x"], b["x"] / 2, delta=1e-3)
            self.assertAlmostEqual(a["wind"], b["wind"] / 2, delta=1e-3)
            self.assertEqual(a["wind_heading"], b["wind_heading"])

    def test_difficulty_follows_the_champion(self):
        self.assertAlmostEqual(lab.next_difficulty(0.2, 6, 0.06), 0.26)
        self.assertEqual(lab.next_difficulty(0.2, 4, 0.06), 0.2)
        self.assertAlmostEqual(lab.next_difficulty(0.2, 1, 0.06), 0.18)
        self.assertEqual(lab.next_difficulty(0.99, 8, 0.06), 1.0)
        self.assertEqual(lab.next_difficulty(0.01, 0, 0.06), 0.0)


class TrainingRunTest(unittest.TestCase):
    def test_short_run_publishes_every_generation(self):
        out = os.path.join(WORK, "runs")
        shutil.rmtree(out, ignore_errors=True)
        os.makedirs(out)
        stale = os.path.join(out, "gen_0099.lua")
        write(stale, "return {}\n")
        keep = os.path.join(out, "notes.txt")
        write(keep, "user file\n")
        self.assertEqual(train.main(["--generations", "3", "--pop-size", "30", "--runner", RUNNER,
                                     "--out", out, "--seed", "7"]), 0)
        self.assertFalse(os.path.exists(stale), "stale generation from an earlier run kept")
        self.assertTrue(os.path.exists(keep), "trainer removed a file it does not own")
        status = read(os.path.join(out, "status.lua"))
        self.assertIn("latest = 3", status)
        self.assertIn("generations = {1, 2, 3}", status)
        self.assertIn("running = false", status)
        for generation in (1, 2, 3):
            text = read(os.path.join(out, f"gen_{generation:04d}.lua"))
            self.assertTrue(text.startswith("return {"))
            self.assertIn(f"generation = {generation},", text)
            self.assertIn(f"scenario = {generation},", text)
            self.assertIn('role = "champion"', text)
            frames = re.search(r'frames = "([^"]*)"', text).group(1).split(";")
            self.assertGreater(len(frames), 10)
            self.assertEqual(len(frames[0].split(",")), lab.FRAME_FIELDS)
            self.assertIn("weather = {", text)
            self.assertIn("rate_history = {", text)
            self.assertIn("speed = ", text)
            self.assertLess(len(text), 1024 * 1024, "viewer files must stay under 1 MiB")
        # The same seed reproduces the same run.
        first = read(os.path.join(out, "gen_0003.lua"))
        train.main(["--generations", "3", "--pop-size", "30", "--runner", RUNNER,
                    "--out", out, "--seed", "7"])
        self.assertEqual(read(os.path.join(out, "gen_0003.lua")), first)

    def test_bad_arguments_fail_before_training(self):
        with self.assertRaises(SystemExit):
            train.main(["--generations", "0", "--runner", RUNNER])
        with self.assertRaises(SystemExit):
            train.main(["--runner", os.path.join(WORK, "missing-runner")])


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    WORK = os.path.abspath(sys.argv.pop(2))
    RUNNER = os.path.abspath(sys.argv.pop(1))
    unittest.main()
