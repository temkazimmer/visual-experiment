"""Weathered stone geometry for Lvl_Dust: real bevels, chips, broken corners and
joint grooves in the mesh, not only in the normal map.

Runs OUTSIDE the editor (plain python3, parallel) so the editor stays responsive:
  1. dump the level's StaticMeshActors (label, mesh, loc, scale, rot) to
     Tools/.bridge/actors.json from inside the editor -- must be the ORIGINAL
     primitives (Cube/Cylinder scales), i.e. right after build_dust/add_ruins
  2. python3 Tools/stone_geo.py            -> Tools/.bridge/geo/*.obj + manifest.json
  3. python3 Tools/ue.py Tools/apply_geo.py

Joints are cut on the same world grids the baked textures use (bake_stone.py):
walls 430 x 190 cm blocks with alternate rows offset by half a block, floor slabs
1600 x 1000 cm with the same half offset. Texture and geometry joints coincide.
"""
import json
import math
import os
import random
import sys
from multiprocessing import Pool

HERE = os.path.dirname(os.path.abspath(__file__))
BRIDGE = os.path.join(HERE, ".bridge")
OUT = os.path.join(BRIDGE, "geo")

COURSE, BLOCK = 190.0, 430.0          # wall masonry
SLAB_W, SLAB_D = 1600.0, 1000.0       # floor slabs

# ---------------------------------------------------------------- noise
_rng = random.Random(1234)
_P = list(range(256)); _rng.shuffle(_P); _P += _P
_V = [_rng.random() for _ in range(256)]


def vn3(x, y, z):
    xi, yi, zi = math.floor(x), math.floor(y), math.floor(z)
    fx, fy, fz = x - xi, y - yi, z - zi
    fx, fy, fz = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy), fz * fz * (3 - 2 * fz)
    xi, yi, zi = xi & 255, yi & 255, zi & 255
    P, V = _P, _V
    a, b = P[xi] + yi, P[xi + 1] + yi
    aa, ab, ba, bb = P[a] + zi, P[a + 1] + zi, P[b] + zi, P[b + 1] + zi
    x1 = V[P[aa] & 255] + (V[P[ba] & 255] - V[P[aa] & 255]) * fx
    x2 = V[P[ab] & 255] + (V[P[bb] & 255] - V[P[ab] & 255]) * fx
    x3 = V[P[aa + 1] & 255] + (V[P[ba + 1] & 255] - V[P[aa + 1] & 255]) * fx
    x4 = V[P[ab + 1] & 255] + (V[P[bb + 1] & 255] - V[P[ab + 1] & 255]) * fx
    y1 = x1 + (x2 - x1) * fy
    y2 = x3 + (x4 - x3) * fy
    return y1 + (y2 - y1) * fz


def fbm3(x, y, z, octaves=3):
    s, a, w = 0.0, 0.5, 0.0
    for _ in range(octaves):
        s += a * vn3(x, y, z); w += a
        x, y, z, a = x * 2.03 + 7.1, y * 2.03 - 3.3, z * 2.03 + 1.7, a * 0.5
    return s / w


def smooth(a, b, x):
    t = min(1.0, max(0.0, (x - a) / (b - a)))
    return t * t * (3 - 2 * t)


# ---------------------------------------------------------------- weathering
def joint_dist(wx, wy, wz, horizontal, axis_h):
    """Distance (cm) to the nearest joint line on the world grids, or 1e9."""
    if horizontal:
        row = math.floor(wy / SLAB_D)
        u = wx / SLAB_W + 0.5 * (abs(row) % 2)
        du = min(u - math.floor(u), math.ceil(u) - u) * SLAB_W
        fy = wy - row * SLAB_D
        return min(du, fy, SLAB_D - fy)
    row = math.floor(wz / COURSE)
    fz = wz - row * COURSE
    d = min(fz, COURSE - fz)
    if axis_h is not None:                       # vertical joints on flat walls
        a = wx if axis_h == 0 else wy
        u = a / BLOCK + 0.5 * (abs(row) % 2)
        d = min(d, min(u - math.floor(u), math.ceil(u) - u) * BLOCK)
    return d


