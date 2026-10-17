"""Embed a compiled SPIR-V shader in the engine binary."""

from pathlib import Path
import sys

source, destination, symbol = sys.argv[1:]
binary = Path(source).read_bytes()
if len(binary) % 4:
    raise ValueError("SPIR-V byte length must be divisible by four")
words = [int.from_bytes(binary[i:i + 4], "little") for i in range(0, len(binary), 4)]
Path(destination).write_text(
    "#pragma once\n#include <cstdint>\n"
    f"inline constexpr uint32_t {symbol}[] = {{\n"
    + "".join(f"    0x{word:08x},\n" for word in words)
    + "};\n"
)
