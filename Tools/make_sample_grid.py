#!/usr/bin/env python3
"""Offline test map: a 6x6 street grid (400 m blocks) with one level crossing. No internet needed."""
import json, os

STEP = 40000          # 400 m in cm
N = 6
HALF = STEP * (N - 1) // 2
roads = []
for i in range(N):
    c = -HALF + i * STEP
    h = "secondary" if i % 2 == 0 else "tertiary"
    w = 8.0 if h == "secondary" else 7.0
    line_h = [[x, c] for x in range(-HALF, HALF + 1, 5000)]
    line_v = [[c, y] for y in range(-HALF, HALF + 1, 5000)]
    roads.append({"h": h, "w": w, "p": line_h})
    roads.append({"h": h, "w": w, "p": line_v})
# a side lane to drive through
roads.append({"h": "residential", "w": 5.5, "p": [[-HALF, -HALF - 20000]] + [[x, -HALF - 20000] for x in range(-HALF, HALF + 1, 5000)]})

doc = {"center": {"lat": 0, "lon": 0}, "roads": roads, "crossings": [[60000, 0]]}
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Content", "Data", "sample_grid.json")
os.makedirs(os.path.dirname(out), exist_ok=True)
json.dump(doc, open(out, "w"), separators=(",", ":"))
print("wrote", out)
