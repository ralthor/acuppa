"""Check a Lua-authored rigid body moves visibly in Vulkan output."""

from pathlib import Path
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
project = Path(sys.argv[2]).resolve()


def capture(frames, path):
    result = subprocess.run(
        [str(binary), "run", str(project), "--scene", "physics",
         "--frames", str(frames), "--capture", str(path)],
        capture_output=True, text=True, timeout=30,
    )
    assert result.returncode == 0 and "Validation Error" not in result.stderr, (
        result.stdout + result.stderr
    )
    header, pixels = path.read_bytes().split(b"\n255\n", 1)
    assert header == b"P6\n1280 720" and len(pixels) == 1280 * 720 * 3
    return pixels


def pixel(pixels, x, y):
    return pixels[(y * 1280 + x) * 3:(y * 1280 + x) * 3 + 3]


def orange(color):
    red, green, blue = color
    return red > green + 20 and green > blue + 20


with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    first = capture(1, root / "first.ppm")
    settled = capture(120, root / "settled.ppm")
    assert orange(pixel(first, 640, 150)), "initial body was not visible above the floor"
    assert not orange(pixel(settled, 640, 150)), "body did not fall from its initial position"
    assert not orange(pixel(first, 640, 420)), "body started at the floor"
    assert orange(pixel(settled, 640, 420)), "body did not render at its resting position"

print("Lua-authored physics body moved and settled in Vulkan captures")
