"""Both authored falling-block games run from relocatable standalone bundles."""

from pathlib import Path
import os
import shutil
import subprocess
import sys
import time


cli = Path(sys.argv[1]).resolve()
games = [Path(sys.argv[2]).resolve(), Path(sys.argv[3]).resolve()]
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


for source in games:
    bundle = work / source.name
    invoke(cli, "build", source, bundle)
    player = bundle / "threepoint-game"
    assert player.is_file() and not (bundle / "threepoint-editor").exists()
    assert not (bundle / "game/README.md").exists()
    assert not (bundle / "game/make_assets.py").exists()
    invoke(player, "--check")
    if not graphical:
        invalid = subprocess.run([str(player), "--hold-key", ""], cwd=work,
                                 capture_output=True, text=True, timeout=10)
        assert invalid.returncode != 0 and "needs a name" in invalid.stderr
        continue

    def capture(name, *extra):
        target = work / f"{source.name}_{name}.ppm"
        invoke(player, "--frames", "2", "--capture", target, *extra)
        header, pixels = target.read_bytes().split(b"\n255\n", 1)
        assert header == b"P6\n1280 720" and len(pixels) == 1280 * 720 * 3
        return pixels

    normal = capture("normal")
    left = capture("left", "--hold-key", "left")
    dropped = capture("drop", "--hold-key", "space")
    assert normal != left and normal != dropped, "packaged input did not change the 3D game"

    live = subprocess.Popen([str(player)], cwd=work, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, text=True)
    try:
        time.sleep(1.5)
        assert live.poll() is None, "packaged game exited during interactive startup"
    finally:
        live.terminate()
        _, stderr = live.communicate(timeout=10)
    assert "Validation Error" not in stderr, stderr
