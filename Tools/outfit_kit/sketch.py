"""Outfit Kit, step 1: draw an outfit on her in Blender, export it as exact body coordinates.

    Tools/outfit_kit/kit.sh new   <Name>    make SourceArt/Character/OutfitKit/<Name>/ (sketch.blend, brief.md)
    Tools/outfit_kit/kit.sh draw  <Name>    open the sketch in Blender, ready to draw on her
    Tools/outfit_kit/kit.sh export <Name>   sketch.json + sketch.md + preview.png (what agents read)

The body is her DEFAULT body (the Character page's starting sliders), the same shape
Tools/blender_outfit.py designs on, with the same vertex numbering. Every exported point is
glued to the skin as (three skin vertices, barycentric weights), so a builder can put it back
on any body shape the sliders make.

Layers (one Grease Pencil layer per kind of piece; rename the slot part freely):

    panel <Slot> [bind]   closed outline of a fabric area (Satin, Sheer, Lace, Net, Latex, Nylon,
                          Tartan, Jersey). "bind" = every edge gets a bound tape.
    strap <cm>            a tape lying on the skin, <cm> wide
    taut <cm>             a strap pulled straight between its ends (spans hollows: garters, V straps)
    hang                  a piece that hangs from its top point (tie, chain, tassel)
    seam                  a flat seam or boning line on a panel
    point <part>          a dot = one piece of hardware (ring, buckle, slider, clip, bow)
    notes                 free scribbles for the agent; the builder ignores them
    guides                her landmarks (locked): neck, bust, underbust, waist, hip, crotch, thigh, knee

Symmetry: draw her left side (your right in the front view) and the export mirrors it. A
stroke that reaches the centre line is joined to its mirror: half a waistband from centre
front round to centre back becomes the whole band, half a brief outline becomes one closed
outline. A stroke that already crosses the centre is kept as it is. Uncheck the "Mirror"
modifier on the Sketch object for an asymmetric outfit.
"""
import json
import math
import os
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Euler, Vector
from mathutils.bvhtree import BVHTree

ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
MODE = ARGS[0] if ARGS else "open"
NAME = ARGS[1] if len(ARGS) > 1 else None
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SKIN = os.path.join(ROOT, "SourceArt", "Character", "android_skin.fbx")
KIT = os.path.join(ROOT, "SourceArt", "Character", "OutfitKit")

# Her default body, as in Tools/blender_outfit.py (DESIGN): change both together.
DESIGN = {"BustUp": 0.80, "BustApart": 1.097, "GlutesUp": 0.5, "Fat": 0.38, "ThighGap": 0.0,
          "AbsLines": 0.734, "AbsPack": 0.12, "BustBounceL_Up": 0.65, "BustBounceR_Up": 0.65}

SLOTS = ["Sheer", "Lace", "Net", "Satin", "Latex", "Trim", "Nylon", "Binding", "Tartan", "Jersey"]
# (layer name, colour) the sketch starts with; the colour is what preview.png and sketch.md use
LAYERS = [
    ("notes", (0.95, 0.95, 0.95)),
    ("point ring", (1.00, 0.85, 0.10)),
    ("hang", (0.20, 0.90, 0.90)),
    ("seam", (0.55, 0.55, 1.00)),
    ("taut 1.0", (1.00, 0.45, 0.10)),
    ("strap 1.0", (1.00, 0.15, 0.55)),
    ("panel Sheer", (0.35, 0.75, 1.00)),
    ("panel Satin bind", (0.20, 0.95, 0.35)),
]
GUIDE_RGB = (0.30, 0.30, 0.30)
STEP = 0.5          # cm between exported points
CENTRE = 1.2        # cm from the centre line that counts as "on" it


# ---- her default body ------------------------------------------------------------------------
def t_pose(arm):
    """Raise both upper arms to horizontal (her T-pose): the views are traced with her arms clear
    of her sides. Vertex numbering does not change, so a point on the T-posed skin maps to the
    same skin vertices in her rest pose."""
    from mathutils import Matrix
    Mi = arm.matrix_world.inverted().to_3x3()
    for s, sgn in (("l", 1.0), ("r", -1.0)):
        pb, lo = arm.pose.bones["upperarm_" + s], arm.pose.bones["lowerarm_" + s]
        cur = (lo.head - pb.head).normalized()
        tgt = (Mi @ Vector((sgn, 0.0, 0.0))).normalized()
        R = Matrix.Translation(pb.head) @ cur.rotation_difference(tgt).to_matrix().to_4x4() @ Matrix.Translation(-pb.head)
        pb.matrix = R @ pb.matrix
        bpy.context.view_layer.update()


def posed_points(obj, keys):
    """World positions of a mesh with shape key values and its armature modifier applied."""
    if obj.data.shape_keys:
        for k in obj.data.shape_keys.key_blocks[1:]:
            k.value = keys.get(k.name, 0.0)
    dg = bpy.context.evaluated_depsgraph_get()
    ev = obj.evaluated_get(dg)
    m = ev.to_mesh()
    P = np.array([obj.matrix_world @ v.co for v in m.vertices])
    ev.to_mesh_clear()
    return P


