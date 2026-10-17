from pathlib import Path
import subprocess
import sys
import tempfile


binary = Path(sys.argv[1]).resolve()


def run(*args):
    return subprocess.run([str(binary), *map(str, args)], capture_output=True, text=True)


with tempfile.TemporaryDirectory() as temporary:
    project = Path(temporary) / "new-game"
    result = run("init", project)
    assert result.returncode == 0, result.stderr
    result = run("init", "")
    assert result.returncode != 0 and "directory path" in result.stderr
    for name in ("game.lua", "scenes/main.scene.lua", "scripts/main.lua", "tests/smoke.test.lua"):
        assert (project / name).is_file(), name

    result = run("run", project)
    assert result.returncode == 0, result.stderr
    assert "started 'New Game'" in result.stdout and "stopped" in result.stdout

    game = project / "game.lua"
    original = game.read_text()
    result = run("init", project)
    assert result.returncode != 0 and "already exists" in result.stderr
    assert game.read_text() == original

    game.write_text("return {\n")
    result = run("run", project)
    assert result.returncode != 0 and "game.lua:" in result.stderr
    game.write_text("error({})\n")
    result = run("run", project)
    assert result.returncode != 0 and "game.lua: Lua error" in result.stderr
    game.write_text(original)

    script = project / "scripts/main.lua"
    original_script = script.read_text()
    script.write_text("return {\n")
    result = run("run", project)
    assert result.returncode != 0 and "scripts/main.lua:" in result.stderr
    script.write_text(original_script)

    (project / "scenes/main.scene.lua").unlink()
    result = run("run", project)
    assert result.returncode != 0 and "scenes/main.scene.lua: file not found" in result.stderr

print("ThreePoint CLI checks passed")
