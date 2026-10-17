"""Captured glTF textures and material spheres must respond to direct light."""

from pathlib import Path
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
project = Path(sys.argv[2]).resolve()


def capture(root, scene, name, *options):
    path = root / name
    result = subprocess.run([str(binary), "run", str(project), "--scene", scene,
                             "--frames", "1", "--capture", str(path), *options],
                            capture_output=True, text=True, timeout=20)
    assert result.returncode == 0 and "Validation Error" not in result.stderr, \
        result.stdout + result.stderr
    header, pixels = path.read_bytes().split(b"\n255\n", 1)
    assert header == b"P6\n1280 720" and len(pixels) == 1280 * 720 * 3

    def pixel(x, y):
        offset = (y * 1280 + x) * 3
        return tuple(pixels[offset:offset + 3])

    return pixel


with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    textured = capture(root, "textured", "textured.ppm")
    red = textured(580, 300)
    green = textured(700, 300)
    blue = textured(580, 420)
    white = textured(700, 420)
    assert red[0] > red[1] > red[2], red
    assert green[1] > green[0] > green[2], green
    assert blue[2] > blue[0] > blue[1], blue
    assert min(white) > 150, white

    lit = capture(root, "spheres", "lit.ppm")
    dark = capture(root, "spheres", "dark.ppm", "--hold-key", "space")
    for x in (370, 640, 910):
        lit_energy = sum(sum(lit(xx, yy)) for xx in range(x - 45, x + 46, 5)
                         for yy in range(315, 406, 5))
        dark_energy = sum(sum(dark(xx, yy)) for xx in range(x - 45, x + 46, 5)
                          for yy in range(315, 406, 5))
        assert lit_energy > dark_energy * 4 + 2000, (x, lit_energy, dark_energy)
    assert lit(370, 360)[0] > lit(370, 360)[1] * 3, \
        "dielectric red sphere lost its base color"
    assert sum(lit(910, 360)) > sum(lit(640, 360)) * 2, \
        "rough metallic sphere should spread its highlight"
    point = capture(root, "point", "point.ppm")
    assert sum(point(640, 360)) > 150, "point light did not illuminate the center sphere"

print("Textured glTF and directional/point-light PBR captures passed")
