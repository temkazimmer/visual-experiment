"""A face for the android skin: MakeHuman's head (CC0) in place of the blank egg.
Headless, after Tools/blender_torso.py:

  /Applications/Blender.app/Contents/MacOS/Blender -b <android_skin.blend> \
      --python Tools/blender_face.py -- <makehuman base.obj> <out.fbx> [<preview_dir>]

Few features, calm ones: ears, nose, lips, and eyes that are only suggested. Per step:
1. cut the head out of the MakeHuman mesh. Its eye sockets are pockets behind the lids;
   each is replaced by a smooth dome (a blank, statue eye) set just behind the lids,
2. subdivide (Catmull-Clark), then relax the whole face so small forms melt away; ears,
   nose and lips are big enough to survive, the lids become a soft line,
3. scale and place it where the egg was (same crown, same back of the skull),
4. cut the skin across the neck, match the cross-section, bridge and relax the seam,
5. weights: face and skull follow the head bone; under the jaw they ease into the skin's
   own neck weights.
Shape keys survive (new vertices sit still in every key). Writes <out>.fbx and <out>.blend.
"""
import os
import sys
from math import hypot

import bmesh
import bpy
import numpy as np
from mathutils import Vector
from mathutils.kdtree import KDTree

args = sys.argv[sys.argv.index("--") + 1:]
MH, OUT = args[0], args[1]
PREVIEW = args[2] if len(args) > 2 else None

LOOK = dict(
    subdiv=2,            # Catmull-Clark levels on the MakeHuman head
    scale=0.96,          # MakeHuman head size against the egg (crown and back of skull kept)
    cut=154.3,           # height where the neck is cut (cm), between neck_01 and neck_02
    gap=0.5,             # cm between the two cut edges, bridged
    match=3.5,           # cm over which the head's neck eases into the skin's cross-section
    blend=2.0,           # cm over which weights ease from the skin's into the head's
    seam_smooth=24,      # relax iterations around the seam
    seam_rings=7,        # how many edge rings each side of the seam are relaxed
    eye_radius=1.0,      # dome radius, times MakeHuman's eyeball
    eye_set=0.12,        # cm the dome sits back behind the lids
    soften=10,           # relax passes over the whole face (0 = MakeHuman's own detail)
    soften_eyes=14,      # extra passes around the eyes, so they stay subtle
    soften_strength=0.5,
    # the stylised turn (Tema's reference: 90s anime, but only halfway there)
    jaw_taper=0.20,      # how much narrower the face gets from the cheekbones to the chin
    chin_drop=0.5,       # cm the chin point is pulled down, for a sharper V
    nose_shrink=0.22,    # nose stands this much less proud
    eye_grow=1.25,       # eyes and lids, scaled about each eye
    # elf ears: the whole ear drawn out along its length, so the rim (helix), the inner
    # fold (antihelix) and the bowl run on up into a leaf-shaped point (Tema's references)
    ear_stretch=1.5,     # the upper ear's length, times the human one
    ear_start=0.05,      # where the stretch starts, as a fraction of the ear above its centre
    ear_back=28.0,       # degrees the drawn-out part sweeps back
    ear_out=1.6,         # cm the tip leans away from the skull
    ear_taper=0.7,       # how far the ear narrows toward its tip (1 = to a needle)
    ear_relax=6,         # relax passes over the drawn-out part, so stretched folds don't streak
)

skin = next(o for o in bpy.data.objects if o.type == "MESH")
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
assert all(abs(skin.matrix_world[i][j] - (i == j)) < 1e-4 for i in range(4) for j in range(4)), "skin expected at identity"


def bone(n):
    return np.array(arm.matrix_world @ arm.data.bones[n].head_local)


def coords(obj):
    a = np.zeros(len(obj.data.vertices) * 3)
    obj.data.vertices.foreach_get("co", a)
    return a.reshape(-1, 3)


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
MV = np.stack([Vraw[:, 0], -Vraw[:, 2], Vraw[:, 1]], 1) * 10.0


