"""Checked-in art and sound must reproduce from the source generator."""

from pathlib import Path
import subprocess
import sys
import tempfile
import wave


game = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory() as directory:
    output = Path(directory)
    subprocess.run([sys.executable, str(game / "make_assets.py"), str(output)], check=True)
    expected = {path.name: path.read_bytes() for path in (game / "assets").iterdir()}
    actual = {path.name: path.read_bytes() for path in output.iterdir()}
    assert expected == actual, "checked-in art or sound differs from make_assets.py output"

    for prefix in ("land", "clear"):
        lengths = []
        for index in range(1, 5):
            with wave.open(str(output / f"{prefix}_{index}.wav"), "rb") as cue:
                assert cue.getnchannels() == 1 and cue.getsampwidth() == 2
                lengths.append(cue.getnframes())
        assert len(set(lengths)) == 4, f"{prefix} cues have identical durations"

print("Falling Blocks Lit art and varied sound cues reproduce from source")
