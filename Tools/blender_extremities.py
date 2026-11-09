"""Real hands and feet for the android skin, grafted from the MakeHuman base mesh (CC0).
Headless, after Tools/blender_skin.py:

  /Applications/Blender.app/Contents/MacOS/Blender -b <android_skin.blend> \
      --python Tools/blender_extremities.py -- <makehuman base.obj> <out.fbx> [<preview_dir>]

Quinn's hands are mannequin gloves and his feet are shoes, so the remeshed skin had melted
fingers and no toes. MakeHuman's base mesh (SourceArt/Character/makehuman_base.obj) has
real ones, with joint markers for every finger and toe segment. Per side:
1. cut the hand (foot) out of the MakeHuman mesh and subdivide it (Catmull-Clark),
2. bend it onto Quinn's bones: the palm by a best-fit similarity, each finger segment by
   its own rotation and stretch, blended smoothly across the knuckles (linear blend
   skinning from MakeHuman's joints to Quinn's); the foot by a frame from ankle to ball,
3. cut the skin's own hand (foot) off just behind, match the cross-section of the cut,
   stitch the two with a bridge and relax the seam,
4. skin weights from the bone each part was bent by (palm -> hand, segments -> fingers,
   toes -> ball), blending into the skin's own weights over the last cm before the seam.
Shape keys survive (new vertices sit still in every key). Writes <out>.fbx and <out>.blend.
"""
import math
import os
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector
from mathutils.kdtree import KDTree

args = sys.argv[sys.argv.index("--") + 1:]
MH, OUT = args[0], args[1]
PREVIEW = args[2] if len(args) > 2 else None

LOOK = dict(
    subdiv=2,            # Catmull-Clark levels on the MakeHuman pieces (its base is low-poly)
    hand_cut=1.5,        # cm behind the wrist (hand bone head) where the skin is cut
    foot_cut=9.0,        # cm above the ankle (foot bone head) along the shin
    gap=0.6,             # cm between the two cut edges, bridged
    match=4.0,           # cm over which the graft's cross-section eases into the skin's
    blend=2.5,           # cm over which weights ease from the skin's into the graft's
    tau=0.35,            # cm: how soft the blend between finger segments is
    seam_smooth=6,       # relax iterations around the seam
)
FINGERS = [("thumb", 1), ("index", 2), ("middle", 3), ("ring", 4), ("pinky", 5)]

skin = next(o for o in bpy.data.objects if o.type == "MESH")
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
assert all(abs(skin.matrix_world[i][j] - (i == j)) < 1e-4 for i in range(4) for j in range(4)), "skin expected at identity"


def bone(n):
    return np.array(arm.matrix_world @ arm.data.bones[n].head_local)


# ---- MakeHuman, in the skin's space (cm, Z up, facing -Y) ---------------------------------
Vraw, F, groups, g = [], [], {}, None
for line in open(MH):
    if line.startswith("v "):
        Vraw.append([float(x) for x in line.split()[1:4]])
    elif line.startswith("g "):
        g = line.split()[1]
    elif line.startswith("f "):
        idx = [int(t.split("/")[0]) - 1 for t in line.split()[1:]]
        groups.setdefault(g, set()).update(idx)
        if g == "body":
            F.append(idx)
Vraw = np.array(Vraw)
MV = np.stack([Vraw[:, 0], -Vraw[:, 2], Vraw[:, 1]], 1) * 10.0   # decimetres, Y up, facing +Z


def J(n):
    return MV[sorted(groups["joint-" + n])].mean(0)


MV[:, 2] -= J("ground")[2]


def piece(name, keep):
    """The MakeHuman body faces whose vertices all pass `keep`, subdivided."""
    used = [f for f in F if all(keep[i] for i in f)]
    verts = sorted({i for f in used for i in f})
    remap = {v: i for i, v in enumerate(verts)}
    me = bpy.data.meshes.new(name)
    me.from_pydata(MV[verts].tolist(), [], [[remap[i] for i in f] for f in used])
    obj = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(obj)
    mod = obj.modifiers.new("Subsurf", "SUBSURF")
    mod.levels = mod.render_levels = LOOK["subdiv"]
    dg = bpy.context.evaluated_depsgraph_get()
    obj.data = bpy.data.meshes.new_from_object(obj.evaluated_get(dg))
    obj.modifiers.clear()
    return obj


