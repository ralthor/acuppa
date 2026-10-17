import os
from pathlib import Path
import subprocess
import sys
import tempfile


binary = Path(sys.argv[1]).resolve()


def run(*args):
    return subprocess.run(
        [str(binary), *map(str, args)], capture_output=True, text=True, timeout=5,
        env={**os.environ, "SDL_VIDEODRIVER": "dummy", "SDL_RENDER_DRIVER": "software"},
    )


with tempfile.TemporaryDirectory() as temporary:
    project = Path(temporary) / "new-game"
    result = run("init", project)
    assert result.returncode == 0, result.stderr
    result = run("init", "")
    assert result.returncode != 0 and "directory path" in result.stderr
    for name in ("game.lua", "scenes/main.scene.lua", "scripts/main.lua", "tests/smoke.test.lua"):
        assert (project / name).is_file(), name

    script = project / "scripts/main.lua"
    original_script = script.read_text()
    script.write_text('local tp = require("threepoint")\nreturn {start = function() print("Hello from ThreePoint"); tp.app.quit() end}\n')
    result = run("run", project)
    assert result.returncode == 0, result.stderr
    assert "started 'New Game'" in result.stdout and "stopped" in result.stdout
    assert "Hello from ThreePoint" in result.stdout

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
    assert "game.lua:1:" in result.stderr
    game.write_text(original)

    script.write_text("return {\n")
    result = run("run", project)
    assert result.returncode != 0 and "scripts/main.lua:2:" in result.stderr, result.stderr

    script.write_text("""local tp = require("threepoint")
local ticks = 0
local updated, drawn = false, false
return {
  start = function() print("callback:start") end,
  fixed_update = function(dt)
    assert(dt == 1 / 60)
    ticks = ticks + 1
    if ticks == 1 then print("callback:fixed_update") end
  end,
  update = function(dt)
    assert(dt >= 0 and dt <= 0.25)
    if not updated then print("callback:update"); updated = true end
  end,
  draw = function()
    if not drawn then print("callback:draw"); drawn = true end
    if ticks >= 2 then tp.app.quit() end
  end
}
""")
    result = run("run", project)
    assert result.returncode == 0, result.stderr
    callbacks = [line for line in result.stdout.splitlines() if line.startswith("callback:")]
    assert callbacks[0] == "callback:start"
    assert set(callbacks) == {"callback:start", "callback:fixed_update", "callback:update", "callback:draw"}
    assert len(callbacks) == 4

    for callback in ("start", "fixed_update", "update", "draw"):
        script.write_text(f'return {{\n  {callback} = function() error("boom") end\n}}\n')
        result = run("run", project)
        assert result.returncode != 0 and "scripts/main.lua:2:" in result.stderr, result.stderr
        assert "boom" in result.stderr and "stopped" not in result.stdout

    script.write_text('error({})\nreturn {}\n')
    result = run("run", project)
    assert result.returncode != 0 and "scripts/main.lua:1:" in result.stderr, result.stderr

    script.write_text('return {start = true}\n')
    result = run("run", project)
    assert result.returncode != 0 and "'start' must be a function" in result.stderr
    script.write_text(original_script)

    (project / "scenes/main.scene.lua").unlink()
    result = run("run", project)
    assert result.returncode != 0 and "scenes/main.scene.lua: file not found" in result.stderr

print("ThreePoint CLI checks passed")
