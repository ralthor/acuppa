"""Check see-through glTF materials and the inset camera in Vulkan captures."""

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


with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    views = capture("views", root / "views.ppm")
    plain = capture("views_off", root / "plain.ppm")
    clear = pixel(plain, 20, 20)

    # 40% opaque blue glass over the emissive red wall shows both colors.
    through = pixel(views, 453, 360)
    assert through[0] > 120 and through[2] > 120 and through[1] < 40, through
    # The wall alone is red, and the glass past the wall's edge is blue over the sky.
    wall = pixel(views, 795, 360)
    assert wall[0] > 180 and wall[2] < 30, wall
    beside = pixel(views, 296, 360)
    assert beside[2] > clear[2] and beside[0] < clear[0], (beside, clear)

    # Inside the inset rectangle (x 980..1240, y 40..220) the second camera
    # shows the cube; without the inset the same pixel is background.
    assert near(pixel(plain, 1110, 130), clear, 2), pixel(plain, 1110, 130)
    cube = pixel(views, 1110, 130)
    assert not near(cube, clear, 30), cube
    # Pixels just outside the rectangle are untouched by the inset.
    for x, y in ((975, 130), (1245, 130), (1110, 35), (1110, 225)):
        assert near(pixel(views, x, y), pixel(plain, x, y), 2), (x, y)
    # The main view is the same in both captures away from the inset.
    for x, y in ((453, 360), (795, 360), (640, 600)):
        assert near(pixel(views, x, y), pixel(plain, x, y), 2), (x, y)

print("views capture: blended glass and inset camera verified")