def weather(wx, wy, wz, edge, jd, s, chip=1.0):
    """Inward displacement (cm) at a world point: chipped/worn edges, rounded and
    broken joints, a slightly uneven face. edge/jd: distance to the shape's edge
    and to the nearest joint."""
    q = 1.0 / 60
    big = fbm3(wx * q + s, wy * q, wz * q)
    e = min(edge, jd * 1.6)
    band = 18 + 55 * smooth(0.4, 0.8, big)                     # how far chips bite in
    m = 1 - smooth(0, band, e)
    d = 0.0
    if m > 0:
        small = vn3(wx / 14 + s, wy / 14, wz / 14)
        d += m * chip * (3 + 26 * smooth(0.48, 0.8, big)) * (0.55 + 0.45 * small)
    if jd < 14:                                                # rounded joint groove
        d += 5.5 * (1 - (jd / 14) ** 2)
    d -= 1.4 * (vn3(wx / 140 - s, wy / 140, wz / 140) - 0.5)  # a not-quite-flat face
    # chips taper into the very edge: deeper than the distance to it folds the surface
    return min(d, 7 + 1.3 * edge)


# ---------------------------------------------------------------- mesh
class Mesh:
    def __init__(self):
        self.v, self.n, self.f = [], [], []

    def grid(self, pts, outward):
        """pts[j][i] positions; adds verts, central-difference normals, triangles."""
        nv, nu = len(pts), len(pts[0])
        base = len(self.v)
        raw, vote = [], 0.0
        for j in range(nv):
            for i in range(nu):
                p = pts[j][i]
                a, b = pts[j][min(i + 1, nu - 1)], pts[j][max(i - 1, 0)]
                c, d = pts[min(j + 1, nv - 1)][i], pts[max(j - 1, 0)][i]
                du = (a[0] - b[0], a[1] - b[1], a[2] - b[2])
                dv = (c[0] - d[0], c[1] - d[1], c[2] - d[2])
                n = (du[1] * dv[2] - du[2] * dv[1], du[2] * dv[0] - du[0] * dv[2], du[0] * dv[1] - du[1] * dv[0])
                l = math.sqrt(n[0] ** 2 + n[1] ** 2 + n[2] ** 2) or 1
                o = outward(p)
                vote += (n[0] * o[0] + n[1] * o[1] + n[2] * o[2]) / l
                raw.append((p, n, l))
        # orientation by majority: a deep chip can fold a corner inward, and a face
        # oriented from that one vertex comes out inside-out (invisible, no collision)
        sign = 1 if vote >= 0 else -1
        for p, n, l in raw:
            self.v.append(p)
            self.n.append((sign * n[0] / l, sign * n[1] / l, sign * n[2] / l))
        for j in range(nv - 1):
            for i in range(nu - 1):
                a = base + j * nu + i
                b, c, d = a + 1, a + nu, a + nu + 1
                if sign > 0:
                    self.f += [(a, c, b), (b, c, d)]
                else:
                    self.f += [(a, b, c), (b, d, c)]

    def write(self, path):
        with open(path, "w") as fh:
            w = fh.write
            for x, y, z in self.v:              # importer keeps Z up but mirrors Y
                w("v %.2f %.2f %.2f\n" % (x, -y, z))
            for x, y, z in self.n:
                w("vn %.4f %.4f %.4f\n" % (x, -y, z))
            w("vt 0 0\n")
            for a, b, c in self.f:
                w("f %d/1/%d %d/1/%d %d/1/%d\n" % (a + 1, a + 1, b + 1, b + 1, c + 1, c + 1))
        return len(self.v)


