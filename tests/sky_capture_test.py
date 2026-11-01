"""Check the source-authored gradient sky in Vulkan captures."""

from pathlib import Path
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
project = Path(sys.argv[2]).resolve()
WIDTH, HEIGHT = 1280, 720


def capture(scene, path):
    result = subprocess.run(
        [str(binary), "run", str(project), "--scene", scene,
         "--frames", "3", "--capture", str(path)],
        capture_output=True, text=True, timeout=30,
    )
    assert result.returncode == 0 and "Validation Error" not in result.stderr, (
        result.stdout + result.stderr
    )
    header, pixels = path.read_bytes().split(b"\n255\n", 1)
    assert header == b"P6\n1280 720" and len(pixels) == WIDTH * HEIGHT * 3
    return pixels


def pixel(pixels, x, y):
    offset = (y * WIDTH + x) * 3
    return tuple(pixels[offset:offset + 3])


def near(a, b, tolerance):
    return all(abs(x - y) <= tolerance for x, y in zip(a, b))


def tonemapped(linear):
    """The renderer's ACES fit and sRGB encoding at exposure 1."""
    result = []
    for x in linear:
        mapped = min(max((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0), 1.0)
        srgb = mapped * 12.92 if mapped <= 0.0031308 else 1.055 * mapped ** (1 / 2.4) - 0.055
        result.append(round(srgb * 255))
    return tuple(result)


with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    sky = capture("sky", root / "sky.ppm")
    plain = capture("sky_off", root / "plain.ppm")

    # Without set_sky the original fixed clear color remains.
    clear = tonemapped((0.08, 0.18, 0.38))
    for x, y in ((20, 20), (640, 10), (1260, 700), (100, 450)):
        assert near(pixel(plain, x, y), clear, 2), (pixel(plain, x, y), clear)

    # Top of the frame is the saturated zenith blue; the horizon band is pale.
    top = pixel(sky, 200, 8)
    assert top[2] > top[0] + 60 and top[2] > top[1] + 20, top
    horizon = pixel(sky, 120, 455)
    assert min(horizon) > 190 and horizon[2] - horizon[0] < 45, horizon
    assert horizon[0] > top[0] + 50, (horizon, top)
    below = pixel(sky, 120, 690)
    assert below[1] > below[0] and below[1] > below[2], below

    # The sun disc is a compact near-white blob, brighter than nearby sky.
    sun = [(x, y) for y in range(0, HEIGHT // 2, 2) for x in range(0, WIDTH, 2)
           if min(pixel(sky, x, y)) >= 250]
    assert 100 < len(sun) < 2000, len(sun)
    cx = sum(x for x, _ in sun) // len(sun)
    cy = sum(y for _, y in sun) // len(sun)
    assert 700 < cx < 920 and 100 < cy < 260, (cx, cy)
    for dx, dy in ((-160, 0), (160, 0), (0, 140)):
        around = pixel(sky, cx + dx, cy + dy)
        assert sum(around) + 60 < sum(pixel(sky, cx, cy)), (around, (dx, dy))
    assert not any(min(pixel(plain, x, y)) >= 250 for x, y in sun), "no-sky frame had a sun"

    # Geometry covers the sky and keeps exactly its no-sky shading.
    for x, y in ((390, 480), (890, 480), (495, 430), (783, 420)):
        assert near(pixel(sky, x, y), pixel(plain, x, y), 2), (x, y)
        assert not near(pixel(sky, x, y), pixel(sky, x, 8), 30), (x, y)

print("Gradient sky, sun disc, covered geometry, and unchanged clear color passed")
