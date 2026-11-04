"""Bridge between neat-python, the C++ rocket runner, and the ACuppa viewer.

The runner evaluates a whole generation per call (see README.md for the text
protocol). Viewer files are Lua data tables that the viewer reads with
tp.data_read; long per-frame tracks are packed into strings because engine
data files allow at most 10,000 table entries.
"""

import math
import os
import random
import re
import subprocess

import neat

PROTOCOL = "acuppa-rocket 1"
SCENARIO_COUNT = 8
SCORE_PARTS = ("center", "landing", "soft", "fuel", "speed")
FRAME_FIELDS = 12  # x y z rx ry rz throttle gimbal_x gimbal_z fuel wind_x wind_z
# Ranges for the random start state and weather drawn for every generation, in
# the runner's scenario order. Tilts are radians; the wind heading is the
# direction it blows in the ground plane.
SCENARIO_RANGES = {"x": (-8, 8), "y": (12, 18), "z": (-8, 8),
                   "vx": (-2, 2), "vy": (-2, 0), "vz": (-2, 2),
                   "tilt_x": (-0.1, 0.1), "tilt_z": (-0.1, 0.1),
                   "wind": (0, 2), "wind_heading": (0, 6.2832), "gust": (0, 1),
                   "gust_period": (1.5, 5)}
SCENARIO_FIELDS = tuple(SCENARIO_RANGES)
INPUT_LABELS = {-1: "DX", -2: "DZ", -3: "HEIGHT", -4: "VX", -5: "VY", -6: "VZ",
                -7: "TILT X", -8: "TILT Z", -9: "SPIN X", -10: "SPIN Z", -11: "FUEL"}
OUTPUT_LABELS = {0: "THROTTLE", 1: "GIMBAL X", 2: "GIMBAL Z"}
MAX_VIEW_LANDERS = 8
HISTORY_LENGTH = 100


class RunnerError(RuntimeError):
    pass


def load_config(path):
    return neat.Config(neat.DefaultGenome, neat.DefaultReproduction,
                       neat.DefaultSpeciesSet, neat.DefaultStagnation, path)


def _number(value):
    value = float(value)
    if not math.isfinite(value):
        raise ValueError(f"non-finite value {value!r}")
    return repr(value)


# Ranges that keep their full size at every difficulty.
FIXED_RANGES = ("y", "wind_heading", "gust_period")


def next_difficulty(level, champion_landed, step):
    """Adaptive curriculum: once the champion lands at least 6 of 8 flights,
    the next generation gets harder by `step`; at 1 or fewer it eases by a
    third of that. Level 0 starts above the pad in calm air; 1 is full size."""
    if champion_landed >= 6:
        return min(1.0, level + step)
    if champion_landed <= 1:
        return max(0.0, level - step / 3)
    return level


def random_scenarios(seed, generation, count=SCENARIO_COUNT, level=1.0):
    """The start states and weather every genome of one generation flies.
    Reproducible from (seed, generation); different for each generation.
    Offsets, speeds, tilts, and wind shrink toward zero as `level` drops."""
    rng = random.Random(seed * 1_000_003 + generation)
    scenarios = []
    for _ in range(count):
        scenario = {}
        for name, (low, high) in SCENARIO_RANGES.items():
            value = rng.uniform(low, high)
            if name not in FIXED_RANGES:
                center = min(max(0.0, low), high)
                value = center + level * (value - center)
            scenario[name] = round(value, 4)
        scenarios.append(scenario)
    return scenarios


def encode_batch(entries, config, scenarios, trace_scenario=0, threads=0):
    """entries: iterable of (key, genome, trace) -> runner input text."""
    if not scenarios:
        raise ValueError("at least one scenario is required")
    if not 0 <= trace_scenario < len(scenarios):
        raise ValueError(f"trace scenario must be 0..{len(scenarios) - 1}")
    entries = list(entries)
    lines = [f"{PROTOCOL}", f"threads {int(threads)}", f"scenarios {len(scenarios)}"]
    for scenario in scenarios:
        lines.append("scenario " + " ".join(_number(scenario[f]) for f in SCENARIO_FIELDS))
    lines += [f"trace_scenario {trace_scenario}", f"genomes {len(entries)}"]
    for key, genome, trace in entries:
        net = neat.nn.FeedForwardNetwork.create(genome, config)
        lines.append(f"genome {int(key)} {1 if trace else 0} {len(net.node_evals)}")
        for node, _act, _agg, bias, response, links in net.node_evals:
            gene = genome.nodes[node]
            parts = [f"node {node}", gene.activation, gene.aggregation, _number(bias),
                     _number(response), str(len(links))]
            for source, weight in links:
                parts += [str(source), _number(weight)]
            lines.append(" ".join(parts))
    lines.append("end")
    return "\n".join(lines) + "\n"