def samples(lo, hi, features, fine=2.5, mid=7.0, coarse=32.0):
    """Positions lo..hi, dense near the given feature positions (edges, joints)."""
    feats = sorted(set([lo, hi] + [f for f in features if lo <= f <= hi]))
    out, x, k = [], lo, 0
    while x < hi:
        out.append(x)
        while k + 1 < len(feats) and feats[k + 1] <= x:
            k += 1
        d = min(abs(x - feats[k]), abs(feats[min(k + 1, len(feats) - 1)] - x))
        x += fine if d < 9 else (mid if d < 40 else coarse)
    out.append(hi)
    return out


def grid_lines(lo, hi, period, offset=0.0):
    k0, k1 = math.floor((lo - offset) / period), math.ceil((hi - offset) / period)
    return [offset + k * period for k in range(k0, k1 + 1)]


# ---------------------------------------------------------------- shapes
def box(centre, half, r, s, faces="xXyYzZ", joints=True, top_joints=False, chip=1.0, local=True, mesh=None,
        quiet=()):
    """Rounded, chipped box. centre: world position; half: half extents. Faces:
    x/X = -x/+x etc. Vertices are relative to centre when local=True. quiet: box
    sides ("Y" etc.) whose edges stay unchipped, e.g. where a step meets the next."""
    cx, cy, cz = centre
    hx, hy, hz = half
    ix, iy, iz = max(hx - r, 0), max(hy - r, 0), max(hz - r, 0)
    m = mesh or Mesh()
    for fc in faces:
        ax = "xyz".index(fc.lower())
        sgn = 1 if fc.isupper() else -1
        t1, t2 = [a for a in range(3) if a != ax]
        h = (hx, hy, hz)
        lo1, hi1, lo2, hi2 = -h[t1], h[t1], -h[t2], h[t2]
        horizontal = ax == 2
        c = (cx, cy, cz)
        f1 = [lo1 + r, hi1 - r]
        f2 = [lo2 + r, hi2 - r]
        if joints and (not horizontal or top_joints):
            if horizontal:
                f1 += [g - c[t1] for g in grid_lines(c[t1] + lo1, c[t1] + hi1, SLAB_W / 2)]
                f2 += [g - c[t2] for g in grid_lines(c[t2] + lo2, c[t2] + hi2, SLAB_D)]
            else:
                f2 += [g - c[2] for g in grid_lines(c[2] + lo2, c[2] + hi2, COURSE)]
                f1 += [g - c[t1] for g in grid_lines(c[t1] + lo1, c[t1] + hi1, BLOCK / 2)]
        u_s, v_s = samples(lo1, hi1, f1), samples(lo2, hi2, f2)
        pts = []
        for v in v_s:
            row = []
            for u in u_s:
                p = [0.0, 0.0, 0.0]
                p[ax], p[t1], p[t2] = sgn * h[ax], u, v
                q = (min(max(p[0], -ix), ix), min(max(p[1], -iy), iy), min(max(p[2], -iz), iz))
                dx, dy, dz = p[0] - q[0], p[1] - q[1], p[2] - q[2]
                l = math.sqrt(dx * dx + dy * dy + dz * dz) or 1
                nx, ny, nz = dx / l, dy / l, dz / l
                px, py, pz = q[0] + nx * r, q[1] + ny * r, q[2] + nz * r
                wx, wy, wz = cx + px, cy + py, cz + pz
                e1, e2 = h[t1] - abs(u), h[t2] - abs(v)
                for qf in quiet:      # distance to a quiet side doesn't count as an edge
                    qa, qs = "xyz".index(qf.lower()), (1 if qf.isupper() else -1)
                    if qa == t1 and (u > 0) == (qs > 0):
                        e1 = 1e9
                    if qa == t2 and (v > 0) == (qs > 0):
                        e2 = 1e9
                edge = min(e1, e2)
                if joints and (not horizontal or top_joints):
                    jd = joint_dist(wx, wy, wz, horizontal, None if horizontal else t1)
                else:
                    jd = 1e9
                d = weather(wx, wy, wz, edge, jd, s, chip)
                px, py, pz = px - nx * d, py - ny * d, pz - nz * d
                row.append((px, py, pz) if local else (wx - nx * d, wy - ny * d, wz - nz * d))
            pts.append(row)
        nrm = [0, 0, 0]; nrm[ax] = sgn
        m.grid(pts, lambda p, nrm=tuple(nrm): nrm)
    return m


