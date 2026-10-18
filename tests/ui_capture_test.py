"""Check that script UI text and focused buttons reach the Vulkan image."""

from pathlib import Path
import subprocess
import sys


binary = Path(sys.argv[1]).resolve()
project = Path(sys.argv[2]).resolve()
capture = Path(sys.argv[3]).resolve() / "ui_menu_capture.ppm"
result = subprocess.run(
    [str(binary), "run", str(project), "--scene", "menu", "--frames", "2",
     "--capture", str(capture)],
    capture_output=True, text=True, check=True,
)
assert "Validation Error" not in result.stderr, result.stderr
raw = capture.read_bytes()
header, pixels = raw.split(b"\n255\n", 1)
assert header == b"P6\n1280 720", header


def rgb(x, y):
    offset = (y * 1280 + x) * 3
    return pixels[offset:offset + 3]


background = rgb(200, 300)
start = rgb(500, 300)
help_button = rgb(500, 390)
assert start[2] > background[2] + 80, (background, start)
assert start[2] > help_button[2] + 20, (start, help_button)
title = [rgb(x, y) for y in range(118, 155) for x in range(470, 720)]
assert sum(all(channel > 200 for channel in pixel) for pixel in title) > 200, \
    "source-defined title text was not drawn"