def coords(obj):
    a = np.zeros(len(obj.data.vertices) * 3)
    obj.data.vertices.foreach_get("co", a)
    return a.reshape(-1, 3)


def similarity(A, B):
    """s, R, t with B ~ s R A + t (Umeyama)."""
    ma, mb = A.mean(0), B.mean(0)
    A0, B0 = A - ma, B - mb
    U, S, Vt = np.linalg.svd(B0.T @ A0)
    D = np.eye(3)
    D[2, 2] = np.sign(np.linalg.det(U @ Vt))
    R = U @ D @ Vt
    s = (S * np.diag(D)).sum() / (A0 ** 2).sum()
    return s, R, mb - s * R @ ma


def min_rotation(a, b):
    a, b = a / np.linalg.norm(a), b / np.linalg.norm(b)
    v, c = np.cross(a, b), float(a @ b)
    if np.linalg.norm(v) < 1e-8:
        return np.eye(3)
    K = np.array([[0, -v[2], v[1]], [v[2], 0, -v[0]], [-v[1], v[0], 0]])
    return np.eye(3) + K + K @ K / (1 + c)


def seg_dist(P, a, b):
    d = b - a
    t = np.clip(((P - a) @ d) / (d @ d), 0, 1)
    return np.linalg.norm(P - (a + t[:, None] * d), axis=1)


# ---- the skin, before cutting: weights and fingertips -------------------------------------
sme = skin.data
SV = coords(skin)
gname = {vg.index: vg.name for vg in skin.vertex_groups}
SW = [{gname[e.group]: e.weight for e in v.groups if e.weight > 0} for v in sme.vertices]


def weight_on(i, names, s=None):
    """Weight on bones starting with any of `names` (twist bones included), on side s."""
    return sum(w for n, w in SW[i].items() if any(n.startswith(k) for k in names) and (s is None or n.endswith("_" + s)))


def q_tip(f, s):
    h3, h2 = bone("%s_03_%s" % (f, s)), bone("%s_02_%s" % (f, s))
    d = (h3 - h2) / np.linalg.norm(h3 - h2)
    gi = skin.vertex_groups["%s_03_%s" % (f, s)].index
    pts = np.array([SV[v.index] for v in sme.vertices if any(e.group == gi and e.weight > 0.5 for e in v.groups)])
    return h3 + d * ((pts - h3) @ d).max()


