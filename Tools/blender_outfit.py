"""Build the wardrobe on the android skin in Blender (headless). Outside the editor:

  /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup \
      --python Tools/blender_outfit.py -- SourceArt/Character/android_skin.fbx \
      SourceArt/Character/Outfits [<preview_dir>] [--only=Noir,Catsuit]

Writes outfit_<name>.fbx per outfit; Tools/char_outfit.py imports them into UE.

Every piece is cut from the skin itself, so it fits her exactly. Depth comes from layers:

  fabric    a piece of skin where a coverage field (cm, 0 on the edge) is positive, cut
            cleanly through the triangles and lifted 0.1-0.6 cm; opaque pieces get a rim
            down to the skin so they read as material with thickness
  piping    a round cord along a piece's edge
  seams     flat tapes sewn along the body (boning, stocking seams, catsuit stripes)
  straps    taut ribbons between two points (garters), lifted where the body bulges
  hardware  small bevelled blocks (sliders, clips, rings)

Each vertex copies bone weights and every shape key from the nearest skin, so animation,
the body-slider morphs and the jiggle drive the clothes with the body.

Material slots: Sheer, Lace, Net, Satin, Latex, Trim, Nylon, Binding. UV0 is a garment
coordinate in cm (around the leg or the torso, and height), wrapped per triangle into one
24 cm period so lace and fishnet patterns run round her without a seam. UV1 is (stretch,
leg-ness) for the nylon: how far the knit is pulled open there, and which denier it is.
"""
import math
import os
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree
from mathutils.kdtree import KDTree

args = sys.argv[sys.argv.index("--") + 1:]
SRC, OUT_DIR = args[0], args[1]
PREVIEW = args[2] if len(args) > 2 and not args[2].startswith("--") else None
ONLY = next((a.split("=", 1)[1].split(",") for a in args if a.startswith("--only=")), None)
os.makedirs(OUT_DIR, exist_ok=True)

SLOTS = ["Sheer", "Lace", "Net", "Satin", "Latex", "Trim", "Nylon", "Binding"]
OPAQUE = ("Satin", "Latex", "Trim")
CL, CP, PERIOD = 48.0, 96.0, 24.0     # nominal leg and torso girth (cm), pattern period

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=SRC)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
skin = next(o for o in bpy.data.objects if o.type == "MESH")
for m in skin.modifiers:
    if m.type == "ARMATURE":
        m.show_viewport = False
me = skin.data
# the FBX also carries her hair (material "Hair"): cloth is fitted to skin only
_hair = [i for i, m in enumerate(me.materials) if m and m.name.lower().startswith("hair")]
if _hair:
    hb = bmesh.new()
    hb.from_mesh(me)
    bmesh.ops.delete(hb, geom=[f for f in hb.faces if f.material_index in _hair], context="FACES")
    bmesh.ops.delete(hb, geom=[v for v in hb.verts if not v.link_faces], context="VERTS")
    hb.to_mesh(me)
    hb.free()
    me.update()
nv = len(me.vertices)
print("skin verts", nv, "(hair removed)" if _hair else "")
M4 = np.array(skin.matrix_world)
R3 = M4[:3, :3]


def vget(seq, attr, n):
    a = np.empty(n * 3)
    seq.foreach_get(attr, a)
    return a.reshape(-1, 3)


CO = vget(me.vertices, "co", nv)
X = CO @ R3.T + M4[:3, 3]                 # world, cm
V3 = lambda a: Vector((float(a[0]), float(a[1]), float(a[2])))
NP = lambda v: np.array((v.x, v.y, v.z))

tb = bmesh.new()
tb.from_mesh(me)
bmesh.ops.triangulate(tb, faces=tb.faces)
T = np.array([[v.index for v in f.verts] for f in tb.faces])
EDGES = np.array([[e.verts[0].index, e.verts[1].index] for e in tb.edges])
tb.free()

gnames = [g.name for g in skin.vertex_groups]
WM = np.zeros((nv, len(gnames)), np.float32)
for v in me.vertices:
    for g in v.groups:
        WM[v.index, g.group] = g.weight


def G(*keys):
    return WM[:, [i for i, n in enumerate(gnames) if any(k in n for k in keys)]].sum(1).astype(float)


keys = me.shape_keys.key_blocks if me.shape_keys else []
DELTAS = {k.name: (vget(k.data, "co", nv) - CO) @ R3.T for k in keys[1:]}
# The rest shape is flat: bust and glutes come from the slider morphs. Design on her
# default body (the Character page's starting values) and derive the rest shape at the end.
DESIGN = {k: w for k, w in (("BustUp", 0.70), ("BustApart", 0.96), ("GlutesUp", 1.0), ("Fat", 0.20), ("ThighGap", 0.35)) if k in DELTAS}
for k, w in DESIGN.items():
    X = X + DELTAS[k] * w
_dm = bpy.data.meshes.new("design")
_dm.from_pydata(X.tolist(), [], T.tolist())
_dm.update()
NRM = vget(_dm.vertices, "normal", nv)
NRM /= np.maximum(np.linalg.norm(NRM, axis=1, keepdims=True), 1e-9)
bpy.data.meshes.remove(_dm)
WV = [V3(p) for p in X]
NV = [V3(n) for n in NRM]

Bn = {b.name: NP(arm.matrix_world @ b.head_local) for b in arm.data.bones}
FWD = (Bn["ball_l"] - Bn["foot_l"]) + (Bn["ball_r"] - Bn["foot_r"])
FWD[2] = 0
FWD /= np.linalg.norm(FWD)
UP = np.array((0.0, 0.0, 1.0))
LEFT = np.cross(UP, FWD)
if (Bn["thigh_l"] - Bn["pelvis"]).dot(LEFT) < 0:
    LEFT = -LEFT
PEL = Bn["pelvis"]
LAT = (X - PEL) @ LEFT          # + her left
FW = (X - PEL) @ FWD            # + in front of the pelvis
Z = X[:, 2]
ALAT = np.abs(LAT)
D = math.radians
SIDE = {1: "l", -1: "r"}
fwd_v, left_v = V3(FWD), V3(LEFT)


def diffuse(a, iters):
    deg = np.zeros(nv)
    np.add.at(deg, EDGES[:, 0], 1.0)
    np.add.at(deg, EDGES[:, 1], 1.0)
    deg = np.maximum(deg, 1.0)
    for _ in range(iters):
        acc = np.zeros_like(a)
        np.add.at(acc, EDGES[:, 0], a[EDGES[:, 1]])
        np.add.at(acc, EDGES[:, 1], a[EDGES[:, 0]])
        a = 0.5 * a + 0.5 * acc / (deg[:, None] if a.ndim == 2 else deg)
    return a


NS = diffuse(NRM, 6)
NS /= np.linalg.norm(NS, axis=1, keepdims=True)

ARMS = G("clavicle", "upperarm", "lowerarm", "hand", "thumb", "index", "middle", "ring", "pinky")
UPPERARM = G("upperarm", "lowerarm", "hand", "thumb", "index", "middle", "ring", "pinky")
HANDS = G("hand", "thumb", "index", "middle", "ring", "pinky")
HEAD = G("head", "neck")
NECK = G("neck")
LEGS = G("thigh", "calf", "foot", "ball")
wT, wTN, wTNT = G("pelvis", "spine", "clavicle"), G("pelvis", "spine", "clavicle", "neck"), G("pelvis", "spine", "clavicle", "neck", "thigh")
wU = G("upperarm")

