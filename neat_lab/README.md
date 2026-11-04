# NEAT lab: evolving a lander with ACuppa physics

This folder trains small neural networks with **NEAT** (NeuroEvolution of
Augmenting Topologies) to land a thruster craft on a pad. The physics is the
engine's Jolt world, and the result plays back as an ordinary ACuppa game.
It is separate from the engine: nothing under `src/`, `include/`, or the root
`CMakeLists.txt` is changed. The lab links the unchanged `acuppa_core` library
and runs its viewer with the normal `build/acuppa` binary.

NEAT is an evolutionary method, not gradient-based reinforcement learning.
Each generation scores every network in the population. The best reproduce
with mutated weights. Occasionally a mutation adds a node or connection, so
networks start minimal and grow only when that helps. Similar networks are
grouped into species so a new structure gets time to improve before it has to
compete with the whole population.

![Viewer replaying generation 10: species leaders fire their jets while the champion network panel shows live activations](docs/viewer.png)

## How it fits together

| Part | Language | Role |
| --- | --- | --- |
| `runner/` → `lander_runner` | C++, links `acuppa_core` | Reads a whole generation of networks on stdin and flies each one through 8 fixed scenarios in Jolt, with no window, one fresh physics world per episode, in parallel threads. Writes the fitness of each network, plus 30 Hz recordings for the networks asked to be traced. |
| `trainer/train.py` | Python, neat-python | Runs the NEAT loop and makes one runner call per generation. Afterwards it replays the champion and the best network of up to 7 other species on that generation's display scenario and writes `viewer/runs/gen_NNNN.lua` plus `status.lua`. |
| `viewer/` | ACuppa Lua project | Polls `runs/status.lua` once a second with `tp.data_read`, replays the recorded flights, and draws the champion's network. Falls back to the checked-in `demo/` run when `runs/` is empty. |

Training does not render. A generation of 150 networks × 8 ten-second
episodes takes about 0.1 s on 12 threads. You watch replays instead: the
viewer shows exactly the recorded flights, so what you see is what was scored.

## Requirements and versions

- The engine's local toolchain from `bash scripts/bootstrap-local.sh` (pinned
  CMake, Zig, Ninja, Jolt v5.6.0, and the other engine dependencies in `.tools/`).
- **neat-python 2.0.0** (BSD-3-Clause) in its own ignored venv, using Python 3.12:

```bash
UV_CACHE_DIR=.tools/uv-cache uv venv .tools/neat-venv --python 3.12
```

```bash
UV_CACHE_DIR=.tools/uv-cache uv pip install --python .tools/neat-venv/bin/python neat-python==2.0.0
```

## Build, train, and watch

From the repository root, build the runner and tests into `build-neat/`:

```bash
bash neat_lab/configure.sh
```

Open the viewer. It plays the demo run until training output appears:

```bash
build/acuppa run neat_lab/viewer
```

In a second terminal, train. `--pace 2` holds each generation for at least two
seconds so the live viewer can show more of them; leave it out for full speed:

```bash
.tools/neat-venv/bin/python neat_lab/trainer/train.py --generations 150 --pace 2
```

Other options: `--seed` (default 1; the same seed reproduces a run exactly),
`--pop-size`, `--threads`, `--config`, and `--out`. A new run deletes earlier
`gen_*.lua` files in the output folder and nothing else.

### Viewer controls

| Key | Action |
| --- | --- |
| N, or the HIDE/SHOW NETWORK button in the top-right corner | Toggle the network panel |
| Left / Right | Step through the generations written so far (leaves live mode) |
| L | Live mode: jump to the newest generation after each replay |
| R | Restart the current replay |
| F | Toggle 1× / 3× replay speed |
| Escape | Quit |

The **gold** lander is the generation's champion. The colored landers behind
it are the best members of other species, one color per species ID. The
header shows the generation, the display scenario (it rotates through all 8),
the species count, and the champion's outcome on that scenario. Bars at the
bottom left show best fitness per generation; green bars are above 100, which
needs successful landings.

The network panel shows inputs on the left (offset from pad, height,
horizontal and vertical speed, fuel), outputs on the right (main engine, side
jets), and hidden nodes in columns by depth. Green dots are positive weights
and red dots negative ones; bigger dots are weights above 1.5. Nodes and
links brighten with the live values recorded during the flight. The engine UI
allows 256 items per frame and has no line shape, so links are rows of dots.
Very large networks keep only their strongest links in the panel.

## The task