def J(n):
    return MV[sorted(groups["joint-" + n])].mean(0)


MV[:, 2] -= J("ground")[2]

# ---- the skin's head, before cutting ------------------------------------------------------
sme = skin.data
SV = coords(skin)
gname = {vg.index: vg.name for vg in skin.vertex_groups}
SW = [{gname[e.group]: e.weight for e in v.groups if e.weight > 0} for v in sme.vertices]
head_w = np.array([w.get("head", 0.0) for w in SW])
egg = SV[head_w > 0.5]
egg_top, egg_back = egg[:, 2].max(), egg[:, 1].max()

# ---- 1. the head piece, eyes filled -------------------------------------------------------
neck_z = J("neck")[2]
keep = (MV[:, 2] > neck_z - 2.0) & (np.abs(MV[:, 0]) < 12.0)
used = [f for f in F if all(keep[i] for i in f)]
verts = sorted({i for f in used for i in f})
remap = {v: i for i, v in enumerate(verts)}
me = bpy.data.meshes.new("Head")
me.from_pydata(MV[verts].tolist(), [], [[remap[i] for i in f] for f in used])
head = bpy.data.objects.new("Head", me)
bpy.context.collection.objects.link(head)

bm = bmesh.new()
bm.from_mesh(me)
bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
bm.normal_update()
eyes = []
for side in ("l", "r"):
    c = MV[sorted(groups["helper-%s-eye" % side])]
    ctr, r = c.mean(0), float((c.max(0) - c.min(0))[[0, 2]].mean() / 2) * LOOK["eye_radius"]
    eyes.append((ctr, r))
    cv = Vector(ctr)
    # the socket: vertices near the eye whose surface looks inward, at the eye's centre
    bag = [v for v in bm.verts if (v.co - cv).length < r * 1.7 and v.normal.dot((cv - v.co).normalized()) > 0.25]
    bmesh.ops.delete(bm, geom=bag, context="VERTS")
    rim_edges = [e for e in bm.edges if e.is_boundary and (e.verts[0].co - cv).length < r * 2.2]
    # order the rim into one loop
    loop, e = [rim_edges[0].verts[0]], rim_edges[0]
    left = set(rim_edges)
    while e:
        left.discard(e)
        nxt = e.other_vert(loop[-1])
        if nxt is loop[0]:
            break
        loop.append(nxt)
        e = next((x for x in nxt.link_edges if x in left), None)
    assert len(loop) == len(rim_edges), "eye rim is not one loop (%d of %d)" % (len(loop), len(rim_edges))
    mid = sum((v.co for v in loop), Vector()) / len(loop)

    def dome(p):
        """p pulled onto the front of the eyeball, a little behind the lids."""
        dx, dz = p.x - cv.x, p.z - cv.z
        rho = min(hypot(dx, dz), r * 0.96)
        k = rho / max(hypot(dx, dz), 1e-6)
        return Vector((cv.x + dx * k, cv.y - (r * r - rho * rho) ** 0.5 + LOOK["eye_set"], cv.z + dz * k))

    rings = [loop]
    for t in (0.4, 0.75):
        rings.append([bm.verts.new(dome(v.co.lerp(mid, t))) for v in loop])
    centre = bm.verts.new(dome(mid))
    n = len(loop)
    for a, b in zip(rings, rings[1:]):
        for i in range(n):
            bm.faces.new((a[i], a[(i + 1) % n], b[(i + 1) % n], b[i]))
    for i in range(n):
        bm.faces.new((rings[-1][i], rings[-1][(i + 1) % n], centre))
bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
bm.to_mesh(me)
bm.free()

# ---- 2. subdivide, then melt the small forms ----------------------------------------------
mod = head.modifiers.new("Subsurf", "SUBSURF")
mod.levels = mod.render_levels = LOOK["subdiv"]
head.data = bpy.data.meshes.new_from_object(head.evaluated_get(bpy.context.evaluated_depsgraph_get()))
head.modifiers.clear()

