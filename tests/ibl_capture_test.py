"""A source-authored HDRI must change PBR surfaces on the actual GPU."""

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

    def pixel(x, y):
        offset = (y * 1280 + x) * 3
        return tuple(pixels[offset:offset + 3])

    return pixel


with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    lit = capture("ibl", root / "lit.ppm")
    unlit = capture("ibl_off", root / "unlit.ppm")
    assert lit(20, 20) == unlit(20, 20), "environment changed the clear color"
    for x in (370, 640, 910):
        assert sum(lit(x, 360)) > sum(unlit(x, 360)) + 100, \
            "HDRI did not illuminate a PBR material"
    assert lit(640, 310)[2] > lit(640, 400)[2] + 80, \
        "upper and lower HDRI radiance did not differ on the metal sphere"
    assert lit(640, 400)[0] > lit(640, 310)[0] + 20, \
        "warm lower HDRI radiance did not reach the metal sphere"

    missing = subprocess.run(
        [str(binary), "run", str(project), "--scene", "ibl_missing", "--frames", "1"],
        capture_output=True, text=True, timeout=10,
    )
    assert missing.returncode != 0 and "assets/missing.hdr" in missing.stderr, \
        missing.stdout + missing.stderr

print("HDRI lighting, direction, and missing-file failure passed on Vulkan")
