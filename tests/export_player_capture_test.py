"""A relocated standalone player renders without editor or source checkout paths."""

from pathlib import Path
import shutil
import subprocess
import sys
import time


cli = Path(sys.argv[1]).resolve()
source = Path(sys.argv[2]).resolve()
work = Path(sys.argv[3]).resolve()
shutil.rmtree(work, ignore_errors=True)
work.mkdir(parents=True)
bundle = work / "bundle"
made = subprocess.run([str(cli), "build", str(source), str(bundle)],
                      capture_output=True, text=True, timeout=30)
assert made.returncode == 0, made.stderr
player = bundle / "threepoint-game"
capture = work / "player.ppm"
result = subprocess.run([str(player), "--frames", "2", "--capture", str(capture)],
                        cwd=work, capture_output=True, text=True, timeout=30)
assert result.returncode == 0 and "Validation Error" not in result.stderr, result.stderr
header, pixels = capture.read_bytes().split(b"\n255\n", 1)
assert header == b"P6\n320 240" and len(pixels) == 320 * 240 * 3
background = pixels[:3]
different = sum(pixels[i:i + 3] != background for i in range(0, len(pixels), 3))
assert different > 1000, "exported cube scene did not render"

live = subprocess.Popen([str(player)], cwd=work, stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE, text=True)
try:
    time.sleep(2)
    assert live.poll() is None, "standalone interactive game exited during startup"
finally:
    live.terminate()
    stdout, stderr = live.communicate(timeout=10)
assert "Validation Error" not in stderr, stderr
