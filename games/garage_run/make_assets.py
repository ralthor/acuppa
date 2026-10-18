"""Generate Garage Run's checked-in glTF, HDRI, and WAV assets from code."""

import json
import math
from pathlib import Path
import struct
import sys
import wave


output = Path(sys.argv[1]) if len(sys.argv) == 2 else Path(__file__).parent / "assets"
output.mkdir(parents=True, exist_ok=True)


def mesh(name, positions, normals, indices, color, metallic=0.0, roughness=0.7):
    chunks = [
        struct.pack(f"<{len(positions) * 3}f", *(v for row in positions for v in row)),
        struct.pack(f"<{len(normals) * 3}f", *(v for row in normals for v in row)),
        struct.pack(f"<{len(indices)}H", *indices),
    ]
    binary = b"".join(chunks)
    buffer_name = f"{name}.bin"
    (output / buffer_name).write_bytes(binary)
    offsets = [0, len(chunks[0]), len(chunks[0]) + len(chunks[1])]
    asset = {
        "asset": {"version": "2.0", "generator": "ThreePoint Garage Run source"},
        "buffers": [{"uri": buffer_name, "byteLength": len(binary)}],
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
            "roughnessFactor": roughness}}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1},
                                   "indices": 2, "material": 0}]}],
        "nodes": [{"mesh": 0}], "scenes": [{"nodes": [0]}], "scene": 0,
    }
    (output / f"{name}.gltf").write_text(json.dumps(asset, indent=2) + "\n")


faces = [
    ((0, 0, 1), [(-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)]),
    ((0, 0, -1), [(1, -1, -1), (-1, -1, -1), (-1, 1, -1), (1, 1, -1)]),
    ((1, 0, 0), [(1, -1, 1), (1, -1, -1), (1, 1, -1), (1, 1, 1)]),
    ((-1, 0, 0), [(-1, -1, -1), (-1, -1, 1), (-1, 1, 1), (-1, 1, -1)]),
    ((0, 1, 0), [(-1, 1, 1), (1, 1, 1), (1, 1, -1), (-1, 1, -1)]),
    ((0, -1, 0), [(-1, -1, -1), (1, -1, -1), (1, -1, 1), (-1, -1, 1)]),
]
box_positions = [vertex for _, vertices in faces for vertex in vertices]
box_normals = [normal for normal, _ in faces for _ in range(4)]
box_indices = [index for face in range(6) for index in
               (face * 4, face * 4 + 1, face * 4 + 2,
                face * 4, face * 4 + 2, face * 4 + 3)]

for name, color, metallic, roughness in [
    ("car_red", (0.8, 0.045, 0.025), 0.4, 0.27),
    ("glass", (0.06, 0.24, 0.34), 0.05, 0.17),
    ("rubber", (0.025, 0.03, 0.035), 0, 0.92),
    ("barrier", (0.27, 0.34, 0.42), 0.25, 0.7),
    ("gold", (0.95, 0.56, 0.06), 0.35, 0.3),
    ("green", (0.04, 0.7, 0.25), 0.2, 0.38),
    ("white", (0.8, 0.85, 0.86), 0, 0.7),
]:
    mesh(name, box_positions, box_normals, box_indices, color, metallic, roughness)

mesh("asphalt", [(-1, 0, -1), (-1, 0, 1), (1, 0, 1), (1, 0, -1)],
     [(0, 1, 0)] * 4, [0, 1, 2, 0, 2, 3], (0.07, 0.085, 0.105))


def rgbe(rgb):
    peak = max(rgb)
    if peak < 1e-32:
        return bytes(4)
    _, exponent = math.frexp(peak)
    scale = math.ldexp(256.0, -exponent)
    return bytes((*(min(255, int(value * scale)) for value in rgb), exponent + 128))


width, height = 32, 16
with (output / "sunset.hdr").open("wb") as file:
    file.write(f"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y {height} +X {width}\n".encode())
    for y in range(height):
        row = []
        for x in range(width):
            u, v = (x + 0.5) / width, (y + 0.5) / height
            sky = max(0, 1 - v)
            glow = 5 * math.exp(-((u - 0.7) / 0.12) ** 4 - ((v - 0.37) / 0.14) ** 4)
            row.append(rgbe((0.38 + 0.4 * sky + glow,
                             0.4 + 0.35 * sky + 0.65 * glow,
                             0.52 + 0.52 * sky + 0.3 * glow)))
        file.write(bytes((2, 2, 0, width)))
        for channel in range(4):
            file.write(bytes((width, *(pixel[channel] for pixel in row))))


def sound(name, seconds, sample):
    frames = bytearray()
    rate = 24000
    for index in range(int(seconds * rate)):
        time = index / rate
        frames += struct.pack("<h", int(max(-1, min(1, sample(time))) * 25000))
    with wave.open(str(output / f"{name}.wav"), "wb") as file:
        file.setnchannels(1)
        file.setsampwidth(2)
        file.setframerate(rate)
        file.writeframes(frames)


sound("motor", 1, lambda t: 0.12 * math.sin(2 * math.pi * 90 * t) +
      0.05 * math.sin(2 * math.pi * 180 * t))
sound("checkpoint", 0.28, lambda t: math.exp(-12 * t) *
      0.6 * math.sin(2 * math.pi * 880 * t))
sound("bump", 0.22, lambda t: math.exp(-18 * t) *
      0.5 * math.sin(2 * math.pi * 105 * t))