bm = bmesh.new()
bm.from_mesh(head.data)
bm.verts.ensure_lookup_table()
P = np.array([tuple(v.co) for v in bm.verts])
nb = [[e.other_vert(v).index for e in v.link_edges] for v in bm.verts]
edge = np.array([any(e.is_boundary for e in v.link_edges) for v in bm.verts])
near_eye = np.min([np.linalg.norm(P - c, axis=1) / r for c, r in eyes], axis=0)
eye_w = np.clip((2.6 - near_eye) / 1.2, 0, 1)            # 1 on the eye and lids, 0 by the brow
# ears keep their folds: they stand off the side of the skull
ear_w = np.clip((np.abs(P[:, 0]) - 6.9) / 0.8, 0, 1) * (P[:, 1] > -8.0)
for it in range(max(LOOK["soften"], LOOK["soften_eyes"])):
    avg = np.array([P[n_].mean(0) if n_ else P[i] for i, n_ in enumerate(nb)])
    amount = np.where(it < LOOK["soften"], 1.0, eye_w) * (1 - 0.8 * ear_w) * ~edge
    P = P + (avg - P) * (LOOK["soften_strength"] * amount)[:, None]
for v, c in zip(bm.verts, P):
    v.co = Vector(c)
bm.to_mesh(head.data)
bm.free()

# ---- 3. where the egg was -----------------------------------------------------------------
P = coords(head)
s = LOOK["scale"]
P = P * s
P[:, 2] += egg_top - P[:, 2].max()
back = P[np.abs(P[:, 0]) < 5.0][:, 1].max()
P[:, 1] += egg_back - back
shift = np.array([0.0, egg_back - back, P[:, 2].max() - (coords(head)[:, 2] * s).max()])

# ---- 3b. the stylised turn ----------------------------------------------------------------
def smooth(x):
    x = np.clip(x, 0, 1)
    return x * x * (3 - 2 * x)


eye_c = [c * s + shift for c, _ in eyes]
eye_r = eyes[0][1] * s
eye_z = float(np.mean([c[2] for c in eye_c]))
front = P[:, 1] < P[:, 1].min() + 5.0
chin_z = float(P[front][:, 2].min())
is_ear = ear_w > 0.5
# eyes a little larger
for c in eye_c:
    d = np.linalg.norm(P - c, axis=1)
    w = np.exp(-(d / (2.4 * eye_r)) ** 2) * ~is_ear
    P[:, [0, 2]] += (P[:, [0, 2]] - c[[0, 2]]) * ((LOOK["eye_grow"] - 1) * w)[:, None]