def build_body(pose=None):
    """Import the skin like Tools/blender_outfit.py does (same hair removal, so the same vertex
    numbering), bake the default sliders in, triangulate. pose="T": arms raised to horizontal.
    Returns (object, design positions, info, masks)."""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=SKIN)
    arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    skin = next(o for o in bpy.data.objects if o.type == "MESH")
    me = skin.data
    hair = [i for i, m in enumerate(me.materials) if m and m.name.lower().startswith("hair")]
    if hair:
        hb = bmesh.new()
        hb.from_mesh(me)
        bmesh.ops.delete(hb, geom=[f for f in hb.faces if f.material_index in hair], context="FACES")
        bmesh.ops.delete(hb, geom=[v for v in hb.verts if not v.link_faces], context="VERTS")
        hb.to_mesh(me)
        hb.free()
        me.update()
    nv = len(me.vertices)
    co = np.empty(nv * 3)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    M = np.array(skin.matrix_world)
    X = co @ M[:3, :3].T + M[:3, 3]
    keys = {k.name: k for k in me.shape_keys.key_blocks} if me.shape_keys else {}
    deltas = {}
    for k, w in DESIGN.items():
        if k in keys:
            kc = np.empty(nv * 3)
            keys[k].data.foreach_get("co", kc)
            deltas[k] = (kc.reshape(-1, 3) - co) @ M[:3, :3].T
            X = X + deltas[k] * w
    # bone weights for the guide masks
    gn = [g.name for g in skin.vertex_groups]
    W = np.zeros((nv, len(gn)), np.float32)
    for v in me.vertices:
        for g in v.groups:
            W[v.index, g.group] = g.weight
    G = lambda *ks: W[:, [i for i, n in enumerate(gn) if any(k in n for k in ks)]].sum(1)
    fingers = ("hand", "thumb", "index", "middle", "ring", "pinky")
    upperarm = G("upperarm", "lowerarm", *fingers) - G("toe_0")
    head = G("head", "neck")
    legs = G("thigh", "calf", "foot", "ball", "toe_0")
    Bn = {b.name: np.array(arm.matrix_world @ b.head_local) for b in arm.data.bones}

    # the body object: design shape (posed if asked), no keys, no rig, triangles
    shown = X
    if pose == "T":
        t_pose(arm)
        shown = posed_points(skin, DESIGN)
        assert len(shown) == nv
    if me.shape_keys:
        skin.shape_key_clear()
    me.vertices.foreach_set("co", shown.ravel())      # world cm; the object sits at the origin
    me.update()
    skin.parent = None
    skin.modifiers.clear()
    skin.matrix_world = ((1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 0, 0, 1))
    for o in list(bpy.data.objects):
        if o != skin:
            bpy.data.objects.remove(o)
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.triangulate(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    for p in me.polygons:
        p.use_smooth = True
    skin.name = me.name = "Body"
    skin.vertex_groups.clear()

    # landmarks (cm), named as in blender_outfit.py's LM
    pel = Bn["pelvis"]
    bd = np.linalg.norm(deltas.get("BustUp", np.zeros_like(X)), axis=1)
    mk = (bd > 0.25 * bd.max()) & (X[:, 0] > 0)
    apex = X[np.nonzero(mk)[0][np.argmin(X[mk][:, 1])]]          # front-most (-Y) bust point, her left
    under = X[mk][:, 2].min()
    LM = dict(neck=Bn["neck_01"][2] - 1.0, bust=apex[2], underbust=under + 0.5,
              waist=(Bn["spine_02"][2] + Bn["spine_03"][2]) * 0.5, hip_top=pel[2] + 3.0,
              crotch=Bn["thigh_l"][2] - 10.0, thigh_mid=(Bn["thigh_l"][2] + Bn["calf_l"][2]) * 0.5,
              knee=Bn["calf_l"][2], ankle=Bn["foot_l"][2])
    info = dict(landmarks={k: round(float(v), 2) for k, v in LM.items()},
                bust_apex_left=[round(float(v), 2) for v in apex], pelvis=[round(float(v), 2) for v in pel],
                thigh_l=[round(float(v), 2) for v in Bn["thigh_l"]], foot_l=[round(float(v), 2) for v in Bn["foot_l"]],
                forward=[0.0, -1.0, 0.0], her_left=[1.0, 0.0, 0.0], vertices=nv)
    skin["kit"] = json.dumps(info)
    masks = dict(arms=(upperarm >= 0.5), torso=(upperarm < 0.3) & (head < 0.5) & ((legs < 0.5) | (X[:, 2] > LM["crotch"] + 4)),
                 leg_l=(legs >= 0.5) & (X[:, 0] > 0), leg_r=(legs >= 0.5) & (X[:, 0] < 0))
    return skin, X, info, masks


def body_bvh(obj, mask=None):
    me = obj.data
    V = [v.co.copy() for v in me.vertices]
    T = [tuple(p.vertices) for p in me.polygons]
    if mask is not None:
        T = [t for t in T if mask[t[0]] and mask[t[1]] and mask[t[2]]]
    return BVHTree.FromPolygons(V, T)


def ring_points(bvh, centre, z, n=96, up=(0, 0, 1)):
    """A horizontal loop on the surface round an axis point at height z (rays from outside in)."""
    pts = []
    for k in range(n):
        t = 2 * math.pi * k / n
        d = Vector((math.sin(t), -math.cos(t), 0.0))        # starts at her front (-Y)
        o = Vector((centre[0], centre[1], z)) + d * 80
        hit = bvh.ray_cast(o, -d, 80)
        if hit[0]:
            pts.append(hit[0] + hit[1] * 0.08)
    return pts


# ---- grease pencil ---------------------------------------------------------------------------
def ink_material(name, rgb):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    if not m.is_grease_pencil:
        bpy.data.materials.create_gpencil_data(m)
    m.grease_pencil.color = (*rgb, 1.0)
    return m


def add_stroke(layer, pts, radius=0.12, cyclic=False):
    fr = layer.frames[0] if len(layer.frames) else layer.frames.new(1)
    dr = fr.drawing
    dr.add_strokes([len(pts)])
    s = dr.strokes[len(dr.strokes) - 1]
    for i, q in enumerate(pts):
        s.points[i].position = q
        s.points[i].radius = radius
    s.cyclic = cyclic
    return s


def make_sketch(body, X, info, masks):
    gp = bpy.data.grease_pencils.new("Sketch")
    ob = bpy.data.objects.new("Sketch", gp)
    bpy.context.scene.collection.objects.link(ob)
    gp.materials.append(ink_material("Ink", (1, 1, 1)))
    # guides first (bottom of the stack), locked
    g = gp.layers.new("guides")
    g.tint_color, g.tint_factor = GUIDE_RGB, 1.0
    LM = info["landmarks"]
    tb = body_bvh(body, masks["torso"])
    pel = info["pelvis"]
    for k in ("neck", "bust", "underbust", "waist", "hip_top"):
        pts = ring_points(tb, pel, LM[k])
        if len(pts) > 20:
            add_stroke(g, pts, 0.06, cyclic=True)
    th, ft = np.array(info["thigh_l"]), np.array(info["foot_l"])
    for side, mk in ((1, "leg_l"), (-1, "leg_r")):
        lb = body_bvh(body, masks[mk])
        for k in ("crotch", "thigh_mid", "knee"):
            z = LM[k] - (3.0 if k == "crotch" else 0.0)
            c = th + (ft - th) * ((th[2] - z) / (th[2] - ft[2]))
            pts = ring_points(lb, (c[0] * side, c[1]), z, 64)
            if len(pts) > 20:
                add_stroke(g, pts, 0.06, cyclic=True)
    # the centre line, front and back
    c = []
    for z in np.arange(LM["crotch"] + 2, LM["neck"] + 6, 1.0):
        for sgn in (-1, 1):
            hit = tb.ray_cast(Vector((0, sgn * 80, z)), Vector((0, -sgn, 0)), 160)
            if hit[0]:
                c.append((sgn, hit[0] + hit[1] * 0.08))
    for sgn in (-1, 1):
        pts = [q for s_, q in c if s_ == sgn]
        if pts:
            add_stroke(g, pts, 0.05)
    g.lock = True
    for name, rgb in LAYERS:
        L = gp.layers.new(name)
        L.tint_color, L.tint_factor = rgb, 1.0
        L.frames.new(1)
    gp.layers.active = gp.layers["strap 1.0"]
    mod = ob.modifiers.new("Mirror", "GREASE_PENCIL_MIRROR")
    mod.use_axis_x = True
    return ob


def clay(body):
    m = bpy.data.materials.new("Clay")
    m.diffuse_color = (0.72, 0.68, 0.64, 1)
    body.data.materials.clear()
    body.data.materials.append(m)


def scene_setup():
    sc = bpy.context.scene
    sc.unit_settings.system = "METRIC"
    sc.unit_settings.scale_length = 0.01
    sc.unit_settings.length_unit = "CENTIMETERS"
    ts = sc.tool_settings
    ts.gpencil_stroke_placement_view3d = "SURFACE"
    ts.gpencil_surface_offset = 0.08


def cmd_new():
    d = os.path.join(KIT, NAME)
    os.makedirs(os.path.join(d, "refs"), exist_ok=True)
    body, X, info, masks = build_body()
    clay(body)
    reg = np.full(len(X), 3, np.int32)                        # 0 torso, 1 leg_l, 2 leg_r, 3 arm/head
    for i, k in enumerate(("leg_r", "leg_l", "torso")):
        reg[masks[k]] = 2 - i
    a = body.data.attributes.new("kit_region", "INT", "POINT")
    a.data.foreach_set("value", reg)
    scene_setup()
    make_sketch(body, X, info, masks)
    body.hide_select = True
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(d, "sketch.blend"))
    brief = os.path.join(d, "brief.md")
    if not os.path.exists(brief):
        open(brief, "w").write(BRIEF.format(name=NAME))
    print("KIT new", d)


