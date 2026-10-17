"""Checked-in art must reproduce from the source generator."""

from pathlib import Path
import subprocess
import sys
import tempfile


game = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory() as directory:
    output = Path(directory)
    subprocess.run([sys.executable, str(game / "make_assets.py"), str(output)], check=True)
    expected = {path.name: path.read_bytes() for path in (game / "assets").iterdir()}
    actual = {path.name: path.read_bytes() for path in output.iterdir()}
    assert expected == actual, "checked-in tiles or HDRI differ from make_assets.py output"

print("Falling Blocks Lit art reproduces from source")
