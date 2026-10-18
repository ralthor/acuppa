"""The interactive editor viewport stays open without a fixed frame delta."""

from pathlib import Path
import subprocess
import sys


editor = Path(sys.argv[1]).resolve()
project = Path(sys.argv[2]).resolve()
process = subprocess.Popen([str(editor), "viewport", str(project), "editor"],
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
try:
    try:
        stdout, stderr = process.communicate(timeout=3)
    except subprocess.TimeoutExpired:
        process.terminate()
        stdout, stderr = process.communicate(timeout=5)
    else:
        raise AssertionError("editor viewport exited during startup:\n" + stdout + stderr)
    assert "ThreePoint editor:" not in stderr and "Validation Error" not in stderr, stderr
finally:
    if process.poll() is None:
        process.kill()
        process.communicate()
