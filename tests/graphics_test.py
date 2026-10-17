import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import time


binary = Path(sys.argv[1]).resolve()
video_env = {
    **os.environ,
    "SDL_VIDEODRIVER": "dummy",
    "SDL_RENDER_DRIVER": "software",
    "SDL_VIDEO_DUMMY_SAVE_FRAMES": "1",
}


def run(*args):
    return subprocess.run(
        [str(binary), *map(str, args)], env=video_env,
        capture_output=True, text=True, timeout=5,
    )


def pixel(data, x, y):
    assert data[:2] == b"BM"
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    depth = struct.unpack_from("<H", data, 28)[0]
    assert depth in (24, 32)
    stride = (width * depth + 31) // 32 * 4
    row = height - 1 - y if height > 0 else y
    position = offset + row * stride + x * (depth // 8)
    blue, green, red = data[position:position + 3]
    return red, green, blue


with tempfile.TemporaryDirectory() as temporary:
    project = Path(temporary) / "game"
    assert run("init", project).returncode == 0
    starter = subprocess.Popen(
        [str(binary), "run", str(project)], env=video_env,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
    )
    try:
        deadline = time.monotonic() + 3
        while len(list(project.glob("SDL_window*.bmp"))) < 4 and time.monotonic() < deadline:
            if starter.poll() is not None:
                break
            time.sleep(0.01)
        starter_frames = sorted(project.glob("SDL_window*.bmp"))
    finally:
        if starter.poll() is None:
            starter.terminate()
        _, starter_error = starter.communicate(timeout=2)
    assert len(starter_frames) >= 4, starter_error
    first_image = starter_frames[0].read_bytes()
    later_image = starter_frames[2].read_bytes()
    first_x = next(x for x in range(0, 400) if pixel(first_image, x, 280) == (85, 187, 255))
    last_x = next(x for x in range(0, 400) if pixel(later_image, x, 280) == (85, 187, 255))
    assert last_x > first_x, (first_x, last_x)
    for frame in project.glob("SDL_window*.bmp"):
        frame.unlink()

    script = project / "scripts/main.lua"
    script.write_text("""local tp = require("threepoint")
local x, ticks, frames = 10, 0, 0
return {
  start = function()
    tp.camera.set(5, 0, 2)
    local world_x, world_y = tp.camera.to_world(10, 20)
    assert(world_x == 10 and world_y == 10)
  end,
  fixed_update = function(dt)
    assert(dt == 1 / 60)
    x, ticks = x + 5, ticks + 1
  end,
  update = function(dt) assert(dt >= 0 and dt <= 0.25) end,
  draw = function()
    frames = frames + 1
    tp.draw.rect(x, 10, 6, 6, "#ff0000")
    tp.draw.text("Move", 10, 30, "#ffffff")
    tp.draw.line(10, 55, 40, 55, "#00ff00")
    tp.draw.circle(70, 20, 8, "#0000ff")
    if frames >= 2 and ticks >= 3 then tp.app.quit() end
  end
}
""")
    result = run("run", project)
    assert result.returncode == 0 and "stopped" in result.stdout, result.stderr
    frames = sorted(project.glob("SDL_window*.bmp"))
    assert len(frames) >= 2
    first_image = frames[0].read_bytes()
    later_image = frames[-1].read_bytes()
    first_x = next(x for x in range(10, 300) if pixel(first_image, x, 24) == (255, 0, 0))
    last_x = next(x for x in range(10, 300) if pixel(later_image, x, 24) == (255, 0, 0))
    assert last_x > first_x, (first_x, last_x)
    assert any(pixel(later_image, x, y) == (255, 255, 255)
               for x in range(10, 75) for y in range(60, 76))
    assert pixel(later_image, 10, 110) == (0, 255, 0)
    assert pixel(later_image, 130, 40) == (0, 0, 255)

    script.write_text('local tp = require("threepoint")\nreturn {start = function() tp.draw.rect(0, 0, 5, 5, "#ffffff") end}\n')
    result = run("run", project)
    assert result.returncode != 0 and "scripts/main.lua:2:" in result.stderr
    assert "only run in draw()" in result.stderr

    script.write_text('local tp = require("threepoint")\nreturn {draw = function() tp.draw.rect(0, 0, 5, 5, "red") end}\n')
    result = run("run", project)
    assert result.returncode != 0 and "scripts/main.lua:2:" in result.stderr
    assert "#RRGGBB" in result.stderr

print("ThreePoint graphics checks passed")
