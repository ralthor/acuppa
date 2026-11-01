"""Siege Sling's aim view must render the sky, fort, and arc on Vulkan."""

from pathlib import Path
import subprocess
import sys


binary = Path(sys.argv[1]).resolve()
game = Path(sys.argv[2]).resolve()
output = Path(sys.argv[3]).resolve()
output.mkdir(parents=True, exist_ok=True)


def capture(name, frames, *extra):
    path = output / name
    result = subprocess.run([str(binary), "run", str(game), "--frames", str(frames),
                             "--capture", str(path), *extra],
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stdout + result.stderr
    assert "Validation Error" not in result.stderr, result.stderr
    header, pixels = path.read_bytes().split(b"\n255\n", 1)
    assert header == b"P6\n1280 720", header
    return pixels


def count(pixels, x0, x1, y0, y1, test):
    return sum(1 for y in range(y0, y1) for x in range(x0, x1)
               if test(*pixels[(y * 1280 + x) * 3:(y * 1280 + x) * 3 + 3]))


aim = capture("siege_aim.ppm", 30)
sky = count(aim, 0, 1280, 90, 230, lambda r, g, b: b > r + 25 and b > 150)
assert sky > 150000, f"blue sky missing above the horizon: {sky}"
fort = (480, 800, 290, 400)
wood = count(aim, *fort, lambda r, g, b: r > 140 and 80 < g < 170 and b < 100 and r > g + 30)
stone = count(aim, *fort, lambda r, g, b: 130 < r < 200 and abs(r - g) < 15 and 0 < b - r < 30)
assert wood > 600 and stone > 1200, f"fort blocks not visible: wood={wood} stone={stone}"
dot = lambda r, g, b: r > 180 and g > 175 and b < 190 and r > b + 25
dots = count(aim, 620, 660, 260, 450, dot)
assert dots > 200, f"aim arc dots not visible: {dots}"

# SSAO banded the open grass in this view, so the game keeps it off: the
# meadow must shade smoothly from the horizon toward the hill.
for x in (150, 300, 1000, 1150):
    column = [sum(aim[(y * 1280 + x) * 3:(y * 1280 + x) * 3 + 3]) for y in range(250, 460)]
    step = max(abs(a - b) for a, b in zip(column, column[1:]))
    assert step <= 6, f"grass shading bands at x={x}: step {step}"

fired = capture("siege_fired.ppm", 50, "--hold-key", "space")
# The follow camera chases the red ball as it reaches the fort.
red = count(fired, 0, 1280, 84, 720, lambda r, g, b: r > 100 and r > 3 * g and r > 3 * b)
assert red > 800, f"follow view does not show the flying ball: {red}"
assert fired != aim, "firing did not change the view"
print("Siege Sling aim view rendered sky, fort, and arc")