def parse_results(text):
    """Returns (results, traces): results[key] = (fitness, landed) and
    traces[key] = dict(outcome, impact, touch_time, fitness, score, node_keys,
    frames, acts)."""
    lines = text.splitlines()
    if not lines or lines[0] != PROTOCOL:
        raise RunnerError("runner output has no protocol header")
    if lines[-1] != "end":
        raise RunnerError("runner output is truncated")
    results, traces = {}, {}
    i = 1
    while i < len(lines) - 1:
        parts = lines[i].split()
        if parts[0] == "result" and len(parts) == 4:
            results[int(parts[1])] = (float(parts[2]), int(parts[3]))
            i += 1
        elif parts[0] == "trace" and len(parts) >= 13:
            key, frame_count, node_count = int(parts[1]), int(parts[11]), int(parts[12])
            node_keys = [int(k) for k in parts[13:]]
            if len(node_keys) != node_count:
                raise RunnerError(f"trace {key} lists the wrong number of nodes")
            frames, acts = [], []
            for row in lines[i + 1:i + 1 + frame_count]:
                values = row.split()
                if values[0] != "f" or len(values) != 1 + FRAME_FIELDS + node_count:
                    raise RunnerError(f"trace {key} has a malformed frame")
                frames.append(values[1:1 + FRAME_FIELDS])
                acts.append(values[1 + FRAME_FIELDS:])
            if len(frames) != frame_count:
                raise RunnerError(f"trace {key} is missing frames")
            traces[key] = {"outcome": parts[2], "impact": float(parts[3]),
                           "touch_time": float(parts[4]), "fitness": float(parts[5]),
                           "score": dict(zip(SCORE_PARTS, map(float, parts[6:11]))),
                           "node_keys": node_keys, "frames": frames, "acts": acts}
            i += 1 + frame_count
        else:
            raise RunnerError(f"unexpected runner line: {lines[i][:80]}")
    return results, traces


def run_batch(runner, text, timeout=600):
    process = subprocess.run([runner], input=text, capture_output=True, text=True,
                             timeout=timeout, check=False)
    if process.returncode != 0:
        raise RunnerError(process.stderr.strip() or f"runner exited {process.returncode}")
    return parse_results(process.stdout)


def layout_network(genome, config):
    """Places the working nodes of a genome on a unit square, inputs left, outputs
    right, and hidden nodes in columns by their longest path from an input."""
    inputs = list(config.genome_config.input_keys)
    outputs = list(config.genome_config.output_keys)
    enabled = [key for key, gene in genome.connections.items() if gene.enabled]
    # Draw only hidden nodes that feed an output, the ones the network evaluates.
    # Mutations also leave dead-end nodes in the genome; those are only counted.
    required = neat.graphs.required_for_output(inputs, outputs, enabled)
    hidden = sorted(k for k in genome.nodes if k not in outputs and k in required)
    depth = {k: 0 for k in inputs}
    # Feed-forward genomes are acyclic, so relaxing once per node settles depths.
    for _ in range(len(hidden) + 1):
        for source, target in enabled:
            if target in hidden and source in depth:
                depth[target] = max(depth.get(target, 1), depth[source] + 1)
    for k in hidden:
        depth.setdefault(k, 1)
    columns = max([depth[k] for k in hidden], default=0) + 1
    placed = {}

    def spread(keys, x):
        for index, key in enumerate(keys):
            placed[key] = (x, (index + 1) / (len(keys) + 1))

    spread(inputs, 0.0)
    spread(outputs, 1.0)
    for column in range(1, columns):
        spread([k for k in hidden if depth[k] == column], column / columns)
    nodes = []
    for key in inputs + outputs + hidden:
        kind = "input" if key in inputs else ("output" if key in outputs else "hidden")
        label = INPUT_LABELS.get(key) or OUTPUT_LABELS.get(key) or ""
        x, y = placed[key]
        nodes.append({"key": key, "kind": kind, "label": label,
                      "x": round(x, 4), "y": round(y, 4)})
    shown = set(inputs) | set(outputs) | set(hidden)
    connections = [{"from": source, "to": target, "weight": round(gene.weight, 4),
                    "enabled": bool(gene.enabled)}
                   for (source, target), gene in sorted(genome.connections.items())
                   if source in shown and target in shown]
    return {"nodes": nodes, "connections": connections, "genome_nodes": len(genome.nodes) +
            len(inputs), "genome_links": sum(gene.enabled for gene in genome.connections.values())}


