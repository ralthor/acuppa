"""Build the checked-in glTF tiles and studio HDRI with Python's standard library."""

import json
import math
from pathlib import Path
import struct
import sys


output = Path(sys.argv[1]) if len(sys.argv) == 2 else Path(__file__).parent / "assets"
output.mkdir(parents=True, exist_ok=True)

faces = (
    ((0, 0, 1), (1, 0, 0), (0, 1, 0)),
    ((0, 0, -1), (-1, 0, 0), (0, 1, 0)),
    ((1, 0, 0), (0, 0, -1), (0, 1, 0)),
    ((-1, 0, 0), (0, 0, 1), (0, 1, 0)),
    ((0, 1, 0), (1, 0, 0), (0, 0, -1)),
    ((0, -1, 0), (1, 0, 0), (0, 0, 1)),
)
grid = (-1.0, -0.82, 0.82, 1.0)
positions, normals, uvs, indices = [], [], [], []
for normal, horizontal, vertical in faces:
    first = len(positions)
    for y, v in enumerate(grid):
        for x, u in enumerate(grid):
            corner = [normal[i] + horizontal[i] * u + vertical[i] * v for i in range(3)]
            inner = [max(-0.82, min(0.82, value)) for value in corner]
            delta = [corner[i] - inner[i] for i in range(3)]
            length = math.sqrt(sum(value * value for value in delta))
            direction = [value / length for value in delta]
            positions.append([inner[i] + 0.18 * direction[i] for i in range(3)])
            normals.append(direction)
            uvs.append((x / 3, y / 3))
    for y in range(3):
        for x in range(3):
            a = first + y * 4 + x
            indices.extend((a, a + 1, a + 5, a, a + 5, a + 4))


def mesh_file(name, vertices, vertex_normals, texcoords, triangles, color, roughness,
              metallic, buffer_name):
    chunks = (
        struct.pack(f"<{len(vertices) * 3}f", *(c for row in vertices for c in row)),
        struct.pack(f"<{len(vertex_normals) * 3}f", *(c for row in vertex_normals for c in row)),
        struct.pack(f"<{len(texcoords) * 2}f", *(c for row in texcoords for c in row)),
        struct.pack(f"<{len(triangles)}H", *triangles),
    )
    buffer = b"".join(chunks)
    (output / buffer_name).write_bytes(buffer)
    offsets = (0, len(chunks[0]), len(chunks[0]) + len(chunks[1]),
               len(chunks[0]) + len(chunks[1]) + len(chunks[2]))
    asset = {
        "asset": {"version": "2.0", "generator": "ThreePoint Falling Blocks source"},
        "buffers": [{"uri": buffer_name, "byteLength": len(buffer)}],
        "bufferViews": [{"buffer": 0, "byteOffset": offset, "byteLength": len(chunk)}
                        for offset, chunk in zip(offsets, chunks)],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": len(vertices),
             "type": "VEC3", "min": [-1, -1, -1], "max": [1, 1, 1]},
            {"bufferView": 1, "componentType": 5126, "count": len(vertices), "type": "VEC3"},
            {"bufferView": 2, "componentType": 5126, "count": len(vertices), "type": "VEC2"},
            {"bufferView": 3, "componentType": 5123, "count": len(triangles), "type": "SCALAR"},
        ],
        "materials": [{"pbrMetallicRoughness": {
            "baseColorFactor": [*color, 1], "metallicFactor": metallic,
            "roughnessFactor": roughness}}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1,
                                               "TEXCOORD_0": 2}, "indices": 3, "material": 0}]}],
        "nodes": [{"mesh": 0}], "scenes": [{"nodes": [0]}], "scene": 0,
    }
    (output / f"{name}.gltf").write_text(json.dumps(asset, indent=2) + "\n")


palette = {
    "I": (0.05, 0.85, 0.95), "O": (1.0, 0.73, 0.05),
    "T": (0.58, 0.18, 0.95), "S": (0.12, 0.82, 0.30),
    "Z": (0.96, 0.12, 0.14), "J": (0.10, 0.30, 0.95),
    "L": (1.0, 0.36, 0.06),
}
mesh_file("I", positions, normals, uvs, indices, palette["I"], 0.27, 0.12, "tile.bin")
for kind in "OTSZJL":
    mesh_file(kind, positions, normals, uvs, indices, palette[kind], 0.27, 0.12,
              "tile.bin")

panel_positions = [(-1, -1, 0), (1, -1, 0), (1, 1, 0), (-1, 1, 0)]
mesh_file("panel", panel_positions, [(0, 0, 1)] * 4,
          [(0, 0), (1, 0), (1, 1), (0, 1)], [0, 1, 2, 0, 2, 3],
          (0.025, 0.045, 0.075), 1.0, 0.0, "panel.bin")


def rgbe(rgb):
    peak = max(rgb)
    if peak < 1e-32:
        return bytes(4)
    _, exponent = math.frexp(peak)
    scale = math.ldexp(256.0, -exponent)
    return bytes((*(min(255, int(channel * scale)) for channel in rgb), exponent + 128))


width, height = 64, 32
with (output / "studio.hdr").open("wb") as file:
    file.write(f"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y {height} +X {width}\n".encode())
    for y in range(height):
        row = []
        for x in range(width):
            u, v = (x + 0.5) / width, (y + 0.5) / height
            sky = max(0.0, 1.0 - v)
            base = (0.30 + 0.40 * sky, 0.34 + 0.55 * sky, 0.42 + 0.85 * sky)
            softbox = 8.0 * math.exp(-((u - 0.72) / 0.09) ** 4 -
                                     ((v - 0.31) / 0.11) ** 4)
            fill = 2.8 * math.exp(-((u - 0.36) / 0.08) ** 4 -
                                  ((v - 0.43) / 0.15) ** 4)
            row.append(rgbe((base[0] + softbox + fill,
                             base[1] + 0.94 * softbox + 0.78 * fill,
                             base[2] + 0.80 * softbox + 0.62 * fill)))
        file.write(bytes((2, 2, width >> 8, width & 255)))
        for channel in range(4):
            file.write(bytes((width, *(pixel[channel] for pixel in row))))