BRIEF = """# {name}

What it is, in a sentence or two:

Fabrics and colours (per layer in the sketch, e.g. "panel Satin bind: black satin, gloss 0.3"):

What must move or hang:

References: put photos in refs/ and say what to take from each (cut, fabric, detail).

Don't:
"""


# ---- export ------------------------------------------------------------------------------------
REGIONS = ("torso", "leg_l", "leg_r", "other")


def resample(P, closed, step=STEP):
    if closed:
        P = np.vstack([P, P[:1]])
    seg = np.linalg.norm(np.diff(P, axis=0), axis=1)
    s = np.concatenate([[0], np.cumsum(seg)])
    if s[-1] < 1e-6:
        return P[:1]
    n = max(2, int(round(s[-1] / step)) + 1)
    t = np.linspace(0, s[-1], n)
    Q = np.stack([np.interp(t, s, P[:, k]) for k in range(3)], 1)
    return Q[:-1] if closed else Q


def smooth(P, closed, iters=3):
    for _ in range(iters):
        if closed:
            P = 0.5 * P + 0.25 * (np.roll(P, 1, 0) + np.roll(P, -1, 0))
        elif len(P) > 2:
            P = np.vstack([P[:1], 0.5 * P[1:-1] + 0.25 * (P[:-2] + P[2:]), P[-1:]])
    return P


