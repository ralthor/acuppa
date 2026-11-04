"""Rebuilds the viewer's checked-in demo run from a fixed-seed training run.

    .tools/neat-venv/bin/python neat_lab/trainer/make_demo.py

The viewer shows demo/ whenever runs/ has no training output, so it plays
without a trainer. Only a spread of generations is kept to stay small.
"""

import os
import shutil
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lab  # noqa: E402
import train  # noqa: E402

GENERATIONS = 150
KEEP = [1, 2, 3, 5, 8, 12, 20, 35, 60, 100, 150]
DEMO = os.path.join(train.LAB, "viewer", "demo")


def main():
    with tempfile.TemporaryDirectory() as work:
        train.main(["--generations", str(GENERATIONS), "--seed", "1", "--out", work])
        for name in os.listdir(DEMO) if os.path.isdir(DEMO) else []:
            if name.startswith("gen_") or name == "status.lua":
                os.remove(os.path.join(DEMO, name))
        os.makedirs(DEMO, exist_ok=True)
        for generation in KEEP:
            name = f"gen_{generation:04d}.lua"
            shutil.copyfile(os.path.join(work, name), os.path.join(DEMO, name))
        lab.write_lua(os.path.join(DEMO, "status.lua"),
                      {"format": 1, "latest": KEEP[-1], "generations": KEEP, "running": False})
    return 0


if __name__ == "__main__":
    sys.exit(main())
