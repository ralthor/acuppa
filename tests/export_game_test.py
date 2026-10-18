"""Exported game is source complete, isolated, relocatable, and non-editable."""

from pathlib import Path
import os
import shutil
import subprocess
import sys


cli = Path(sys.argv[1]).resolve()
source = Path(sys.argv[2]).resolve()
work = Path(sys.argv[3]).resolve()
shutil.rmtree(work, ignore_errors=True)
work.mkdir(parents=True)
os.environ["XDG_DATA_HOME"] = str(work / "user_data")
project = work / "project"
shutil.copytree(source, project)
(project / "scripts/main.lua").write_text('''local tp = require("threepoint")
return {
  start = function()
    local old = tp.load("release_probe")
    tp.save("release_probe", {count = old and old.count + 1 or 1})
  end,
  fixed_update = function() end,
  update = function() end,
  draw = function() end,
}
''')
(project / "README.md").write_text("Development instructions only\n")
(project / "make_assets.py").write_text("print('not needed at runtime')\n")
(project / "tests").mkdir()
(project / "tests" / "fixture.lua").write_text("error('not part of game')\n")
(project / "saves").mkdir()
(project / "saves" / "old.tpsave").write_text("old save\n")


def build(root, output):
    return subprocess.run([str(cli), "build", str(root), str(output)],
                          capture_output=True, text=True, timeout=30)


bundle = work / "bundle"
result = build(project, bundle)
assert result.returncode == 0, result.stderr
files = {p.relative_to(bundle).as_posix() for p in bundle.rglob("*") if p.is_file()}
assert files == {
    "threepoint-game", "game.id", "game/game.lua", "game/scenes/main.scene.lua",
    "game/scenes/alternate.scene.lua", "game/scripts/main.lua",
    "game/scripts/alternate.lua",
}, files
assert (bundle / "threepoint-game").stat().st_mode & 0o111
relocated = work / "relocated"
bundle.rename(relocated)
player = relocated / "threepoint-game"
(relocated / "game").chmod(0o555)
checked = subprocess.run([str(player), "--check"], cwd=work,
                         capture_output=True, text=True, timeout=10)
assert checked.returncode == 0 and "passed" in checked.stdout, checked.stderr
identity = (relocated / "game.id").read_text().strip()
saved = work / "user_data/threepoint" / identity / "saves/release_probe.tpsave"
assert saved.is_file() and not (relocated / "game/saves").exists(), \
    "exported game did not save outside the bundle"
first_save = saved.read_bytes()
checked = subprocess.run([str(player), "--check"], cwd=work,
                         capture_output=True, text=True, timeout=10)
assert checked.returncode == 0 and saved.read_bytes() != first_save, checked.stderr
(relocated / "game.id").write_text("../unsafe\n")
unsafe = subprocess.run([str(player), "--check"], cwd=work,
                        capture_output=True, text=True, timeout=10)
assert unsafe.returncode != 0 and "game.id" in unsafe.stderr, unsafe.stderr
(relocated / "game.id").write_text(identity + "\n")
(relocated / "game").chmod(0o755)
assert build(project, relocated).returncode != 0
assert build(project, project / "export").returncode != 0

broken = work / "broken"
shutil.copytree(source, broken)
(broken / "scripts/alternate.lua").write_text("return {\n")
assert build(broken, work / "bad-script").returncode != 0
assert not (work / "bad-script").exists()

linked = work / "linked"
shutil.copytree(source, linked)
(linked / "assets").mkdir()
(linked / "assets" / "alias.wav").symlink_to(linked / "game.lua")
result = build(linked, work / "bad-link")
assert result.returncode != 0 and "symlinks" in result.stderr, result.stderr
assert not (work / "bad-link").exists()
