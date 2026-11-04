"""Generate the NEAT rocket viewer's checked-in glTF boxes, HDRI, and sounds from code."""

import json
import math
from pathlib import Path
import random
import struct
import sys
import wave

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
        "asset": {"version": "2.0", "generator": "ACuppa NEAT rocket viewer source"},
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
box("flame", (1.0, 0.45, 0.08), 0.0, 0.9, (4.0, 1.4, 0.2))
box("ground", (0.06, 0.06, 0.07), 0.0, 0.95)
box("pad", (0.05, 0.5, 0.22), 0.1, 0.5, (0.01, 0.12, 0.04))
box("beacon", (0.1, 1.0, 0.35), 0.0, 0.4, (0.3, 3.0, 0.9))
box("scorch", (0.03, 0.025, 0.02), 0.0, 1.0)
box("mark", (0.3, 0.32, 0.36), 0.0, 0.9, (0.03, 0.035, 0.045))
box("streak", (0.7, 0.75, 0.85), 0.0, 1.0, (0.25, 0.27, 0.32))
GHOSTS = [(0.15, 0.45, 0.75), (0.65, 0.2, 0.6), (0.35, 0.6, 0.15), (0.75, 0.35, 0.12),
          (0.4, 0.3, 0.75), (0.1, 0.55, 0.5), (0.7, 0.25, 0.3)]
# One muted color per species ghost; the viewer cycles through them by species ID.
for index, color in enumerate(GHOSTS, 1):
    box(f"ghost_{index}", color, 0.2, 0.6)


def shape(bin_name, triangles):
    """Packs flat-shaded triangles [(a, b, c), ...] into a shared binary buffer
    and returns a writer for colored glTF files that use it."""
    pos, nor = [], []
    for a, b, c in triangles:
        u = [b[i] - a[i] for i in range(3)]
        v = [c[i] - a[i] for i in range(3)]
        n = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]]
        length = math.sqrt(sum(x * x for x in n)) or 1.0
        n = [x / length for x in n]
        for p in (a, b, c):
            pos.append(p)
            nor.append(n)
    idx = list(range(len(pos)))
    parts = [struct.pack(f"<{len(pos) * 3}f", *(x for p in pos for x in p)),
             struct.pack(f"<{len(nor) * 3}f", *(x for n in nor for x in n)),
             struct.pack(f"<{len(idx)}H", *idx)]
    data = b"".join(parts)
    (output / bin_name).write_bytes(data)
    starts = [0, len(parts[0]), len(parts[0]) + len(parts[1])]
    low = [min(p[i] for p in pos) for i in range(3)]
    high = [max(p[i] for p in pos) for i in range(3)]

    def write(name, color, metallic=0.0, roughness=0.6, emissive=(0, 0, 0), alpha=1):
        asset = {
            "asset": {"version": "2.0", "generator": "ACuppa NEAT rocket viewer source"},
            "buffers": [{"uri": bin_name, "byteLength": len(data)}],
            "bufferViews": [{"buffer": 0, "byteOffset": o, "byteLength": len(c)}
                            for o, c in zip(starts, parts)],
            "accessors": [
                {"bufferView": 0, "componentType": 5126, "count": len(pos), "type": "VEC3",
                 "min": low, "max": high},
                {"bufferView": 1, "componentType": 5126, "count": len(nor), "type": "VEC3"},
                {"bufferView": 2, "componentType": 5123, "count": len(idx), "type": "SCALAR"},
            ],
            "materials": [{"pbrMetallicRoughness": {
                "baseColorFactor": [*color, alpha], "metallicFactor": metallic,
                "roughnessFactor": roughness}, "emissiveFactor": list(emissive)}],
            "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1},
                                       "indices": 2, "material": 0}]}],
            "nodes": [{"mesh": 0}], "scenes": [{"nodes": [0]}], "scene": 0,
        }
        if alpha < 1:
            asset["materials"][0]["alphaMode"] = "BLEND"
        (output / f"{name}.gltf").write_text(json.dumps(asset, indent=2) + "\n")
    return write


def ring(radius, y, sides=16):
    return [(radius * math.cos(2 * math.pi * k / sides), y,
             radius * math.sin(2 * math.pi * k / sides)) for k in range(sides)]


