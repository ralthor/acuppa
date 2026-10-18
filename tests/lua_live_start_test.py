"""The interactive Lua scene must survive startup without a fixed timestep."""

from pathlib import Path
import subprocess
import sys


binary = Path(sys.argv[1]).resolve()
project = Path(sys.argv[2]).resolve()
command = [str(binary), "run", str(project)]
if len(sys.argv) == 4:
    command.extend(["--scene", sys.argv[3]])
process = subprocess.Popen(command,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                           text=True)
try:
    try:
        stdout, stderr = process.communicate(timeout=3)
    except subprocess.TimeoutExpired:
        process.terminate()
        stdout, stderr = process.communicate(timeout=5)
    else:
        raise AssertionError("interactive scene exited during startup:\n" + stdout + stderr)
    assert "ACuppa:" not in stderr and "Validation Error" not in stderr, stderr
finally:
    if process.poll() is None:
        process.kill()
        process.communicate()

print("Interactive Lua scene stayed open through startup")
