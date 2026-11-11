"""The menu's lean column, dense and carved: SM_LeanColumn_HD, the drum she leans on.

A fallen Doric drum: 20 shallow flutes with worn arrises, a chamfered rim at each end that
chips and spalls (one big break at the far end), end faces dressed the ancient way
(a smooth outer band, a pick-dressed recess, a square dowel hole), fine pitting all over
and a hairline crack. Through the middle, where her hips, back and hands go, the flutes
stay whole and nothing chips, so the contact surface is the carved stone itself.
Built at its final size, 400 segments around; imported without Nanite, collision
complex-as-simple: traces land on the drawn triangles.

  python3 Tools/lean_column_geo.py            -> Tools/.bridge/geo/SM_LeanColumn_HD.obj
  python3 Tools/ue.py Tools/lean_column_import.py

Frame: X along the drum (centred), Z up, pivot on the axis. Unreal-side Y is mirrored by
the OBJ writer, as for the other stone. RADIUS is the arris radius (the outermost stone).
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stone_geo as sg

RADIUS = 47.0        # arris radius: the old drum's measured surface (47.2), rounded
LENGTH = 320.0
FLUTES = 20
PER_FLUTE = 20       # segments across one flute
FLUTE_DEPTH = 2.4    # cm, at the flute's middle
ARRIS_ROUND = 0.16   # fraction of a flute the worn arris rounds over
CHAMFER = 3.5        # cm, the rim at each end
SMOOTH = 115.0       # |x| below this: no chips, no break (she sits ~60 cm off the middle)
SEED = 200
SEGS = FLUTES * PER_FLUTE


def flute(t, x, s):
    """Inward depth of the fluting at angle t: shallow arcs meeting at rounded arrises,
    the arrises worn a little unevenly along the drum."""
    u = (t * FLUTES / (2 * math.pi)) % 1.0
    w = 2 * u - 1                                    # -1 .. 1 across one flute
    arc = math.sqrt(max(0.0, 1 - w * w))              # 0 at the arris, 1 mid-flute
    wear = ARRIS_ROUND * (0.7 + 0.6 * sg.vn3(x / 40 + s, math.cos(t) * 2, math.sin(t) * 2 + 1.3))
    edge = 1 - abs(w)                                 # 0 at the arris
    round_ = sg.smooth(0.0, wear, edge)               # rounds the arris over
    arris_wear = (1 - round_) * 0.9 * (0.5 + sg.vn3(x / 18 - s, math.cos(t) * 6, math.sin(t) * 6 + 4.1))
    return FLUTE_DEPTH * arc + arris_wear


def side(x, t, s):
    """Inward displacement (cm) of the drum's side at x along it, angle t around."""
    x0, x1 = -LENGTH / 2, LENGTH / 2
    y, z = math.cos(t) * RADIUS, math.sin(t) * RADIUS
    edge = min(x - x0, x1 - x)
    d = flute(t, x, s)
    d += max(0.0, CHAMFER - edge)                                        # 45-degree rim
    pit = sg.vn3(x / 1.4 + s, y / 1.4, z / 1.4)                          # fine pitting
    d += 0.22 * sg.smooth(0.7, 0.92, pit) * sg.smooth(0.35, 0.6, sg.vn3(x / 25, y / 25 + s, z / 25))
    d += 0.35 * sg.fbm3(x / 9, y / 9 - s, z / 9)                         # weathered undulation
    c = sg.fbm3(x / 90 + s, y / 90, z / 90)                              # a hairline crack
    if sg.vn3(x / 160 - s, y / 160, z / 160) > 0.55:
        d += 0.7 * (1 - sg.smooth(0.0, 0.007, abs(c - 0.5)))
    keep = sg.smooth(SMOOTH, SMOOTH + 25.0, abs(x))                      # 0 where she sits
    if keep > 0:
        d += sg.weather(x, y, z + 5000.0, edge, 1e9, s, 0.4) * keep
        # one big spall off the far rim, low on the back
        da = math.atan2(math.sin(t - 3.6), math.cos(t - 3.6))
        m = 1 - (da / 0.55) ** 2 - ((x1 - x) / 38.0) ** 2
        m += 0.25 * (sg.vn3(x / 9, math.cos(t) * 5, math.sin(t) * 5 + 2.2) - 0.5)
        if m > 0:
            d += min(1.0, m * 4) * (9 + 2.5 * sg.vn3(x / 4, math.cos(t) * 12, math.sin(t) * 12 + 7.7))
    return d


