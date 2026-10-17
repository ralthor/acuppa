"""Directional and point shadows must visibly affect PBR receivers on Vulkan."""

from pathlib import Path
import subprocess
import sys
import tempfile


binary = Path(sys.argv[1]).resolve()
project = Path(sys.argv[2]).resolve()


def capture(scene, path):
    result = subprocess.run(
        [str(binary), "run", str(project), "--scene", scene, "--frames", "1",
         "--capture", str(path)], capture_output=True, text=True, timeout=20,
    )
    assert result.returncode == 0 and "Validation Error" not in result.stderr, \
        result.stdout + result.stderr
    header, pixels = path.read_bytes().split(b"\n255\n", 1)
    assert header == b"P6\n1280 720" and len(pixels) == 1280 * 720 * 3
    return pixels


def red(pixels, x, y):
    return pixels[(y * 1280 + x) * 3]


with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    sun = capture("shadow", root / "sun.ppm")
    off = capture("shadow_off", root / "off.ppm")
    shifted = capture("shadow_shift", root / "shift.ppm")
    point = capture("shadow_point", root / "point.ppm")
    point_off = capture("shadow_point_off", root / "point_off.ppm")
    point_second = capture("shadow_point_second", root / "point_second.ppm")

    assert red(sun, 700, 390) < 70 and red(off, 700, 390) > 190, \
        "the directional light did not cast a visible shadow"
    assert 70 < red(sun, 650, 400) < red(off, 650, 400), \
        "directional shadow edge was not softened"
    for x in range(420, 530, 20):
        for y in range(180, 550, 20):
            assert abs(red(sun, x, y) - red(off, x, y)) <= 2, \
                "shadow acne appeared away from the occluder"
    for x in range(550, 630, 10):
        for y in range(210, 310, 10):
            assert abs(red(sun, x, y) - red(off, x, y)) <= 2, \
                "a fully cut-out glTF mesh cast a shadow"

    def shadow_mask(pixels):
        return {(x, y) for y in range(355, 430) for x in range(670, 760)
                if red(pixels, x, y) < 100}

    fixed, moved = shadow_mask(sun), shadow_mask(shifted)
    assert len(fixed) > 1000 and len(moved) > 1000
    overlap = max(len(fixed & {(x + dx, y) for x, y in moved}) /
                  len(fixed | {(x + dx, y) for x, y in moved})
                  for dx in range(-5, 6))
    assert overlap > 0.92, "shadow shape changed as the camera moved"

    assert red(point, 580, 410) < 70 and red(point_off, 580, 410) > 150, \
        "the point light did not cast a visible shadow"
    assert 50 < red(point, 536, 410) < red(point_off, 536, 410), \
        "point-light shadow edge was not softened"
    assert abs(red(point, 700, 390) - red(point_off, 700, 390)) <= 2, \
        "point shadow changed an unoccluded receiver"
    assert point_second == point, "a shadowed second light used the wrong atlas row"

print("Directional and point shadows passed Vulkan capture checks")