# ---- graft: bend a piece onto Quinn's bones -----------------------------------------------
def hand(s):
    m = s  # MakeHuman uses the same l / r
    mw, me_ = J("%s-hand" % m), J("%s-elbow" % m)
    n_mh = (mw - me_) / np.linalg.norm(mw - me_)
    side = np.sign(mw[0])
    keep = (np.sign(MV[:, 0]) == side) & ((MV - mw) @ n_mh > -8.0) & (np.linalg.norm(MV - mw, axis=1) < 26.0)
    obj = piece("Hand_" + s, keep)
    P = coords(obj)
    # palm: similarity from MakeHuman's wrist + knuckles to Quinn's
    A = [mw] + [J("%s-finger-%d-1" % (m, k)) for _, k in FINGERS]
    B = [bone("hand_" + s)] + [bone("%s_01_%s" % (f, s)) for f, _ in FINGERS]
    sc, R, t = similarity(np.array(A), np.array(B))
    palm = (sc * R, lambda X: (sc * (R @ X.T)).T + t)
    segs = []   # (finger index, mh a, mh b, q a, linear map, bone)
    for fi, (f, k) in enumerate(FINGERS):
        mj = [J("%s-finger-%d-%d" % (m, k, j)) for j in range(1, 5)]
        qj = [bone("%s_0%d_%s" % (f, j, s)) for j in range(1, 4)] + [q_tip(f, s)]
        for j in range(3):
            dm = sc * R @ (mj[j + 1] - mj[j])
            dq = qj[j + 1] - qj[j]
            rot = min_rotation(dm, dq)
            u = dq / np.linalg.norm(dq)
            stretch = np.eye(3) + (np.linalg.norm(dq) / np.linalg.norm(dm) - 1.0) * np.outer(u, u)
            segs.append((fi, mj[j], mj[j + 1], qj[j], stretch @ rot @ (sc * R), "%s_0%d_%s" % (f, j + 1, s)))
    # distances in MakeHuman space
    d_palm = np.min([seg_dist(P, mw, J("%s-finger-%d-1" % (m, k))) for _, k in FINGERS], axis=0)
    D = np.stack([seg_dist(P, a, b) for _, a, b, *_ in segs], 1)
    near_seg = np.argmin(D, 1)
    palm_wins = d_palm <= D.min(1)
    chain = np.array([segs[i][0] for i in near_seg])
    cand = np.zeros((len(P), len(segs) + 1), bool)
    cand[:, 0] = True
    for i, sg in enumerate(segs):
        own = (chain == sg[0]) & ~palm_wins
        prox = palm_wins & (i % 3 == 0)
        cand[:, i + 1] = own | prox
    Dall = np.concatenate([d_palm[:, None], D], 1)
    Dall[~cand] = 1e9
    Wt = np.exp(-(Dall - Dall.min(1, keepdims=True)) / LOOK["tau"])
    Wt /= Wt.sum(1, keepdims=True)
    out = Wt[:, :1] * palm[1](P)
    for i, (_, a, _, qa, L, _) in enumerate(segs):
        out += Wt[:, i + 1:i + 2] * (qa + ((P - a) @ L.T))
    names = ["hand_" + s] + [sg[5] for sg in segs]
    lower = bone("lowerarm_" + s)
    n = (bone("hand_" + s) - lower) / np.linalg.norm(bone("hand_" + s) - lower)
    return obj, out, Wt, names, bone("hand_" + s) - n * LOOK["hand_cut"], n, s, ("lowerarm", "hand", *(f for f, _ in FINGERS))


def foot(s):
    m = s
    ma, mk = J("%s-ankle" % m), J("%s-knee" % m)
    n_mh = (ma - mk) / np.linalg.norm(ma - mk)
    side = np.sign(ma[0])
    keep = (np.sign(MV[:, 0]) == side) & ((MV - ma) @ n_mh > -(LOOK["foot_cut"] + 7.0)) & (MV[:, 2] < 40)
    obj = piece("Foot_" + s, keep)
    P = coords(obj)
    toes = np.array([J("%s-toe-%d-1" % (m, k)) for k in range(1, 6)])
    mball = toes.mean(0)
    qa, qb = bone("foot_" + s), bone("ball_" + s)

    def frame(o, b):
        f = b - o
        f[2] = 0
        f /= np.linalg.norm(f)
        z = np.array([0, 0, 1.0])
        return np.stack([f, np.cross(z, f), z], 1)
    R = frame(qa, qb) @ frame(ma, mball).T
    sc = np.linalg.norm(qb - qa) / np.linalg.norm(mball - ma)
    out = (sc * (R @ (P - ma).T)).T + qa
    # stand on the same ground as the old feet
    sole = SV[[i for i in range(len(SV)) if weight_on(i, ("foot_" + s, "ball_" + s)) > 0.5]][:, 2].min()
    out[:, 2] += sole - out[:, 2].min()
    # toes -> ball, the rest -> foot, soft across the knuckles
    fdir = R[:, 0]
    knuckle = (sc * (R @ (toes - ma).T)).T + qa
    edge = (out - knuckle.mean(0)) @ fdir
    w_ball = np.clip(0.5 + edge / 2.0, 0, 1)
    Wt = np.stack([1 - w_ball, w_ball], 1)
    calf = bone("calf_" + s)
    n = (qa - calf) / np.linalg.norm(qa - calf)
    return obj, out, Wt, ["foot_" + s, "ball_" + s], qa - n * LOOK["foot_cut"], n, s, ("calf", "foot", "ball")


# ---- cut, match, stitch -------------------------------------------------------------------
kd_skin = KDTree(len(SV))
for i, c in enumerate(SV):
    kd_skin.insert(c, i)
