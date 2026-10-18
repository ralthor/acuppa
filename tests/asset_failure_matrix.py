"""Whole projects and bundles reject damaged assets with named errors."""

from pathlib import Path
import os
import shutil
import subprocess
import sys


cli = Path(sys.argv[1]).resolve()
lit = Path(sys.argv[2]).resolve()
garage = Path(sys.argv[3]).resolve()
work = Path(sys.argv[4]).resolve()
shutil.rmtree(work, ignore_errors=True)
work.mkdir(parents=True)
os.environ["XDG_DATA_HOME"] = str(work / "user_data")


def check_failure(binary, *args, expected):
    result = subprocess.run([str(binary), *map(str, args)], cwd=work,
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 1, (result.returncode, result.stdout, result.stderr)
    assert expected in result.stderr, result.stderr


def damaged(source, name, asset, content=None):
    target = work / name
    shutil.copytree(source, target)
    file = target / asset
    if content is None:
        file.unlink()
    else:
        file.write_bytes(content)
    return target


check_failure(cli, "test", damaged(lit, "missing_mesh_buffer", "assets/tile.bin"),
              expected="tile.bin")
check_failure(cli, "test", damaged(lit, "bad_hdri", "assets/studio.hdr", b"bad HDRI"),
              expected="studio.hdr")
check_failure(cli, "test", damaged(garage, "missing_sound", "assets/motor.wav"),
              expected="motor.wav")
check_failure(cli, "test", damaged(garage, "bad_sound", "assets/motor.wav", b"bad WAV"),
              expected="motor.wav")
check_failure(cli, "test", damaged(garage, "bad_mesh", "assets/car_red.gltf", b"{"),
              expected="car_red.gltf")

bundle = work / "bundle"
result = subprocess.run([str(cli), "build", str(garage), str(bundle)], cwd=work,
                        capture_output=True, text=True, timeout=30)
assert result.returncode == 0, result.stderr
(bundle / "game/assets/motor.wav").unlink()
check_failure(bundle / "threepoint-game", "--check", expected="motor.wav")