def mirror_parts(P, closed):
    """Your strokes on her left -> the pieces, per the symmetry rules in the header.
    Returns [(points, closed, how)]."""
    x = P[:, 0]
    if x.min() < -CENTRE and x.max() > CENTRE:
        return [(P, closed, "crosses the centre: kept as drawn")]
    M = P * np.array((-1.0, 1.0, 1.0))
    near = np.abs(x) < CENTRE
    if closed and near.any() and not near.all():
        # a closed half-outline with an edge on the centre line: open it there
        k = int(np.argmax(near))
        P2, n2 = np.roll(P, -k, 0), np.roll(near, -k)
        runs, cur = [], []
        for i, nn in enumerate(n2):
            if not nn:
                cur.append(i)
            elif cur:
                runs.append(cur)
                cur = []
        if cur:
            runs.append(cur)
        best = max(runs, key=len)
        P = P2[best[0] - 1: best[-1] + 2] if best[-1] + 2 <= len(P2) else P2[best[0] - 1:]
        closed = False
        M = P * np.array((-1.0, 1.0, 1.0))
        x = P[:, 0]
    if closed:
        return [(P, True, "mirrored pair"), (M[::-1], True, "mirror")]
    a, b = abs(x[0]) < CENTRE, abs(x[-1]) < CENTRE
    if a and b:
        P[0, 0] = P[-1, 0] = 0.0
        return [(np.vstack([P, M[::-1][1:-1]]), True, "half + mirror, joined into one loop")]
    if a:
        P[0, 0] = 0.0
        return [(np.vstack([M[::-1], P[1:]]), False, "half + mirror, joined at the centre")]
    if b:
        P[-1, 0] = 0.0
        return [(np.vstack([P, M[::-1][1:]]), False, "half + mirror, joined at the centre")]
    return [(P, False, "mirrored pair"), (M, False, "mirror")]


def parse_layer(name):
    t = name.split()
    kind = t[0].lower() if t else "notes"
    d = dict(kind=kind)
    if kind == "panel":
        d["slot"] = next((x for x in t[1:] if x in SLOTS), "Satin")
        d["bind"] = "bind" in t[1:]
    elif kind in ("strap", "taut"):
        nums = [float(x) for x in t[1:] if x.replace(".", "", 1).isdigit()]
        d["width_cm"] = nums[0] if nums else 1.0
        d["slot"] = next((x for x in t[1:] if x in SLOTS), "Binding")
    elif kind in ("hang", "seam"):
        d["slot"] = next((x for x in t[1:] if x in SLOTS), "Binding" if kind == "hang" else "Trim")
    elif kind == "point":
        d["part"] = " ".join(t[1:]) or "ring"
    return d


class Snapper:
    def __init__(self, body):
        me = body.data
        self.V = np.array([v.co[:] for v in me.vertices])
        self.T = np.array([p.vertices[:] for p in me.polygons])
        self.bvh = BVHTree.FromPolygons([Vector(v) for v in self.V], self.T.tolist())
        a = me.attributes.get("kit_region")
        self.reg = np.zeros(len(self.V), np.int32)
        if a:
            a.data.foreach_get("value", self.reg)

    def dist(self, q):
        return self.bvh.find_nearest(Vector(q))[3]

    def snap(self, q):
        loc, nrm, fi, _ = self.bvh.find_nearest(Vector(q))
        tri = self.T[fi]
        A, B, C = self.V[tri]
        p = np.array(loc)
        v0, v1, v2 = B - A, C - A, p - A
        d00, d01, d11, d20, d21 = v0 @ v0, v0 @ v1, v1 @ v1, v2 @ v0, v2 @ v1
        den = d00 * d11 - d01 * d01 or 1e-12
        bv = (d11 * d20 - d01 * d21) / den
        bw = (d00 * d21 - d01 * d20) / den
        bary = np.clip([1 - bv - bw, bv, bw], 0, 1)
        bary /= bary.sum()
        return p, np.array(nrm), tri, bary, int(self.reg[tri[int(np.argmax(bary))]])


def describe(p, info):
    """Where a point is, in words an agent can check against the photos."""
    LM, pel = info["landmarks"], info["pelvis"]
    ang = math.degrees(math.atan2(p[0] - pel[0], -(p[1] - pel[1])))     # 0 front, +90 her left, 180 back
    k = min(LM, key=lambda n: abs(LM[n] - p[2]))
    dz = p[2] - LM[k]
    side = "centre" if abs(p[0]) < CENTRE else ("her left" if p[0] > 0 else "her right")
    face = ("front" if abs(ang) < 45 else "back" if abs(ang) > 135 else "side")
    return "%s %s, %+.0f deg, %s %+.1f cm (z %.1f)" % (face, side, ang, k, dz, p[2])