def column(centre, radius, height, s, cap=True, segs=160, courses=True, chip=1.0):
    """Drum-built column: each course its own slightly different drum, rims worn
    and chipped, joints grooved. Vertices relative to centre."""
    cx, cy, cz = centre
    z0, z1 = -height / 2, height / 2
    feats = [z0, z1] + ([g - cz for g in grid_lines(cz + z0, cz + z1, COURSE)] if courses else [])
    zs = samples(z0, z1, feats, fine=2.5, mid=8, coarse=30)
    rng = random.Random(s)
    drum = {}
    pts = []
    for z in zs:
        wz = cz + z
        k = math.floor(wz / COURSE)
        if k not in drum:
            drum[k] = (rng.uniform(-7, 7), rng.uniform(0, 6.283))
        dr, ph = drum[k]
        fz = wz - k * COURSE
        jd = min(fz, COURSE - fz) if courses else 1e9
        edge = min(z - z0, z1 - z)
        row = []
        for i in range(segs + 1):
            t = 6.2832 * i / segs
            ct, st = math.cos(t), math.sin(t)
            wx, wy = cx + ct * radius, cy + st * radius
            d = weather(wx, wy, wz, edge, jd, s, chip)
            d += 2.5 * math.sin(t * 3 + ph) * (1 if courses else 0)     # drums not perfectly round
            rr = radius + dr - d
            row.append((ct * rr, st * rr, z))
        pts.append(row)
    m = Mesh()
    m.grid(pts, lambda p: (p[0], p[1], 0))
    if cap:
        top = pts[-1]
        rings = []
        for f in (1.0, 0.85, 0.6, 0.3, 0.0):
            rings.append([(p[0] * f, p[1] * f, z1 - 1.5 * (1 - f) * (1 - f)) for p in top])
        m.grid(rings, lambda p: (0, 0, 1))
    return m


# ---------------------------------------------------------------- jobs
def job(spec):
    kind, name = spec["kind"], spec["name"]
    s = spec.get("seed", 0) * 13.7
    if kind == "box":
        m = box(spec["centre"], spec["half"], spec.get("r", 5), s, spec.get("faces", "xXyYzZ"),
                spec.get("joints", True), spec.get("top_joints", False), spec.get("chip", 1.0),
                quiet=spec.get("quiet", ()))
    elif kind == "column":
        m = column(spec["centre"], spec["radius"], spec["height"], s, courses=spec.get("courses", True),
                   segs=spec.get("segs", 160), chip=spec.get("chip", 1.0))
    elif kind == "slabs":
        m = Mesh()
        for sl in spec["slabs"]:
            box(sl["centre"], sl["half"], 4, sl["seed"], "xXyYZ", joints=False, chip=sl.get("chip", 0.8), local=False, mesh=m)
    n = m.write(os.path.join(OUT, name + ".obj"))
    return name, n


# column height multiplier (1 = the original 100 m colossi); 5 sends them up into the sky
TALL = float(os.environ.get("VISEX_TALL", "5"))


