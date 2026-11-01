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
sky = count(aim, 0, 1280, 50, 170, lambda r, g, b: b > r + 25 and b > 150)
assert sky > 140000, f"blue sky missing above the horizon: {sky}"
fort = (360, 920, 150, 420)
wood = count(aim, *fort, lambda r, g, b: r > 140 and 80 < g < 170 and b < 100 and r > g + 30)
stone = count(aim, *fort, lambda r, g, b: 130 < r < 210 and abs(r - g) < 15 and 0 < b - r < 30)
crate = count(aim, *fort, lambda r, g, b: r > 150 and g < 90 and b < 60)
assert wood > 15000 and stone > 8000 and crate > 600, \
    f"fortress not visible: wood={wood} stone={stone} crates={crate}"
dot = lambda r, g, b: r > 180 and g > 175 and b < 190 and r > b + 25
dots = count(aim, 620, 660, 260, 460, dot)
assert dots > 200, f"aim arc dots not visible: {dots}"

# Distant hills and tree crowns rise above the meadow's horizon, where the
# same view without scenery shows only sky (0 green pixels in this band).
scenery = count(aim, 0, 1280, 170, 215, lambda r, g, b: g > b + 20)
assert scenery > 8000, f"horizon scenery missing: {scenery}"

fired = capture("siege_fired.ppm", 50, "--hold-key", "space")
# The follow camera chases the red ball as it reaches the fort.
red = count(fired, 0, 1280, 84, 720, lambda r, g, b: r > 100 and r > 3 * g and r > 3 * b)
assert red > 800, f"follow view does not show the flying ball: {red}"
assert fired != aim, "firing did not change the view"
print("Siege Sling aim view rendered sky, fort, and arc")