AIR = 2.0           # cm: a point further than this from her skin was drawn in the air
MERGE = 1.3         # cm: overdrawn strokes closer than this become one line
SPUR = 2.5          # cm: loose ends and scribbles shorter than this are dropped
STUB = 5.0          # cm: a cleaned line shorter than this is dropped
KNOT = 7.0          # cm: a closed loop shorter than this is a mark, not an outline


def split_off_skin(P, cyc, snap):
    ok = np.array([snap.dist(q) <= AIR for q in P])
    if ok.all():
        return [(P, cyc)]
    runs, cur = [], []
    for q, k in zip(P, ok):
        if k:
            cur.append(q)
        elif cur:
            runs.append(cur)
            cur = []
    if cur:
        runs.append(cur)
    return [(np.array(r), False) for r in runs if len(r) >= 1]


GAP = 3.0           # cm: loose ends this close to another line are joined; junctions this close merge
TURN = 0.6          # cos of the largest bend a line may take to carry on through a junction (~53 deg)


def continue_through(lines, C, adj):
    """At each junction, a line carries on into the branch that continues it most straight, so a
    bra band crossing a side seam stays one band. Returns joined (node path, closed) lines."""
    from collections import defaultdict

    def tangent(path, at_start):
        P = C[path] if at_start else C[path[::-1]]
        s = np.cumsum(np.r_[0, np.linalg.norm(np.diff(P, axis=0), axis=1)])
        k = int(np.searchsorted(s, 3.0))
        v = P[min(k, len(P) - 1)] - P[0]
        return v / (np.linalg.norm(v) or 1)
    ends = defaultdict(list)                    # junction node -> [(line index, at its start?)]
    for i, (path, closed) in enumerate(lines):
        if closed:
            continue
        ends[path[0]].append((i, True))
        ends[path[-1]].append((i, False))
    link = {}
    for node, es in ends.items():
        if len(es) < 2:
            continue
        cand = []
        for x in range(len(es)):
            for y in range(x + 1, len(es)):
                (i, si), (j, sj) = es[x], es[y]
                if i == j:
                    continue
                d = float(tangent(lines[i][0], si) @ tangent(lines[j][0], sj))
                if d < -TURN:
                    cand.append((d, es[x], es[y]))
        for d, a, b in sorted(cand):
            if a not in link and b not in link:
                link[a], link[b] = b, a
    out, done = [], set()
    for i, (path, closed) in enumerate(lines):
        if i in done:
            continue
        if closed:
            out.append((path, True))
            done.add(i)
            continue
        # walk back to the start of the chain this line belongs to
        cur, at_start, seen = i, True, {i}
        while (cur, at_start) in link:
            j, sj = link[(cur, at_start)]
            if j in seen:
                break
            seen.add(j)
            cur, at_start = j, not sj
        # now walk forward from (cur, at_start) collecting paths
        seq, k, s_ = [], cur, at_start
        loop = False
        while True:
            done.add(k)
            p = lines[k][0] if s_ else lines[k][0][::-1]
            seq = seq + (p[1:] if seq else p)
            nxt = link.get((k, not s_))
            if nxt is None:
                break
            if nxt[0] in done:
                loop = nxt[0] == cur
                break
            k, s_ = nxt[0], nxt[1]
        out.append((seq, loop or (len(seq) > 2 and seq[0] == seq[-1])))
    return out


