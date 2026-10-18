"""The source-authored car and driving state must render on Vulkan."""

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


first = capture("garage_start.ppm", 2)
driven = capture("garage_driven.ppm", 120, "--hold-key", "w")
assert first != driven, "driving did not change the rendered 3D view"
red = sum(1 for i in range(0, len(first), 3)
          if first[i] > 90 and first[i] > first[i + 1] * 1.4 and
          first[i] > first[i + 2] * 1.4)
assert red > 100, "the red PBR car is not visible in the Vulkan capture"

# The right obstacle's oblique face is a single flat mesh face. SSAO produced
# camera-moving bands here; the game leaves it off until that effect is fixed.
for y, left, right in [(460, 960, 1000), (480, 970, 1000)]:
    face = [driven[(y * 1280 + x) * 3] for x in range(left, right)]
    assert max(face) - min(face) <= 4, "SSAO striped the flat obstacle face"
