"""A human or agent can inspect and change a source scene with editor commands."""

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


def command(*arguments, success=True):
    result = subprocess.run([str(editor), *map(str, arguments)],
                            capture_output=True, text=True)
    assert (result.returncode == 0) == success, result.stdout + result.stderr
    return result.stdout if success else result.stderr


tree = command("tree", work, "editor")
assert "anchor [transform]\n  actor [mesh, transform]" in tree, tree
details = command("inspect", work, "editor", "actor")
assert "builtin:cube" in details and "parent" in details, details
command("add", work, "editor", "cli cube")
command("set", work, "editor", "cli cube", "transform", "position", "{3, 0, 0}")
command("set", work, "editor", "cli cube", "mesh", "mesh", '"builtin:cube"')
assert "cli cube [mesh, transform]" in command("tree", work, "editor")
assert "builtin:cube" in command("inspect", work, "editor", "cli cube")
before = (work / "scenes/editor.scene.lua").read_bytes()
assert "duplicate" in command("add", work, "editor", "cli cube", success=False)
assert "unsupported" in command("set", work, "editor", "cli cube",
                                "mesh", "mesh", "function() end", success=False)
assert "declarative" in command("tree", work, "shadow", success=False)
assert (work / "scenes/editor.scene.lua").read_bytes() == before
