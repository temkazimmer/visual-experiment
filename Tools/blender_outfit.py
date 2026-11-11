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

SLOTS = ["Sheer", "Lace", "Net", "Satin", "Latex", "Trim", "Nylon", "Binding", "Tartan"]
OPAQUE = ("Satin", "Latex", "Trim", "Tartan")
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
DESIGN = {k: w for k, w in (("BustUp", 0.80), ("BustApart", 1.097), ("GlutesUp", 0.5), ("Fat", 0.38),
                                         ("ThighGap", 0.0), ("AbsLines", 0.734), ("AbsPack", 0.12),
                                         # Bust height 0.5 x BustLiftRange 1.3 rides the bounce morphs
                                         ("BustBounceL_Up", 0.65), ("BustBounceR_Up", 0.65)) if k in DELTAS}
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
UP_V = Vector((0.0, 0.0, 1.0))


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

ARMS = G("clavicle", "upperarm", "lowerarm", "hand", "thumb", "index", "middle", "ring", "pinky") - G("toe_0")
UPPERARM = G("upperarm", "lowerarm", "hand", "thumb", "index", "middle", "ring", "pinky") - G("toe_0")
# toe bones (bigtoe_01_l, indextoe_02_r, ...) belong to the legs: "index", "middle" and "ring"
# would otherwise count them as fingers, and without them the stocking was cut off the toes
HANDS = G("hand", "thumb", "index", "middle", "ring", "pinky") - G("toe_0")
HEAD = G("head", "neck")
NECK = G("neck")
LEGS = G("thigh", "calf", "foot", "ball", "toe_0")
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
PIECES = []          # (bmesh, slot, sources[, edge distance per vertex]) of the outfit being built
SRC_OF = {}          # outfit object name -> per-vertex (skin a, skin b, t), a = -1 if unknown
SEAM_OF = {}         # outfit object name -> per-vertex (edge distance, signed seam distance), cm
SEAMS = []           # seam centre lines of the outfit being built (lists of Vectors)
EXTRA_OF = {}        # outfit object name -> per-vertex overrides (weights, morph scale, UV0, seam y)
META = {}            # id(piece bmesh) -> how it sits on the body (the fit solve, fit_outfit())
FIT_OF = {}          # outfit object name -> per-vertex fit data gathered in assemble()
# depth from stacked pieces (Docs/Research/STUDY_SKIRT_CORSET.md): structural trim stands 0.3 cm
# proud of the panel under it, hardware 0.5-0.8 cm; flat seams stay low and live in the normal
TRIM_PROUD, HW_PROUD = 0.3, 0.6
NO_SEAM = 9.0        # UVSeam value far from any edge or seam (the material's profile is flat there)


def edge_loops(bm, min_len, srcof=None):
    """Boundary loops of a piece: (points, closed, skin source per point or None)."""
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
            loops.append((pts, start in adj[chain[-1]], [srcof[v] for v in chain] if srcof else None))
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
    loops = edge_loops(bm, min_loop, srcof) if (pipe or bind) else []
    # true distance from every vertex to the cut edge, for the seam/piping normal (UVSeam.x)
    bpts = []
    for e in bm.edges:
        if e.is_boundary:
            a, b = e.verts[0].co, e.verts[1].co
            k = max(1, int((b - a).length / 0.1))
            bpts += [a.lerp(b, i / k) for i in range(k + 1)]
    if bpts:
        ek = KDTree(len(bpts))
        for i, p in enumerate(bpts):
            ek.insert(p, i)
        ek.balance()
        edist = [min(ek.find(v.co)[2], NO_SEAM - 0.01) for v in bm.verts]    # < NO_SEAM marks fabric
    else:
        edist = [NO_SEAM - 0.01] * len(bm.verts)
    if rim if rim is not None else slot in OPAQUE:
        inner = {}
        for e in [e for e in bm.edges if e.is_boundary]:
            a, b = e.verts
            for v in (a, b):
                if v not in inner:
                    inner[v] = bm.verts.new(v.co - nrm[v] * max(0.0, offset - 0.03))
                    src.append(srcof[v])
                    edist.append(0.0)
            bm.faces.new((a, b, inner[b], inner[a]))
    PIECES.append((bm, slot, src, edist))
    META[id(bm)] = dict(kind="panel", cover=val)
    for pts, closed, esrc in loops:
        # trims ride the skin their panel edge rides: by nearest skin vertex, a binding in a
        # deep crease (Mermaid's hip) took the thigh's side of the fold and split off the edge
        if pipe:
            tube(pts, pipe_r, pipe, closed, push=0.08, src_from=(pts, esrc))
        if bind:      # a rounded binding standing TRIM_PROUD off the panel edge it covers
            # 0.4 mm off the panel: flush, its flat underside z-fights with the panel and
            # flecks of a glossy panel show through a matte binding (Noir's cincher)
            tube(pts, 0.0, bind[0], closed, flat=(bind[1], TRIM_PROUD), step=0.5, push=0.04, src_from=(pts, esrc))
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


def tube(pts, r, slot, closed=False, sides=6, push=0.0, step=0.8, flat=None, rigid=None, src_from=None, turn=0):
    """A round cord along a point list. flat=(width, height): a tape lying on the surface
    instead, its top rounded like a bound edge. rigid: one skin vertex the whole piece rides
    (jewellery)."""
    if flat:
        # cross-section: flat underside, then a half-ellipse crown (smooth-shaded piping)
        prof = [(-1.0, 0.0), (1.0, 0.0)] + [(math.cos(math.pi * k / 6), math.sin(math.pi * k / 6)) for k in range(1, 6)]
        sides = len(prof)
    pts = smooth_pts(resample(pts, closed, step), closed)
    n = len(pts)
    if n < 4:
        return
    bm = bmesh.new()
    rings, src = [], []
    ekd = None
    if src_from and src_from[1]:                            # src_from: (edge points, their skin sources)
        ekd = KDTree(len(src_from[0]))
        for k, q in enumerate(src_from[0]):
            ekd.insert(q, k)
        ekd.balance()
    # turn: smooth the tape's facing along it over this many passes, so a strap that leaves the
    # skin to span a hollow doesn't twist where the surface under it changes
    nn = [nearest_normal(p) for p in pts]
    if turn:
        nn = [v.normalized() for v in smooth_pts(nn, closed, turn)]
    for i, p in enumerate(pts):
        if ekd is not None:                                 # the ring rides the panel edge under it
            src += [src_from[1][ekd.find(p)[1]]] * sides
        else:
            j = rigid if rigid is not None else nearest(p)  # the whole ring rides this skin vertex
            src += [(j, j, 0.0)] * sides
        a_, b_ = (pts[i - 1], pts[(i + 1) % n]) if closed else (pts[max(i - 1, 0)], pts[min(i + 1, n - 1)])
        t = (b_ - a_).normalized()
        out = nn[i]
        a = out - t * out.dot(t)
        if a.length < 1e-4:
            a = t.orthogonal()
        a.normalize()
        b = t.cross(a)
        c = p + out * push
        if flat:
            w_, t_ = flat
            rings.append([bm.verts.new(c + b * sx * w_ * 0.5 + a * h * t_) for sx, h in prof])
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
    # fit: a free strap pulls taut over the body; an edge trim rides its panel; jewellery is rigid
    META[id(bm)] = dict(kind="edge" if ekd is not None else ("rigid" if rigid is not None else "strap"),
                        line=np.array([NP(p) for p in pts]), per=sides, width=flat[0] if flat else 2 * r)


