"""The editor viewport and selection handles must appear in Vulkan captures."""

from pathlib import Path
import subprocess
import sys


editor = Path(sys.argv[1]).resolve()
project = Path(sys.argv[2]).resolve()
output = Path(sys.argv[3]).resolve()


def capture(name, selected=False):
    path = output / f"editor_{name}.ppm"
    command = [str(editor), "viewport", str(project), "editor"]
    if selected:
        command.extend(["--select", "actor"])
    command.extend(["--frames", "2", "--capture", str(path)])
    result = subprocess.run(command, capture_output=True, text=True, check=True)
    assert "Validation Error" not in result.stderr, result.stderr
    raw = path.read_bytes()
    header, pixels = raw.split(b"\n255\n", 1)
    assert header == b"P6\n1280 720", header
    return pixels


plain = capture("plain")
selected = capture("selected", True)


def red_axis_pixels(pixels):
    count = 0
    for y in range(180, 550):
        for x in range(640, 1150):
            offset = (y * 1280 + x) * 3
            r, g, b = pixels[offset:offset + 3]
            if r > 225 and g < 155 and b < 160:
                count += 1
    return count


assert red_axis_pixels(selected) > red_axis_pixels(plain) + 400, \
    "selected transform axis was not visible over the scene"
assert plain != selected, "selection did not alter the viewport image"
