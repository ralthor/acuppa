"""Headless CLI checks every declared scene, including one beyond the first."""

from pathlib import Path
import shutil
import subprocess
import sys


binary = Path(sys.argv[1]).resolve()
project = Path(sys.argv[2]).resolve()
work = Path(sys.argv[3]).resolve()


def run(root):
    return subprocess.run([str(binary), "test", str(root)], capture_output=True,
                          text=True, timeout=30)


result = run(project)
assert result.returncode == 0, result.stderr
assert "PASS main" in result.stdout and "PASS alternate" in result.stdout
assert "source scenes headlessly" in result.stdout

shutil.rmtree(work, ignore_errors=True)
shutil.copytree(project, work, ignore=shutil.ignore_patterns("saves"))
(work / "scripts/alternate.lua").write_text("return {\n")
result = run(work)
assert result.returncode == 1 and "scene 'alternate'" in result.stderr, result.stderr
assert "alternate.lua" in result.stderr, result.stderr

result = subprocess.run([str(binary), "test"], capture_output=True, text=True)
assert result.returncode != 0 and "Usage:" in result.stderr
