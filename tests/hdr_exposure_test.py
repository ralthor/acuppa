"""A bright PBR highlight must survive the HDR pass and respond to exposure."""

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
    normal = capture("hdr", root / "normal.ppm")
    dim = capture("hdr_dim", root / "dim.ppm")
    assert normal(640, 360)[0] > dim(640, 360)[0] * 2, \
        "exposure did not change the PBR surface"
    assert normal(650, 350)[0] > dim(650, 350)[0] + 30, \
        "exposure did not change the bright highlight"
    assert 150 < dim(650, 350)[0] < 250, \
        "highlight was clipped before exposure could recover it"
    assert dim(645, 350)[0] + 20 < dim(650, 350)[0] < dim(655, 350)[0], \
        "recovered highlight lost its internal gradient"

print("HDR highlight detail and exposure passed on Vulkan")
