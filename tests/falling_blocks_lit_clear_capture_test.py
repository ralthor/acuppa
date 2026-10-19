"""A real Vulkan frame must show stronger row feedback for a four-line clear."""

from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


binary = Path(sys.argv[1]).resolve()
source = Path(sys.argv[2]).resolve()


def capture(root, count):
    project = root / f"clear_{count}"
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
    image = root / f"clear_{count}.ppm"
    result = subprocess.run(
        [str(binary), "run", str(project), "--frames", "2", "--capture", str(image),
         "--hold-key", "space"], capture_output=True, text=True, timeout=25,
    )
    assert result.returncode == 0 and "Validation Error" not in result.stderr, (
        result.stdout + result.stderr
    )
    header, pixels = image.read_bytes().split(b"\n255\n", 1)
    assert header == b"P6\n1280 720" and len(pixels) == 1280 * 720 * 3
    return pixels


def brightness(pixels, y):
    return sum(pixels[(y * 1280 + x) * 3] for x in range(590, 691))


with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    ordinary = capture(root, 0)
    single = capture(root, 1)
    four = capture(root, 4)
    # The cleared rows occupy the bottom four board cells in this fixture.
    assert brightness(single, 607) > brightness(ordinary, 607) + 2500
    for y in (581, 555, 529):
        assert brightness(four, y) > brightness(single, y) + 2500, (
            "four-line clear did not flash every removed row"
        )

print("Falling Blocks Lit line-clear flash rendered in Vulkan")