def lathe(profile, sides=16):
    """Triangles of a closed surface of revolution through (radius, y) points
    listed from bottom to top, wound to face outward."""
    rings = [ring(r, y, sides) for r, y in profile]
    triangles = []
    for low, high in zip(rings, rings[1:]):
        for k in range(sides):
            a, b = low[k], low[(k + 1) % sides]
            c, d = high[k], high[(k + 1) % sides]
            if math.dist(a, b) > 1e-6:
                triangles.append((a, c, b))
            if math.dist(c, d) > 1e-6:
                triangles.append((b, c, d))
    bottom = rings[0]
    center = (0.0, profile[0][1], 0.0)
    if profile[0][0] > 0:
        triangles += [(center, bottom[k], bottom[(k + 1) % sides]) for k in range(sides)]
    return triangles


def fin(angle):
    """A thin triangular fin on the body side, in unit body space."""
    c, s = math.cos(angle), math.sin(angle)
    def at(r, y, t=0.0):
        return (r * c - t * s, y, r * s + t * c)
    triangles = []
    for t in (-0.06, 0.06):
        a, b, d = at(0.95, -1.0, t), at(1.6, -1.0, t), at(0.95, -0.35, t)
        triangles.append((a, b, d) if t > 0 else (a, d, b))
    a0, b0, d0 = at(0.95, -1.0, -0.06), at(1.6, -1.0, -0.06), at(0.95, -0.35, -0.06)
    a1, b1, d1 = at(0.95, -1.0, 0.06), at(1.6, -1.0, 0.06), at(0.95, -0.35, 0.06)
    triangles += [(b0, b1, d1), (b0, d1, d0), (a0, a1, b1), (a0, b1, b0)]
    return triangles


# The hull is drawn in unit body space (y from -1 to 1) and scaled to the
# 1.0 x 1.8 x 1.0 m collider: a round body, a cone nose, and four fins.
hull = lathe([(1.0, -1.0), (1.0, 0.55), (0.0, 1.35)])
for k in range(4):
    hull += fin(math.pi / 4 + k * math.pi / 2)
rocket = shape("rocket.bin", hull)
rocket("rocket_champion", (0.95, 0.66, 0.18), 0.65, 0.28)
for index, color in enumerate(GHOSTS, 1):
    rocket(f"rocket_ghost_{index}", color, 0.2, 0.6)
# The engine bell flares toward its open bottom, in unit space (y from -1 to 1).
shape("bell.bin", lathe([(1.0, -1.0), (0.45, 1.0), (0.0, 1.0)]))(
    "bell", (0.12, 0.12, 0.13), 0.8, 0.35)

# The explosion flash: a sphere about 38% see-through, so the rocket's last
# moment and the ground show through it.
shape("sphere.bin", lathe([(math.sin(math.pi * k / 12), -math.cos(math.pi * k / 12))
                           for k in range(13)]))(
    "blast", (1.0, 0.45, 0.1), 0.0, 0.9, (0.9, 0.3, 0.04), alpha=0.62)
# A flat ground mark shaped like a triangle, in unit space (y from -1 to 1).
corners = [(math.cos(a), math.sin(a)) for a in (math.pi / 2, math.pi * 7 / 6, math.pi * 11 / 6)]
top = [(x, 1.0, z) for x, z in corners]
bottom = [(x, -1.0, z) for x, z in corners]
prism = [(top[0], top[2], top[1]), (bottom[0], bottom[1], bottom[2])]
for k in range(3):
    a, b = k, (k + 1) % 3
    prism += [(bottom[a], top[a], bottom[b]), (bottom[b], top[a], top[b])]
shape("triangle.bin", prism)("mark_triangle", (0.3, 0.32, 0.36), 0.0, 0.9, (0.03, 0.035, 0.045))


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


def sound(name, seconds, sample):
    frames = bytearray()
    rate = 24000
    for index in range(int(seconds * rate)):
        frames += struct.pack("<h", int(max(-1, min(1, sample(index / rate))) * 25000))
    with wave.open(str(output / f"{name}.wav"), "wb") as file:
        file.setnchannels(1)
        file.setsampwidth(2)
        file.setframerate(rate)
        file.writeframes(frames)


noise = random.Random(7)
rumble = [noise.uniform(-1, 1) for _ in range(24000)]
# A crash: a noise burst with a low thump, decaying over 0.9 s.
sound("boom", 0.9, lambda t: math.exp(-5 * t) * (0.55 * rumble[int(t * 24000) % 24000] +
      0.5 * math.sin(2 * math.pi * (70 - 30 * t) * t)))
# A landing: two rising chime notes.
sound("chime", 0.5, lambda t: 0.4 * math.exp(-6 * t) * (math.sin(2 * math.pi * 660 * t) +
      (math.sin(2 * math.pi * 990 * t) if t > 0.12 else 0)))
