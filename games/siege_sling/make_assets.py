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
import wave
import zlib


output = Path(sys.argv[1]) if len(sys.argv) == 2 else Path(__file__).parent / "assets"
output.mkdir(parents=True, exist_ok=True)


def mesh(name, positions, normals, uvs, indices, color, metallic=0.0, roughness=0.7,
         texture=None):
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
    if texture:
        asset["images"] = [{"uri": f"{texture}.png"}]
        asset["textures"] = [{"source": 0}]
        asset["materials"][0]["pbrMetallicRoughness"]["baseColorTexture"] = {"index": 0}
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


def hash01(*values):
    """Deterministic value noise in [0, 1) from integer coordinates."""
    h = 2166136261
    for value in values:
        h = ((h ^ (value & 0xffffffff)) * 16777619) & 0xffffffff
    h ^= h >> 13
    h = (h * 1274126177) & 0xffffffff
    return (h ^ (h >> 16)) / 2 ** 32


def smooth_noise(x, y, cell, seed, wrap):
    gx, gy = x / cell, y / cell
    x0, y0 = int(math.floor(gx)), int(math.floor(gy))
    fx, fy = gx - x0, gy - y0
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    cells = wrap // cell
    corner = lambda cx, cy: hash01(seed, cx % cells, cy % cells)
    top = corner(x0, y0) * (1 - fx) + corner(x0 + 1, y0) * fx
    bottom = corner(x0, y0 + 1) * (1 - fx) + corner(x0 + 1, y0 + 1) * fx
    return top * (1 - fy) + bottom * fy


def png(name, size, shade):
    rows = []
    for y in range(size):
        row = bytearray([0])
        for x in range(size):
            row += bytes(max(0, min(255, int(v))) for v in shade(x, y))
        rows.append(bytes(row))
    def chunk(kind, data):
        return (struct.pack(">I", len(data)) + kind + data +
                struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff))
    (output / f"{name}.png").write_bytes(
        b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0)) +
        chunk(b"IDAT", zlib.compress(b"".join(rows), 9)) + chunk(b"IEND", b""))


def wood_texture(x, y):
    plank = y // 64                       # four planks with dark seams
    seam = 0.55 if y % 64 in (0, 1, 63) else 1.0
    grain = smooth_noise(x * 0.25, (y % 64) * 3 + plank * 40, 8, 11, 64 * 3 + 160)
    ring = 0.5 + 0.5 * math.sin((y % 64) * 0.45 + grain * 6 + x * 0.02 + plank)
    v = (0.82 + 0.18 * ring) * seam
    return (255 * v, 205 * v, 150 * v)


