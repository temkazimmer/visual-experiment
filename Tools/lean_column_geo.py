"""The menu's lean column, dense: SM_LeanColumn_HD, the drum she leans on (SpawnLeanColumn).

SM_Drum_0 (stone_geo.py) is Nanite with an auto fallback of 424 triangles, and its collision
was a capsule: traces met a surface up to 5.6 cm off the stone she was drawn on. This one is
built at its final size (no stretch), 180 segments around (0.01 cm off a true circle), a
true cylinder through the middle of its length where her hips, back and hands go, worn and
chipped only toward the ends like the other drums. Imported without Nanite, collision
complex-as-simple: traces hit the drawn triangles exactly.

  python3 Tools/lean_column_geo.py            -> Tools/.bridge/geo/SM_LeanColumn_HD.obj
  python3 Tools/ue.py Tools/lean_column_import.py

Frame: X along the drum (centred), Z up, pivot on the axis. Unreal-side Y is mirrored by the
OBJ writer, as for the other stone.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stone_geo as sg

RADIUS = 47.0        # the old drum's measured surface radius at Play (47.2), rounded
LENGTH = 320.0
SEGS = 180
SMOOTH = 115.0       # |x| below this: a true cylinder (she sits ~60 cm off the middle)
SEED = 200           # SM_Drum_0's seed, so the ends wear the same way


def build():
    s = SEED * 13.7
    x0, x1 = -LENGTH / 2, LENGTH / 2
    xs = sg.samples(x0, x1, [x0, x1], fine=2.5, mid=6.0, coarse=12.0)
    pts = []
    for x in xs:
        edge = min(x - x0, x1 - x)
        keep = sg.smooth(SMOOTH, SMOOTH + 25.0, abs(x))      # 0 in the middle: no wear
        row = []
        for i in range(SEGS + 1):
            t = 2 * math.pi * i / SEGS
            ct, st = math.cos(t), math.sin(t)
            d = sg.weather(x, ct * RADIUS, st * RADIUS + 5000.0, edge, 1e9, s, 0.45) * keep if keep > 0 else 0.0
            r = RADIUS - d
            row.append((x, ct * r, st * r))
        pts.append(row)
    m = sg.Mesh()
    m.grid(pts, lambda p: (0, p[1], p[2]))
    for end, sign in ((pts[0], -1), (pts[-1], 1)):
        rings = [[(p[0] + sign * -1.5 * (1 - f) ** 2, p[1] * f, p[2] * f) for p in end]
                 for f in (1.0, 0.85, 0.6, 0.3, 0.0)]
        m.grid(rings, lambda p, sign=sign: (sign, 0, 0))
    return m


if __name__ == "__main__":
    os.makedirs(sg.OUT, exist_ok=True)
    m = build()
    path = os.path.join(sg.OUT, "SM_LeanColumn_HD.obj")
    m.write(path)
    # the middle against a true cylinder, and the facets' own sag
    mid = [v for v in m.v if abs(v[0]) < SMOOTH and abs(math.hypot(v[1], v[2]) - RADIUS) < 20]
    dev = max(abs(math.hypot(v[1], v[2]) - RADIUS) for v in mid)
    sag = RADIUS * (1 - math.cos(math.pi / SEGS))
    print("%s: %d verts, %d tris; middle off a cylinder by %.3f cm, facet sag %.3f cm"
          % (path, len(m.v), len(m.f), dev, sag))