# the nose stands less proud
tip = P[np.argmin(P[:, 1])].copy()
w = np.exp(-(np.linalg.norm(P - tip, axis=1) / 2.6) ** 2)
P[:, 1] += (np.percentile(P[front][:, 1], 60) - P[:, 1]).clip(0, None) * LOOK["nose_shrink"] * w
# jaw narrows to a small chin; the back of the skull and the ears stay
down = smooth((eye_z - 1.0 - P[:, 2]) / (eye_z - 1.0 - chin_z))
fy = smooth((2.0 - P[:, 1]) / 6.0) * smooth((P[:, 2] - (chin_z - 2.5)) / 2.0)
P[:, 0] *= 1 - LOOK["jaw_taper"] * down * fy * ~is_ear
w = np.exp(-((P[:, 0] / 2.2) ** 2 + ((P[:, 2] - chin_z) / 2.0) ** 2)) * front
P[:, 2] -= LOOK["chin_drop"] * w
# ears: the whole ear drawn out into a leaf-shaped point, swept back
for side in (-1.0, 1.0):
    # only what stands off the skull: |x| against the skull's own width at that spot (the
    # mean of the nearest plain-skull vertices in side view), so the skull around the root stays
    sk = np.nonzero((P[:, 0] * side > 0) & ~is_ear & (np.abs(P[:, 0]) > 3.0))[0]
    kd = KDTree(len(sk))
    for j, i in enumerate(sk):
        kd.insert((0.0, P[i, 1], P[i, 2]), j)
    kd.balance()
    cand = np.nonzero(is_ear & (P[:, 0] * side > 0))[0]
    off = np.array([abs(P[i, 0]) - np.mean([abs(P[sk[j], 0]) for _, j, _ in kd.find_n((0.0, P[i, 1], P[i, 2]), 8)])
                    for i in cand])
    keep_ = smooth((off - 0.25) / 0.6)
    m = np.zeros(len(P), bool)
    m[cand[keep_ > 0]] = True
    E = P[m].copy()
    we = keep_[keep_ > 0]
    core = E[we > 0.9]
    cy, cz = (core[:, 1].min() + core[:, 1].max()) / 2, (core[:, 2].min() + core[:, 2].max()) / 2
    h, v = E[:, 2] - cz, E[:, 1] - cy
    H = h.max()
    h0 = LOOK["ear_start"] * H
    t = np.clip((h - h0) / (H - h0), 0, 1)
    # the ear's own centre line at each height, so it narrows about itself
    bins = np.linspace(h.min(), H, 24)
    k_ = np.clip(np.digitize(h, bins) - 1, 0, 22)
    mids = np.array([(v[k_ == b].min() + v[k_ == b].max()) / 2 if (k_ == b).any() else np.nan for b in range(23)])
    ok = ~np.isnan(mids)
    mid = np.interp(h, (bins[:-1] + bins[1:])[ok] / 2, mids[ok])
    v2 = mid + (v - mid) * (1 - LOOK["ear_taper"] * t ** 2.2)
    h2 = np.where(h > h0, h0 + (h - h0) * (1 + (LOOK["ear_stretch"] - 1) * t ** 0.7), h)
    # sweep back about the start of the stretch (front is -Y, so back is +Y)
    a_ = np.radians(LOOK["ear_back"]) * smooth(t)
    y2 = (v2 - mid * 0) * np.cos(a_) + (h2 - h0) * np.sin(a_)
    z2 = h0 + (h2 - h0) * np.cos(a_) - v2 * np.sin(a_)
    N = E.copy()
    N[:, 1] = cy + np.where(h > h0, y2, v2)
    N[:, 2] = cz + np.where(h > h0, z2, h2)
    N[:, 0] += side * LOOK["ear_out"] * t ** 2
    P[m] = E + (N - E) * we[:, None]
    tw = np.zeros(len(P))
    tw[np.nonzero(m)[0]] = smooth(t * 1.4) * we
    for _ in range(LOOK["ear_relax"]):
        idx = np.nonzero(tw > 0.01)[0]
        avg = np.array([P[nb[i]].mean(0) for i in idx])
        P[idx] += (avg - P[idx]) * (0.5 * tw[idx])[:, None]
    tip = P[m][np.argmax(P[m][:, 2])]
    print("ear %+d: tip at x %.1f y %.1f z %.1f (centre y %.1f z %.1f)" % (side, *tip, cy, cz))
print("head: chin z %.1f, nose y %.1f, crown %.1f, half width %.1f" % (
    P[P[:, 1] < P[:, 1].min() + 4][:, 2].min(), P[:, 1].min(), P[:, 2].max(), np.abs(P[:, 0]).max()))

# the ears are marked, for Tools/blender_hair.py: hair passes over them
ear_mark = np.zeros(len(P))
ear_mark[is_ear] = 1.0

# ---- 4. cut, match, stitch ----------------------------------------------------------------
n = bone("head") - bone("neck_01")
n /= np.linalg.norm(n)
p = np.array([0.0, bone("neck_02")[1], LOOK["cut"]])

kd_skin = KDTree(len(SV))
for i, c in enumerate(SV):
    kd_skin.insert(c, i)
kd_skin.balance()

