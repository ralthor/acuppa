"""Exported Siege Sling keeps its assets, rules, gameplay, and Vulkan view."""

from pathlib import Path
import os
import shutil
import subprocess
import sys
import time


cli = Path(sys.argv[1]).resolve()
source = Path(sys.argv[2]).resolve()
gameplay_test = Path(sys.argv[3]).resolve()
work = Path(sys.argv[4]).resolve()
graphical = len(sys.argv) == 6 and sys.argv[5] == "--graphical"
shutil.rmtree(work, ignore_errors=True)
work.mkdir(parents=True)
os.environ["XDG_DATA_HOME"] = str(work / "user_data")


def invoke(*command, timeout=30):
    result = subprocess.run([str(value) for value in command], cwd=work,
                            capture_output=True, text=True, timeout=timeout)
    assert result.returncode == 0 and "Validation Error" not in result.stderr, (
        result.stdout + result.stderr)
    return result


bundle = work / "bundle"
invoke(cli, "build", source, bundle)
player = bundle / "acuppa-game"
assert player.is_file() and not (bundle / "acuppa-editor").exists()
for asset in ("twang.wav", "wood.gltf", "wood.png", "day.hdr"):
    assert (bundle / "game/assets" / asset).is_file(), asset
assert (bundle / "game/levels/level1.lua").is_file()
assert not (bundle / "game/make_assets.py").exists()
invoke(player, "--check")
invoke(gameplay_test, bundle / "game", work / "gameplay_fixture")

if graphical:
    def capture(name, *extra):
        target = work / f"{name}.ppm"
        invoke(player, "--frames", "120" if extra else "2", "--capture", target,
               *extra, timeout=40)
        header, pixels = target.read_bytes().split(b"\n255\n", 1)
        assert header == b"P6\n1280 720" and len(pixels) == 1280 * 720 * 3
        return pixels

    aiming = capture("start")
    fired = capture("fired", "--hold-key", "space")
    assert aiming != fired, "exported sling did not fire in the Vulkan view"

    live = subprocess.Popen([str(player)], cwd=work, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, text=True)
    try:
        time.sleep(1.5)
        assert live.poll() is None, "exported Siege Sling exited during startup"
    finally:
        live.terminate()
        _, stderr = live.communicate(timeout=10)
    assert "Validation Error" not in stderr, stderr
