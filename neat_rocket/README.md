# NEAT rocket: a swiveling engine in 3D

A copy of [the NEAT lander](../neat_lab/README.md) with a harder craft. A tall
rocket flies in full 3D and has **one engine at its base that swivels up to 30°
each way on two axes**, measured from the rocket's own axis. It has no side
jets: to move sideways it must tip over and then straighten up again before
touchdown. The engine pushes through `apply_force_at`, the engine API added
for this, so a swiveled engine both pushes and turns the rocket.

![Generation 400 of the demo run: the champion rocket fires straight down over the square pad](docs/viewer.png)

## Run

```bash
bash neat_rocket/configure.sh
```

```bash
build/acuppa run neat_rocket/viewer
```

```bash
.tools/neat-venv/bin/python neat_rocket/trainer/train.py --generations 400 --pace 2
```

The trainer prints one line per generation. The viewer controls match the
lander's: N network, H scoring, Left/Right older/newer generation, L live,
R replay, F speed, Esc quit.

## The task

| | |
| --- | --- |
| Rocket | 1.0 × 1.8 × 1.0 m Jolt box, 540 kg, engine at the bottom centre |
| Engine | throttle up to 2× weight; swivels ±30° about the body's X and Z axes |
| Inputs (11) | pad offset x/z, height, velocity x/y/z, tilt x/z, spin rate x/z, fuel |
| Outputs (3) | throttle, gimbal x, gimbal z |
| Landed | touchdown ≤ 2 m/s, within 15° of upright 1 s later, centre on the 3 m square pad |
| Failures | crash, missed pad, tipped over, tumbled (past horizontal in the air), out of time (12 s), flew away |
| Weather | steady wind up to 2 m/s² in any ground direction, gusts up to 1 m/s², unseen by the network |

The score has the lander's five parts. Failures get partial credit, so slowing
down pays before landing is learned: a crash earns up to 40 the slower it is,
a tumble up to 20 the longer it stayed upright, and running out of time 20.

**Difficulty** starts at 0%: above the pad, upright, in calm air. Start
offsets, speeds, tilts, and wind grow by 5% after each generation whose
champion lands 6 of 8 flights, and shrink by about 1.7% after one that lands
1 or fewer. The header shows the current level.

## Current result

With seed 1 and a population of 150, the rockets learn to land from straight
above within 20 generations. Difficulty then levels off at 25–30% for 400
generations; the champion lands 3–5 of 8, and the population landing rate
swings between 5% and 30%. The network panel shows why: the champions use
only height and vertical speed to set the throttle, and leave the gimbal at
0. They drop straight down, which lands only when the start is close to the
pad. These changes did not get them to discover steering:

- a population of 300
- angular damping on the rocket
- start offsets that never shrink below 40%

A hand-written controller in the tests (`oracle` in `tests/rocket_test.cpp`)
lands at least 70% of full-difficulty flights using the gimbal. So the task can
be solved, but the search does not yet find a way to steer.

## Tests

```bash
.tools/venv/bin/ctest --test-dir build-rocket -L neat_rocket --output-on-failure
```

- **`neat_rocket_physics`:**
  - free fall, climb, and centered thrust staying upright
  - +X and +Z gimbal turning the rocket about the expected axes, wind direction
  - the oracle landing every sample and most random flights
  - deterministic and parallel-equal batches, and protocol errors
- **`neat_rocket_runner_protocol`:** runner outputs match neat-python's own
  network for tilted starts; weather changes fitness; damaged input and output.
- **`neat_rocket_trainer`:**
  - the difficulty levels and their adaptive steps
  - reproducible scenarios
  - a seeded 3-generation run with its viewer files
- **`neat_rocket_viewer_headless`:**
  - the HUD (wind heading, engine angles, difficulty) and engine bells with flames
  - network and scoring toggles, and browsing back to live
  - crash debris, live updates, and damaged files

The rocket's runner protocol is the lander's with the header
`acuppa-rocket 1`. A scenario has 12 values:
`x y z vx vy vz tilt_x tilt_z wind wind_heading gust gust_period`. A frame has 12
values: `x y z rx ry rz throttle gimbal_x gimbal_z fuel wind_x wind_z`.