def hanging_tie(zt, zb, over=(), slot="Tartan", off=0.42, bind=("Binding", 0.55), step=0.5, cols=9):
    """A tie hanging from zt down to a point at zb, in front of her centre line. Row by row from
    the top it lies on whatever is furthest forward under it (skin, or the points in over); once
    the body falls away under the bust it hangs straight down. Each row rides the skin it last
    touched, so the hanging end swings with the chest, not the belly."""
    rows = np.arange(zt, zb - 1e-6, -step)
    hw = 1.3 + 1.0 * np.clip((zt - rows) / (0.8 * (zt - zb)), 0, 1)        # half-width, wider down
    hw = np.where(rows < zb + 2.6, hw * np.clip((rows - zb) / 2.6, 0.04, 1.0), hw)   # pointed end
    cand = np.nonzero(TORSO & (FW > 0) & (HEAD < 0.6))[0]
    obst = [NP(p) for p in over]
    depth, src_j, prev, prev_j = [], [], -1e9, None
    for z, w in zip(rows, hw):
        sel = cand[(np.abs(Z[cand] - z) < 0.4) & (ALAT[cand] < w + 0.3)]
        c, j = (FW[sel].max(), int(sel[np.argmax(FW[sel])])) if len(sel) else (-1e9, None)
        for q in obst:
            if abs(q[2] - z) < 0.6 and abs((q - PEL) @ LEFT) < w + 0.3:
                c = max(c, (q - PEL) @ FWD + 0.2)
        if c >= prev:                       # touching here: rides this skin
            prev, prev_j = c, (j if j is not None else prev_j)
        depth.append(prev)
        src_j.append(prev_j if prev_j is not None else nearest(V3(PEL + UP * (z - PEL[2]))))
    bm = bmesh.new()
    grid, src, ed, uvs = [], [], [], []
    for r, (z, w, d, j) in enumerate(zip(rows, hw, depth, src_j)):
        row = []
        for k in range(cols):
            x = (k / (cols - 1) * 2 - 1) * w
            p = PEL + LEFT * x + FWD * (d + off - 0.04 * x * x) + UP * (z - PEL[2])   # a slight curve across
            row.append(bm.verts.new(V3(p)))
            src.append((j, j, 0.0))
            ed.append(min(w - abs(x), zt - z, NO_SEAM - 0.01))
            uvs.append((x + 12.0, zt - z))          # its own cloth coordinates: the tartan runs down it
        grid.append(row)
    for r0, r1 in zip(grid, grid[1:]):
        for k in range(cols - 1):
            bm.faces.new((r0[k], r0[k + 1], r1[k + 1], r1[k]))
    nv_ = len(src)
    PIECES.append((bm, slot, src, ed, dict(w=np.full((nv_, len(gnames)), np.nan), m=np.ones(nv_),
                                           uv=np.array(uvs, float), y=np.full(nv_, np.nan))))
    centre = np.array([NP(r_[cols // 2].co) for r_ in grid])
    META[id(bm)] = dict(kind="strap", line=centre, per=cols, width=2.0 * float(hw.mean()))
    if bind:
        loop = [r_[0].co.copy() for r_ in grid] + [r_[-1].co.copy() for r_ in grid[::-1]]
        lsrc = [s_ for s_ in src[::cols]] + [s_ for s_ in src[::cols][::-1]]
        tube(loop, 0.0, bind[0], closed=True, flat=(bind[1], TRIM_PROUD), step=0.4, push=0.04, src_from=(loop, lsrc))
    return rows, np.array(depth) + off, hw


def seam(pts, r, slot, field=None):
    """A flat tape sewn along surface points (boning channel, stocking seam, stripe); with a
    field, only where the garment is. Kept low: the engine's screen-space shadow trace turns
    a raised cord into a row of dark dashes on the skin beside it in raking light. The fabric
    pulled into it is in the normal instead (UVSeam.y, the signed distance to SEAMS)."""
    pts = [p for p in pts if p]
    if len(pts) > 5:
        SEAMS.append(smooth_pts(resample(pts, False, 0.1), False, 8))
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
    META[id(bm)] = dict(kind="strap", line=np.array([NP(p) for p in pts]), per=4, width=w)
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
    META[id(bm)] = dict(kind="rigid")


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
        clip(at + nearest_normal(at) * HW_PROUD, nearest_normal(at), t, size)


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


def tanga_string_paths(fit, rise=6.0):
    """The tanga's strings as point lines traced over the skin: from each top corner of the
    front panel up over the hip bone and down to the top of the thong at the small of the back."""
    cr, hb = LM["crotch"], LM["hip_top"]
    top_f, top_b = hb - 1.5, hb - 0.5
    bvh = bvh_of(TORSO & (HANDS < 0.5) if "HANDS" in globals() else TORSO)
    lines = []
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
        lines.append(pts)
    return lines


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
    for s in (1, -1):   # hip cutouts: from below the ribs down over the hip bone, the belt bridging them (Tema: bigger)
        side_pt = surface_near(PEL + LEFT * s * 30 + UP * (LM["waist"] - PEL[2] - 2), LEFT * s)
        r = np.sqrt(((Z - side_pt[2]) / 9.0) ** 2 + ((FW - (side_pt - PEL).dot(FWD) - 1.5) / 7.5) ** 2)   # from just under the belt to below the ribs
        f = np.minimum(f, (np.where(((LAT * s) > 0) & (UPPERARM < 0.3), r, 9.0) - 1.0) * 5.0)   # torso only: not the arm beside it
    # keyhole between the breasts: a teardrop, round at the bottom, narrowing to a point at the top
    zc = LM["bust"] - 1.0
    dz = Z - zc
    up_ = np.clip(dz / 7.0, 0.0, 1.0)
    w = 3.0 * (1.0 - 0.8 * up_ ** 1.3)
    e = np.sqrt((ALAT / w) ** 2 + (dz / np.where(dz > 0, 7.0, 4.5)) ** 2)
    return np.minimum(f, (np.where(FW > 0, e, 9.0) - 1.0) * 3.5)


LEGMASK = UPPERARM < 0.3
SIDE_BLEND = smooth01(-3.5, 3.5, FW)     # 0 behind the side seam, 1 in front of it
ST_TOP = LM["thigh_mid"] + 5.0
BELT_Z = LM["waist"] - 3.0


# ---- shared sets --------------------------------------------------------------------------------
def bra(cup_slot, cup_top=0.30, band_w=1.6, strap_w=1.0, pipe="Satin", bind=None):
    """bind=(slot, width): a flat bound edge on the cups instead of round piping."""
    region(cups(cup_top), 0.30, cup_slot, pipe=None if bind else pipe, pipe_r=0.15, bind=bind, min_loop=6.0 if bind else 10.0)
    region(underband(band_w), 0.46, "Satin")
    shoulder_straps(strap_w, cup_top)
    hardware(NP(ray(TORSO_BVH, pel_v, LM["fold"] - band_w * 0.5, 0.0, 0.0)), UP, (1.3, 1.3, 0.45), 0.6)   # centre ring


def stockings(top=ST_TOP, welt="Lace", welt_h=6.0, slot="Sheer", bind=None):
    """bind=(slot, width): flat bound welt edges instead of round satin piping."""
    region(where(LEGMASK & (Z < LM["crotch"]), top - Z), 0.26, slot)
    region(band(top - welt_h * 0.5, welt_h, LEGMASK & (Z < LM["crotch"])), 0.32, welt,
           pipe=None if bind else "Satin", pipe_r=0.16, bind=bind)
    for s in (1, -1):
        seam(leg_line(s, 180, top - welt_h, LM["ankle"] + 3, 0.29), 0.12, "Satin")


def garters(belt_z, bot_z, back=True, off_top=0.6, off_bot=0.5, slot="Satin", width=1.2):
    for s in (1, -1):
        garter(s, 18, 6, belt_z, bot_z, off_top, off_bot, slot, width)
        if back:
            garter(s, 140, 172, belt_z, bot_z, off_top, off_bot, slot, width)


# ---- the outfits ---------------------------------------------------------------------------------
def noir():
    """Nylon bodysuit with boning and a satin waist cincher, lace cups, satin straps, seamed
    nylon thigh-highs with lace welts. Refreshed 2026-11-11 (Tema): the bodystocking's nylon
    instead of flat sheer, five straight boning channels instead of nine wobbly lines, flat
    bound edges instead of round piping, a cincher with a front clasp instead of a stray band,
    wider straps."""
    BIND = ("Binding", 0.55)
    # sweetheart top (Tema's review, 2026-11-11): in front the nylon stops at the cups' edge,
    # so the lace cups are the neckline, not two shapes under a straight sheer strip; round
    # the sides and back it ends just under the satin underband, its binding the band's lower
    # edge (under the band, the binding stood proud through it in flecks); between and under the
    # cups it rises just over the band's top edge
    level = LM["fold"] - 2.4 + 2.7 * smooth01(0.3, 0.6, NS @ FWD)
    body = np.minimum(bodysuit(), where(TORSO, np.maximum(cups(0.55), level - Z)))
    region(body, 0.26, "Nylon", bind=BIND, min_loop=6.0)
    bra("Lace", 0.55, 1.8, 1.4, bind=BIND)
    z0 = LM["fold"] - 2.2
    cz, ch = LM["waist"] - 1.5, 7.0                                       # waist cincher
    for th in (22, -22, 70, -70, 180):                                   # boning channels, underbust into the cincher
        seam(torso_line(th, z0, cz, 0.29), 0.22, "Satin", body)
    region(np.minimum(band(cz, ch), body + 2.0), 0.46, "Satin", bind=BIND)
    # its clasp: hook-and-eye pairs down the front, on the belly's front (not in the navel)
    front = [q for q in (ray(TORSO_BVH, pel_v, cz, D(t), 0.0) for t in (-12, -8, -4, 4, 8, 12)) if q]
    if front:
        depth = max((q - pel_v).dot(fwd_v) for q in front)
        for k in range(4):
            z = cz - ch * 0.5 + 1.2 + k * (ch - 2.4) / 3
            clip(Vector((pel_v.x, pel_v.y, z)) + fwd_v * (depth + 0.75), fwd_v, left_v, (1.2, 0.5, 0.3))
    stockings(slot="Nylon", bind=BIND)


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
    """Glossy navy latex catsuit, all one colour: a teardrop keyhole, hip cutouts, every opening
    bound in the same latex, a front zip with a steel pull, torso panel seams, collar, cuffs and
    a belt with a steel buckle. Refreshed 2026-11-11 (Tema): bound edges instead of thin piping,
    a keyhole instead of the diamond, then no white anywhere and bigger hip cutouts."""
    f = catsuit()
    BIND = ("Latex", 0.7)                                                            # self-coloured bindings: the edge reads by its gloss
    region(f, 0.25, "Latex", bind=BIND, min_loop=6.0)
    top = LM["shoulder"] + 5.0
    seam(torso_line(0, top - 1.0, LM["crotch"] + 1, 0.28), 0.3, "Latex", f)           # zip tape, stops at the keyhole
    pull = ray(TORSO_BVH, pel_v, top - 3.5, 0.0, 0.0)
    if pull:
        hardware(NP(pull), UP, (1.0, 2.4, 0.35), 0.5)                                  # zip pull under the collar
    bz = LM["hip_top"] + 5.0
    for th in (34, -34, 150, -150):
        seam(torso_line(th, LM["fold"] - 1, bz + 2.5, 0.28), 0.15, "Latex", f)       # panel seams end under the belt
    for s in (1, -1):                                                                # ankle cuffs
        region(where(LEGMASK & (LEGS > 0.5), 1.6 - np.abs(Z - (LM["ankle"] + 7.5))), 0.5, "Latex", bind=("Latex", 0.5), min_loop=6.0)
                                                                                     # belt, buckle front and centre
    region(band(bz, 4.0, UPPERARM < 0.3), 0.6, "Latex", bind=("Latex", 0.5))       # a level cut all round
    # the buckle sits on the belly's front, square to it: a ray down the middle falls into the
    # navel (2.5 cm deep), and the skin's normal there tips inward, which buried it
    front = [q for q in (ray(TORSO_BVH, pel_v, bz, D(t), 0.0) for t in (-12, -8, -4, 4, 8, 12)) if q]
    if front:
        depth = max((q - pel_v).dot(fwd_v) for q in front)
        buckle = Vector((pel_v.x, pel_v.y, bz)) + fwd_v * depth
        c = buckle + fwd_v * 1.0
        W, H, bar = 4.8, 3.6, 0.6                                                     # a rectangular frame, the belt showing through
        side_v = left_v * (W - bar) * 0.5
        up_v = Vector((0, 0, (H - bar) * 0.5))
        for dv, along, size in ((side_v, UP_V, (bar, H, 0.5)), (-side_v, UP_V, (bar, H, 0.5)),
                                (up_v, left_v, (bar, W, 0.5)), (-up_v, left_v, (bar, W, 0.5)),
                                (Vector((0, 0, 0)), left_v, (0.3, W - bar, 0.35))):   # the prong across the middle
            clip(c + dv, fwd_v, along, size)
    wrist = np.minimum((0.62 - HANDS) * 25.0, (HANDS - 0.22) * 25.0)                 # cuff bands
    region(where(UPPERARM > 0.5, wrist), 0.5, "Latex", bind=("Latex", 0.5), min_loop=6.0)
    region(band(top - 1.5, 3.0, UPPERARM < 0.3), 0.5, "Latex", bind=("Latex", 0.5))    # collar


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
    # a sheer string tanga under the nylon (Tema's references): small see-through front, bound
    # edges, thin strings rising high over the hips (they show in the hip windows), thong back.
    # Its bindings stand TRIM_PROUD, so the nylon over it is lifted clear and eases back down.
    tanga, strings = string_tanga(), tanga_string_paths(fit)
    under = (tanga > -0.6).astype(float)
    for line in strings:
        for q in line[::3]:
            for _, i, _ in kd.find_range(q, 0.8):
                under[i] = 1.0
    ease = np.clip(diffuse(under, 40) * 2.0, 0.0, 1.0)            # 1 over it, easing out over ~4 cm
    over = fit + np.maximum(under, ease * ease * (3.0 - 2.0 * ease)) * (TRIM_PROUD + 0.06)
    region(g, 0.09, "Nylon", lift=over, bind=("Binding", 0.6), min_loop=6.0)
    region(tanga, 0.05, "Sheer", lift=fit, bind=("Binding", 0.4), min_loop=4.0)
    for line in strings:
        tube(line, 0.0, "Binding", flat=(0.32, 0.1), step=0.35)
    # the stocking tops are knitted double: a welt from the hip crease down the thigh, stacked
    # TRIM_PROUD over the leg
    region(np.minimum(g, np.minimum((LEGS - 0.5) * 12.0, Z - (cs - 12.5))), 0.09 + TRIM_PROUD, "Nylon", lift=over)
    for th in (0, 180):                                          # centre seams, front and back
        line = [q + nearest_normal(q) * float(over[nearest(q)]) for q in torso_line(th, zn - 6, cs, 0.19) if q]
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


def tartan():
    """Bundle 03 (Tema's references, 2026-11-11): a red tartan slingshot one-piece. Cups cut
    high over the bust with the lower pole open, a black satin shirt collar with a tartan tie
    down the sternum, straps in a deep V from the cups to a small tartan front, side straps
    over the hips to a thong, and white sheer thigh-highs with a wide lace top. Every tartan
    edge is bound in black."""
    BIND = ("Binding", 0.55)
    zn = LM["neck"] + 1.5                                          # collar line at the base of the neck
    # cups: high and full on top, the lower third open (underbust showing)
    cup = np.full(nv, NEG)
    low = {}
    for s in (1, -1):
        f, u, l = frame(FWD, s, 12, 6)
        Dv = X - ANCHOR[s]
        x, y = Dv @ l, Dv @ u
        c_ = CUP[s]
        rx = np.where(x > 0, c_["out"], c_["inn"]) * 1.12
        ry = np.where(y > 0, c_["up"], c_["dn"]) * 1.12
        r = np.sqrt((x / rx) ** 2 + (y / ry) ** 2)
        cut = 0.42 * c_["dn"]                                       # bottom edge, below the apex
        # a triangle cup: wide under the apex, its sides closing to a point at the halter strap
        bi, bo = np.array((-c_["inn"] * 0.78, -cut)), np.array((c_["out"] * 1.05, -cut * 0.55))
        ap = np.array((c_["out"] * 0.1, c_["up"] * 1.55))
        side = lambda p0, p1: ((p1[0] - p0[0]) * (y - p0[1]) - (p1[1] - p0[1]) * (x - p0[0])) / np.linalg.norm(p1 - p0)
        tri = np.minimum(-side(bi, ap), side(bo, ap))               # inside both sides: positive
        c = np.minimum.reduce([(1.0 - r) * 7.0, tri, y + cut - 0.45 * np.clip(x / c_["out"], 0, 1) * cut])
        cup = np.maximum(cup, where(TORSO & (NS @ FWD > -0.2) & (Dv @ f > -8), c))
        # the straps start 0.25 cm up, under the cup's bound edge, so they read as sewn to it
        # (from below the edge they left a loose dash under each cup)
        low[s] = dict(inner=ANCHOR[s] + l * bi[0] * 0.8 + u * (bi[1] + 0.25),
                      outer=ANCHOR[s] + l * bo[0] * 0.9 + u * (bo[1] + 0.25),
                      top=ANCHOR[s] + l * ap[0] + u * (ap[1] - 1.2))
    region(cup, 0.30, "Tartan", bind=BIND, min_loop=6.0)
    # every string starts at a corner of its cup's triangle (Tema, 2026-11-11): found on the cup's
    # real edge, then 0.4 cm in, so the string's end tucks under the binding
    for s in (1, -1):
        f, u, l = frame(FWD, s, 12, 6)
        e = np.nonzero((cup > -0.05) & (cup < 0.35) & (LAT * s > 0))[0]
        x, y = (X[e] - ANCHOR[s]) @ l, (X[e] - ANCHOR[s]) @ u
        mid = X[np.nonzero((cup > 1.5) & (LAT * s > 0))[0]].mean(0)
        for key, k in (("inner", np.argmin(x + y)), ("outer", np.argmax(x - y))):
            q = X[e[k]]
            low[s][key] = q + (mid - q) / np.linalg.norm(mid - q) * 0.4

    # the bridge (Tema's sketch, 2026-11-11): a strap between the cups' inner bottom corners,
    # pulled straight across the cleavage
    # the tie hangs from the knot under the collar: on the chest it lies on the skin, past the
    # bust it falls straight down in front of her, over the cleavage and the bridge, to a point
    # below the cups (Tema's sketch: it hangs, it does not follow her in)
    zt = zn - 3.0
    rows, tdepth, thw = hanging_tie(zt, LM["bust"] - 10.0, bind=BIND)
    # the bridge between the cups' inner corners runs over the tie: the tie passes under it
    ends = [V3(low[s]["inner"]) for s in (1, -1)]
    bridge = taut(*(e + nearest_normal(e) * 0.35 for e in ends), 0.35)
    for i, q in enumerate(bridge):
        r = int(np.argmin(np.abs(rows - q.z)))
        if abs((NP(q) - PEL) @ LEFT) < thw[r] + 0.6:
            need = tdepth[r] + 0.25 - (NP(q) - PEL) @ FWD
            if need > 0:
                bridge[i] = q + fwd_v * need
    tube(smooth_pts(bridge, False, 4), 0.0, "Binding", flat=(0.9, 0.16), step=0.35)
    knot = np.sqrt((ALAT / 1.6) ** 2 + ((Z - (zt + 0.6)) / 1.6) ** 2)
    region(where((FW > 0) & (HEAD < 0.6), (1.0 - knot) * 3.0), 0.75, "Tartan", bind=BIND, min_loop=3.0)

    # the collar: a black satin band round the neck and two points lying on the collarbones
    region(where((G("head") < 0.5) & (UPPERARM < 0.3), np.minimum(zn + 2.6 - Z, Z - (zn - 0.2))), 0.5, "Satin", bind=BIND, min_loop=6.0)
    pts_ = np.full(nv, NEG)
    for s in (1, -1):
        a, b, c = np.array((1.0, zn)), np.array((7.5, zn + 0.5)), np.array((5.0, zn - 6.5))   # (lateral, z): a collar point
        q = np.stack([LAT * s, Z], 1)
        d = np.full(nv, 1e9)
        inside = np.ones(nv, bool)
        for p0, p1 in ((a, b), (b, c), (c, a)):
            e = p1 - p0
            t = np.clip(((q - p0) @ e) / (e @ e), 0, 1)
            d = np.minimum(d, np.linalg.norm(q - (p0 + t[:, None] * e), axis=1))
            inside &= (e[0] * (q[:, 1] - p0[1]) - e[1] * (q[:, 0] - p0[0])) <= 0
        pts_ = np.maximum(pts_, where(TORSO & (FW > 0) & (LAT * s > 0), np.where(inside, d, -d)))
    region(pts_, 0.62, "Satin", bind=BIND, min_loop=4.0)

    # halter straps: cup top, up over the collarbone, round the back of the collar
    for s in (1, -1):
        top = V3(surface_near(low[s]["top"], FWD))
        neck = V3(surface_near(Bn["neck_01"] + LEFT * s * 5.0 + UP * 1.0 - FWD * 2.0, LEFT * s))
        a = top + nearest_normal(top) * 0.4
        b = neck + nearest_normal(neck) * 0.6
        ribbon(taut(a, b, 0.45), 0.9, 0.2, "Binding")

    # body straps on the skin: a deep V from each cup's inner bottom to the front of the tanga,
    # and a side strap from the cup's outer edge over the hip to the top of the thong
    cr, hb = LM["crotch"], LM["hip_top"]
    def wrap(keys, width):
        """A strap through (angle from the front toward her left, deg; height, cm) keys, laid as a
        smooth flat tape: each sample is found by a ray in toward the body axis, lifted over the
        hollows (BRIDGE), and the tape rides the skin beneath it. Cut from the skin as a field, the
        thin straps went ragged on the flanks and the back band dipped in and out over the
        shoulder blades (clothes-02, 2026-11-11). Samples under a cup are dropped, so a strap
        starts tucked under the cup's binding."""
        pts = []
        for (t0, z0), (t1, z1) in zip(keys[:-1], keys[1:]):
            for k in np.linspace(0, 1, 64, endpoint=False):
                q = ray(TORSO_BVH, pel_v, z0 + (z1 - z0) * k, D(t0 + (t1 - t0) * k), 0.0)
                if q and not (cup[nearest(q)] > -0.25 and len(pts) == 0):
                    j = nearest(q)
                    pts.append(q + NV[j] * (0.2 + float(BRIDGE[j])))
        tube(pts, 0.0, "Binding", flat=(width, 0.14), step=0.35)

    ang = lambda p: (math.degrees(math.atan2((p - PEL) @ LEFT, (p - PEL) @ FWD)), float(p[2]))
    for s in (1, -1):
        to, zo = ang(low[s]["outer"])
        # the slingshot V (Tema's source): from the middle of each cup's lower edge down the flank,
        # converging onto the small front panel's top corner
        tm, zm = ang((low[s]["inner"] + low[s]["outer"]) * 0.5)
        corner = (s * 15.0, hb - 1.7)
        # pulled straight from the cup to the corner (Tema's sketch): it spans the waist's hollow
        # instead of following it in
        a_ = V3(low[s]["outer"])       # the band, the side string and the V all leave the outer corner
        b_ = ray(TORSO_BVH, pel_v, corner[1], D(corner[0]), 0.0)
        if a_ and b_:
            a_, b_ = a_ + nearest_normal(a_) * 0.2, b_ + nearest_normal(b_) * 0.2
            tube(taut(a_, b_, 0.2, n=80), 0.0, "Binding", flat=(1.0, 0.14), step=0.35, turn=40)
        # high-cut hip strings: from the panel's corner up over the hip bone, round to the side
        # strap above the hip, which carries on to the thong at the back
        wrap([corner, (s * 45.0, hb + 2.5), (s * 75.0, hb + 4.5), (s * 100.0, hb + 3.0)], 0.8)
        wrap([(to, zo), (s * 82.0, LM["waist"] + 1.0), (s * 100.0, hb + 3.0), (s * 150.0, hb + 0.5), (s * 176.0, hb - 0.8)], 1.0)
        # a thin band from the cup's outer corner round the back at the bra line
        wrap([(to, zo), (s * 105.0, LM["fold"] + 3.0), (s * 186.0, LM["fold"] + 3.0)], 0.9)   # the halves overlap at the spine

    # a small tartan front and a thong behind (the sheer tanga's cut, opaque and bound)
    # string_tanga()'s rear is cut against the leg weights, which aren't symmetric at the crease:
    # opaque, the back came out lopsided with a ragged foot. Here both halves are one field in
    # (height, distance from the centre plane), a thong narrowing evenly into the gusset.
    cs = LM["crotch_skin"]
    top_f, top_b = hb - 1.5, hb - 0.8
    tf = np.clip((Z - cs) / (top_f - cs), 0, 1)
    front = np.minimum(top_f - Z, 1.3 + 3.4 * tf ** 1.3 - ALAT)        # small, as in the source: 9.4 cm across the top
    tb = np.clip((Z - cs) / (top_b - cs), 0, 1)
    rear = np.minimum(top_b - Z, 0.7 + 3.3 * tb ** 2.2 - ALAT)        # 1.4 cm low, 8 cm at the top
    panel = rear + (front - rear) * SIDE_BLEND
    region(where((UPPERARM < 0.3) & (Z > cs - 6.0) & (Z < hb + 4.0) & (ALAT < 10.0), panel),
           0.30, "Tartan", bind=("Binding", 0.45), min_loop=4.0)

    # white sheer thigh-highs with a wide lace top, no back seam
    st_top = ST_TOP + 1.0
    region(where(LEGMASK & (Z < LM["crotch"]), st_top - 7.5 - Z + 0.4), 0.24, "Sheer")
    region(band(st_top - 4.0, 8.0, LEGMASK & (Z < LM["crotch"])), 0.30, "Lace")


def skirt(top=None, hem=None, off=0.75, flare=9.0, lobes=5, swing=5.0, band_h=3.5, slot="Satin", skin_to=None, tight=False, rigid_from=None, follow=False, cling=0.0):
    """A short satin A-line worn over the bodystocking (Docs/Research/DESIGN_SKIRT.md). Not cut
    from the skin: rings lofted from the waist down. Each ring follows her hull to the hip, then
    hangs straight from the widest point above it, flares out (A-line) and swings in broad
    sculpted folds, so the rest pose already reads as draped. Double waistband 0.3 cm apart
    with a bound top; side and back seams and a rolled hem live in the seam normal.
    Weights: the skin's down to the hip (or to skin_to, cm: a tight skirt follows her hip and
    thigh down to there), then a simple pelvis/thigh blend.
    tight: for the cloth, no kink anywhere down the skirt (Tema saw a ridge at the crotch line):
    the ease grows as u squared from the hip, and below skin_to each column keeps the weights of
    the skin at skin_to, with no thigh pull.
    rigid_from (cm): from here down the weights blend to the pelvis alone, so the ring keeps its
    length round her: the cloth's rest ring is the reference pose's, and where the legs pulled the
    ring shorter in idle the free cloth bulged out into a ridge.
    follow: for the skinned skirt (the cloth sim is off by default): every ring takes the weights
    of the skin straight in from it (waist, hip, glute, thigh), blurred round the ring and down, so
    it rides her legs when she sits, kneels or lies down. With one ring's weights from the crotch
    down it stayed with the pelvis, and in FEETUP, GLANCE and MERMAID her thighs went through the
    front, her seat showed through the back and the hem sank into the sand.
    cling (0-1): how much of the hang below her widest point is taken back towards her body. A
    skinned skirt has no gravity: in FEETUP (lying on her front) the part hanging straight down
    from the glutes stood off her seat and thighs as a stiff ring (Tema's circle, 2026-11-11).
    Clinging under the glutes, the hem lies on her thighs whichever way they point."""
    cr, hb = LM["crotch"], LM["hip_top"]
    top = LM["waist"] + 1.0 if top is None else top
    hem = cr - 6.0 if hem is None else hem
    nt = 144
    th = math.pi + np.linspace(0.0, 2 * math.pi, nt + 1)          # starts and ends at centre back
    zs = np.linspace(top, hem, int(round((top - hem) / 0.7)) + 1)
    dirs = np.stack([np.outer(np.cos(th), FWD) + np.outer(np.sin(th), LEFT)])[0]
    R = np.zeros((len(zs), nt + 1))
    J = np.zeros((len(zs), nt + 1), int)
    for i, z in enumerate(zs):
        for k in range(nt + 1):
            d = V3(dirs[k])
            o = Vector((PEL[0], PEL[1], z)) + d * 80
            hit = TORSO_BVH.ray_cast(o, -d, 80)
            R[i, k] = 80 - hit[3] if hit[0] else (R[i - 1, k] if i else 10.0)
            J[i, k] = nearest(hit[0]) if hit[0] else (J[i - 1, k] if i else 0)
    hang = np.maximum.accumulate(R, axis=0)                        # hangs from the widest point above
    R = hang - cling * (hang - R)                                  # ... or clings back towards her
    hull = R.copy()
    for _ in range(3):                                             # soften round and down
        R[:, :-1] = (np.roll(R[:, :-1], 1, 1) + 2 * R[:, :-1] + np.roll(R[:, :-1], -1, 1)) / 4
        R[1:-1] = (R[:-2] + 2 * R[1:-1] + R[2:]) / 4
        R[:, -1] = R[:, 0]
    R = np.maximum(R, hull)                                        # softening must not cut into her (the hip front showed through)
    u = np.clip((hb - zs) / (hb - hem), 0, 1)[:, None]             # 0 at the hip, 1 at the hem
    t = th[None, :] - math.pi
    fold = np.sin(lobes * t + 0.7 * np.sin(2 * t + 1.0) + 0.4)
    ease = (1.5 + flare) * u ** 2 if tight else 1.5 * u + flare * u ** 1.4
    r = R + off + ease + swing * u ** 1.6 * fold                    # more room where the legs move
    sk = hb if skin_to is None else skin_to
    hip_row = int(np.argmin(np.abs(zs - sk)))
    Jm = np.where(zs[:, None] >= sk, J, J[hip_row][None, :])          # below that: the last ring's vertex
    uw = np.clip((sk - zs) / (sk - hem), 0, 1)                         # 0 where skin weights end, 1 at the hem

    nb = len(gnames)
    PELVIS_W = np.zeros(nb)
    PELVIS_W[gnames.index("pelvis")] = 1.0
    kl, kr = nt // 4, 3 * nt // 4                                  # columns straight out over her left and right leg
    if follow:
        WF = WM[J[:, :-1]].astype(float)                               # rows, columns (the last repeats the first), bones
        sc, sr = 6.0, 2.0                                              # blur sigma: 6 columns (~4.5 cm) round, 2 rows (1.4 cm) down
        f = np.fft.fftfreq(nt)
        WF = np.real(np.fft.ifft(np.fft.fft(WF, axis=1) * np.exp(-2 * (math.pi * f * sc) ** 2)[None, :, None], axis=1))
        g = np.exp(-0.5 * (np.arange(-6, 7) / sr) ** 2)
        g /= g.sum()
        pad = np.concatenate([np.repeat(WF[:1], 6, 0), WF, np.repeat(WF[-1:], 6, 0)], 0)
        WF = sum(pad[o:o + len(zs)] * g[o] for o in range(13))
        WF = np.clip(WF, 0.0, None)
        WF /= np.maximum(WF.sum(2, keepdims=True), 1e-9)
        WF = np.concatenate([WF, WF[:, :1]], 1)

    def sheet(rr, z0, z1, piece_slot, own_uv=True):
        rows = [i for i, z in enumerate(zs) if z1 - 1e-6 <= z <= z0 + 1e-6]
        bm = bmesh.new()
        grid, src, ed, ys, uvs, ws, ms = [], [], [], [], [], [], []
        arc = np.zeros(nt + 1)
        for i in rows:
            pts = [Vector((PEL[0], PEL[1], zs[i])) + V3(dirs[k]) * float(rr[i, k]) for k in range(nt + 1)]
            arc[1:] = np.cumsum([(pts[k + 1] - pts[k]).length for k in range(nt)])
            grid.append([bm.verts.new(p) for p in pts])
            for k in range(nt + 1):
                j = int(Jm[i, k])
                src.append((j, j, 0.0))
                ed.append(min(zs[i] - zs[rows[-1]], zs[rows[0]] - zs[i], NO_SEAM - 0.01))
                # signed distance (cm, round the ring) to the nearest side or back seam
                a = (t[0, k] + math.pi) % (2 * math.pi) - math.pi      # 0 = front
                cand = [a - math.pi / 2, a + math.pi / 2, a - math.pi if a > 0 else a + math.pi]
                ys.append(float(min(cand, key=abs)) * float(rr[i, k]))
                uvs.append((arc[k], zs[i]) if own_uv else (np.nan, np.nan))
                # pelvis = the hip ring's skin weights; each thigh pulls the part of the skirt
                # that hangs over it, more towards the hem (stand-in for cloth, see the doc)
                uu = float(uw[i])
                if follow:
                    ws.append(WF[i, k])
                    ms.append(1.0 - float(u[i, 0]))
                    continue
                # tight (cloth): no thigh pull. Each column keeps one set of weights below the
                # hand-over, so it keeps its length when the thighs move; a pull that grew down the
                # skirt shortened the front in idle and the sim folded it (the ridge). Colliders do the legs.
                grow = 0.0 if tight else 0.9 * smooth01(0.0, 0.8, np.array(uu)).item()
                tl = grow * max(0.0, math.sin(t[0, k])) ** 1.5
                tr = grow * max(0.0, -math.sin(t[0, k])) ** 1.5
                w = WM[j] * (1.0 - tl - tr) + WM[int(J[i, kl])] * tl + WM[int(J[i, kr])] * tr
                b = smooth01(0.0, 1.0 if tight else 0.25, np.array(uu)).item()   # skin weights above the hip
                w = w * b + WM[j] * (1.0 - b)
                if rigid_from is not None:
                    pr = smooth01(rigid_from + 3.0, rigid_from, np.array(zs[i])).item()
                    w = w * (1.0 - pr) + PELVIS_W * pr
                ws.append(w if (b > 0 or rigid_from is not None) else np.full(nb, np.nan))
                ms.append(1.0 - float(u[i, 0]))                        # body morphs fade out down the skirt
        for r0, r1 in zip(grid, grid[1:]):
            for k in range(nt):
                bm.faces.new((r0[k], r1[k], r1[k + 1], r0[k + 1]))      # facing out
        ws = np.array(ws)
        PIECES.append((bm, piece_slot, src, ed, dict(w=ws, m=np.array(ms), uv=np.array(uvs, float), y=np.array(ys))))
        return grid

    sheet(r, top, hem, slot)
    band = r + TRIM_PROUD                                          # outer waistband, stacked 0.3 cm out
    grid = sheet(band, top, top - band_h, slot)
    top_ring = [v.co.copy() for v in grid[0][:-1]]
    tube(top_ring, 0.0, slot, closed=True, flat=(0.7, TRIM_PROUD), step=0.5)


OUTFITS = {"Noir": noir, "Garter": garter_set, "Cocoa": cocoa, "Fishnet": fishnet,
           "Catsuit": catsuit_set, "Sheer": sheer, "Bodystocking": bodystocking,
           "BodystockingSkirt": lambda: (bodystocking(), skirt()), "Tartan": tartan,
           "Skirt": lambda: skirt(off=1.0, flare=1.5, swing=0.0, skin_to=LM["crotch"] + 2.0, tight=True, follow=True, cling=0.7)}   # the cloth skirt: follows her shape, only the hem moves (Tema 2026-11-10); 1 cm off at the top, 4 at the hem (the cloth gets no bounce morphs); skin weights to just above the crotch, so the hip front cannot push through
CLOTH = {"Skirt"}                                # simulated in UE: the default body's morphs baked into the rest shape


# ---- assemble: one mesh, weights, shape keys, garment UVs ------------------------------------------
def assemble(name):
    out_bm = bmesh.new()
    srcs, edist, extras = [], [], []
    fit = dict(kind=[], slot=[], strap=[], ring=[], straps=[], covers=[])
    for bm, slot, src, *ed in PIECES:
        n_ = len(bm.verts)
        mt = META.pop(id(bm), dict(kind="loose"))
        fit["kind"] += [mt["kind"]] * n_
        fit["slot"] += [slot] * n_
        if mt["kind"] == "strap":
            fit["strap"] += [len(fit["straps"])] * n_
            fit["ring"] += [k // mt["per"] for k in range(n_)]
            fit["straps"].append(dict(line=mt["line"], width=mt["width"], slot=slot))
        else:
            fit["strap"] += [-1] * n_
            fit["ring"] += [0] * n_
        if mt["kind"] == "panel":
            fit["covers"].append((mt["cover"], slot))
        srcs += src if src is not None else [(-1, -1, 0.0)] * n_
        assert src is None or len(src) == n_
        edist += ed[0] if ed and ed[0] is not None else [NO_SEAM] * n_   # cords and tapes: shape is geometry
        assert len(edist) == len(srcs)
        ex = ed[1] if len(ed) > 1 else {}
        nb = len(gnames)
        extras.append(dict(w=ex.get("w", np.full((n_, nb), np.nan)), m=ex.get("m", np.ones(n_)),
                           uv=ex.get("uv", np.full((n_, 2), np.nan)), y=ex.get("y", np.full(n_, np.nan))))
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
    # signed distance to the nearest seam line, on fabric panels only (UVSeam.y): the sign
    # flips across the line so it interpolates cleanly over a triangle the seam runs through
    ed = np.array(edist, float)
    sd = np.full(len(ed), NO_SEAM)
    pts = [(q, (line[min(i + 1, len(line) - 1)] - line[max(i - 1, 0)]).normalized())
           for line in SEAMS for i, q in enumerate(line)]
    if pts:
        sk = KDTree(len(pts))
        for i, (q, _) in enumerate(pts):
            sk.insert(q, i)
        sk.balance()
        for i in np.nonzero(ed < NO_SEAM)[0].tolist():
            p = om.vertices[i].co
            q, j, dist = sk.find(p)
            if dist < 3.0:
                side = (pts[j][1].cross(nearest_normal(p))).dot(p - q)
                sd[i] = dist if side >= 0 else -dist
    SEAMS.clear()
    ex = {k: np.concatenate([e[k] for e in extras]) for k in ("w", "m", "uv", "y")}
    sd = np.where(np.isnan(ex["y"]), sd, ex["y"])
    EXTRA_OF[ob.name] = ex
    SEAM_OF[ob.name] = np.stack([ed, sd], 1)
    FIT_OF[ob.name] = fit
    return ob


# what a garment must clear, relative to the default body: the rest pose, then each morph at
# full strength on its own (arm-raise and hip-flexion correctives, bust sway and bounce)
CLEAR_KEYS = ["ArmUp_l", "ArmUp_r", "ThighUp_l", "ThighUp_r", "BustSwayL_In", "BustSwayR_In",
              "BustSwayL_Out", "BustSwayR_Out", "BustBounceL_Up", "BustBounceR_Up"]
CLEARANCE = 0.08     # cm of fabric kept above the skin


def _shortfall(S_, C_, fab):
    """Per fabric vertex: how far short of CLEARANCE it is, and the skin normal to push along."""
    bvh = BVHTree.FromPolygons([V3(p) for p in S_], T.tolist())
    out = np.zeros((len(C_), 3))
    for i in fab.tolist():
        loc, nrm, _, dist = bvh.find_nearest(V3(C_[i]))
        if loc is None or dist > 2.0:                            # the skirt and other loose pieces
            continue
        short = CLEARANCE - (V3(C_[i]) - loc).dot(nrm)
        if short > 0:
            out[i] = np.array(nrm) * short
    return out


def _smooth_push(push, idx3, w3, fab):
    """Gather a per-vertex push onto the skin, spread it so it has no edge, read it back."""
    acc = np.zeros((nv, 3))
    mag = np.zeros(nv)
    m = np.linalg.norm(push, axis=1)
    for i in np.nonzero(m > 0)[0].tolist():
        j = int(idx3[i, 0])
        if m[i] > mag[j]:
            mag[j], acc[j] = m[i], push[i]
    torso = (Z >= LM["crotch"]) & (HANDS <= 0.5)               # toes and fingers fold too tight
    acc[~torso] = 0.0
    sm = diffuse(acc, 6)
    sn, an = np.linalg.norm(sm, axis=1), np.linalg.norm(acc, axis=1)
    acc = np.where((an > sn)[:, None], acc, sm * 1.5)
    out = sum(acc[idx3[:, k]] * w3[:, k, None] for k in range(3))
    out[~fab] = 0.0
    return out, int((an > 1e-4).sum())


def clear_skin(oc, idx3, w3, fab, dl):
    """Fabric copies its skin's deltas, but a delta that turns the surface (ArmUp at the shoulder)
    or a cut between two skin vertices can bring skin through it. Lift the rest shape where the
    default body pokes through, then add to each morph's own delta whatever it needs on top,
    along the morphed surface, so the fix rides that morph only."""
    fab = np.asarray(fab)
    idx = np.nonzero(fab)[0]
    push, n = _smooth_push(_shortfall(X, oc, idx), idx3, w3, fab)
    oc = oc + push
    report = ["rest %d" % n]
    for k in CLEAR_KEYS * 2:                                       # a second pass picks up what the first moved
        if k not in DELTAS or k not in dl:
            continue
        push, n = _smooth_push(_shortfall(X + DELTAS[k], oc + dl[k], idx), idx3, w3, fab)
        if n:
            dl[k] = dl[k] + push
            report.append("%s %d" % (k, n))
    print("cleared (skin verts short):", ", ".join(report))
    return oc


# ---- fit: the clothes and the body push on each other (--fit) ------------------------------------
# Each fabric holds its shape against the body growing under it (hold: 0 follows the skin, 1 does
# not give) and seats into soft tissue at rest (seat, cm). What a garment holds back presses the
# skin under it in and spills out past its edge; a free strap pulls taut, so it spans hollows
# (between the breasts) instead of following the skin into them. Morphs, per outfit O:
#   Fit_O        the seat, on while O is worn
#   Fit_O_<Key>  growth past the design body for Key (BustUp, GlutesUp, Fat), weight max(0, w - design)
# The skin gets the same names (Tools/blender_skin_fit.py), so one curve drives both meshes.
FIT = "--fit" in args or os.environ.get("OUTFIT_FIT") == "1"
HOLD = dict(Sheer=(0.10, 0.03), Net=(0.0, 0.0), Nylon=(0.25, 0.05), Lace=(0.45, 0.07), Satin=(0.75, 0.14),
            Latex=(0.55, 0.08), Binding=(0.85, 0.16), Tartan=(0.65, 0.10), Trim=(1.0, 0.0))
FIT_KEYS = [k for k in ("BustUp", "GlutesUp", "Fat") if k in DELTAS]
EDGE_HOLD = 0.85     # a bound edge barely gives
MID_GIVE = 0.4       # share of its hold a garment keeps away from its edges
SPILL = 0.6          # share of the volume pressed in that swells out past the edge (the rest is taken up)
_E0, _E1 = EDGES[:, 0], EDGES[:, 1]
_DEG = np.maximum(np.bincount(_E0, minlength=nv) + np.bincount(_E1, minlength=nv), 1).astype(float)


def _blur(a, iters):
    """diffuse(), fast enough for the hundreds of passes a 2-3 cm spread takes on this mesh
    (vertices are ~0.27 cm apart)."""
    a = np.asarray(a, float)
    for _ in range(iters):
        acc = np.bincount(_E0, a[_E1], nv) + np.bincount(_E1, a[_E0], nv)
        a = 0.5 * a + 0.5 * acc / _DEG
    return a


_soft = sum(np.linalg.norm(DELTAS[k], axis=1) / max(np.linalg.norm(DELTAS[k], axis=1).max(), 1e-6) * g
            for k, g in (("BustUp", 1.6), ("GlutesUp", 1.6), ("Fat", 1.0)) if k in DELTAS)
SOFT = np.clip(_blur(np.clip(_soft, 0, 1), 30), 0, 1)      # 1 on breast, glute and belly tissue, 0 on bone
SKIN_FIT = {}        # outfit name -> {morph: per-skin-vertex delta, world cm}


def _strap_cover(line, width):
    """Per skin vertex: (width/2 - distance to the strap's centre line), NEG far from it."""
    out = np.full(nv, NEG)
    for p in line[::2]:
        for _, j, d in kd.find_range(V3(p), width * 0.5 + 0.6):
            out[j] = max(out[j], width * 0.5 - d)
    return out


def _majorant(s, a):
    """The least concave function over a(s) with the same ends: a string pulled over bumps."""
    hull = []
    for i in range(len(s)):
        while len(hull) >= 2:
            i0, i1 = hull[-2], hull[-1]
            if (a[i1] - a[i0]) * (s[i] - s[i0]) <= (a[i] - a[i0]) * (s[i1] - s[i0]):
                hull.pop()
            else:
                break
        hull.append(i)
    return np.interp(s, s[hull], a[hull])


def fit_outfit(name, fit, oc, idx3, w3, dl, fab):
    kind, slot = np.array(fit["kind"]), np.array(fit["slot"])
    hold_v = np.array([HOLD.get(s, (0.0, 0.0))[0] for s in slot])
    # skin side: how hard and how deep the garment holds each skin vertex
    H, SEAT = np.zeros(nv), np.zeros(nv)
    covers = list(fit["covers"]) + [(_strap_cover(st["line"], st["width"]), st["slot"]) for st in fit["straps"]]
    for val, sl in covers:
        h, se = HOLD.get(sl, (0.0, 0.0))
        inside = np.asarray(val) >= 0
        H = np.where(inside, np.maximum(H, h), H)
        SEAT = np.where(inside, np.maximum(SEAT, se), SEAT)
    C = H > 0
    # a garment holds hardest at its bound edges; its middle stretches over the body (a cup
    # gives, its binding does not), so the press is the edge's, fading in over ~1.5 cm
    EDGE = np.where(C, np.clip((1.0 - _blur(C.astype(float), 40)) * 3.0, 0, 1), 0.0)
    H = H * MID_GIVE + (np.maximum(H, EDGE_HOLD) - H * MID_GIVE) * EDGE
    interp = lambda f: sum(f[idx3[:, k]] * w3[:, k, None] for k in range(3)) if f.ndim == 2 else \
        sum(f[idx3[:, k]] * w3[:, k] for k in range(3))

    COV = _blur(C.astype(float), 10)                       # coverage with a ~0.8 cm soft edge

    def skin_delta(p):
        """Press p (cm, >= 0) in under the garment, and spill part of what it displaces out
        past the edge as a soft roll (~2.5 cm wide), the way tissue swells over a band."""
        p = np.where(C, p, 0.0)
        ps = _blur(p, 14)
        ring = _blur(p, 200) * (1.0 - COV) ** 2
        spill = ring * (SPILL * ps.sum() / max(ring.sum(), 1e-9))
        return (spill - ps)[:, None] * NRM, ps

    morphs = {}
    # the seat: soft tissue under tight fabric and straps sinks in a little
    sd, p0 = skin_delta(SEAT * SOFT)
    morphs["Fit_" + name] = (sd, -interp(p0)[:, None] * interp(NRM))
    for k in FIT_KEYS:
        a = np.clip(np.einsum("ij,ij->i", DELTAS[k], NRM), 0, None)        # outward growth per unit
        sd, p = skin_delta(H * a)
        g = -interp(p)[:, None] * interp(NRM)                              # panels sit on the pressed skin
        g[kind == "rigid"] = 0.0
        # straps: taut over the grown body, lifted off the hollows they span
        strap, ring = np.array(fit["strap"]), np.array(fit["ring"])
        for si, st in enumerate(fit["straps"]):
            line = st["line"]
            j = np.array([nearest(V3(q)) for q in line])
            grown = a[j] + np.einsum("ij,ij->i", sd[j], NRM[j])
            s_ = np.concatenate([[0.0], np.cumsum(np.linalg.norm(np.diff(line, axis=0), axis=1))])
            lift = _majorant(s_, grown) - a[j]                             # dl[k] already carries a
            m = strap == si
            g[m] = (lift[np.clip(ring[m], 0, len(line) - 1)])[:, None] * NRM[j][np.clip(ring[m], 0, len(line) - 1)]
        morphs["Fit_%s_%s" % (name, k)] = (sd, g)
    # nothing of the skin through the cloth at full strength
    for mk, (sd, g) in morphs.items():
        base = DELTAS.get(mk.rsplit("_", 1)[-1], 0.0) if mk.count("_") > 1 else 0.0
        push, n = _smooth_push(_shortfall(X + base + sd, oc + (dl.get(mk.rsplit("_", 1)[-1], 0.0) if mk.count("_") > 1 else 0.0) + g,
                                          np.nonzero(fab)[0]), idx3, w3, fab)
        dl[mk] = g + push
        print("fit", mk, "skin moved max %.2f cm, cloth %.2f cm, cleared %d" %
              (np.abs(sd).max(), np.linalg.norm(g, axis=1).max(), n))
    SKIN_FIT[name] = {mk: sd for mk, (sd, g) in morphs.items()}
    np.savez_compressed(os.path.join(OUT_DIR, "skinfit_%s.npz" % name.lower()), **SKIN_FIT[name])


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
    ex = EXTRA_OF.pop(ob.name)
    own = ~np.isnan(ex["w"][:, 0])                         # pieces that bring their own weights (skirt)
    ow[own] = ex["w"][own]
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

    dl = {kname: sum(DL[idx3[:, k]] * w3[:, k, None] for k in range(3)) * ex["m"][:, None] for kname, DL in DELTAS.items()}
    oc = clear_skin(oc, idx3, w3, SEAM_OF[ob.name][:, 0] < NO_SEAM, dl)     # fabric panels only
    fit = FIT_OF.pop(ob.name, None)
    if FIT and fit and name not in CLOTH:
        fit_outfit(name, fit, oc, idx3, w3, dl, SEAM_OF[ob.name][:, 0] < NO_SEAM)
    if name in CLOTH:                                          # a cloth sim gets no body morphs:
        om.vertices.foreach_set("co", oc.ravel())              # keep the default body baked in
        dl = {}
    rest = oc - sum(dl[k] * w for k, w in DESIGN.items() if k in dl)    # designed on the default body
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
    lu = ex["uv"][loops].reshape(-1, 3, 2)                  # pieces with their own UV0 (skirt: cm round and down)
    mine = ~np.isnan(lu[..., 0])
    u, v = np.where(mine, lu[..., 0], u), np.where(mine, lu[..., 1], v)
    om.uv_layers.new(name="UVMap").data.foreach_set("uv", np.stack([u, v], -1).ravel())
    per_vert = lambda arr: sum(arr[idx3[:, k]] * w3[:, k] for k in range(3))[loops]
    om.uv_layers.new(name="UVStretch").data.foreach_set("uv", np.stack([per_vert(STRETCH), per_vert(ZONE)], -1).ravel())
    om.uv_layers.new(name="UVSeam").data.foreach_set("uv", SEAM_OF.pop(ob.name)[loops].ravel())   # UV2, cm
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
            Nylon=pmat("Nylon", (0.006, 0.006, 0.008), 0.5, alpha=0.5), Binding=pmat("Binding", (0.006, 0.006, 0.008), 0.7),
            Tartan=pmat("Tartan", (0.45, 0.02, 0.02), 0.55))
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
        shots = [("front", FWD * 0.9 + LEFT * 0.42, 92, 560), ("back", -FWD * 0.9 + LEFT * 0.42, 92, 560)]
        if os.environ.get("OUTFIT_CLOSE"):                     # torso close-ups for checking edges
            shots += [("front_close", FWD * 0.9 + LEFT * 0.3, LM["waist"], 190), ("back_close", -FWD * 0.9 + LEFT * 0.3, LM["waist"], 190)]
        for label, d, tz, dist in shots:
            tgt = Vector((pel_v.x, pel_v.y, tz))
            cam.location = tgt + V3(d).normalized() * dist
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
