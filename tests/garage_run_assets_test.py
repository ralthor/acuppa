"""Every checked-in Garage Run asset must reproduce from its source generator."""

from pathlib import Path
import shutil
import subprocess
import sys


game = Path(sys.argv[1]).resolve()
output = Path(sys.argv[2]).resolve()
if output.exists():
    shutil.rmtree(output)
output.mkdir(parents=True)
subprocess.run([sys.executable, str(game / "make_assets.py"), str(output)], check=True)
expected = {path.name: path.read_bytes() for path in (game / "assets").iterdir()}
actual = {path.name: path.read_bytes() for path in output.iterdir()}
assert expected == actual, "Garage Run assets differ from their code generator"
