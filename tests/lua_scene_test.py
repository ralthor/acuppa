"""Code-only Lua scene integration against the real GPU and invalid inputs."""

from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


binary = Path(sys.argv[1]).resolve()
fixture = Path(sys.argv[2]).resolve()


def run(*arguments):
    return subprocess.run([str(binary), *map(str, arguments)],
                          capture_output=True, text=True, timeout=20)


def load(path):
    header, data = path.read_bytes().split(b"\n255\n", 1)
    assert header == b"P6\n1280 720", header
    assert len(data) == 1280 * 720 * 3, len(data)
    return data


def pixel(data, x, y):
    offset = (y * 1280 + x) * 3
    return tuple(data[offset:offset + 3])


with tempfile.TemporaryDirectory() as directory:
    temp = Path(directory)
    first_path = temp / "first.ppm"
    later_path = temp / "later.ppm"
    first = run("run", fixture, "--frames", 1, "--capture", first_path)
    assert first.returncode == 0, first.stdout + first.stderr
    later = run("run", fixture, "--frames", 60, "--capture", later_path)
    assert later.returncode == 0, later.stdout + later.stderr
    first_pixels = load(first_path)
    later_pixels = load(later_path)
    early_center = pixel(first_pixels, 640, 360)
    late_center = pixel(later_pixels, 640, 360)
    assert early_center[0] > 220 and early_center[1] < 130, early_center
    assert late_center[0] > 220 and late_center[1] > 160, late_center
    assert first_pixels != later_pixels, "Lua rotation produced identical frames"
    assert pixel(first_pixels, 0, 0) == pixel(later_pixels, 0, 0)

    invalid_project = temp / "invalid"
    shutil.copytree(fixture, invalid_project)
    script = invalid_project / "scripts/main.lua"
    script.write_text('local tp = require("threepoint")\nreturn {start=function() '
                      'tp.spawn({mesh="not:a:mesh"}) end, '
                      'fixed_update=function() end, update=function() end}\n')
    bad_mesh = run("run", invalid_project, "--frames", 1)
    assert bad_mesh.returncode != 0 and "mesh path must end" in bad_mesh.stderr, bad_mesh.stderr

    script.write_text('local tp = require("threepoint")\nreturn {start=function() '
                      'tp.spawn({mesh="builtin:cube", scale={0,1,1}}) end, '
                      'fixed_update=function() end, update=function() end}\n')
    bad_scale = run("run", invalid_project, "--frames", 1)
    assert bad_scale.returncode != 0 and "scale" in bad_scale.stderr, bad_scale.stderr

    script.write_text('local tp = require("threepoint")\nreturn {start=function() '
                      'tp.set_rotation(999, {0,0,0}) end, '
                      'fixed_update=function() end, update=function() end}\n')
    bad_id = run("run", invalid_project, "--frames", 1)
    assert bad_id.returncode != 0 and "unknown entity ID" in bad_id.stderr, bad_id.stderr

    script.write_text('local tp = require("threepoint")\nreturn {start=function() '
                      'tp.time(1) end, fixed_update=function() end, '
                      'update=function() end}\n')
    bad_time = run("run", invalid_project, "--frames", 1)
    assert bad_time.returncode != 0 and "time() expects no arguments" in bad_time.stderr, \
        bad_time.stderr

    for expression, expected in (("tp.spawn()", "spawn() expects one table"),
                                 ("tp.quit(1)", "quit() expects no arguments")):
        script.write_text('local tp = require("threepoint")\nreturn {start=function() '
                          + expression + ' end, fixed_update=function() end, '
                          'update=function() end}\n')
        invalid_call = run("run", invalid_project, "--frames", 1)
        assert invalid_call.returncode != 0 and expected in invalid_call.stderr, \
            invalid_call.stderr

    script.write_text('local tp = require("threepoint")\nreturn {start=function() end, '
                      'fixed_update=function() end, update=function() tp.quit() end}\n')
    quit_result = run("run", invalid_project, "--frames", 1)
    assert quit_result.returncode == 0 and "Rendered 0 project frames" in quit_result.stdout, \
        quit_result.stdout + quit_result.stderr

    scene = invalid_project / "scenes/main.scene.lua"
    scene.write_text('return {entities={}, script="../outside.lua"}\n')
    outside_path = run("run", invalid_project, "--frames", 1)
    assert outside_path.returncode != 0 and "outside project" in outside_path.stderr, \
        outside_path.stderr

    scene.write_text('return {entities={}, script="scripts/main.lua"}\n')
    manifest = invalid_project / "game.lua"
    manifest.write_text(manifest.read_text().replace('keyboard = "space"',
                                                    'keyboard_typo = "space"'))
    bad_binding = run("run", invalid_project, "--frames", 1)
    assert bad_binding.returncode != 0 and "unknown input binding field" in bad_binding.stderr, \
        bad_binding.stderr

print("Lua-only cube scene, callbacks, timing, GPU animation, and invalid inputs passed")