# ---- landmarks (cm, rest pose) --------------------------------------------------------------
LM = dict(
    crotch=Bn["thigh_l"][2] - 10.0,
    hip_top=PEL[2] + 3.0,                  # low-rise waistline of briefs
    waist=(Bn["spine_02"][2] + Bn["spine_03"][2]) * 0.5,
    hip_half=abs(Bn["thigh_l"].dot(LEFT) - PEL.dot(LEFT)) + 7.5,
    knee=Bn["calf_l"][2],
    thigh_mid=(Bn["thigh_l"][2] + Bn["calf_l"][2]) * 0.5,
    shoulder=Bn["clavicle_l"][2],
    ankle=Bn["foot_l"][2],
    neck=Bn["neck_01"][2],
    chin=Bn["head"][2] - 4.0,
)


def surface_near(point, direction=None):
    """The skin vertex nearest to a point (optionally preferring one facing a direction)."""
    d = np.linalg.norm(X - point, axis=1)
    if direction is not None:
        d = d - 2.0 * np.clip(NS @ direction, 0, 1)
    return X[np.argmin(d + 100.0 * (UPPERARM > 0.3))]


def frame(direction, s, yaw, pitch):
    out = LEFT * s
    f = direction * math.cos(D(yaw)) + out * math.sin(D(yaw))
    f = f * math.cos(D(pitch)) - UP * math.sin(D(pitch))
    f /= np.linalg.norm(f)
    u = UP - f * UP.dot(f)
    u /= np.linalg.norm(u)
    l = out - f * out.dot(f) - u * out.dot(u)
    return f, u, l / np.linalg.norm(l)


# the bust is wherever the Bust morph moves the skin: apex and extent per side
_bd = np.linalg.norm(DELTAS["BustUp"], axis=1)
ANCHOR, CUP = {}, {}
for s in (1, -1):
    mk = (_bd > 0.25 * _bd.max()) & (LAT * s > 0)
    i = np.nonzero(mk)[0]
    ANCHOR[s] = X[i[np.argmax(FW[i])]]
    f, u, l = frame(FWD, s, 12, 6)
    Dv = X[i] - ANCHOR[s]
    x, y = Dv @ l, Dv @ u
    CUP[s] = dict(out=x.max(), inn=min(-x.min(), abs((ANCHOR[s] - PEL) @ LEFT) - 0.6), up=y.max(), dn=-y.min())
LM["bust"] = ANCHOR[1][2]
LM["fold"] = LM["bust"] - CUP[1]["dn"] * 0.92
print("bust", ANCHOR[1].round(1), {k: round(float(v), 1) for k, v in CUP[1].items()})
m_ = (ALAT < 1.2) & (Z > 60) & (Z < PEL[2]) & (LEGS + wT > 0.5)
LM["crotch_skin"] = Z[m_].min()
m_ = (np.abs(Z - (PEL[2] - 6)) < 0.8) & (G("pelvis", "spine", "thigh") > 0.5)
LM["hip_skin"] = ALAT[m_].max()
print("landmarks", {k: round(float(v), 1) for k, v in LM.items()})

TORSO = (ARMS < 0.35) & (HEAD < 0.5)
NEG = -50.0
where = lambda mask, f: np.where(mask, f, NEG)
smooth01 = lambda a, b, x: (lambda t: t * t * (3 - 2 * t))(np.clip((x - a) / (b - a), 0, 1))

# ---- garment coordinates for patterns --------------------------------------------------------
# Leg and torso angles turn the same way and agree on the outer hip, so the pattern only
# shears a little where one hands over to the other (crotch to hip).
_UL = np.zeros(nv)
_RLEG = np.zeros(nv)
for s in (1, -1):
    a, b = Bn["thigh_" + SIDE[s]], Bn["foot_" + SIDE[s]]
    k = np.clip((a[2] - Z) / (a[2] - b[2]), -0.3, 1.2)
    d = X - (a + (b - a) * k[:, None])
    _RLEG = np.where((LAT >= 0) == (s == 1), np.linalg.norm(d[:, :2], axis=1), _RLEG)
    c = s * math.pi / 2                                   # branch cut on the inner thigh
    th = (np.arctan2(d @ LEFT, d @ FWD) - c + math.pi) % (2 * math.pi) - math.pi + c
    _UL = np.where((LAT >= 0) == (s == 1), th / (2 * math.pi) * CL + s * CL / 4, _UL)
_UPL = np.arctan2(LAT, FW) / (2 * math.pi) * CP
_TB = np.maximum(smooth01(LM["crotch"] + 1, LM["crotch"] + 11, Z),
                 (1 - smooth01(1.5, 7.0, ALAT)) * smooth01(LM["crotch"] - 1, LM["crotch"] + 2, Z))
_TB = np.where(UPPERARM > 0.5, 1.0, _TB)

# Nylon: where the knit is pulled open (bust, glutes, thighs: 1) or relaxed (ankles: 0),
# and how much of a leg it is (the legs are a heavier denier than the top).
_gd = np.linalg.norm(DELTAS["GlutesUp"], axis=1) if "GlutesUp" in DELTAS else np.zeros(nv)
_LEGNESS = (1 - smooth01(LM["crotch_skin"], LM["crotch_skin"] + 8, Z)) * (UPPERARM < 0.3)
STRETCH = diffuse(np.where(_LEGNESS > 0.5, smooth01(3.0, 9.5, _RLEG),
                           np.clip(0.3 + 0.7 * np.maximum(_bd / _bd.max(), _gd / max(_gd.max(), 1e-6)), 0, 1)), 10)
ZONE = diffuse(_LEGNESS, 8)
# Tight cloth spans small hollows (navel, spine, creases) instead of following them in:
# how far a smoothed body lies outside the real one, per vertex, in cm.
_Xs = diffuse(X, 45)
BRIDGE = diffuse(np.clip(np.einsum("ij,ij->i", _Xs - X, NRM), 0.0, 1.5), 4)

# ---- spatial helpers ---------------------------------------------------------------------------
kd = KDTree(nv)
for i, p in enumerate(WV):
    kd.insert(p, i)
kd.balance()
nearest = lambda p: kd.find(p)[1]
nearest_normal = lambda p: NV[kd.find(p)[1]]


def bvh_of(mask):
    return BVHTree.FromPolygons(WV, T[mask[T].all(1)].tolist())


BODY_BVH = bvh_of(np.ones(nv, bool))
TORSO_BVH = bvh_of(UPPERARM < 0.3)
LEG_BVH = {s: bvh_of((LEGS >= 0.5) & ((LAT > 0) == (s == 1))) for s in (1, -1)}
pel_v = V3(PEL)


def leg_axis(s, z):
    a, b = Bn["thigh_" + SIDE[s]], Bn["foot_" + SIDE[s]]
    return V3(a + (b - a) * ((a[2] - z) / (a[2] - b[2])))


def ray(bvh, c, z, theta, offset):
    d = fwd_v * math.cos(theta) + left_v * math.sin(theta)
    hit = bvh.ray_cast(Vector((c.x, c.y, z)) + d * 80, -d, 80)
    return hit[0] + hit[1] * offset if hit[0] else None


# ---- builders ----------------------------------------------------------------------------------
PIECES = []          # (bmesh, slot, sources) of the outfit being built
SRC_OF = {}          # outfit object name -> per-vertex (skin a, skin b, t), a = -1 if unknown


def edge_loops(bm, min_len):
    adj = {}
    for e in bm.edges:
        if e.is_boundary:
            a, b = e.verts
            adj.setdefault(a, []).append(b)
            adj.setdefault(b, []).append(a)
    seen, loops = set(), []
    for start in adj:
        if start in seen:
            continue
        chain, prev, cur = [start], None, start
        seen.add(start)
        while True:
            nxt = [n for n in adj[cur] if n is not prev and n not in seen]
            if not nxt:
                break
            prev, cur = cur, nxt[0]
            seen.add(cur)
            chain.append(cur)
        pts = [v.co.copy() for v in chain]
        if sum((a - b).length for a, b in zip(pts, pts[1:])) >= min_len:
            loops.append((pts, start in adj[chain[-1]]))
    return loops