def stone_texture(x, y):
    row = y // 64                         # staggered stone courses
    shift = 64 if row % 2 else 0
    mortar = (y % 64) < 3 or ((x + shift) % 128) < 3
    if mortar:
        return (120, 120, 118)
    n = 0.6 * smooth_noise(x, y, 16, 3, 256) + 0.4 * smooth_noise(x, y, 4, 5, 256)
    tint = hash01(17, (x + shift) // 128, row) * 0.12
    v = 0.86 + 0.18 * n - tint
    return (255 * v, 252 * v, 245 * v)


def glass_texture(x, y):
    n = smooth_noise(x, y, 8, 7, 128)
    edge = min(x, y, 127 - x, 127 - y)
    frame = 1.0 if edge < 6 else 0.86 + 0.1 * n
    streak = 0.06 if (x + y) % 48 < 6 else 0
    v = min(1.0, frame + streak)
    return (255 * v, 255 * v, 255 * v)


def grass_texture(x, y):
    n = 0.5 * smooth_noise(x, y, 32, 21, 256) + 0.5 * smooth_noise(x, y, 8, 23, 256)
    v = 0.86 + 0.14 * n
    return (255 * v * 0.97, 255 * v, 255 * v * 0.9)


def crate_texture(x, y):
    """Explosive crate: dark frame, diagonal brace, and a yellow hazard band."""
    edge = min(x, y, 127 - x, 127 - y)
    grain = 0.9 + 0.1 * smooth_noise(x * 0.3, y * 2, 8, 31, 128)
    if edge < 10 or abs(x - y) < 7:
        v = 0.55 * grain
        return (255 * v, 120 * v, 60 * v)
    if 52 <= y < 76:
        stripe = ((x + y) // 12) % 2
        return (40, 30, 20) if stripe else (255, 215, 40)
    return (255 * grain, 150 * grain, 70 * grain)


def ice_texture(x, y):
    """Frosted ice: bright cracks over soft cloudy blue-white."""
    n = 0.6 * smooth_noise(x, y, 32, 41, 128) + 0.4 * smooth_noise(x, y, 8, 43, 128)
    crack = min(abs((x * 3 + y * 2) % 97 - 48), abs((x - y * 3) % 71 - 35))
    v = 0.84 + 0.12 * n + (0.1 if crack < 2 else 0)
    return (235 * v, 250 * v, 255 * min(1.0, v + 0.04))


def metal_texture(x, y):
    """Brushed steel plate with a rivet in each corner."""
    brushed = 0.9 + 0.08 * smooth_noise(x * 0.1, y * 4, 8, 51, 128)
    edge = min(x, y, 127 - x, 127 - y)
    rivet = any((x - cx) ** 2 + (y - cy) ** 2 < 36 for cx in (14, 113) for cy in (14, 113))
    v = brushed * (0.75 if edge < 4 else 1.0) * (1.15 if rivet else 1.0)
    return (250 * v, 252 * v, 255 * v)


def rubber_texture(x, y):
    """Dark rubber with a raised diamond tread."""
    tread = ((x + y) // 16 + (x - y + 512) // 16) % 2
    n = 0.92 + 0.08 * smooth_noise(x, y, 8, 61, 128)
    v = (0.75 if tread else 0.95) * n
    return (255 * v, 255 * v, 255 * v)


def brick_texture(x, y):
    """Running-bond bricks with pale mortar."""
    row = y // 32
    shift = 32 if row % 2 else 0
    if (y % 32) < 3 or ((x + shift) % 64) < 3:
        return (225, 215, 200)
    n = 0.85 + 0.15 * smooth_noise(x, y, 8, 71, 128)
    tint = hash01(73, (x + shift) // 64, row) * 0.14
    v = n - tint
    return (255 * v, 235 * v, 220 * v)


png("ice", 128, ice_texture)
png("metal", 128, metal_texture)
png("rubber", 128, rubber_texture)
png("brick", 128, brick_texture)
png("tnt", 128, crate_texture)
png("wood", 256, wood_texture)
png("stone", 256, stone_texture)
png("glass", 128, glass_texture)
png("grass", 256, grass_texture)

# Bright, saturated, cheerful palette (linear base colors; textures multiply).
for name, color, metallic, roughness, texture in [
    ("wood", (0.8, 0.42, 0.13), 0.0, 0.62, "wood"),
    ("stone", (0.5, 0.54, 0.6), 0.0, 0.78, "stone"),
    ("glass", (0.6, 0.93, 1.0), 0.0, 0.08, "glass"),
    ("sling_wood", (0.36, 0.17, 0.05), 0.0, 0.55, None),
    ("band", (0.65, 0.08, 0.04), 0.0, 0.45, None),
    ("fence", (0.62, 0.36, 0.16), 0.0, 0.7, "wood"),
    ("trunk", (0.3, 0.16, 0.06), 0.0, 0.8, None),
    ("chip_wood", (0.75, 0.4, 0.12), 0.0, 0.6, None),
    ("chip_stone", (0.48, 0.52, 0.58), 0.0, 0.8, None),
    ("chip_glass", (0.65, 0.95, 1.0), 0.0, 0.1, None),
    ("chip_grump", (0.25, 0.85, 0.1), 0.0, 0.5, None),
    ("tnt", (0.95, 0.32, 0.08), 0.0, 0.55, "tnt"),
    ("chip_tnt", (1.0, 0.45, 0.05), 0.0, 0.5, None),
    ("ice", (0.62, 0.86, 1.0), 0.0, 0.12, "ice"),
    ("metal", (0.62, 0.64, 0.68), 1.0, 0.28, "metal"),
    ("rubber", (0.07, 0.07, 0.08), 0.0, 0.9, "rubber"),
    ("brick", (0.78, 0.26, 0.12), 0.0, 0.85, "brick"),
    ("chip_ice", (0.7, 0.9, 1.0), 0.0, 0.1, None),
    ("chip_metal", (0.6, 0.62, 0.66), 1.0, 0.3, None),
    ("chip_rubber", (0.08, 0.08, 0.09), 0.0, 0.9, None),
    ("chip_brick", (0.75, 0.25, 0.1), 0.0, 0.85, None),
    # Level editor markers: moving/fallen (red), preview (yellow), selection (cyan).
    ("marker_red", (1.0, 0.05, 0.03), 0.0, 0.4, None),
    ("marker_preview", (1.0, 0.85, 0.1), 0.0, 0.4, None),
    ("marker_select", (0.1, 0.85, 1.0), 0.0, 0.4, None),
]:
    mesh(name, *box(), color, metallic, roughness, texture)

for name, color, metallic, roughness in [
    ("ball", (0.55, 0.02, 0.03), 0.1, 0.32),
    ("grump", (0.2, 0.85, 0.08), 0.0, 0.45),
    ("eye", (0.95, 0.95, 0.92), 0.0, 0.3),
    ("pupil", (0.02, 0.02, 0.03), 0.0, 0.25),
    ("dot", (1.0, 0.92, 0.35), 0.0, 0.4),
]:
    mesh(name, *sphere(), color, metallic, roughness)

# Boulders: sphere blocks in every material, sharing the block textures.
for name, color, metallic, roughness, texture in [
    ("wood", (0.8, 0.42, 0.13), 0.0, 0.62, "wood"),
    ("stone", (0.5, 0.54, 0.6), 0.0, 0.78, "stone"),
    ("glass", (0.6, 0.93, 1.0), 0.0, 0.08, "glass"),
    ("tnt", (0.95, 0.32, 0.08), 0.0, 0.55, "tnt"),
    ("ice", (0.62, 0.86, 1.0), 0.0, 0.12, "ice"),
    ("metal", (0.62, 0.64, 0.68), 1.0, 0.28, "metal"),
    ("rubber", (0.07, 0.07, 0.08), 0.0, 0.9, "rubber"),
    ("brick", (0.78, 0.26, 0.12), 0.0, 0.85, "brick"),
]:
    mesh(f"boulder_{name}", *sphere(24, 12), color, metallic, roughness, texture)

for name, color, roughness in [
    ("foliage", (0.12, 0.5, 0.08), 0.85),
    ("foliage_light", (0.3, 0.62, 0.08), 0.85),
    ("rock", (0.4, 0.42, 0.44), 0.85),
]:
    mesh(name, *sphere(16, 8), color, 0.0, roughness)

mesh("grass", *plane(75), (0.22, 0.6, 0.11), 0.0, 0.92, "grass")
mesh("hill", *hill(), (0.3, 0.68, 0.14), 0.0, 0.9, "grass")
mesh("far_hill", *hill(10, 32, 1.0, 12.0, 2.0), (0.2, 0.5, 0.16), 0.0, 0.95)


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


def rattle(t, seed):
    """Deterministic noise burst for impact texture."""
    return hash01(seed, int(t * 24000)) * 2 - 1


sine = lambda f, t: math.sin(2 * math.pi * f * t)
sound("knock", 0.18, lambda t: math.exp(-30 * t) * (0.55 * sine(320, t) + 0.3 * sine(710, t)) +
      math.exp(-90 * t) * 0.25 * rattle(t, 1))
sound("thud", 0.3, lambda t: math.exp(-16 * t) * (0.7 * sine(85, t) + 0.25 * sine(140, t)) +
      math.exp(-60 * t) * 0.2 * rattle(t, 2))
sound("clink", 0.35, lambda t: math.exp(-14 * t) * (0.35 * sine(2350, t) + 0.25 * sine(3720, t)) +
      math.exp(-120 * t) * 0.2 * rattle(t, 3))
sound("pop", 0.22, lambda t: math.exp(-18 * t) * 0.7 * sine(260 + 900 * t, t) +
      math.exp(-200 * t) * 0.4 * rattle(t, 4))
sound("twang", 0.45, lambda t: math.exp(-7 * t) * (0.5 * sine(150 * (1 + 0.3 * math.exp(-20 * t)), t) +
      0.2 * sine(300, t)) * (1 - math.exp(-400 * t)))
sound("boom", 0.9, lambda t: math.exp(-5 * t) * (0.6 * sine(48 + 30 * math.exp(-8 * t), t) +
      0.5 * rattle(t, 5) * math.exp(-3 * t)) * (1 - math.exp(-600 * t)))
sound("crack", 0.25, lambda t: math.exp(-25 * t) * (0.3 * sine(1650, t) + 0.25 * sine(2900, t)) +
      math.exp(-70 * t) * 0.45 * rattle(t, 6))
sound("clang", 0.6, lambda t: math.exp(-6 * t) * (0.35 * sine(620, t) + 0.3 * sine(1470, t) +
      0.15 * sine(2210, t)) + math.exp(-150 * t) * 0.2 * rattle(t, 7))
sound("boing", 0.4, lambda t: math.exp(-8 * t) * 0.7 * sine(110 + 160 * math.exp(-10 * t), t) *
      (1 - math.exp(-300 * t)))