_NAME = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")


def to_lua(value, indent=0):
    """Serializes dicts with identifier keys, lists, strings, numbers, and
    booleans as a Lua data expression the engine's data reader accepts."""
    pad = "  " * (indent + 1)
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, int):
        return str(value)
    if isinstance(value, float):
        return _number(value)
    if isinstance(value, str):
        if any(ord(c) < 32 or ord(c) > 126 for c in value):
            raise ValueError("strings must be printable ASCII")
        return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'
    if isinstance(value, dict):
        items = []
        for key in value:
            if not isinstance(key, str) or not _NAME.match(key):
                raise ValueError(f"invalid Lua field name {key!r}")
            items.append(f"{pad}{key} = {to_lua(value[key], indent + 1)}")
        return "{\n" + ",\n".join(items) + "\n" + "  " * indent + "}" if items else "{}"
    if isinstance(value, (list, tuple)):
        if all(isinstance(v, (int, float)) and not isinstance(v, bool) for v in value):
            return "{" + ", ".join(to_lua(v) for v in value) + "}"
        items = [pad + to_lua(v, indent + 1) for v in value]
        return "{\n" + ",\n".join(items) + "\n" + "  " * indent + "}" if items else "{}"
    raise ValueError(f"unsupported value {type(value).__name__}")


def pack_rows(rows):
    return ";".join(",".join(row) for row in rows)


def write_lua(path, value):
    """Atomically replaces a viewer data file."""
    temp = f"{path}.tmp"
    with open(temp, "w", encoding="ascii") as handle:
        handle.write("return " + to_lua(value) + "\n")
    os.replace(temp, path)


def view_landers(population, species_set, best):
    """Champion first, then the best member of each other species, by fitness."""
    picks = [("champion", best)]
    leaders = []
    for sid, species in species_set.species.items():
        members = [g for g in species.members.values() if g.fitness is not None]
        if not members:
            continue
        leader = max(members, key=lambda g: g.fitness)
        if leader.key != best.key:
            leaders.append((leader.fitness, sid, leader))
    leaders.sort(key=lambda item: (-item[0], item[1]))
    for _fitness, _sid, leader in leaders[:MAX_VIEW_LANDERS - 1]:
        picks.append(("species", leader))
    species_of = {}
    for sid, species in species_set.species.items():
        for key in species.members:
            species_of[key] = sid
    return [(role, genome, species_of.get(genome.key, 0)) for role, genome in picks]


def build_run(generation, scenario, weather, config, population, species_set, best, picks,
              traces, landed, landing_rate, history, rate_history, level=1.0):
    """One viewer file: the generation's statistics, the champion network,
    and the recorded flights on the display scenario `weather`."""
    fitnesses = [g.fitness for g in population.values() if g.fitness is not None]
    champion = traces[best.key]
    landers = []
    for role, genome, sid in picks:
        trace = traces[genome.key]
        landers.append({"role": role, "species": sid, "genome": genome.key,
                        "fitness": round(genome.fitness, 3), "outcome": trace["outcome"],
                        "impact": round(trace["impact"], 3),
                        "touch_time": round(trace["touch_time"], 3),
                        "score": {k: round(v, 2) for k, v in trace["score"].items()},
                        "run_score": round(trace["fitness"], 2),
                        "frames": pack_rows(trace["frames"])})
    return {
        "format": 2,
        "generation": generation,
        "scenario": scenario + 1,
        "scenarios": SCENARIO_COUNT,
        "difficulty": round(level, 3),
        "weather": dict(weather),
        "population": len(population),
        "species_count": len(species_set.species),
        "best_fitness": round(best.fitness, 3),
        "mean_fitness": round(sum(fitnesses) / max(1, len(fitnesses)), 3),
        "landed": landed,
        "landing_rate": round(landing_rate, 4),
        "history": [round(v, 2) for v in history[-HISTORY_LENGTH:]],
        "rate_history": [round(v, 4) for v in rate_history[-HISTORY_LENGTH:]],
        "network": layout_network(best, config),
        "node_keys": champion["node_keys"],
        "champion_acts": pack_rows(champion["acts"]),
        "landers": landers,
    }