def region(val, offset, slot, rim=None, pipe=None, pipe_r=0.18, min_loop=10.0, lift=None, bind=None):
    """The part of the skin where val >= 0, cut cleanly along the zero line and lifted
    off the skin. rim: a wall from the edge down to the skin (default: opaque slots).
    pipe: slot of a cord run along every edge. lift: extra height per skin vertex (BRIDGE).
    bind: (slot, width) of a flat binding sewn along every edge."""
    val = np.asarray(val, float)
    ll = lift.tolist() if lift is not None else None
    sel = np.nonzero((val[T] >= 0).any(1))[0]
    if not len(sel):
        return None
    vl = val.tolist()
    bm = bmesh.new()
    made, nrm, srcof, src = {}, {}, {}, []      # src: the skin vertices each new vertex came from

    def vert(a, b=None):
        key = a if b is None else (min(a, b), max(a, b))
        v = made.get(key)
        if v is None:
            if b is None:
                p, n, sv, up_ = WV[a], NV[a], (a, a, 0.0), (ll[a] if ll else 0.0)
            else:
                t = vl[a] / (vl[a] - vl[b])
                p, n, sv = WV[a].lerp(WV[b], t), NV[a].lerp(NV[b], t).normalized(), (a, b, t)
                up_ = ll[a] + (ll[b] - ll[a]) * t if ll else 0.0
            v = made[key] = bm.verts.new(p + n * (offset + up_))
            nrm[v] = n
            srcof[v] = sv
            src.append(sv)
        return v

    for f in T[sel].tolist():
        poly = []
        for k in range(3):
            a, b = f[k], f[(k + 1) % 3]
            if vl[a] >= 0:
                poly.append(vert(a))
            if (vl[a] >= 0) != (vl[b] >= 0):
                poly.append(vert(a, b))
        poly = [v for k, v in enumerate(poly) if v is not poly[k - 1]]
        if len(poly) >= 3:
            try:
                bm.faces.new(poly)
            except ValueError:
                pass
    loops = edge_loops(bm, min_loop) if (pipe or bind) else []
    if rim if rim is not None else slot in OPAQUE:
        inner = {}
        for e in [e for e in bm.edges if e.is_boundary]:
            a, b = e.verts
            for v in (a, b):
                if v not in inner:
                    inner[v] = bm.verts.new(v.co - nrm[v] * max(0.0, offset - 0.03))
                    src.append(srcof[v])
            bm.faces.new((a, b, inner[b], inner[a]))
    PIECES.append((bm, slot, src))
    for pts, closed in loops:
        if pipe:
            tube(pts, pipe_r, pipe, closed, push=0.08)
        if bind:
            tube(pts, 0.0, bind[0], closed, push=0.015, flat=(bind[1], 0.045), step=0.5)
    return bm


def smooth_pts(pts, closed, iters=6):
    for _ in range(iters):
        n = len(pts)
        pts = [pts[i] if (not closed and i in (0, n - 1)) else
               (pts[(i - 1) % n] + pts[i] * 2 + pts[(i + 1) % n]) / 4 for i in range(n)]
    return pts


def resample(pts, closed, step):
    seq = pts + ([pts[0]] if closed else [])
    out = [seq[0]]
    need = step
    for a, b in zip(seq, seq[1:]):
        L = (b - a).length
        pos = 0.0
        while L - pos >= need:
            pos += need
            out.append(a.lerp(b, pos / L))
            need = step
        need -= L - pos
    if closed and len(out) > 2 and (out[-1] - out[0]).length < step * 0.5:
        out.pop()
    return out


def tube(pts, r, slot, closed=False, sides=6, push=0.0, step=0.8, flat=None, rigid=None):
    """A round cord along a point list. flat=(width, thickness): a tape lying on the
    surface instead. rigid: one skin vertex the whole piece rides (jewellery)."""
    if flat:
        sides = 4
    pts = smooth_pts(resample(pts, closed, step), closed)
    n = len(pts)
    if n < 4:
        return
    bm = bmesh.new()
    rings, src = [], []
    for i, p in enumerate(pts):
        j = rigid if rigid is not None else nearest(p)      # the whole ring rides this skin vertex
        src += [(j, j, 0.0)] * sides
        a_, b_ = (pts[i - 1], pts[(i + 1) % n]) if closed else (pts[max(i - 1, 0)], pts[min(i + 1, n - 1)])
        t = (b_ - a_).normalized()
        out = nearest_normal(p)
        a = out - t * out.dot(t)
        if a.length < 1e-4:
            a = t.orthogonal()
        a.normalize()
        b = t.cross(a)
        c = p + out * push
        if flat:
            w_, t_ = flat
            rings.append([bm.verts.new(c + b * sx * w_ * 0.5 + a * h) for sx, h in ((-1, 0), (1, 0), (1, t_), (-1, t_))])
        else:
            rings.append([bm.verts.new(c + (a * math.cos(2 * math.pi * k / sides) + b * math.sin(2 * math.pi * k / sides)) * r)
                          for k in range(sides)])
    for i in range(n if closed else n - 1):
        r0, r1 = rings[i], rings[(i + 1) % n]
        for k in range(sides):
            bm.faces.new((r0[k], r0[(k + 1) % sides], r1[(k + 1) % sides], r1[k]))
    if not closed:
        bm.faces.new(rings[0][::-1])
        bm.faces.new(rings[-1])
    PIECES.append((bm, slot, src))


def seam(pts, r, slot, field=None):
    """A flat tape sewn along surface points (boning channel, stocking seam, stripe); with a
    field, only where the garment is. Kept low: the engine's screen-space shadow trace turns
    a raised cord into a row of dark dashes on the skin beside it in raking light."""
    pts = [p for p in pts if p]
    tape = lambda run: ribbon(run, r * 2.6, 0.07, slot) if len(run) > 5 else None
    if field is None:
        return tape(pts)
    run = []
    for p in pts + [None]:
        if p is not None and field[nearest(p)] > 0.6:
            run.append(p)
        else:
            tape(run)
            run = []


def torso_line(theta, z0, z1, offset, step=0.8):
    n = max(2, int(abs(z1 - z0) / step))
    return [ray(TORSO_BVH, pel_v, z0 + (z1 - z0) * k / n, D(theta), offset) for k in range(n + 1)]


def leg_line(s, theta, z0, z1, offset, step=0.8):
    n = max(2, int(abs(z1 - z0) / step))
    zs = [z0 + (z1 - z0) * k / n for k in range(n + 1)]
    return [ray(LEG_BVH[s], leg_axis(s, z), z, D(theta), offset) for z in zs]


def ribbon(pts, w, thick, slot):
    pts = smooth_pts(resample(pts, False, 0.8), False, 8)
    bm = bmesh.new()
    rows, src = [], []
    for i, p in enumerate(pts):
        j = nearest(p)
        src += [(j, j, 0.0)] * 4
        t = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
        n = nearest_normal(p)
        b = t.cross(n).normalized()
        rows.append([bm.verts.new(p + b * s * w / 2 + n * h) for s, h in ((-1, 0), (1, 0), (1, thick), (-1, thick))])
    for r0, r1 in zip(rows, rows[1:]):
        for k in range(4):
            bm.faces.new((r0[k], r0[(k + 1) % 4], r1[(k + 1) % 4], r1[k]))
    bm.faces.new(rows[0][::-1])
    bm.faces.new(rows[-1])
    PIECES.append((bm, slot, src))
    return pts


