"""The imported source scene must render its glTF triangle under Vulkan."""

from pathlib import Path
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
fixture = Path(sys.argv[2]).resolve()

with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    capture = root / "triangle.ppm"
    result = subprocess.run([str(binary), "run", str(fixture), "--scene", "imported",
                             "--frames", "1",
                             "--capture", str(capture)], capture_output=True, text=True,
                            timeout=20)
    assert result.returncode == 0, result.stdout + result.stderr
    header, pixels = capture.read_bytes().split(b"\n255\n", 1)
    assert header == b"P6\n1280 720", header
    assert len(pixels) == 1280 * 720 * 3
    background = pixels[:3]
    assert background[0] < background[1] < background[2]
    center = (360 * 1280 + 640) * 3
    assert pixels[center:center + 3] != background, \
        "imported triangle did not cover the image center"
    assert sum(pixels[i:i + 3] != background for i in range(0, len(pixels), 300)) > 100, \
        "imported triangle did not occupy visible pixels"
    invalid = subprocess.run([str(binary), "run", str(fixture), "--scene", "absent",
                              "--frames", "1"], capture_output=True, text=True, timeout=10)
    assert invalid.returncode != 0 and "unknown scene" in invalid.stderr, invalid.stderr

print("glTF triangle rendered from a source scene")
