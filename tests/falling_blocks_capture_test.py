"""The playable game must render input-driven state on the actual GPU."""

from pathlib import Path
import subprocess
import sys
import tempfile


binary = Path(sys.argv[1]).resolve()
project = Path(sys.argv[2]).resolve()


def capture(path, *extra):
    result = subprocess.run(
        [str(binary), "run", str(project), "--frames", "2", "--capture", str(path),
         *extra], capture_output=True, text=True, timeout=20,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    assert "Validation Error" not in result.stderr, result.stderr
    header, pixels = path.read_bytes().split(b"\n255\n", 1)
    assert header == b"P6\n1280 720", header
    return pixels


with tempfile.TemporaryDirectory() as directory:
    temp = Path(directory)
    normal = capture(temp / "normal.ppm")
    left = capture(temp / "left.ppm", "--hold-key", "left")
    dropped = capture(temp / "drop.ppm", "--hold-key", "space")
    assert normal != left, "left action did not change the rendered piece"
    assert normal != dropped, "hard drop did not change the rendered board"
    assert normal[:3] == left[:3] == dropped[:3], "background changed unexpectedly"

print("Falling-block input changed the Vulkan frame")
