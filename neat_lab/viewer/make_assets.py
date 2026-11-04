"""Generate the NEAT lander viewer's checked-in glTF boxes and HDRI from code."""

import json
import math
from pathlib import Path
import struct
import sys

output = Path(sys.argv[1]) if len(sys.argv) == 2 else Path(__file__).parent / "assets"
output.mkdir(parents=True, exist_ok=True)

faces = [
    ((0, 0, 1), [(-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)]),
    ((0, 0, -1), [(1, -1, -1), (-1, -1, -1), (-1, 1, -1), (1, 1, -1)]),
    ((1, 0, 0), [(1, -1, 1), (1, -1, -1), (1, 1, -1), (1, 1, 1)]),
    ((-1, 0, 0), [(-1, -1, -1), (-1, -1, 1), (-1, 1, 1), (-1, 1, -1)]),
    ((0, 1, 0), [(-1, 1, 1), (1, 1, 1), (1, 1, -1), (-1, 1, -1)]),
    ((0, -1, 0), [(-1, -1, -1), (1, -1, -1), (1, -1, 1), (-1, -1, 1)]),
]
positions = [vertex for _, vertices in faces for vertex in vertices]
normals = [normal for normal, _ in faces for _ in range(4)]
indices = [index for face in range(6) for index in
           (face * 4, face * 4 + 1, face * 4 + 2, face * 4, face * 4 + 2, face * 4 + 3)]
chunks = [
    struct.pack(f"<{len(positions) * 3}f", *(v for row in positions for v in row)),
    struct.pack(f"<{len(normals) * 3}f", *(v for row in normals for v in row)),
    struct.pack(f"<{len(indices)}H", *indices),
]
binary = b"".join(chunks)
(output / "box.bin").write_bytes(binary)
offsets = [0, len(chunks[0]), len(chunks[0]) + len(chunks[1])]


def box(name, color, metallic=0.0, roughness=0.7, emissive=(0, 0, 0)):
    asset = {
        "asset": {"version": "2.0", "generator": "ACuppa NEAT lander viewer source"},
        "buffers": [{"uri": "box.bin", "byteLength": len(binary)}],
        "bufferViews": [{"buffer": 0, "byteOffset": offset, "byteLength": len(chunk)}
                        for offset, chunk in zip(offsets, chunks)],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": len(positions),
             "type": "VEC3", "min": [-1, -1, -1], "max": [1, 1, 1]},
            {"bufferView": 1, "componentType": 5126, "count": len(normals), "type": "VEC3"},
            {"bufferView": 2, "componentType": 5123, "count": len(indices), "type": "SCALAR"},
        ],
        "materials": [{"pbrMetallicRoughness": {
            "baseColorFactor": [*color, 1], "metallicFactor": metallic,
            "roughnessFactor": roughness}, "emissiveFactor": list(emissive)}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1},
                                   "indices": 2, "material": 0}]}],
        "nodes": [{"mesh": 0}], "scenes": [{"nodes": [0]}], "scene": 0,
    }
    (output / f"{name}.gltf").write_text(json.dumps(asset, indent=2) + "\n")


box("champion", (0.95, 0.66, 0.18), 0.65, 0.28)
box("cabin", (0.82, 0.86, 0.9), 0.1, 0.25)
box("flame", (1.0, 0.45, 0.08), 0.0, 0.9, (4.0, 1.4, 0.2))
box("ground", (0.06, 0.06, 0.07), 0.0, 0.95)
box("pad", (0.05, 0.5, 0.22), 0.1, 0.5, (0.01, 0.12, 0.04))
# One muted color per species ghost; the viewer cycles through them by species ID.
for index, color in enumerate([(0.15, 0.45, 0.75), (0.65, 0.2, 0.6), (0.35, 0.6, 0.15),
                               (0.75, 0.35, 0.12), (0.4, 0.3, 0.75), (0.1, 0.55, 0.5),
                               (0.7, 0.25, 0.3)], 1):
    box(f"ghost_{index}", color, 0.2, 0.6)


def rgbe(rgb):
    peak = max(rgb)
    if peak < 1e-32:
        return bytes(4)
    _, exponent = math.frexp(peak)
    scale = math.ldexp(256.0, -exponent)
    return bytes((*(min(255, int(value * scale)) for value in rgb), exponent + 128))


# A dim night-sky studio: cool overhead light and a faint warm horizon band.
width, height = 32, 16
with (output / "night.hdr").open("wb") as file:
    file.write(f"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y {height} +X {width}\n".encode())
    for y in range(height):
        row = []
        for x in range(width):
            v = (y + 0.5) / height
            sky = max(0.0, 1 - 2 * v)
            band = math.exp(-((v - 0.5) / 0.08) ** 2)
            row.append(rgbe((0.18 + 0.25 * sky + 0.5 * band,
                             0.2 + 0.3 * sky + 0.35 * band,
                             0.3 + 0.55 * sky + 0.2 * band)))
        file.write(bytes((2, 2, 0, width)))
        for channel in range(4):
            file.write(bytes((width, *(pixel[channel] for pixel in row))))