def consolidate(strokes, snap):
    """Sketchy, overdrawn strokes -> clean lines. Points closer than MERGE share a node; nodes
    are linked along the strokes; zig-zag triangles, short loose ends and dots are removed; what
    is left is cut into lines at the junctions (a loop comes out closed)."""
    from collections import defaultdict
    cen, cnt, grid, seqs = [], [], defaultdict(list), []
    for P, cyc in strokes:
        ids = []
        for p in P:
            c = tuple((p // MERGE).astype(int))
            best, bd = None, MERGE
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    for dz in (-1, 0, 1):
                        for k in grid.get((c[0] + dx, c[1] + dy, c[2] + dz), ()):
                            dd = np.linalg.norm(cen[k] / cnt[k] - p)
                            if dd < bd:
                                best, bd = k, dd
            if best is None:
                best = len(cen)
                cen.append(p.copy())
                cnt.append(1)
                grid[c].append(best)
            else:
                cen[best] = cen[best] + p
                cnt[best] += 1
            ids.append(best)
        if cyc and len(ids) > 2:
            ids.append(ids[0])
        seqs.append(ids)
    C = np.array([c / n for c, n in zip(cen, cnt)]) if cen else np.zeros((0, 3))
    adj = defaultdict(set)
    for ids in seqs:
        for a, b in zip(ids[:-1], ids[1:]):
            if a != b:
                adj[a].add(b)
                adj[b].add(a)
    L = lambda a, b: float(np.linalg.norm(C[a] - C[b]))

    def cut(a, b):
        adj[a].discard(b)
        adj[b].discard(a)
    changed = True
    while changed:                              # zig-zag triangles: drop their longest side
        changed = False
        for a in list(adj):
            for b in list(adj[a]):
                for c in list(adj[a] & adj[b]):
                    e = max(((a, b), (b, c), (a, c)), key=lambda t: L(*t))
                    cut(*e)
                    changed = True
                    break
                if changed:
                    break
            if changed:
                break

    def walk(a, b):
        """From node a through b along degree-2 nodes; returns the node list."""
        path, prev, cur = [a, b], a, b
        while len(adj[cur]) == 2 and cur != a:
            nxt = next(n for n in adj[cur] if n != prev)
            path.append(nxt)
            prev, cur = cur, nxt
        return path
    changed = True
    while changed:                              # loose ends and scribbles
        changed = False
        for a in [n for n in adj if len(adj[n]) == 1]:
            if len(adj[a]) != 1:
                continue
            path = walk(a, next(iter(adj[a])))
            length = sum(L(p, q) for p, q in zip(path[:-1], path[1:]))
            end_deg = len(adj[path[-1]])
            if length < SPUR and (end_deg >= 3 or end_deg == 1):
                for p, q in zip(path[:-1], path[1:]):
                    cut(p, q)
                changed = True
    # gaps: a loose end close to another line is joined to it (strokes that didn't quite touch)
    ends = [n for n in adj if len(adj[n]) == 1]
    for a in ends:
        own = set(walk(a, next(iter(adj[a])))) if adj[a] else {a}
        near = [n for n in adj if adj[n] and n not in own and L(a, n) < GAP]
        if near:
            b = min(near, key=lambda n: L(a, n))
            adj[a].add(b)
            adj[b].add(a)
    # crossings drawn as a cluster of junctions a few mm apart become one junction
    for _ in range(3):
        parent = {n: n for n in adj}

        def root(n):
            while parent[n] != n:
                parent[n] = parent[parent[n]]
                n = parent[n]
            return n
        merged = False
        for a in [n for n in adj if len(adj[n]) >= 3]:
            for b in list(adj[a]):
                path = walk(a, b)
                length = sum(L(p, q) for p, q in zip(path[:-1], path[1:]))
                if len(adj[path[-1]]) >= 3 and length < GAP and path[-1] != a:
                    for p in path:
                        parent[root(p)] = root(a)
                    merged = True
        if not merged:
            break
        groups = defaultdict(list)
        for n in adj:
            groups[root(n)].append(n)
        for r, ns in groups.items():
            if len(ns) > 1:
                C[r] = C[ns].mean(0)
        new = defaultdict(set)
        for a in adj:
            for b in adj[a]:
                ra, rb = root(a), root(b)
                if ra != rb:
                    new[ra].add(rb)
                    new[rb].add(ra)
        adj.clear()
        adj.update(new)
    lines, used = [], set()
    starts = [n for n in adj if len(adj[n]) not in (0, 2)]
    for a in starts:
        for b in list(adj[a]):
            if (a, b) in used:
                continue
            path = walk(a, b)
            for p, q in zip(path[:-1], path[1:]):
                used.add((p, q))
                used.add((q, p))
            lines.append((path, path[0] == path[-1]))
    for a in list(adj):                         # pure loops (no junction on them)
        for b in list(adj[a]):
            if (a, b) in used:
                continue
            path = walk(a, b)
            for p, q in zip(path[:-1], path[1:]):
                used.add((p, q))
                used.add((q, p))
            lines.append((path, True))
    lines = continue_through(lines, C, adj)
    out = []
    for path, closed in lines:
        if closed and path[0] == path[-1]:
            path = path[:-1]
        P = C[path]
        if len(P) > 2:
            P = smooth(resample(P, closed), closed, 6)       # (smoothing shrinks a jagged little knot)
        length = np.linalg.norm(np.diff(np.vstack([P, P[:1]]) if closed else P, axis=0), axis=1).sum()
        if closed and length < KNOT:                # a little knot of ink: a mark (bow, ring, clasp?)
            out.append((P.mean(0)[None], False, "small mark: a knot of ink, likely hardware or a bow"))
            continue
        if length < STUB or len(P) < 2:
            continue
        out.append((P, closed, "cleaned"))
    return out


def layer_rgb(name):
    L = bpy.data.objects["Sketch"].data.layers.get(name)
    return tuple(L.tint_color) if L else (1, 1, 1)


def cmd_export():
    d = os.path.join(KIT, NAME)
    body, sk = bpy.data.objects["Body"], bpy.data.objects["Sketch"]
    info = json.loads(body["kit"])
    mirror = any(m.type == "GREASE_PENCIL_MIRROR" and m.show_viewport for m in sk.modifiers)
    snap = Snapper(body)
    Mw = np.array(sk.matrix_world)
    pieces = []
    for L in sk.data.layers:
        if L.name == "guides" or not len(L.frames):
            continue
        meta = parse_layer(L.name)
        raw = []
        for st in L.frames[0].drawing.strokes:
            P = np.array([p.position[:] for p in st.points])
            if len(P):
                raw.append((P @ Mw[:3, :3].T + Mw[:3, 3], bool(st.cyclic)))
        on_skin, dropped = [], 0
        for P, cyc in raw:                       # strokes that missed her land in the air: drop those points
            for run, c in split_off_skin(P, cyc, snap):
                on_skin.append((run, c))
            dropped += sum(snap.dist(q) > AIR for q in P)
        if dropped:
            print("KIT %s: dropped %d points drawn off her body" % (L.name, dropped))
        parts = []
        if meta["kind"] == "point":
            for P, _ in on_skin:
                c = P.mean(0)
                parts += [(c[None], False, "on the centre")] if abs(c[0]) < CENTRE or not mirror else \
                         [(c[None], False, "mirrored pair"), ((c * (-1, 1, 1))[None], False, "mirror")]
        else:
            strokes = []
            for P, cyc in on_skin:
                P = resample(P, cyc) if len(P) > 1 else P
                if mirror:
                    for e in (0, -1):            # ends near the centre line sit on it, so halves meet
                        if abs(P[e, 0]) < CENTRE:
                            P[e, 0] = 0.0
                strokes.append((P, cyc))
                if mirror:
                    strokes.append((P * np.array((-1.0, 1.0, 1.0)), cyc))
            for Q, closed, how in consolidate(strokes, snap):
                parts.append((Q, closed, how + (", mirrored" if mirror else "")))
        for Q, closed, how in parts:
            if True:
                meta_p = dict(meta, kind="mark") if how.startswith("small mark") else meta
                snapped = [snap.snap(q) for q in Q]
                pos = np.array([s_[0] for s_ in snapped])
                seg = np.linalg.norm(np.diff(np.vstack([pos, pos[:1]]) if closed else pos, axis=0), axis=1)
                gap = float(seg.max()) if len(seg) else 0.0
                if gap > 4 * STEP:
                    print("KIT warning: %s jumps %.1f cm (a point snapped to another body part)" % (L.name, gap))
                pieces.append(dict(
                    id=len(pieces) + 1, layer=L.name, **meta_p, closed=closed, symmetry=how,
                    length_cm=round(float(seg.sum()), 1),
                    warning=("jumps %.1f cm between points: part of it snapped to another body part" % gap) if gap > 4 * STEP else None,
                    rgb=[round(c, 3) for c in layer_rgb(L.name)],
                    region=REGIONS[np.bincount([s_[4] for s_ in snapped], minlength=4).argmax()],
                    points=np.round(pos, 3).tolist(),
                    normals=np.round([s_[1] for s_ in snapped], 3).tolist(),
                    anchors=[[int(a) for a in s_[2]] + [round(float(b), 4) for b in s_[3]] for s_ in snapped]))
    out = dict(outfit=NAME, units="cm", body="android_skin.fbx, default sliders (DESIGN), hair removed",
               axes="forward -Y, her left +X, up +Z", anchors="[v0, v1, v2, w0, w1, w2]: skin vertex ids "
               "(blender_outfit.py numbering) and barycentric weights", mirror=mirror, **info, pieces=pieces)
    with open(os.path.join(d, "sketch.json"), "w") as f:
        json.dump(out, f, indent=1)
    write_md(d, out)
    preview(d, pieces)
    print("KIT export", len(pieces), "pieces ->", d)


def write_md(d, out):
    info = out
    lines = ["# %s: sketch" % out["outfit"], "",
             "Generated by `Tools/outfit_kit/kit.sh export %s` from sketch.blend. Exact geometry is in "
             "sketch.json; preview.png shows every piece with its number (front, her left, back, 3/4)." % out["outfit"],
             "Angles: 0 = centre front, +90 = her left side, 180 = centre back. Heights relative to her "
             "landmarks: " + ", ".join("%s %.1f" % kv for kv in out["landmarks"].items()) + " (cm).", ""]
    for p in out["pieces"]:
        P = np.array(p["points"])
        extra = {k: p[k] for k in ("slot", "bind", "width_cm", "part") if k in p}
        if p.get("warning"):
            lines.append("WARNING #%d: %s" % (p["id"], p["warning"]))
        lines.append("**#%d %s** (%s): %s, %s, %.1f cm, %s, %s" % (
            p["id"], p["layer"], "rgb %.2f %.2f %.2f" % tuple(p["rgb"]), p["kind"],
            "closed outline" if p["closed"] else ("point" if len(P) == 1 else "open line"),
            p["length_cm"], p["region"], p["symmetry"]) + (" " + json.dumps(extra) if extra else ""))
        if len(P) == 1:
            lines.append("- at: " + describe(P[0], info))
        else:
            lines.append("- start: " + describe(P[0], info))
            if p["closed"]:
                lines.append("- top: " + describe(P[np.argmax(P[:, 2])], info))
                lines.append("- bottom: " + describe(P[np.argmin(P[:, 2])], info))
                lines.append("- outermost: " + describe(P[np.argmax(np.abs(P[:, 0]))], info))
            else:
                lines.append("- middle: " + describe(P[len(P) // 2], info))
                lines.append("- end: " + describe(P[-1], info))
        lines.append("")
    with open(os.path.join(d, "sketch.md"), "w") as f:
        f.write("\n".join(lines))


VIEWS = [("front", (0, -1, 0)), ("her left", (1, 0, 0)), ("back", (0, 1, 0)), ("3/4", (0.7, -0.7, 0))]


def preview(d, pieces, w=900, h=1500):
    """preview.png: the body with every piece in its layer colour and its number, four views."""
    sc = bpy.context.scene
    sk = bpy.data.objects.get("Sketch")
    if sk:
        sk.hide_render = True
    objs = []
    for p in pieces:
        P = p["points"]
        if len(P) == 1:
            bpy.ops.mesh.primitive_uv_sphere_add(radius=0.9, location=P[0])
            o = bpy.context.object
        else:
            cu = bpy.data.curves.new("c%d" % p["id"], "CURVE")
            cu.dimensions, cu.bevel_depth = "3D", 0.28 if p["kind"] != "notes" else 0.15
            sp = cu.splines.new("POLY")
            sp.points.add(len(P) - 1)
            N = np.array(p["normals"])
            for i, q in enumerate(P):
                sp.points[i].co = (*(np.array(q) + N[i] * 0.25), 1)
            sp.use_cyclic_u = p["closed"]
            o = bpy.data.objects.new("c%d" % p["id"], cu)
            sc.collection.objects.link(o)
        o.color = (*p["rgb"], 1)
        objs.append(o)
    sc.render.engine = "BLENDER_WORKBENCH"
    sh = sc.display.shading
    sh.light, sh.color_type = "STUDIO", "OBJECT"
    sh.show_cavity = True
    bpy.data.objects["Body"].color = (0.72, 0.68, 0.64, 1)
    sc.render.resolution_x, sc.render.resolution_y = w, h
    sc.render.film_transparent = False
    sc.world = sc.world or bpy.data.worlds.new("W")
    cam = bpy.data.objects.new("PrevCam", bpy.data.cameras.new("PrevCam"))
    sc.collection.objects.link(cam)
    cam.data.type, cam.data.ortho_scale = "ORTHO", 190
    sc.camera = cam
    tiles = []
    for vi, (vn, dv) in enumerate(VIEWS):
        dv = Vector(dv).normalized()
        cam.location = Vector((0, 0, 92)) + dv * 400
        cam.rotation_euler = (-dv).to_track_quat("-Z", "Y").to_euler()
        labels = []
        for p, o in zip(pieces, objs):
            P, N = np.array(p["points"]), np.array(p["normals"])
            k = int(np.argmax(N @ np.array(dv)))            # the point of this piece that faces the camera most
            if N[k] @ np.array(dv) < 0.3:
                continue
            q = Vector(P[k]) + Vector(N[k]) * 3.0
            cu = bpy.data.curves.new("t", "FONT")
            cu.body, cu.size, cu.align_x, cu.align_y = str(p["id"]), 5.0, "CENTER", "CENTER"
            t = bpy.data.objects.new("t", cu)
            sc.collection.objects.link(t)
            t.location = q + dv * 5
            t.rotation_euler = (-dv).to_track_quat("-Z", "Y").to_euler()
            t.color = (*p["rgb"], 1)
            labels.append(t)
        f = os.path.join(d, "_view%d.png" % vi)
        sc.render.filepath = f
        bpy.ops.render.render(write_still=True)
        for t in labels:
            bpy.data.objects.remove(t)
        img = bpy.data.images.load(f)
        a = np.array(img.pixels[:]).reshape(h, w, 4)
        tiles.append(a)
        bpy.data.images.remove(img)
        os.remove(f)
    big = np.concatenate(tiles, axis=1)
    out = bpy.data.images.new("preview", big.shape[1], big.shape[0])
    out.pixels = big.ravel()
    out.filepath_raw = os.path.join(d, "preview.png")
    out.file_format = "PNG"
    out.save()


def cmd_open():
    """GUI: select the sketch, go to Draw mode, front view, layers panel showing."""
    def setup():
        sk = bpy.data.objects.get("Sketch")
        if not sk:
            return None
        for o in bpy.context.view_layer.objects:
            o.select_set(False)
        bpy.context.view_layer.objects.active = sk
        sk.select_set(True)
        for win in bpy.context.window_manager.windows:
            for area in win.screen.areas:
                if area.type == "VIEW_3D":
                    sp = area.spaces.active
                    sp.clip_start, sp.clip_end = 0.5, 5000
                    sp.shading.type = "SOLID"
                    sp.overlay.show_floor = False
                    r3 = sp.region_3d
                    r3.view_perspective = "PERSP"
                    r3.view_location = (0, 0, 100)
                    r3.view_rotation = Euler((math.radians(90), 0, 0)).to_quaternion()
                    r3.view_distance = 260
                    with bpy.context.temp_override(window=win, area=area):
                        bpy.ops.object.mode_set(mode="PAINT_GREASE_PENCIL")
        for win in bpy.context.window_manager.windows:
            for area in win.screen.areas:
                if area.type == "PROPERTIES":
                    try:
                        area.spaces.active.context = "DATA"      # the layer list
                    except TypeError:
                        pass
        print("KIT draw ready, mode", bpy.context.object.mode if bpy.context.object else None)
        return None
    bpy.app.timers.register(setup, first_interval=1.0)


# ---- main --------------------------------------------------------------------------------------
if __name__ == "__main__":
    {"new": lambda: cmd_new(), "open": lambda: cmd_open(), "export": lambda: cmd_export()}[MODE]()
