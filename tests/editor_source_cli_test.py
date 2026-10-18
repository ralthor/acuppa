"""Raw source commands round-trip authored Lua without editor clicks."""

from pathlib import Path
import shutil
import subprocess
import sys


editor = Path(sys.argv[1]).resolve()
source = Path(sys.argv[2]).resolve()
work = Path(sys.argv[3]).resolve()
if work.exists():
    shutil.rmtree(work)
shutil.copytree(source, work)


def command(*args, input_text=None, success=True):
    result = subprocess.run([str(editor), *map(str, args)], input=input_text,
                            capture_output=True, text=True)
    assert (result.returncode == 0) == success, result.stdout + result.stderr
    return result.stdout if success else result.stderr


scene = "scenes/editor.scene.lua"
script = "scripts/editor.lua"
original = (work / scene).read_text()
assert command("source", work, scene) == original
assert command("source", work, "scenes/shadow.scene.lua")

updated = original.replace("{1, 0, 0}", "{5, 0, 0}") + "-- source-view edit\n"
command("replace", work, scene, input_text=updated)
assert command("source", work, scene) == updated
assert (work / scene).read_text() == updated
assert "actor" in command("tree", work, "editor")

before = (work / script).read_text()
assert "invalid Lua" in command("replace", work, script,
                                input_text="return {\n", success=False)
assert (work / script).read_text() == before
assert "outside project" in command("source", work, "../other.lua", success=False)
assert "relative .lua" in command("source", work, "assets/triangle.gltf", success=False)
