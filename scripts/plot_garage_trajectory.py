#!/usr/bin/env python3
"""Draw the device-free Garage Run key-sequence trace as a top-down SVG."""

import csv
from collections import defaultdict
from html import escape
from pathlib import Path
import sys


if len(sys.argv) != 3:
    raise SystemExit("usage: plot_garage_trajectory.py trajectory.csv output.svg")

traces = defaultdict(list)
with Path(sys.argv[1]).open(newline="") as source:
    for row in csv.DictReader(source):
        traces[row["scenario"]].append({
            "time": float(row["time"]),
            "x": float(row["x"]),
            "z": float(row["z"]),
            "keys": row["keys"],
        })

panels = [
    ("Forward, 1.5 s", [
        ("forward", "W", "#4678c8"),
        ("forward_left", "W + A", "#d58a36"),
        ("forward_right", "W + D", "#1f9b8d"),
    ]),
    ("Reverse, 2 s", [
        ("reverse", "S", "#4678c8"),
        ("reverse_left", "S + A", "#d58a36"),
        ("reverse_right", "S + D", "#1f9b8d"),
    ]),
    ("Release after 2 s reverse", [
        ("reverse_then_left", "S → A", "#4678c8"),
        ("reverse_left_release_reverse", "S + A → A", "#d58a36"),
        ("reverse_left_release_left", "S + A → S", "#1f9b8d"),
    ]),
    ("Opposing keys and release", [
        ("both_pedals", "W + S", "#8256ad"),
        ("both_pedals_release_reverse", "W + S → W", "#4678c8"),
        ("both_pedals_release_forward", "W + S → S", "#d58a36"),
        ("reverse_both_steer", "S + A + D", "#1f9b8d"),
        ("reverse_both_steer_release_right", "S + A + D → S + A", "#d25673"),
    ]),
]

parts = ['''<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="890"
  viewBox="0 0 1200 890" role="img" aria-labelledby="title desc">
<title id="title">Garage Run headless car trajectories</title>
<desc id="desc">Top-down car positions under simultaneous and released keyboard controls.
No game window or graphical renderer was used.</desc>
<rect width="1200" height="890" fill="#f6f8fb"/>
<text x="36" y="43" font-family="sans-serif" font-size="26" font-weight="700"
 fill="#203044">Garage Run · headless trajectory test</text>
<text x="36" y="69" font-family="sans-serif" font-size="15" fill="#526274">
Top-down X/Z position, sampled every 0.1 s · north (−Z) is up · circles mark release points
</text>''']

for index, (title, lines) in enumerate(panels):
    panel_x = 32 + (index % 2) * 584
    panel_y = 93 + (index // 2) * 391
    panel_w, panel_h = 552, 370
    plot_x, plot_y = panel_x + 54, panel_y + 76
    plot_w, plot_h = 470, 253
    selected = [traces[name] for name, _, _ in lines]
    if any(not points for points in selected):
        raise SystemExit(f"missing trajectory for panel: {title}")
    xs = [point["x"] for points in selected for point in points]
    zs = [point["z"] for points in selected for point in points]
    xpad = max(1.0, (max(xs) - min(xs)) * 0.12)
    zpad = max(1.0, (max(zs) - min(zs)) * 0.12)
    xmin, xmax = min(xs) - xpad, max(xs) + xpad
    zmin, zmax = min(zs) - zpad, max(zs) + zpad

    def pixel(point):
        return (plot_x + (point["x"] - xmin) / (xmax - xmin) * plot_w,
                plot_y + (point["z"] - zmin) / (zmax - zmin) * plot_h)

    parts.append(f'<rect x="{panel_x}" y="{panel_y}" width="{panel_w}" '
                 f'height="{panel_h}" rx="10" fill="white" stroke="#d4dce7"/>')
    parts.append(f'<text x="{panel_x + 18}" y="{panel_y + 29}" '
                 f'font-family="sans-serif" font-size="18" font-weight="700" '
                 f'fill="#203044">{escape(title)}</text>')
    legend_x = panel_x + 18
    for name, label, color in lines:
        parts.append(f'<line x1="{legend_x}" y1="{panel_y + 52}" '
                     f'x2="{legend_x + 18}" y2="{panel_y + 52}" '
                     f'stroke="{color}" stroke-width="3"/>')
        parts.append(f'<text x="{legend_x + 24}" y="{panel_y + 57}" '
                     f'font-family="sans-serif" font-size="12" fill="#394b60">'
                     f'{escape(label)}</text>')
        legend_x += 38 + 7 * len(label)
    parts.append(f'<rect x="{plot_x}" y="{plot_y}" width="{plot_w}" '
                 f'height="{plot_h}" fill="#fbfcfe" stroke="#b8c4d1"/>')
    for tick in range(5):
        xx = plot_x + plot_w * tick / 4
        yy = plot_y + plot_h * tick / 4
        world_x = xmin + (xmax - xmin) * tick / 4
        world_z = zmin + (zmax - zmin) * tick / 4
        parts.append(f'<path d="M{xx:.1f} {plot_y}V{plot_y + plot_h} '
                     f'M{plot_x} {yy:.1f}H{plot_x + plot_w}" '
                     f'stroke="#e2e8f0" stroke-width="1"/>')
        parts.append(f'<text x="{xx:.1f}" y="{plot_y + plot_h + 17}" '
                     f'text-anchor="middle" font-family="sans-serif" '
                     f'font-size="11" fill="#65788a">{world_x:.1f}</text>')
        parts.append(f'<text x="{plot_x - 8}" y="{yy + 4:.1f}" '
                     f'text-anchor="end" font-family="sans-serif" '
                     f'font-size="11" fill="#65788a">{world_z:.1f}</text>')
    parts.append(f'<text x="{plot_x + plot_w / 2}" y="{panel_y + panel_h - 12}" '
                 f'text-anchor="middle" font-family="sans-serif" font-size="12" '
                 f'fill="#526274">X (m)</text>')
    parts.append(f'<text x="{panel_x + 15}" y="{plot_y + plot_h / 2}" '
                 f'transform="rotate(-90 {panel_x + 15} {plot_y + plot_h / 2})" '
                 f'text-anchor="middle" font-family="sans-serif" font-size="12" '
                 f'fill="#526274">Z (m)</text>')
    for name, _, color in lines:
        points = traces[name]
        coordinates = " ".join(f"{px:.1f},{py:.1f}" for px, py in map(pixel, points))
        parts.append(f'<polyline points="{coordinates}" fill="none" '
                     f'stroke="{color}" stroke-width="2.7" '
                     f'stroke-linejoin="round" stroke-linecap="round"/>')
        for previous, point in zip(points, points[1:]):
            if previous["time"] > 0 and previous["keys"] != point["keys"]:
                px, py = pixel(point)
                parts.append(f'<circle cx="{px:.1f}" cy="{py:.1f}" r="5" '
                             f'fill="white" stroke="{color}" stroke-width="2"/>')
        px, py = pixel(points[-1])
        parts.append(f'<circle cx="{px:.1f}" cy="{py:.1f}" r="3.5" fill="{color}"/>')
    sx, sy = pixel(selected[0][0])
    parts.append(f'<circle cx="{sx:.1f}" cy="{sy:.1f}" r="4" fill="#172334"/>')

parts.append('</svg>')
Path(sys.argv[2]).write_text("\n".join(parts) + "\n")
print(f"Wrote {sys.argv[2]}")
