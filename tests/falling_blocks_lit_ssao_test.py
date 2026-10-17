"""The lit game's optional SSAO should add local depth without changing its HUD."""

from pathlib import Path
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
project = Path(sys.argv[2]).resolve()


def capture(scene, path):
    result = subprocess.run(
        [str(binary), "run", str(project), "--scene", scene, "--frames", "2",
         "--capture", str(path)], capture_output=True, text=True, timeout=20,
    )
    assert result.returncode == 0 and "Validation Error" not in result.stderr, (
        result.stdout + result.stderr
    )
    header, pixels = path.read_bytes().split(b"\n255\n", 1)
    assert header == b"P6\n1280 720" and len(pixels) == 1280 * 720 * 3
    return pixels


def color(pixels, x, y):
    index = (y * 1280 + x) * 3
    return pixels[index:index + 3]


with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    enabled = capture("main", root / "ssao.ppm")
    disabled = capture("ssao_off", root / "off.ppm")
    assert enabled != disabled, "SSAO made no visible change to the game"
    assert color(enabled, 640, 185)[0] < color(disabled, 640, 185)[0] - 5, (
        "active piece did not gain ambient contact shading"
    )
    assert color(enabled, 500, 300)[0] < color(disabled, 500, 300)[0] - 10, (
        "board edge did not gain ambient shading"
    )
    for x, y in [(100, 100), (350, 300), (950, 300), (640, 350)]:
        assert color(enabled, x, y) == color(disabled, x, y), (
            "SSAO changed the background or unoccluded board"
        )

print("Falling Blocks Lit SSAO darkened contacts and preserved distant pixels")