past = (SV - p) @ n > -LOOK["gap"] / 2
mine = np.array([sum(w for k, w in SW[i].items() if k in ("head", "neck_01", "neck_02")) > 0.5 for i in range(len(SV))])
dead = past & mine
bm = bmesh.new()
bm.from_mesh(sme)
bm.verts.ensure_lookup_table()
bmesh.ops.delete(bm, geom=[bm.verts[i] for i in np.nonzero(dead)[0]], context="VERTS")
hole = np.array([tuple(v.co) for e in bm.edges if e.is_boundary for v in e.verts])
bm.to_mesh(sme)
bm.free()
assert len(hole), "the neck cut left no open edge"

bm = bmesh.new()
bm.from_mesh(head.data)
bm.verts.ensure_lookup_table()
keep = (P - p) @ n > LOOK["gap"] / 2
bmesh.ops.delete(bm, geom=[v for v, k in zip(bm.verts, keep) if not k], context="VERTS")
bm.verts.ensure_lookup_table()
co = P[keep]
bnd = np.array([co[v.index] for v in bm.verts if any(e.is_boundary for e in v.link_edges)])


# the neck's outline around its axis, as radius by angle: the head's is eased into the skin's
ax1 = np.cross(n, [1.0, 0, 0])
ax1 /= np.linalg.norm(ax1)
ax2 = np.cross(n, ax1)


def outline(c, ctr):
    """Radius by angle (72 bins, gaps filled, lightly smoothed) of a loop around the axis."""
    rr = c - ctr
    ang = np.arctan2(rr @ ax2, rr @ ax1)
    rad = np.hypot(rr @ ax1, rr @ ax2)
    bins = ((ang + np.pi) / (2 * np.pi) * 72).astype(int) % 72
    prof = np.array([rad[bins == k].mean() if (bins == k).any() else np.nan for k in range(72)])
    ok = ~np.isnan(prof)
    prof = np.interp(np.arange(72), np.nonzero(ok)[0], prof[ok], period=72)
    for _ in range(2):
        prof = (np.roll(prof, 1) + 2 * prof + np.roll(prof, -1)) / 4
    return prof


cg, cs = bnd.mean(0), hole.mean(0)
cs = cs + n * ((cg - cs) @ n)                      # same height on the axis as the head's rim
ratio = outline(hole, cs) / outline(bnd, cg)
along = (co - p) @ n - LOOK["gap"] / 2
t = np.clip(1.0 - along / LOOK["match"], 0, 1)
t = t * t * (3 - 2 * t)
rel = co - cg
ang = np.arctan2(rel @ ax2, rel @ ax1)
k = np.interp((ang + np.pi) / (2 * np.pi) * 72 - 0.5, np.arange(72), ratio, period=72)
axial = np.outer(rel @ n, n)
grow = (1 + t * (k - 1))[:, None]
co = cg + t[:, None] * (cs - cg) + axial + (rel - axial) * grow
for v, c in zip(bm.verts, co):
    v.co = Vector(c)
for f in bm.faces:
    f.smooth = True
bm.to_mesh(head.data)
bm.free()
head.data.attributes.new("ear", "FLOAT", "POINT").data.foreach_set("value", ear_mark[keep])

# ---- 5. weights ---------------------------------------------------------------------------
# face and skull -> head; low and behind, the skin's own neck weights
face = np.clip((co[:, 2] - 155.0) / 7.0 + (-co[:, 1] - 2.0) / 8.0, 0, 1)
tb = np.clip(along / LOOK["blend"], 0, 1)
for i, c in enumerate(co):
    _, j, _ = kd_skin.find(c)
    k = face[i] * tb[i]
    w = {nm: (1 - k) * v for nm, v in SW[j].items()}
    w["head"] = w.get("head", 0) + k
    top = sorted(w.items(), key=lambda kv: -kv[1])[:4]
    tot = sum(v for _, v in top) or 1.0
    for nm, v in top:
        if v > 1e-3:
            vg = head.vertex_groups.get(nm) or head.vertex_groups.new(name=nm)
            vg.add([i], v / tot, "REPLACE")