kd_skin.balance()

grafts = []
for part in (hand, foot):
    for s in ("l", "r"):
        grafts.append(part(s))

# 1. skin: delete everything past each cut that belongs to that limb
dead = np.zeros(len(SV), bool)
for obj, out, Wt, names, p, n, side, chain in grafts:
    past = (SV - p) @ n > -LOOK["gap"] / 2
    mine = np.array([weight_on(i, chain, side) > 0.5 for i in range(len(SV))])
    near = np.linalg.norm(SV - p, axis=1) < 40
    dead |= past & mine & near
bm = bmesh.new()
bm.from_mesh(sme)
bm.verts.ensure_lookup_table()
bmesh.ops.delete(bm, geom=[bm.verts[i] for i in np.nonzero(dead)[0]], context="VERTS")
# strays: little islands left behind
bm.to_mesh(sme)
bm.free()
SV2 = coords(skin)


def boundary_loop(mesh_obj, p, n, radius=14.0):
    bm_ = bmesh.new()
    bm_.from_mesh(mesh_obj.data)
    vs = {v for e in bm_.edges if e.is_boundary for v in e.verts}
    co = np.array([tuple(v.co) for v in vs]) if vs else np.zeros((0, 3))
    bm_.free()
    if not len(co):
        return co
    return co[np.linalg.norm(co - p, axis=1) < radius]


# 2. graft pieces: cut at the plane, ease the cross-section into the skin's hole
for gi, (obj, out, Wt, names, p, n, side, chain) in enumerate(grafts):
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.verts.ensure_lookup_table()
    for v, c in zip(bm.verts, out):
        v.co = Vector(c)
    keep = (out - p) @ n > LOOK["gap"] / 2
    bmesh.ops.delete(bm, geom=[v for v, k in zip(bm.verts, keep) if not k], context="VERTS")
    bm.verts.ensure_lookup_table()
    idx = [i for i, k in enumerate(keep) if k]
    Wt_k = Wt[idx]
    co = np.array([tuple(v.co) for v in bm.verts])
    bnd = np.array([tuple(v.co) for v in bm.verts if any(e.is_boundary for e in v.link_edges)])
    hole = boundary_loop(skin, p, n)

    def ring(c):
        ctr = c.mean(0)
        r = c - ctr
        r -= np.outer(r @ n, n)
        return ctr, r

    cg, rg = ring(bnd)
    cs, rs = ring(hole)
    # the cross-section's shape: second moments of each ring; T maps the graft's onto the hole's
    def sqrtm(C):
        w, V = np.linalg.eigh(C)
        return V @ np.diag(np.sqrt(np.maximum(w, 1e-6))) @ V.T
    T = sqrtm(np.cov(rs.T) + np.outer(n, n)) @ np.linalg.inv(sqrtm(np.cov(rg.T) + np.outer(n, n)))
    t = np.clip(1.0 - ((co - p) @ n - LOOK["gap"] / 2) / LOOK["match"], 0, 1)[:, None]
    rel = co - cg
    axial = np.outer(rel @ n, n)
    radial = rel - axial
    fixed = cs + axial + radial @ T.T
    co = (1 - t) * co + t * fixed
    for v, c in zip(bm.verts, co):
        v.co = Vector(c)
    bm.to_mesh(obj.data)
    bm.free()
    # weights: the graft's own, easing into the skin's near the seam (as vertex groups on
    # the piece, so the join merges them by name)
    tb = np.clip(((co - p) @ n - LOOK["gap"] / 2) / LOOK["blend"], 0, 1)
    for i, c in enumerate(co):
        _, j, _ = kd_skin.find(c)
        w = {k: (1 - tb[i]) * v for k, v in SW[j].items()}
        for k, nm in enumerate(names):
            w[nm] = w.get(nm, 0) + tb[i] * Wt_k[i, k]
        top = sorted(w.items(), key=lambda kv: -kv[1])[:4]
        tot = sum(v for _, v in top) or 1.0
        for nm, v in top:
            if v > 1e-3:
                vg = obj.vertex_groups.get(nm) or obj.vertex_groups.new(name=nm)
                vg.add([i], v / tot, "REPLACE")
    grafts[gi] = (obj, p, n)