def plan(actors):
    specs, assign = [], []
    rng = random.Random(7)
    by = {a["label"]: a for a in actors}
    for a in actors:
        L, loc, sc, mesh = a["label"], a["loc"], a["scale"], a["mesh"]
        size = [v * 100 for v in sc]
        if L.startswith("Plinth_"):
            name = "SM_" + L
            specs.append(dict(kind="box", name=name, centre=loc, half=[v / 2 for v in size], r=6, seed=len(specs), faces="xXyYZ"))
            assign.append(dict(label=L, asset=name, scale=[1, 1, 1], rot=[0, 0, 0], collide=True))
        elif L.startswith("Column_"):
            name = "SM_" + L
            # TALL: columns rise into the dust ceiling; base stays on the plinth
            h = size[2] * TALL
            c = [loc[0], loc[1], loc[2] - size[2] / 2 + h / 2]
            specs.append(dict(kind="column", name=name, centre=c, radius=size[0] / 2, height=h, seed=len(specs)))
            assign.append(dict(label=L, asset=name, scale=[1, 1, 1], loc=c, collide=True))
        elif L.startswith("Step_") and size[2] > 1:
            name = "SM_" + L
            # only the visible part: tread, the top of the riser, the ends
            specs.append(dict(kind="box", name=name, centre=loc, half=[v / 2 for v in size], r=5, seed=len(specs),
                              faces="yxXZ", top_joints=True, chip=0.7, quiet=("Y",)))
            assign.append(dict(label=L, asset=name, scale=[1, 1, 1], collide=True))
    # rubble: a few unit variants, chunkier chips, keep each actor's own scale
    for i in range(6):
        specs.append(dict(kind="box", name="SM_Rubble_%d" % i, centre=[i * 977.0, 0, 0], half=[50, 50, 50], r=4,
                          seed=100 + i, joints=False, chip=0.45))
    for i in range(4):
        specs.append(dict(kind="column", name="SM_Drum_%d" % i, centre=[i * 733.0, 5000, 0], radius=50, height=100,
                          seed=200 + i, courses=False, segs=96, chip=0.45))
    for a in actors:
        if a["label"].startswith("Ruin_"):
            v = "SM_Rubble_%d" % rng.randrange(6) if a["mesh"] == "Cube" else "SM_Drum_%d" % rng.randrange(4)
            assign.append(dict(label=a["label"], asset=v))
    # the plaza floor as separate giant slabs on the world slab grid
    slabs, srng = [], random.Random(3)
    x0, x1, y0, y1, T = -5000, 35000, -1500, 3000, 60.0
    for row in range(math.floor(y0 / SLAB_D), math.ceil(y1 / SLAB_D)):
        ya, yb = max(row * SLAB_D, y0), min((row + 1) * SLAB_D, y1)
        off = 0.5 * (abs(row) % 2) * SLAB_W
        k = math.floor((x0 + off) / SLAB_W)
        while True:
            xa, xb = k * SLAB_W - off, (k + 1) * SLAB_W - off
            k += 1
            if xa >= x1:
                break
            xa, xb = max(xa, x0), min(xb, x1)
            sink = srng.uniform(0, 5)
            slabs.append(dict(centre=[(xa + xb) / 2, (ya + yb) / 2, -T / 2 - sink],
                              half=[(xb - xa) / 2 - 0.4, (yb - ya) / 2 - 0.4, T / 2], seed=srng.uniform(0, 500),
                              chip=srng.uniform(0.6, 1.2)))
    # split into a few meshes so each stays a sensible size
    for c in range(4):
        part = slabs[c::4]
        specs.append(dict(kind="slabs", name="SM_PlazaSlabs_%d" % c, slabs=part))
    return specs, assign, dict(plaza_top=-T)


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    actors = json.load(open(os.path.join(BRIDGE, "actors.json")))
    specs, assign, extra = plan(actors)
    only = sys.argv[1:]
    run = [s for s in specs if not only or any(s["name"].startswith(o) for o in only)]
    with Pool() as pool:
        for name, n in pool.imap_unordered(job, sorted(run, key=lambda s: s["kind"] != "column")):
            print("%-22s %7d verts" % (name, n), flush=True)
    json.dump(dict(meshes=[s["name"] for s in specs], assign=assign, **extra),
              open(os.path.join(OUT, "manifest.json"), "w"), indent=1)
    print("manifest:", len(specs), "meshes,", len(assign), "assignments")
