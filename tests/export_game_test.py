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
(project / "scripts/main.lua").write_text('''local tp = require("acuppa")
return {
  start = function()
    local old = tp.load("release_probe")
    tp.save("release_probe", {count = old and old.count + 1 or 1})
    local level = tp.data_read("levels/level1.lua")
    assert(level and level.blocks == 3, "bundled data file was not readable")
    local ok, err = pcall(tp.data_write, "levels/level1.lua", {blocks = 3})
    if not ok then
      assert(err:find("read-only in exported games", 1, true), err)
      tp.save("data_probe", {read_only = true})
    end
  end,
  fixed_update = function() end,
  update = function() end,
  draw = function() end,
}
''')
game_lua = project / "game.lua"
game_lua.write_text(game_lua.read_text().replace(
    "  input = {},", '  input = {},\n  writable = {"levels"},'))
(project / "levels").mkdir()
level_text = "return {\n  blocks = 3,\n}\n"
(project / "levels" / "level1.lua").write_text(level_text)
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
    "acuppa-game", "game.id", "game/game.lua", "game/scenes/main.scene.lua",
    "game/scenes/alternate.scene.lua", "game/scripts/main.lua",
    "game/scripts/alternate.lua", "game/levels/level1.lua",
}, files
assert (bundle / "game/levels/level1.lua").read_text() == level_text
assert (bundle / "acuppa-game").stat().st_mode & 0o111
relocated = work / "relocated"
bundle.rename(relocated)
player = relocated / "acuppa-game"
(relocated / "game").chmod(0o555)
checked = subprocess.run([str(player), "--check"], cwd=work,
                         capture_output=True, text=True, timeout=10)
assert checked.returncode == 0 and "passed" in checked.stdout, checked.stderr
identity = (relocated / "game.id").read_text().strip()
saved = work / "user_data/acuppa" / identity / "saves/release_probe.tpsave"
assert saved.is_file() and not (relocated / "game/saves").exists(), \
    "exported game did not save outside the bundle"
first_save = saved.read_bytes()
probe = work / "user_data/acuppa" / identity / "saves/data_probe.tpsave"
assert probe.is_file(), "exported data_write did not raise the read-only error"
assert (relocated / "game/levels/level1.lua").read_text() == level_text
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