def clip(center, n, t, size, slot="Trim"):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    b = t.cross(n)
    if b.length < 1e-4:
        b = n.orthogonal()
    b.normalize()
    t = n.cross(b)
    Mx = Matrix((b * size[0], t * size[1], n * size[2])).transposed().to_4x4()
    Mx.translation = center
    bmesh.ops.transform(bm, matrix=Mx, verts=bm.verts)
    bmesh.ops.bevel(bm, geom=bm.edges[:], offset=min(size) * 0.22, segments=2, affect="EDGES")
    j = nearest(center)                     # rigid: the whole piece rides one skin vertex
    PIECES.append((bm, slot, [(j, j, 0.0)] * len(bm.verts)))


def hardware(point, along, size, lift=0.5):
    """A slider, ring or buckle sitting on the skin at a point (numpy), turned along a direction."""
    p = V3(point)
    n = nearest_normal(p)
    clip(p + n * lift, n, V3(along).normalized(), size)


def garter(s, th_belt, th_leg, belt_z, bot_z, off_top, off_bot, slot="Satin", width=1.3, lift=0.9):
    """A taut strap from the belt to the stocking top, with a clip at each end."""
    top = ray(TORSO_BVH, pel_v, belt_z, D(th_belt * s), off_top)
    bot = ray(LEG_BVH[s], leg_axis(s, bot_z), bot_z, D(th_leg * s), off_bot)
    if not top or not bot:
        return
    pts = []
    for k in range(49):
        p = top.lerp(bot, k / 48)
        loc, nrm, _, _ = BODY_BVH.find_nearest(p)
        out = loc + nrm * lift * (0.35 + 0.65 * math.sin(math.pi * k / 48))
        pts.append(out if (p - loc).dot(nrm) < (out - loc).dot(nrm) else p)
    pts = ribbon(pts, width, 0.22, slot)
    t = (pts[-1] - pts[-5]).normalized()
    for at, size in ((pts[-2], (width + 0.8, 2.4, 0.5)), (pts[1], (width + 0.6, 1.2, 0.45)),
                     (pts[len(pts) // 2], (width + 0.4, 0.8, 0.4))):
        clip(at + nearest_normal(at) * 0.3, nearest_normal(at), t, size)


# ---- coverage fields (cm, 0 on the edge) ----------------------------------------------------------
def cups(top=0.30, scale=1.0):
    """Teardrop cups over the bust; top cuts the upper pole (0 = at the apex, 1 = full)."""
    out = np.full(nv, NEG)
    for s in (1, -1):
        f, u, l = frame(FWD, s, 12, 6)
        Dv = X - ANCHOR[s]
        x, y = Dv @ l, Dv @ u
        c_ = CUP[s]
        rx = np.where(x > 0, c_["out"], c_["inn"]) * scale
        ry = np.where(y > 0, c_["up"], c_["dn"]) * scale
        r = np.sqrt((x / rx) ** 2 + (y / ry) ** 2)
        c = np.minimum((1.0 - r) * 7.0, top * c_["up"] - y)
        out = np.maximum(out, where(TORSO & (NS @ FWD > -0.2) & (Dv @ f > -8), c))
    return out


def band(z, width, mask=TORSO):
    return where(mask, width * 0.5 - np.abs(Z - z))


def underband(width=1.8):
    return band(LM["fold"] - width * 0.5, width)


def path(points, width, mask=None):
    """A strap along points that hug the skin: distance to the polyline."""
    pts = []
    for a, b in zip(points[:-1], points[1:]):
        for t in np.linspace(0, 1, 16, endpoint=False):
            pts.append(surface_near(a + (b - a) * t))
    pts.append(surface_near(points[-1]))
    d = np.full(nv, 1e9)
    for a, b in zip(pts[:-1], pts[1:]):
        ab = b - a
        t = np.clip(((X - a) @ ab) / max(ab @ ab, 1e-6), 0, 1)
        d = np.minimum(d, np.linalg.norm(X - (a + t[:, None] * ab), axis=1))
    return where(mask if mask is not None else (UPPERARM < 0.5) & (HEAD < 0.5), width * 0.5 - d)


def taut(a, b, lift, n=40):
    """A straight line from a to b, pushed out wherever the body is in the way."""
    pts = []
    for k in range(n + 1):
        p = a.lerp(b, k / n)
        loc, nrm, _, _ = BODY_BVH.find_nearest(p)
        pts.append(loc + nrm * lift if (p - loc).dot(nrm) < lift else p)
    return pts


def shoulder_straps(width=1.0, cup_top=0.30, slot="Satin"):
    """Cup top, over the shoulder, down to the band at the back: ribbons with a slider."""
    for s in (1, -1):
        f, u, l = frame(FWD, s, 12, 6)
        start = surface_near(ANCHOR[s] + u * (cup_top * CUP[s]["up"] - 0.8) + l * 0.5 + f * 4, FWD)
        top = surface_near((Bn["clavicle_" + SIDE[s]] + Bn["upperarm_" + SIDE[s]]) * 0.5 + UP * 8.0, UP)
        back = np.array(start) - FWD * 40.0
        back[2] = LM["fold"]
        back = surface_near(back, -FWD)
        a, b, c = (V3(q) + nearest_normal(V3(q)) * 0.5 for q in (start, top, back))
        pts = ribbon(taut(a, b, 0.5)[:-1] + taut(b, c, 0.5), width, 0.2, slot)
        k = len(pts) // 5
        clip(pts[k] + nearest_normal(pts[k]) * 0.25, nearest_normal(pts[k]), (pts[k + 1] - pts[k - 1]).normalized(),
             (width + 0.7, 0.9, 0.4))


def brief(back="thong", rise=0.0, cut=1.6):
    """Front panel up to a low-rise waist with high-cut legs; back as thong or cheeky."""
    top = LM["hip_top"] + rise
    a = np.clip(ALAT / LM["hip_half"], 0, 1)
    z_leg = LM["crotch"] + (top - LM["crotch"]) * a ** cut
    front = np.minimum(top - Z, Z - z_leg)
    if back == "thong":
        t = np.clip((Z - LM["crotch"]) / (top - LM["crotch"]), 0, 1)
        rear = np.minimum(top - Z, 0.6 + 5.5 * t ** 2 - ALAT)
        rear = np.maximum(rear, np.minimum(0.6 - np.abs(Z - (top - 0.6)), 30.0))   # waist string
    else:   # cheeky: the leg line rises steeply over the cheek
        z_back = LM["crotch"] + (top - LM["crotch"]) * np.clip(a * 1.15, 0, 1) ** 0.7
        rear = np.minimum(top - Z, Z - z_back + 2.0)
    f = rear + (front - rear) * SIDE_BLEND       # front and back lines meet smoothly at the side
    reg = TORSO & (Z > LM["crotch"] - 4) & (Z < top + 2) & ~((LEGS > 0.9) & (Z < LM["crotch"] - 1))
    return where(reg, f)


def string_tanga():
    """A sheer string tanga: a small front triangle down to the gusset, a thong behind, and
    thin strings from the panel's top corners rising high over the hip bones (tanga_strings)."""
    cr, hb = LM["crotch"], LM["hip_top"]
    top_f = hb - 1.5                                             # low in front
    t = np.clip((Z - cr) / (top_f - cr), 0, 1)
    front = np.minimum(top_f - Z, 3.6 + 3.4 * t ** 1.6 - ALAT)    # covers the open gusset, 7 cm at the top
    tb = np.clip((Z - cr) / (hb - cr), 0, 1)
    rear = np.minimum(hb - 0.5 - Z, 0.5 + 3.2 * tb ** 2 + 2.6 * (1 - smooth01(cr + 1.0, cr + 6.0, Z)) - ALAT)  # thong, gusset-wide low down
    panel = rear + (front - rear) * SIDE_BLEND
    reg = TORSO & (Z > cr - 4) & (Z < hb + 8) & ~((LEGS > 0.9) & (Z < cr - 1))
    return where(reg, panel)


def tanga_strings(fit, rise=6.0, width=0.32):
    """The tanga's strings as flat tapes traced over the skin: from each top corner of the
    front panel up over the hip bone and down to the top of the thong at the small of the back."""
    cr, hb = LM["crotch"], LM["hip_top"]
    top_f, top_b = hb - 1.5, hb - 0.5
    bvh = bvh_of(TORSO & (HANDS < 0.5) if "HANDS" in globals() else TORSO)
    for s in (1, -1):
        pts, started = [], False
        for k in range(181):
            phi = math.radians(k)
            dv = V3(FWD * math.cos(phi) + LEFT * s * math.sin(phi))
            u = k / 180.0
            # rises from the front corner to a peak over the hip bone, falls to the thong top behind
            z = top_f + (top_b - top_f) * u + rise * math.sin(math.pi * min(max((u - 0.12) / 0.8, 0.0), 1.0)) ** 1.4
            hit = bvh.ray_cast(V3((PEL[0], PEL[1], z)), dv, 60)
            if not hit[0]:
                continue
            lat = abs((np.array(hit[0]) - PEL) @ LEFT)
            if not started and lat < 6.6:
                continue                                    # still inside the front panel
            started = True
            pts.append(hit[0] + nearest_normal(hit[0]) * (0.06 + float(fit[nearest(hit[0])])))
        tube(pts, 0.0, "Binding", flat=(width, 0.04), step=0.35)


def bodysuit(neck=None):
    neck = neck if neck is not None else LM["bust"] + 8.0
    a = np.clip(ALAT / LM["hip_half"], 0, 1)
    z_leg = LM["crotch"] + (LM["hip_top"] + 2 - LM["crotch"]) * a ** 1.6
    f_rear = Z - (LM["crotch"] + (LM["hip_top"] - LM["crotch"]) * np.clip(a * 1.15, 0, 1) ** 0.7) + 2.0
    f_leg = f_rear + (Z - z_leg - f_rear) * SIDE_BLEND
    f = np.minimum(np.minimum(neck - Z, (0.30 - ARMS) * 25.0), f_leg)
    return where((HEAD < 0.5) & (Z > LM["crotch"] - 4), f)


def catsuit():
    f = np.minimum(LM["shoulder"] + 5.0 - Z, (0.25 - HANDS) * 25.0)
    f = np.minimum(f, (0.5 - HEAD) * 30.0)
    f = np.minimum(f, Z - (LM["ankle"] + 5.0))            # ends above the ankle: her feet stay bare
    for s in (1, -1):   # side waist ovals
        side_pt = surface_near(PEL + LEFT * s * 30 + UP * (LM["waist"] - PEL[2] + 1), LEFT * s)
        r = np.sqrt(((Z - side_pt[2]) / 7.5) ** 2 + ((FW - (side_pt - PEL).dot(FWD)) / 4.0) ** 2)
        f = np.minimum(f, (np.where(((LAT * s) > 0) & (UPPERARM < 0.3), r, 9.0) - 1.0) * 4.0)   # torso only: not the arm beside it
    rw = ALAT / 4.0 + np.abs(Z - (LM["bust"] + 1.0)) / 7.0   # window between the breasts
    return np.minimum(f, (np.where(FW > 0, rw, 9.0) - 1.0) * 4.0)


LEGMASK = UPPERARM < 0.3
SIDE_BLEND = smooth01(-3.5, 3.5, FW)     # 0 behind the side seam, 1 in front of it
ST_TOP = LM["thigh_mid"] + 5.0
BELT_Z = LM["waist"] - 3.0


# ---- shared sets --------------------------------------------------------------------------------
def bra(cup_slot, cup_top=0.30, band_w=1.6, strap_w=1.0, pipe="Satin"):
    region(cups(cup_top), 0.30, cup_slot, pipe=pipe, pipe_r=0.15)
    region(underband(band_w), 0.46, "Satin")
    shoulder_straps(strap_w, cup_top)
    hardware(NP(ray(TORSO_BVH, pel_v, LM["fold"] - band_w * 0.5, 0.0, 0.0)), UP, (1.3, 1.3, 0.45), 0.6)   # centre ring


def stockings(top=ST_TOP, welt="Lace", welt_h=6.0, slot="Sheer"):
    region(where(LEGMASK & (Z < LM["crotch"]), top - Z), 0.26, slot)
    region(band(top - welt_h * 0.5, welt_h, LEGMASK & (Z < LM["crotch"])), 0.32, welt, pipe="Satin", pipe_r=0.16)
    for s in (1, -1):
        seam(leg_line(s, 180, top - welt_h, LM["ankle"] + 3, 0.29), 0.12, "Satin")


def garters(belt_z, bot_z, back=True, off_top=0.6, off_bot=0.5, slot="Satin", width=1.2):
    for s in (1, -1):
        garter(s, 18, 6, belt_z, bot_z, off_top, off_bot, slot, width)
        if back:
            garter(s, 140, 172, belt_z, bot_z, off_top, off_bot, slot, width)


# ---- the outfits ---------------------------------------------------------------------------------
def noir():
    """Sheer bodysuit with boning, lace cups, satin underband and straps, seamed thigh-highs."""
    body = bodysuit()
    region(body, 0.26, "Sheer", pipe="Satin", pipe_r=0.15)
    bra("Lace", 0.55, 1.8, 1.0)
    z0 = LM["fold"] - 2.2
    for th in (18, -18, 52, -52, 98, -98, 148, -148, 180):
        seam(torso_line(th, z0, LM["crotch"] - 2, 0.29), 0.15, "Satin", body)
    seam(torso_line(0, z0, LM["crotch"] - 1, 0.29), 0.13, "Satin", body)
    region(band(LM["waist"] - 4.4, 1.4), 0.38, "Satin")     # clear of the leg line: piping under a band flickers through it
    stockings()


def garter_set():
    """Lace balconette and cheeky briefs, a wide lace garter belt, four straps, stockings."""
    bra("Lace", 0.30, 1.5, 0.9)
    region(brief("cheeky"), 0.24, "Lace", pipe="Satin", pipe_r=0.14)
    region(band(BELT_Z, 7.0), 0.42, "Lace", pipe="Satin", pipe_r=0.17)
    for th in (0, 40, -40, 90, -90, 140, -140, 180):
        seam(torso_line(th, BELT_Z + 3.4, BELT_Z - 3.4, 0.45), 0.13, "Satin")
    hardware(surface_near(PEL + FWD * 30 + UP * (BELT_Z - 3.0 - PEL[2]), FWD), LEFT, (1.6, 1.0, 0.5), 0.7)
    garters(BELT_Z - 3.2, ST_TOP - 1.5)
    stockings()


def cocoa():
    """Satin bra and thong, slim garter belt, front straps to buckled thigh bands, choker."""
    bra("Satin", 0.35, 1.6, 1.0, pipe="Trim")
    region(brief("thong"), 0.26, "Satin")
    region(band(BELT_Z, 2.5), 0.42, "Satin", pipe="Trim", pipe_r=0.12)
    tz = LM["thigh_mid"] + 3.0
    region(band(tz, 1.8, LEGMASK & (Z < LM["crotch"] - 1)), 0.42, "Satin", pipe="Trim", pipe_r=0.12)
    garters(BELT_Z - 1.0, tz + 0.4, back=False, off_bot=0.6, width=1.0)
    for s in (1, -1):
        hardware(NP(ray(LEG_BVH[s], leg_axis(s, tz), tz, D(90 * s), 0.0)), UP, (1.6, 2.4, 0.5), 0.7)
        hardware(NP(ray(TORSO_BVH, pel_v, BELT_Z, D(90 * s), 0.0)), UP, (1.4, 2.9, 0.5), 0.7)
    cz = LM["neck"] + 3.5
    region(where(NECK > 0.4, 0.9 - np.abs(Z - cz)), 0.3, "Satin", pipe="Trim", pipe_r=0.1)
    hardware(NP(ray(BODY_BVH, V3(Bn["neck_01"]), cz, 0.0, 0.0)), UP, (1.5, 1.5, 0.45), 0.55)


def fishnet():
    """Fishnet tights with a satin waistband, satin bra and a high thong over them."""
    top = LM["hip_top"] + 4.0
    region(where(LEGMASK, top - Z), 0.22, "Net")
    region(band(top - 1.1, 2.2, LEGMASK), 0.38, "Satin")
    bra("Satin", 0.25, 1.4, 0.8)
    region(brief("thong", rise=0.4), 0.46, "Satin")
    for s in (1, -1):
        seam(leg_line(s, 180, LM["crotch"] - 6, LM["ankle"] + 3, 0.25), 0.14, "Satin")


def catsuit_set():
    """Glossy catsuit with waist and chest cutouts, piped openings, stripes, collar and belt."""
    f = catsuit()
    region(f, 0.25, "Latex", pipe="Trim", pipe_r=0.22)
    top = LM["shoulder"] + 5.0
    seam(torso_line(0, top - 1.0, LM["crotch"] + 1, 0.28), 0.16, "Trim", f)          # zip line
    for th in (34, -34, 150, -150):
        seam(torso_line(th, LM["fold"] - 1, LM["crotch"] + 6, 0.28), 0.15, "Latex", f)
    for s in (1, -1):                                                                # leg stripes, ankle cuffs
        seam(leg_line(s, 90 * s, LM["hip_top"] - 4, LM["ankle"] + 6, 0.28), 0.2, "Trim", f)
        region(where(LEGMASK & (LEGS > 0.5), 1.6 - np.abs(Z - (LM["ankle"] + 7.5))), 0.5, "Latex", pipe="Trim", pipe_r=0.14)
    bz = LM["hip_top"] + 5.0
    region(band(bz, 3.0), 0.55, "Latex", pipe="Trim", pipe_r=0.14)
    hardware(NP(ray(TORSO_BVH, pel_v, bz, 0.0, 0.0)), LEFT, (3.4, 2.6, 0.6), 0.85)
    wrist = np.minimum((0.62 - HANDS) * 25.0, (HANDS - 0.22) * 25.0)                 # cuff bands
    region(where(UPPERARM > 0.5, wrist), 0.5, "Latex", pipe="Trim", pipe_r=0.12)
    region(band(top - 1.5, 3.0, UPPERARM < 0.3), 0.5, "Latex")                       # collar


def sheer():
    """High-neck sheer bodysuit, latex yoke, underbust band and garter belt, six straps,
    stockings with latex welts, opera gloves."""
    chin_z = LM["chin"]
    m = (Z > 115) & (Z < 140) & (ALAT < 15) & (ARMS < 0.3)
    bust_z = Z[m][np.argmax(FW[m])]
    crotch_z = LM["crotch_skin"]
    leg_cut = crotch_z + (PEL[2] + 4 - crotch_z) * np.clip(ALAT / LM["hip_skin"], 0, 1) ** 1.5
    torso = np.minimum(wTNT - 0.5, 0.3 - wU)
    region(np.minimum(np.minimum(torso, chin_z - 1 - Z), Z - leg_cut), 0.25, "Sheer", pipe="Latex", pipe_r=0.19)
    yoke_lo = bust_z + 4.0
    yoke = np.minimum(np.minimum(wTN - 0.5, 0.28 - wU), np.minimum(chin_z - 1.4 - Z, Z - yoke_lo + 2.5 * (ALAT / 14.0) ** 2))
    region(yoke, 0.55, "Latex", pipe="Trim", pipe_r=0.22)
    ub_top, band_h, belt_h = bust_z - 7.5, 3.2, 4.5
    region(np.minimum(np.minimum(wT - 0.5, 0.12 - wU), np.minimum(ub_top - Z, Z - (ub_top - band_h))), 0.65, "Latex", pipe="Trim", pipe_r=0.22)
    belt_lo = PEL[2] + 7.0
    dip = 2.2 * np.maximum(0.0, 1 - (ALAT / 9.0) ** 2) * np.clip(FW / 4.0, 0, 1)     # soft V at the front
    region(np.minimum(wT - 0.5, np.minimum(belt_lo + belt_h - Z, Z - belt_lo + dip)), 0.75, "Latex", pipe="Trim", pipe_r=0.22)
    for th in (16, -16, 50, -50, 92, -92, 140, -140, 180):
        seam(torso_line(th, ub_top - band_h + 0.3, belt_lo + belt_h - 0.3, 0.28), 0.2, "Latex")
    for th in (24, -24):
        seam(torso_line(th, ub_top, yoke_lo + 0.5, 0.28), 0.2, "Latex")
    welt_top, welt_h = crotch_z - 17.0, 6.0
    leg = LEGS - 0.5
    region(np.minimum(leg, welt_top - 1 - Z), 0.26, "Sheer")
    region(np.minimum(leg, np.minimum(welt_top - Z, Z - (welt_top - welt_h))), 0.5, "Latex", pipe="Trim", pipe_r=0.22)
    for s in (1, -1):
        seam(leg_line(s, 180, welt_top - welt_h, LM["ankle"] + 3, 0.29), 0.13, "Latex")
        for th_belt, th_leg in ((16, 6), (72, 88), (140, 172)):
            garter(s, th_belt, th_leg, belt_lo + 0.6, welt_top - 2.2, 0.95, 0.7, "Latex", 1.3)
    along = np.zeros(nv)
    for s in (1, -1):
        a, b = Bn["upperarm_" + SIDE[s]], Bn["hand_" + SIDE[s]]
        along = np.where((LAT > 0) == (s == 1), ((X - a) @ (b - a)) / ((b - a) @ (b - a)), along)
    arm_f = UPPERARM - 0.5
    region(np.minimum(arm_f, (along - 0.42) * 30), 0.28, "Latex", pipe="Trim", pipe_r=0.22)
    region(np.minimum(arm_f, np.minimum((along - 0.45) * 30, (0.53 - along) * 30)), 0.58, "Latex", pipe="Trim", pipe_r=0.2)


def bodystocking():
    """The hero outfit, from Tema's references: one piece of sheer black nylon. Scoop-neck
    tank with wide straps and deep armholes, a deep V back, open hips from the waist round
    to the back, an open gusset, and stocking legs held by a front and a back strap each
    side, a sheer string tanga underneath. Bound edges, centre seams front and back, thin gold
    jewellery."""
    cs = LM["crotch_skin"]
    # neckline: a scoop in front, a deeper one behind, sharing the strap over the shoulder
    zn = LM["neck"] + 2.0
    rz = 30.0 + (22.5 - 30.0) * SIDE_BLEND                      # back depth -> front depth
    f_neck = (np.sqrt((ALAT / 7.0) ** 2 + (np.minimum(Z - zn, 0.0) / rz) ** 2) - 1.0) * 7.0
    f_neck = np.where(Z > zn, ALAT - 7.0, f_neck)
    # armholes: from the shoulder point down to the side of the bust
    cx, cz = abs((Bn["upperarm_l"] - PEL).dot(LEFT)) + 1.5, LM["shoulder"] - 4.0
    f_arm = (np.sqrt(((ALAT - cx) / 7.0) ** 2 + ((Z - cz) / 15.5) ** 2) - 1.0) * 7.0
    f_arm = np.where(ALAT > cx, np.minimum(f_arm, -(ALAT - cx) + np.where(np.abs(Z - cz) < 15.5, -1.0, 9.0)), f_arm)
    # open hips: a rounded window on each side, front strap edge round to the back strap edge
    f_hip = np.full(nv, 99.0)
    zb, zt, p_ = cs - 8.5, LM["waist"] + 0.5, 2.7
    for s in (1, -1):
        d = X - Bn["thigh_" + SIDE[s]]
        phi = np.degrees(np.arctan2(d @ (LEFT * s), d @ FWD))
        phi = np.where(phi < -80.0, phi + 360.0, phi)
        e = (np.abs((phi - 99.0) / 104.0) ** p_ + np.abs((Z - (zb + zt) * 0.5) / ((zt - zb) * 0.5)) ** p_) ** (1.0 / p_)
        f_hip = np.where((LAT * s > 0) & (Z < zt + 4) & (Z > zb - 4), np.minimum(f_hip, (e - 1.0) * 11.0), f_hip)
    # open gusset: a teardrop below the navel, narrowing through to the small of the back
    close = smooth01(cs - 7.0, cs + 1.5, Z)                     # runs out to a point on the inner thigh
    wf = 3.1 * np.clip((LM["hip_top"] + 1.0 - Z) / 10.0, 0, 1) ** 0.85 * close
    wb = 2.0 * np.clip((PEL[2] + 5.0 - Z) / 11.0, 0, 1) ** 0.85 * close
    w_open = wb + (wf - wb) * SIDE_BLEND
    f_open = np.where(w_open > 0.02, ALAT - w_open, 99.0)       # no opening: no edge either
    g = np.minimum.reduce([(0.3 - UPPERARM) * 25.0, (0.5 - HEAD) * 30.0, f_neck, f_arm, f_hip, f_open])

    fit = BRIDGE
    region(g, 0.09, "Nylon", lift=fit, bind=("Binding", 0.3), min_loop=6.0)
    # a sheer string tanga under the nylon (Tema's references): small see-through front, bound
    # edges, thin strings rising high over the hips (they show in the hip windows), thong back
    region(string_tanga(), 0.05, "Sheer", lift=fit, bind=("Binding", 0.2), min_loop=4.0)
    tanga_strings(fit)
    # the stocking tops are knitted double: a second layer from the hip crease down the thigh
    region(np.minimum(g, np.minimum((LEGS - 0.5) * 12.0, Z - (cs - 12.5))), 0.115, "Nylon", lift=fit)
    for th in (0, 180):                                          # centre seams, front and back
        line = [q + nearest_normal(q) * float(fit[nearest(q)]) for q in torso_line(th, zn - 6, cs, 0.19) if q]
        seam(line, 0.085, "Binding", g)

    # jewellery: a fine chain resting on the collarbones, oval bangles close round the wrists
    axis = V3(Bn["neck_01"])
    chain = []
    for k in range(72):
        th = 2 * math.pi * k / 72
        z = LM["neck"] + 2.2 + (-4.3 - 2.2) * (0.5 + 0.5 * math.cos(th)) ** 1.5
        dv = fwd_v * math.cos(th) + left_v * math.sin(th)
        hit = BODY_BVH.ray_cast(Vector((axis.x - fwd_v.x * 1.5, axis.y - fwd_v.y * 1.5, z)), dv, 30)
        if hit[0]:
            chain.append(hit[0] + hit[1] * (0.1 if hit[1].dot(dv) > 0 else -0.1))
    tube(chain, 0.065, "Trim", closed=True, sides=6, step=0.5)
    clip(chain[0] - Vector((0, 0, 0.55)) + nearest_normal(chain[0]) * 0.12, nearest_normal(chain[0]), Vector((0, 0, 1)), (0.5, 0.9, 0.22))
    for s, offs in ((1, (0.90, 0.955)), (-1, (0.93,))):
        a, b = Bn["lowerarm_" + SIDE[s]], Bn["hand_" + SIDE[s]]
        u = (b - a) / np.linalg.norm(b - a)
        e1 = np.cross(u, UP)
        e1 /= np.linalg.norm(e1)
        e2 = np.cross(u, e1)
        for k_, t_ in enumerate(offs):
            c = a + (b - a) * t_
            cv = V3(c)
            # an oval cuff fitted round the wrist, not a circle sized to its widest point
            tilt = u * (0.07 if k_ else -0.05)                    # a touch off square to the arm
            dirs = [e1 * math.cos(2 * math.pi * q / 48) + (e2 + tilt) * math.sin(2 * math.pi * q / 48) for q in range(48)]
            rads = []
            for dv in dirs:
                hit = BODY_BVH.ray_cast(cv, V3(dv / np.linalg.norm(dv)), 8)
                rads.append(hit[3] if hit[0] else 3.0)
            rads = np.array(rads)
            rads = np.maximum(rads, np.convolve(np.r_[rads[-4:], rads, rads[:4]], np.ones(9) / 9, "valid"))
            ring = [V3(c + dv / np.linalg.norm(dv) * (r_ + 0.22)) for dv, r_ in zip(dirs, rads)]
            tube(ring, 0.1, "Trim", closed=True, sides=8, step=0.35, rigid=nearest(cv))


OUTFITS = {"Noir": noir, "Garter": garter_set, "Cocoa": cocoa, "Fishnet": fishnet,
           "Catsuit": catsuit_set, "Sheer": sheer, "Bodystocking": bodystocking}


# ---- assemble: one mesh, weights, shape keys, garment UVs ------------------------------------------
def assemble(name):
    out_bm = bmesh.new()
    srcs = []
    for bm, slot, src in PIECES:
        srcs += src if src is not None else [(-1, -1, 0.0)] * len(bm.verts)
        assert src is None or len(src) == len(bm.verts)
        tmp = bpy.data.meshes.new("tmp")
        bm.to_mesh(tmp)
        bm.free()
        start = len(out_bm.faces)
        out_bm.from_mesh(tmp)
        bpy.data.meshes.remove(tmp)
        out_bm.faces.ensure_lookup_table()
        for f in out_bm.faces[start:]:
            f.material_index = SLOTS.index(slot)
            f.smooth = True
    PIECES.clear()
    bmesh.ops.triangulate(out_bm, faces=out_bm.faces)
    om = bpy.data.meshes.new("SKM_Outfit_" + name)
    out_bm.to_mesh(om)
    out_bm.free()
    ob = bpy.data.objects.new("SKM_Outfit_" + name, om)
    bpy.context.collection.objects.link(ob)
    for s in SLOTS:
        om.materials.append(MATS[s])
    assert len(srcs) == len(om.vertices)
    SRC_OF[ob.name] = np.array(srcs, float)
    return ob


def finalize(ob):
    """Weights, the rest shape with every body morph, and garment UVs."""
    om, name = ob.data, ob.name[len("SKM_Outfit_"):]
    n = len(om.vertices)
    oc = vget(om.vertices, "co", n)

    # fabric cut from the skin moves exactly with the skin vertices it came from, so it
    # cannot sink into the body at a joint; cords, straps and hardware take the nearest skin
    idx3 = np.empty((n, 3), int)
    w3 = np.empty((n, 3))
    src = SRC_OF.pop(ob.name)
    known = src[:, 0] >= 0
    idx3[known] = np.stack([src[known, 0], src[known, 1], src[known, 0]], 1).astype(int)
    w3[known] = np.stack([1.0 - src[known, 2], src[known, 2], np.zeros(known.sum())], 1)
    for i in np.nonzero(~known)[0].tolist():
        h = kd.find_n(oc[i], 3)
        idx3[i] = (h[0][1], h[1][1], h[2][1])
        w3[i] = (1.0 / (h[0][2] + 0.05), 1.0 / (h[1][2] + 0.05), 1.0 / (h[2][2] + 0.05))
    w3 /= w3.sum(1, keepdims=True)

    ow = sum(WM[idx3[:, k]] * w3[:, k, None] for k in range(3))
    # keep every influence the skin has (it carries up to 7 round the hips): cutting to 4
    # makes cloth and skin deform differently there, and skin shows through in slivers
    top = np.argsort(-ow, axis=1)[:, :8]
    tw = np.take_along_axis(ow, top, 1)
    tw /= np.maximum(tw.sum(1, keepdims=True), 1e-9)
    vgs = [ob.vertex_groups.new(name=g) for g in gnames]
    top_l, tw_l = top.tolist(), tw.tolist()
    for i in range(n):
        for g, w in zip(top_l[i], tw_l[i]):
            if w > 1e-4:
                vgs[g].add((i,), w, "REPLACE")

    dl = {kname: sum(DL[idx3[:, k]] * w3[:, k, None] for k in range(3)) for kname, DL in DELTAS.items()}
    rest = oc - sum(dl[k] * w for k, w in DESIGN.items())      # designed on the default body
    om.vertices.foreach_set("co", rest.ravel())
    ob.shape_key_add(name="Basis", from_mix=False)
    for kname, d in dl.items():
        kb = ob.shape_key_add(name=kname, from_mix=False)
        kb.data.foreach_set("co", (rest + d).ravel())

    loops = np.empty(len(om.loops), int)
    om.loops.foreach_get("vertex_index", loops)
    assert len(loops) == 3 * len(om.polygons)
    src = idx3[:, 0][loops.reshape(-1, 3)]
    ul, up, t, v = _UL[src], _UPL[src], _TB[src], Z[src]
    ul = ul - CL * np.round((ul - ul[:, :1]) / CL)
    up = up - CP * np.round((up - up[:, :1]) / CP)
    u = ul * (1 - t) + up * t
    u = u - PERIOD * np.floor(u.mean(1, keepdims=True) / PERIOD)
    v = v - PERIOD * np.floor(v.mean(1, keepdims=True) / PERIOD)
    om.uv_layers.new(name="UVMap").data.foreach_set("uv", np.stack([u, v], -1).ravel())
    per_vert = lambda arr: sum(arr[idx3[:, k]] * w3[:, k] for k in range(3))[loops]
    om.uv_layers.new(name="UVStretch").data.foreach_set("uv", np.stack([per_vert(STRETCH), per_vert(ZONE)], -1).ravel())
    print(name, "verts", n, "tris", len(om.polygons))


# ---- preview and export -------------------------------------------------------------------------------
def pmat(name, col, rough, coat=0.0, alpha=None, metal=0.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    p = m.node_tree.nodes["Principled BSDF"]
    p.inputs["Base Color"].default_value = (*col, 1)
    p.inputs["Roughness"].default_value = rough
    p.inputs["Metallic"].default_value = metal
    p.inputs["Coat Weight"].default_value = coat
    p.inputs["Coat Roughness"].default_value = 0.06
    if alpha is not None:
        lw = m.node_tree.nodes.new("ShaderNodeLayerWeight")
        lw.inputs["Blend"].default_value = 0.35
        mr = m.node_tree.nodes.new("ShaderNodeMapRange")
        mr.inputs["To Min"].default_value, mr.inputs["To Max"].default_value = alpha, 0.97
        m.node_tree.links.new(lw.outputs["Facing"], mr.inputs["Value"])
        m.node_tree.links.new(mr.outputs["Result"], p.inputs["Alpha"])
        for attr, val in (("surface_render_method", "BLENDED"), ("blend_method", "BLEND")):
            try:
                setattr(m, attr, val)
            except Exception:
                pass
    return m


MATS = dict(Sheer=pmat("Sheer", (0.01, 0.01, 0.012), 0.6, alpha=0.4), Lace=pmat("Lace", (0.01, 0.01, 0.012), 0.7, alpha=0.75),
            Net=pmat("Net", (0.01, 0.01, 0.012), 0.6, alpha=0.3), Satin=pmat("Satin", (0.02, 0.02, 0.025), 0.35),
            Latex=pmat("Latex", (0.012, 0.013, 0.02), 0.3, coat=1.0), Trim=pmat("Trim", (0.75, 0.78, 0.82), 0.25, metal=1.0),
            Nylon=pmat("Nylon", (0.006, 0.006, 0.008), 0.5, alpha=0.5), Binding=pmat("Binding", (0.006, 0.006, 0.008), 0.7))
scn = bpy.context.scene
if PREVIEW:
    os.makedirs(PREVIEW, exist_ok=True)
    # a plain copy of the design body: the imported skin renders with its own key values
    pm = bpy.data.meshes.new("PreviewSkin")
    pm.from_pydata(X.tolist(), [], T.tolist())
    pm.materials.append(pmat("Skin", (0.62, 0.4, 0.32), 0.45))
    for p in pm.polygons:
        p.use_smooth = True
    bpy.context.collection.objects.link(bpy.data.objects.new("PreviewSkin", pm))
    skin.hide_render = True
    for eng in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT"):
        try:
            scn.render.engine = eng
            break
        except TypeError:
            pass
    scn.render.resolution_x, scn.render.resolution_y = 800, 1200
    world = bpy.data.worlds.new("Dust")
    scn.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes["Background"]
    bg.inputs[0].default_value = (0.9, 0.5, 0.25, 1)
    bg.inputs[1].default_value = 0.8
    for nm, d, e, col in (("Key", FWD * 0.5 + LEFT * 0.7 + UP * 0.35, 4.0, (1, 0.85, 0.7)),
                          ("Rim", -FWD * 0.8 - LEFT * 0.5 + UP * 0.3, 5.0, (0.7, 0.85, 1))):
        ld = bpy.data.lights.new(nm, "SUN")
        ld.energy, ld.color, ld.angle = e, col, 0.1
        lo = bpy.data.objects.new(nm, ld)
        bpy.context.collection.objects.link(lo)
        lo.rotation_euler = (-V3(d)).normalized().to_track_quat("-Z", "Y").to_euler()
    cd = bpy.data.cameras.new("Cam")
    cd.lens = 70
    cam = bpy.data.objects.new("Cam", cd)
    bpy.context.collection.objects.link(cam)
    scn.camera = cam

for name, build in OUTFITS.items():
    if ONLY and name not in ONLY:
        continue
    build()
    ob = assemble(name)
    if PREVIEW:
        for label, d in (("front", FWD * 0.9 + LEFT * 0.42), ("back", -FWD * 0.9 + LEFT * 0.42)):
            tgt = Vector((pel_v.x, pel_v.y, 92))
            cam.location = tgt + V3(d).normalized() * 560
            cam.rotation_euler = (tgt - cam.location).to_track_quat("-Z", "Y").to_euler()
            scn.render.filepath = os.path.join(PREVIEW, "%s_%s.png" % (name, label))
            bpy.ops.render.render(write_still=True)
    finalize(ob)
    ob.modifiers.new("Armature", "ARMATURE").object = arm
    bpy.ops.object.select_all(action="DESELECT")
    arm.select_set(True)
    ob.select_set(True)
    bpy.context.view_layer.objects.active = arm
    out = os.path.join(OUT_DIR, "outfit_%s.fbx" % name.lower())
    bpy.ops.export_scene.fbx(filepath=out, use_selection=True, object_types={"ARMATURE", "MESH"},
                             add_leaf_bones=False, use_armature_deform_only=False,
                             mesh_smooth_type="FACE", bake_anim=False, axis_forward="-Z", axis_up="Y")
    print("wrote", out)
    om = ob.data
    bpy.data.objects.remove(ob, do_unlink=True)
    bpy.data.meshes.remove(om)
