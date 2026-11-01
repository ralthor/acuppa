"""Generate Siege Sling's checked-in glTF meshes and HDRI from code.

Every mesh has normals and TEXCOORD_0 so Phase 33 can add textures without
changing geometry. Boxes span -1..1 (scale = physics half extents), spheres
have radius 1, and the ground plane spans -1..1 on X and Z.
"""

import json
import math
from pathlib import Path
import struct
import sys


output = Path(sys.argv[1]) if len(sys.argv) == 2 else Path(__file__).parent / "assets"
output.mkdir(parents=True, exist_ok=True)


def mesh(name, positions, normals, uvs, indices, color, metallic=0.0, roughness=0.7):
    chunks = [
        struct.pack(f"<{len(positions) * 3}f", *(v for row in positions for v in row)),
        struct.pack(f"<{len(normals) * 3}f", *(v for row in normals for v in row)),
        struct.pack(f"<{len(uvs) * 2}f", *(v for row in uvs for v in row)),
        struct.pack(f"<{len(indices)}H", *indices),
    ]
    if len(chunks[3]) % 4:
        chunks[3] += b"\0\0"
    binary = b"".join(chunks)
    buffer_name = f"{name}.bin"
    (output / buffer_name).write_bytes(binary)
    offsets = [sum(len(chunk) for chunk in chunks[:i]) for i in range(4)]
    low = [round(min(p[i] for p in positions), 6) for i in range(3)]
    high = [round(max(p[i] for p in positions), 6) for i in range(3)]
    asset = {
        "asset": {"version": "2.0", "generator": "ACuppa Siege Sling source"},
        "buffers": [{"uri": buffer_name, "byteLength": len(binary)}],
        "bufferViews": [{"buffer": 0, "byteOffset": offset,
                         "byteLength": len(chunk) if i < 3 else len(indices) * 2}
                        for i, (offset, chunk) in enumerate(zip(offsets, chunks))],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": len(positions),
             "type": "VEC3", "min": low, "max": high},
            {"bufferView": 1, "componentType": 5126, "count": len(normals), "type": "VEC3"},
            {"bufferView": 2, "componentType": 5126, "count": len(uvs), "type": "VEC2"},
            {"bufferView": 3, "componentType": 5123, "count": len(indices), "type": "SCALAR"},
        ],
        "materials": [{"pbrMetallicRoughness": {
            "baseColorFactor": [*color, 1], "metallicFactor": metallic,
            "roughnessFactor": roughness}}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1,
                                                   "TEXCOORD_0": 2},
                                   "indices": 3, "material": 0}]}],
        "nodes": [{"mesh": 0}], "scenes": [{"nodes": [0]}], "scene": 0,
    }
    (output / f"{name}.gltf").write_text(json.dumps(asset, indent=2) + "\n")


def box():
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
    uvs = [uv for _ in faces for uv in ((0, 1), (1, 1), (1, 0), (0, 0))]
    indices = [index for face in range(6) for index in
               (face * 4, face * 4 + 1, face * 4 + 2, face * 4, face * 4 + 2, face * 4 + 3)]
    return positions, normals, uvs, indices


def sphere(columns=32, rows=16):
    positions, normals, uvs, indices = [], [], [], []
    for row in range(rows + 1):
        v = row / rows
        theta = v * math.pi
        for column in range(columns + 1):
            u = column / columns
            phi = u * 2 * math.pi
            point = (round(math.sin(theta) * math.sin(phi), 6), round(math.cos(theta), 6),
                     round(math.sin(theta) * math.cos(phi), 6))
            positions.append(point)
            normals.append(point)
            uvs.append((u, v))
    for row in range(rows):
        for column in range(columns):
            a = row * (columns + 1) + column
            b = a + columns + 1
            if row > 0:
                indices += [a, b, a + 1]
            if row < rows - 1:
                indices += [a + 1, b, b + 1]
    return positions, normals, uvs, indices


