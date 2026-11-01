"""A real Vulkan frame must show stronger row feedback for a four-line clear."""

from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


binary = Path(sys.argv[1]).resolve()
source = Path(sys.argv[2]).resolve()


def capture(root, count, drop=True):
    project = root / f"clear_{count}_{'drop' if drop else 'idle'}"
    shutil.copytree(source, project)
    board = project / "scripts/board.lua"
    contents = board.read_text()
    assert contents.endswith("return Board\n")
    fixture = f"""
local original_new, original_spawn = Board.new, Board.spawn
Board.new = function(seed)
  local self = original_new(seed)
  for y = 1, {count} do
    for x = 2, Board.width do self.rows[y][x] = "T" end
  end
  return self
end
Board.spawn = function(self, ...)
  if not self.fixture_spawned then
    self.fixture_spawned = true
    self.active = {{kind = "I", x = -2, y = 16, rotation = 1}}
    return true
  end
  return original_spawn(self, ...)
end
"""
    board.write_text(contents[:-len("return Board\n")] + fixture + "return Board\n")
    image = root / f"clear_{count}_{'drop' if drop else 'idle'}.ppm"
    result = subprocess.run(
        [str(binary), "run", str(project), "--frames", "2", "--capture", str(image),
         *(["--hold-key", "space"] if drop else [])],
        capture_output=True, text=True, timeout=25,
    )
    assert result.returncode == 0 and "Validation Error" not in result.stderr, (
        result.stdout + result.stderr
    )
    header, pixels = image.read_bytes().split(b"\n255\n", 1)
    assert header == b"P6\n1280 720" and len(pixels) == 1280 * 720 * 3
    return pixels


def extra_brightness(pixels, baseline, y):
    return max(sum(pixels[(row * 1280 + x) * 3] -
                   baseline[(row * 1280 + x) * 3] for x in range(590, 691))
               for row in range(y - 12, y + 13))


def title_top(pixels):
    for y in range(0, 90):
        if sum(1 for x in range(350, 900)
               if pixels[(y * 1280 + x) * 3] > 140 and
               pixels[(y * 1280 + x) * 3 + 1] > 110) > 10:
            return y
    raise AssertionError("title was absent from the capture")


def spark_pixels(pixels):
    return sum(1 for y in range(490, 625) for x in range(510, 770)
               if pixels[(y * 1280 + x) * 3] > 210 and
               pixels[(y * 1280 + x) * 3 + 1] > 190 and
               pixels[(y * 1280 + x) * 3 + 2] < 150)


with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    idle = capture(root, 0, False)
    ordinary = capture(root, 0)
    single = capture(root, 1)
    four = capture(root, 4)
    assert title_top(ordinary) <= title_top(idle) - 2, "landing bump was invisible"
    assert title_top(single) <= title_top(ordinary) - 2, "single-line shake was too small"
    assert title_top(four) <= title_top(single) - 8, "four-line shake was too small"
    assert spark_pixels(single) >= 100 and spark_pixels(four) >= 3 * spark_pixels(single), (
        "the colored burst did not grow with the line count"
    )
    # The cleared rows occupy the bottom four board cells in this fixture.
    assert extra_brightness(single, ordinary, 607) > 8000
    for y in (581, 555, 529):
        assert extra_brightness(four, single, y) > 8000, (
            "four-line clear did not flash every removed row"
        )

print("Falling Blocks Lit line-clear flash rendered in Vulkan")
