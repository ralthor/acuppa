"""SSAO contact darkening, disable path, quality, and frame stability on Vulkan."""

from pathlib import Path
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
project = Path(sys.argv[2]).resolve()


def capture(scene, path):
    result = subprocess.run(
        [str(binary), "run", str(project), "--scene", scene, "--frames", "1",
         "--capture", str(path)], capture_output=True, text=True, timeout=20,
    )
    assert result.returncode == 0 and "Validation Error" not in result.stderr, (
        result.stdout + result.stderr
    )
    header, pixels = path.read_bytes().split(b"\n255\n", 1)
    assert header == b"P6\n1280 720" and len(pixels) == 1280 * 720 * 3
    return pixels


def red(pixels, x, y):
    return pixels[(y * 1280 + x) * 3]


with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    high = capture("ssao", root / "high.ppm")
    off = capture("ssao_off", root / "off.ppm")
    low = capture("ssao_low", root / "low.ppm")
    shifted = capture("ssao_shift", root / "shifted.ppm")
    repeat = capture("ssao", root / "repeat.ppm")

    assert high == repeat, "identical fixed frames produced changing AO"
    assert red(off, 640, 325) - red(high, 640, 325) >= 15, (
        "SSAO did not darken the block/plane contact"
    )
    assert red(high, 640, 325) > 150, "SSAO made a large black contact halo"
    assert abs(red(high, 500, 360) - red(off, 500, 360)) <= 2, (
        "SSAO darkened a distant, unoccluded plane"
    )
    for x, y in [(580, 360), (700, 360), (640, 300), (640, 420)]:
        assert abs(red(high, x, y) - red(off, x, y)) <= 2, (
            "SSAO left a large halo outside the contact region"
        )
    quality_difference = sum(
        abs(red(high, x, y) - red(low, x, y))
        for y in range(315, 405, 5) for x in range(595, 686, 5)
    )
    assert quality_difference > 100, "SSAO quality level did not affect sampling"
    assert red(shifted, 640, 325) < red(off, 640, 325) - 8, (
        "camera movement lost the contact effect"
    )

print("SSAO passed Vulkan contact, quality, disable, and stability checks")