# 3. join, stitch each seam with a bridge, relax it
base_count = len(skin.data.vertices)
for o in bpy.data.objects:
    o.select_set(False)
for obj, *_ in grafts:
    obj.select_set(True)
skin.select_set(True)
bpy.context.view_layer.objects.active = skin
bpy.ops.object.join()

bm = bmesh.new()
bm.from_mesh(skin.data)
bm.verts.ensure_lookup_table()
shape_layers = list(bm.verts.layers.shape.values())
for _, p, n in grafts:
    pv = Vector(p)
    loops = [e for e in bm.edges if e.is_boundary and (e.verts[0].co - pv).length < 14.0]
    res = bmesh.ops.bridge_loops(bm, edges=loops, use_pairs=False, use_cyclic=False, use_merge=False)
    seam = {v for f in res["faces"] for v in f.verts}
    ring_ = set(seam)
    for _ in range(2):
        ring_ |= {o for v in list(ring_) for e in v.link_edges for o in e.verts}
    for _ in range(LOOK["seam_smooth"]):
        moves = {}
        for v in ring_:
            nb = [e.other_vert(v).co for e in v.link_edges]
            moves[v] = (sum(nb, Vector()) / len(nb) - v.co) * 0.5
        for v, d in moves.items():
            v.co += d
            for sl in shape_layers:
                v[sl] += d
for f in bm.faces:
    f.smooth = True
bm.to_mesh(skin.data)
bm.free()

bpy.context.view_layer.objects.active = skin
bpy.ops.object.mode_set(mode="WEIGHT_PAINT")
bpy.ops.object.vertex_group_limit_total(group_select_mode="ALL", limit=4)
bpy.ops.object.vertex_group_normalize_all(group_select_mode="ALL", lock_active=False)
bpy.ops.object.mode_set(mode="OBJECT")
bm = bmesh.new()
bm.from_mesh(skin.data)
print("skin verts", base_count, "->", len(skin.data.vertices), "open edges left", sum(e.is_boundary for e in bm.edges))
bm.free()

# ---- preview ------------------------------------------------------------------------------
if PREVIEW:
    os.makedirs(PREVIEW, exist_ok=True)
    scn = bpy.context.scene
    scn.render.engine = "BLENDER_WORKBENCH"
    scn.display.shading.light = "STUDIO"
    scn.display.shading.color_type = "SINGLE"
    scn.display.shading.single_color = (0.8, 0.6, 0.5)
    scn.render.resolution_x = scn.render.resolution_y = 900
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    scn.collection.objects.link(cam)
    scn.camera = cam
    cam.data.lens = 85
    for label, target, offset in (("hand_r_side", bone("hand_r") + np.array([-3, -2, -7]), (-45, 0, 0)),
                                  ("hand_r_back", bone("hand_r") + np.array([-3, -2, -7]), (0, -45, 0)),
                                  ("foot_r_side", bone("foot_r") + np.array([0, -6, -4]), (-55, -5, 8)),
                                  ("foot_r_top", bone("foot_r") + np.array([0, -6, -4]), (-10, -40, 35))):
        cam.location = Vector(target) + Vector(offset)
        cam.rotation_euler = (Vector(target) - cam.location).to_track_quat("-Z", "Y").to_euler()
        scn.render.filepath = os.path.join(PREVIEW, label + ".png")
        bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(cam, do_unlink=True)

# ---- export -------------------------------------------------------------------------------
bpy.ops.wm.save_as_mainfile(filepath=os.path.splitext(OUT)[0] + ".blend")   # Tools/blender_outfits.py starts here
bpy.ops.object.select_all(action="DESELECT")
arm.select_set(True)
skin.select_set(True)
bpy.ops.export_scene.fbx(filepath=OUT, use_selection=True, object_types={"ARMATURE", "MESH"},
                         add_leaf_bones=False, use_armature_deform_only=False,
                         mesh_smooth_type="FACE", bake_anim=False, axis_forward="-Z", axis_up="Y")
print("wrote", OUT)
