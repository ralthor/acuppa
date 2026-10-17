"""Injected named input must change a Lua scene's real rendered frame."""

from pathlib import Path
import subprocess
import sys
import tempfile


binary = Path(sys.argv[1]).resolve()
fixture = Path(sys.argv[2]).resolve()


def capture(path, *extra):
    result = subprocess.run(
        [str(binary), "run", str(fixture), "--frames", "60", "--capture", str(path),
         *extra], capture_output=True, text=True, timeout=20,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    header, pixels = path.read_bytes().split(b"\n255\n", 1)
    assert header == b"P6\n1280 720", header
    return pixels


with tempfile.TemporaryDirectory() as directory:
    temp = Path(directory)
    normal = capture(temp / "normal.ppm")
    held = capture(temp / "held.ppm", "--hold-key", "space")
    assert normal != held, "holding the named key did not change rendered geometry"
    assert normal[:3] == held[:3], "background changed instead of the scene"

    bad = subprocess.run(
        [str(binary), "run", str(fixture), "--frames", "1", "--hold-key", "nope"],
        capture_output=True, text=True, timeout=10,
    )
    assert bad.returncode != 0 and "invalid input event" in bad.stderr, bad.stderr

print("Injected key changed the Lua-authored GPU frame")