base_count = len(skin.data.vertices)
for o in bpy.data.objects:
    o.select_set(False)
head.select_set(True)
skin.select_set(True)
bpy.context.view_layer.objects.active = skin
bpy.ops.object.join()

bm = bmesh.new()
bm.from_mesh(skin.data)
bm.verts.ensure_lookup_table()
shape_layers = list(bm.verts.layers.shape.values())
pv = Vector(p)
loops = [e for e in bm.edges if e.is_boundary and (e.verts[0].co - pv).length < 14.0]
res = bmesh.ops.bridge_loops(bm, edges=loops, use_pairs=False, use_cyclic=False, use_merge=False)
ring_ = {v for f in res["faces"] for v in f.verts}
for _ in range(LOOK["seam_rings"]):
    ring_ |= {o for v in list(ring_) for e in v.link_edges for o in e.verts}
for _ in range(LOOK["seam_smooth"]):
    moves = {}
    for v in ring_:
        around = [e.other_vert(v).co for e in v.link_edges]
        moves[v] = (sum(around, Vector()) / len(around) - v.co) * 0.5
    for v, d in moves.items():
        v.co += d
        for sl in shape_layers:
            v[sl] += d
for f in bm.faces:
    f.smooth = True
bm.to_mesh(skin.data)
print("skin verts", base_count, "->", len(skin.data.vertices), "open edges left", sum(e.is_boundary for e in bm.edges))
bm.free()

bpy.context.view_layer.objects.active = skin
bpy.ops.object.mode_set(mode="WEIGHT_PAINT")
bpy.ops.object.vertex_group_limit_total(group_select_mode="ALL", limit=4)
bpy.ops.object.vertex_group_normalize_all(group_select_mode="ALL", lock_active=False)
bpy.ops.object.mode_set(mode="OBJECT")

# ---- preview ------------------------------------------------------------------------------
if PREVIEW:
    os.makedirs(PREVIEW, exist_ok=True)
    scn = bpy.context.scene
    scn.render.engine = "BLENDER_WORKBENCH"
    sh = scn.display.shading
    sh.light = "STUDIO"
    sh.color_type = "SINGLE"
    sh.single_color = (0.8, 0.6, 0.5)
    sh.show_cavity = True
    sh.cavity_type = "BOTH"
    sh.show_shadows = True
    scn.display.light_direction = (0.6, 0.3, 0.74)
    scn.render.resolution_x, scn.render.resolution_y = 900, 1000
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    scn.collection.objects.link(cam)
    scn.camera = cam
    cam.data.lens = 85
    target = (0, 0, 166)
    for label, offset in (("front", (0, -95, 0)), ("three_quarter", (55, -78, 4)), ("side", (95, 0, 0)),
                          ("back", (-30, 90, 6)), ("bust", (30, -170, 0))):
        tg = Vector(target) - (Vector((0, 0, 14)) if label == "bust" else Vector())
        cam.location = tg + Vector(offset)
        cam.rotation_euler = (tg - cam.location).to_track_quat("-Z", "Y").to_euler()
        scn.render.filepath = os.path.join(PREVIEW, "face_%s.png" % label)
        bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(cam, do_unlink=True)

# ---- export -------------------------------------------------------------------------------
bpy.ops.wm.save_as_mainfile(filepath=os.path.splitext(OUT)[0] + ".blend")   # Tools/blender_outfit.py starts here
bpy.ops.object.select_all(action="DESELECT")
arm.select_set(True)
skin.select_set(True)
bpy.ops.export_scene.fbx(filepath=OUT, use_selection=True, object_types={"ARMATURE", "MESH"},
                         add_leaf_bones=False, use_armature_deform_only=False,
                         mesh_smooth_type="FACE", bake_anim=False, axis_forward="-Z", axis_up="Y")
print("wrote", OUT)
