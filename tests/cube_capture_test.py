"""GPU readback checks for the built-in perspective cube and its depth buffer."""

from pathlib import Path
import re
import subprocess
import sys
import tempfile


binary = Path(sys.argv[1]).resolve()


def run(*arguments):
    return subprocess.run(
        [str(binary), *map(str, arguments)], capture_output=True, text=True, timeout=15,
    )


def load(path):
    header, pixels = path.read_bytes().split(b"\n255\n", 1)
    assert header == b"P6\n960 540", header
    assert len(pixels) == 960 * 540 * 3, len(pixels)
    return pixels


def pixel(pixels, x, y):
    offset = (y * 960 + x) * 3
    return tuple(pixels[offset:offset + 3])


def orange(rgb):
    return rgb[0] > 210 and rgb[0] > rgb[1] + 25 and \
        rgb[1] > rgb[2] + 25 and rgb[2] < 170


def yellow(rgb):
    return rgb[0] > 210 and rgb[1] > 200 and \
        abs(rgb[0] - rgb[1]) < 25 and rgb[2] < 170


with tempfile.TemporaryDirectory() as directory:
    front_path = Path(directory) / "front.ppm"
    turned_path = Path(directory) / "turned.ppm"
    front_result = run("--capture-cube", front_path, "0")
    assert front_result.returncode == 0, front_result.stdout + front_result.stderr
    turned_result = run("--capture-cube", turned_path, "0.9")
    assert turned_result.returncode == 0, turned_result.stdout + turned_result.stderr
    front = load(front_path)
    turned = load(turned_path)
    background = pixel(front, 0, 0)
    assert background[0] < background[1] < background[2], background
    assert pixel(front, 959, 539) == background
    assert pixel(turned, 0, 0) == background
    # The purple rear face is submitted after the orange front face. It must fail depth.
    assert orange(pixel(front, 480, 270)), pixel(front, 480, 270)
    assert any(
        (rgb[2] > 220 and rgb[1] > 200 and rgb[0] < 170)
        for rgb in (pixel(front, x, y) for x in range(300, 660, 4)
                    for y in range(100, 240, 4))
    ), "top face did not appear"
    assert yellow(pixel(turned, 430, 270)), pixel(turned, 430, 270)
    assert any(orange(pixel(turned, x, 270)) for x in range(500, 630, 4)), \
        "turning the cube hid the front face"
    assert front != turned, "rotation did not change the capture"

    smoke = run("--smoke-cube")
    assert smoke.returncode == 0, smoke.stdout + smoke.stderr
    match = re.search(r"Rendered 6 cube frames; swapchain rebuilds: (\d+)", smoke.stdout)
    assert match and int(match.group(1)) >= 1, smoke.stdout
    assert "Swapchain: 1056x604" in smoke.stdout, smoke.stdout

    invalid = run("--capture-cube", front_path, "nan")
    assert invalid.returncode != 0 and "finite" in invalid.stderr, invalid.stderr

print("Cube perspective, rotation, depth occlusion, resize, and invalid-angle checks passed")