def hill(rings=18, segments=48, top=2.6, outer=12.0, height=2.0):
    """A round mound with a flat top for the sling; y spans 0..height."""
    def profile(r):
        if r <= top:
            return height, 0.0
        s = (r - top) / (outer - top)
        smooth = 1 - (3 * s * s - 2 * s ** 3)
        slope = -height * (6 * s - 6 * s * s) / (outer - top)
        return height * smooth, slope

    radii = [top * i / 3 for i in range(4)] + \
            [top + (outer - top) * i / rings for i in range(1, rings + 1)]
    positions, normals, uvs, indices = [], [], [], []
    for r in radii:
        y, slope = profile(r)
        for segment in range(segments + 1):
            angle = 2 * math.pi * segment / segments
            x, z = r * math.cos(angle), r * math.sin(angle)
            length = math.sqrt(slope * slope + 1)
            positions.append((round(x, 6), round(y, 6), round(z, 6)))
            normals.append((round(-slope * math.cos(angle) / length, 6), round(1 / length, 6),
                            round(-slope * math.sin(angle) / length, 6)))
            uvs.append((round(x / 4, 6), round(z / 4, 6)))
    for ring in range(len(radii) - 1):
        for segment in range(segments):
            a = ring * (segments + 1) + segment
            b = a + segments + 1
            indices += [a, a + 1, b, a + 1, b + 1, b]
    return positions, normals, uvs, indices


def plane(tiles):
    positions = [(-1, 0, -1), (-1, 0, 1), (1, 0, 1), (1, 0, -1)]
    uvs = [(0, 0), (0, tiles), (tiles, tiles), (tiles, 0)]
    return positions, [(0, 1, 0)] * 4, uvs, [0, 1, 2, 0, 2, 3]


# Bright, saturated, cheerful palette (linear base colors).
for name, color, metallic, roughness in [
    ("wood", (0.72, 0.36, 0.1), 0.0, 0.62),
    ("stone", (0.42, 0.46, 0.52), 0.0, 0.78),
    ("glass", (0.55, 0.9, 1.0), 0.0, 0.08),
    ("sling_wood", (0.36, 0.17, 0.05), 0.0, 0.55),
    ("band", (0.65, 0.08, 0.04), 0.0, 0.45),
]:
    mesh(name, *box(), color, metallic, roughness)

for name, color, metallic, roughness in [
    ("ball", (0.55, 0.02, 0.03), 0.1, 0.32),
    ("grump", (0.2, 0.85, 0.08), 0.0, 0.45),
    ("eye", (0.95, 0.95, 0.92), 0.0, 0.3),
    ("pupil", (0.02, 0.02, 0.03), 0.0, 0.25),
    ("dot", (1.0, 0.92, 0.35), 0.0, 0.4),
]:
    mesh(name, *sphere(), color, metallic, roughness)

mesh("grass", *plane(75), (0.2, 0.62, 0.1), 0.0, 0.92)
mesh("hill", *hill(), (0.3, 0.7, 0.14), 0.0, 0.9)


def rgbe(rgb):
    peak = max(rgb)
    if peak < 1e-32:
        return bytes(4)
    _, exponent = math.frexp(peak)
    scale = math.ldexp(256.0, -exponent)
    return bytes((*(min(255, int(value * scale)) for value in rgb), exponent + 128))


# A soft daytime sky: blue above, bright haze at the horizon, warm sun glow,
# and green bounce light from below so shaded faces never go black.
width, height = 64, 32
with (output / "day.hdr").open("wb") as file:
    file.write(f"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y {height} +X {width}\n".encode())
    for y in range(height):
        row = []
        for x in range(width):
            u, v = (x + 0.5) / width, (y + 0.5) / height
            up = math.cos(v * math.pi)
            if up >= 0:
                sky = up ** 0.6
                rgb = [0.95 - 0.6 * sky, 1.05 - 0.45 * sky, 1.2 - 0.1 * sky]
            else:
                rgb = [0.32, 0.45, 0.2]
            glow = 4 * math.exp(-((u - 0.62) / 0.06) ** 2 - ((v - 0.3) / 0.08) ** 2)
            row.append(rgbe((rgb[0] + glow, rgb[1] + 0.9 * glow, rgb[2] + 0.7 * glow)))
        file.write(bytes((2, 2, 0, width)))
        for channel in range(4):
            file.write(bytes((width, *(pixel[channel] for pixel in row))))