def end_face(t, f, rim_d, s, sign):
    """How far an end face lies inside the drum's end at radius fraction f: a smooth outer
    band, a pick-dressed recess, a square dowel hole; chips at the rim wrap onto it."""
    r = f * RADIUS
    y, z = math.cos(t) * r, math.sin(t) * r
    d = 0.8 * sg.smooth(0.78, 0.74, f)                                    # the recess
    d += 0.3 * sg.vn3(y / 1.6 + sign * 3, z / 1.6, s) * sg.smooth(0.8, 0.74, f)
    d += 0.1 * sg.vn3(y / 6, z / 6, s + sign)
    sq = max(abs(y), abs(z))
    d += 4.0 * sg.smooth(4.4, 3.9, sq)                                    # dowel hole
    d += 0.7 * max(0.0, rim_d - CHAMFER - FLUTE_DEPTH) * sg.smooth(0.7, 1.0, f)
    return d


def write(m, path):
    """stone_geo's OBJ writer (Y mirrored for the importer), with per-vertex UVs."""
    assert len(m.uv) == len(m.v)
    with open(path, "w") as fh:
        w = fh.write
        for x, y, z in m.v:
            w("v %.3f %.3f %.3f\n" % (x, -y, z))
        for x, y, z in m.n:
            w("vn %.4f %.4f %.4f\n" % (x, -y, z))
        for u, v in m.uv:
            w("vt %.4f %.4f\n" % (u, v))
        for a, b, c in m.f:
            w("f %d/%d/%d %d/%d/%d %d/%d/%d\n" % (a + 1, a + 1, a + 1, b + 1, b + 1, b + 1, c + 1, c + 1, c + 1))


def build():
    s = SEED * 13.7
    x0, x1 = -LENGTH / 2, LENGTH / 2
    xs = sg.samples(x0, x1, [x0, x1], fine=0.7, mid=1.4, coarse=1.6)
    ts = [2 * math.pi * i / SEGS for i in range(SEGS + 1)]
    pts = []
    for x in xs:
        pts.append([(x, math.cos(t) * (RADIUS - side(x, t, s)), math.sin(t) * (RADIUS - side(x, t, s)))
                    for t in ts])
    m = sg.Mesh()
    m.grid(pts, lambda p: (0, p[1], p[2]))
    # real UVs (the material is world-projected; these only tell texture streaming the true
    # texel density, so the stone keeps its full-res mips up close): 1 unit = 1 m
    m.uv = [(i / SEGS * 2 * math.pi * RADIUS / 100.0, x / 100.0) for x in xs for i in range(SEGS + 1)]
    fs = [1.0 - 0.01 * k for k in range(0, 8)] + [0.9 - 0.04 * k for k in range(0, 5)] + \
         [0.75 - 0.01 * k for k in range(0, 4)] + [0.7 - 0.05 * k for k in range(0, 10)] + \
         [0.2 - 0.02 * k for k in range(0, 10)] + [0.0]
    for row, sign in ((pts[0], -1), (pts[-1], 1)):
        xe = row[0][0]
        rings = []
        for f in fs:
            ring = []
            for i, t in enumerate(ts):
                rim = row[i]
                rim_r = math.hypot(rim[1], rim[2])
                rim_d = RADIUS - rim_r
                d = end_face(t, f, rim_d, s, sign) if f < 1.0 else 0.0
                r = rim_r * f if f > 0.0 else 0.0
                ring.append((xe - sign * d, math.cos(t) * r, math.sin(t) * r))
            rings.append(ring)
        m.grid(rings, lambda p, sign=sign: (sign, 0, 0))
        m.uv += [(p[1] / 100.0, p[2] / 100.0) for ring in rings for p in ring]
    return m


if __name__ == "__main__":
    os.makedirs(sg.OUT, exist_ok=True)
    m = build()
    path = os.path.join(sg.OUT, "SM_LeanColumn_HD.obj")
    write(m, path)
    mid = [v for v in m.v if abs(v[0]) < SMOOTH and abs(math.hypot(v[1], v[2]) - RADIUS) < 20]
    rs = [math.hypot(v[1], v[2]) for v in mid]
    print("%s: %d verts, %d tris; middle radius %.2f..%.2f (arris %.1f, flute depth %.1f)"
          % (path, len(m.v), len(m.f), min(rs), max(rs), RADIUS, FLUTE_DEPTH))
