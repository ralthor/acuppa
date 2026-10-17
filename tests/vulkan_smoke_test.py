import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile


binary = Path(sys.argv[1]).resolve()


def run(*arguments, environment=None):
    return subprocess.run(
        [str(binary), *map(str, arguments)], capture_output=True, text=True,
        timeout=15, env=environment,
    )


with tempfile.TemporaryDirectory() as directory:
    capture = Path(directory) / "clear.ppm"
    result = run("--capture-clear", capture)
    assert result.returncode == 0, result.stdout + result.stderr
    header, pixels = capture.read_bytes().split(b"\n255\n", 1)
    assert header == b"P6\n960 540", header
    assert len(pixels) == 960 * 540 * 3
    rgb = pixels[:3]
    assert rgb[0] < rgb[1] < rgb[2], rgb
    assert pixels == rgb * (960 * 540), "the clear did not cover the whole swapchain"

    result = run("--smoke-vulkan")
    assert result.returncode == 0, result.stdout + result.stderr
    match = re.search(r"Rendered 6 clear frames; swapchain rebuilds: (\d+)", result.stdout)
    assert match and int(match.group(1)) >= 1, result.stdout
    assert "Swapchain: 1056x604" in result.stdout, result.stdout

    missing_driver = {**os.environ, "VK_ICD_FILENAMES": "/nonexistent/threepoint-icd.json",
                      "VK_DRIVER_FILES": "/nonexistent/threepoint-icd.json"}
    result = run("--smoke-vulkan", environment=missing_driver)
    assert result.returncode != 0, result.stdout + result.stderr
    assert "Vulkan" in result.stderr, result.stderr

print("Vulkan clear, resize, shutdown, and missing-driver checks passed")