The lander is a 0.9 × 0.6 × 0.9 m Jolt box (194 kg) moving in the X/Y plane.
The pad is 3 m wide at x = 0. Each tick, the network gets five inputs:
`x/10`, `height/10`, `vx/5`, `vy/5`, and `fuel` (0 to 1). Two tanh outputs
become a main throttle (0 to 1, up to 2× the lander's weight) and side jets
(−1 to 1, up to 0.5× weight). A full tank lasts 6 s of full throttle, and the
side jets burn half as fast. Each of the 8 scenarios has a different start
position, velocity, and steady sideways wind, which the network has to infer
from its drift. The engines cut at touchdown.

| Outcome | Rule | Fitness |
| --- | --- | --- |
| Landed | Touchdown ≤ 2.5 m/s, upright after 1 s, centre within the pad | 100 + proximity + 40 × fuel left + up to 30 for a soft touchdown |
| Off pad | Soft and upright, outside the pad | proximity + 40 |
| Tipped | Soft touchdown, but tilted more than about 18° after 1 s | proximity + 20 |
| Crashed | Touchdown above 2.5 m/s | proximity + up to 20, less for harder hits |
| Timeout | No touchdown in 10 s | proximity / 2 + up to 10 for staying low |
| Out of bounds | \|x\| > 30 m or y > 40 m | 0 |

`proximity` is 60 × (1 − |x| / 15), floored at 0. A network's fitness is the
mean over all 8 scenarios. A perfect score is about 230, and the run stops
early if the champion reaches 215. With seed 1, the champion lands all 8
scenarios within the first 10 generations. Later generations raise the score by touching down
more softly and saving fuel. The species count varies between about 2 and 12.

Engine limits that shaped the task: the engine exposes boxes, spheres, and
forces at the centre of mass, but no joints or torques. The lander therefore
stays upright in flight and can tip only on contact.

## Runner protocol

Input (whitespace-separated tokens):

```text
acuppa-lander 1
threads <n, 0 = all hardware threads>
trace_scenario <0..7>
genomes <count>
genome <key> <trace 0|1> <node count>
node <key> <activation> <aggregation> <bias> <response> <link count> [<input key> <weight>]...
end
```

Nodes are neat-python `FeedForwardNetwork.node_evals` in evaluation order.
Input keys are −1..−5 and output keys are 0 and 1. Supported activations are
`tanh`, `sigmoid`, `relu`, `identity`, and `clamped`, using neat-python's
formulas. The only supported aggregation is `sum`.

Output:

```text
acuppa-lander 1
result <key> <mean fitness> <landed scenarios>
trace <key> <outcome> <impact> <fitness> <frames> <nodes> <node keys...>
f <x> <y> <rx> <ry> <rz> <main> <side> <fuel> <node values...>
end
```

`result` comes once per genome. A traced genome adds a `trace` line followed
by its `f` frame lines. Malformed input exits with code 2 and a message on
stderr that names the bad token or genome.

## Viewer data files

`gen_NNNN.lua` is an engine Lua data file, read with `tp.data_read`. It holds
the generation statistics, the fitness history, the champion network
(`network.nodes` with layout positions, and `network.connections` including
disabled genes), the champion's activations, and up to 8 landers. Each flight
is packed into one string of `;`-separated frames, because engine data files
allow at most 10,000 table entries and 1 MiB. `status.lua` lists the
generations available and whether training is still running.
`trainer/make_demo.py` rebuilds `viewer/demo/` from a 150-generation, seed-1
run, keeping 11 of its generations. `viewer/make_assets.py` regenerates the
glTF boxes and HDRI.

## Tests

```bash
.tools/venv/bin/ctest --test-dir build-neat -L neat_lab --output-on-failure
```

- `neat_lander_physics`:
  - free fall, climbing out of bounds, fuel use, fuel exhaustion, and side-jet direction
  - a hand-written controller that lands all 8 scenarios, which proves the task is solvable
  - deterministic repeat episodes, and identical results with 1 or 8 threads
  - network maths matching neat-python, and rejection of bad networks and protocol input
- `neat_runner_protocol`: mutated neat-python genomes with hidden nodes go through
  the real runner; first-frame outputs must equal neat-python's own `activate()`.
  Also covers malformed batches and damaged runner output.
- `neat_trainer`: the Lua data writer, network layout, and a seeded 3-generation
  training run. Checks the published files, deletion of stale generations,
  sizes under 1 MiB, and an identical second run with the same seed.
- `neat_viewer_headless`: runs the viewer through `LuaProject` with no window.
  Covers the waiting screen, the demo, replay motion, the N key and corner
  button toggles, browsing limits, live mode loading a new generation, and
  damaged or unparsable run files.

`build/acuppa test neat_lab/viewer` also checks the viewer scene with the
engine's own command. The screenshot above is a Vulkan capture
(`--frames 40 --capture`). The live window was also run without `--frames`
while paced training wrote new generations.
