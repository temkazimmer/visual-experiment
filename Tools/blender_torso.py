"""The middle of the body, front and back, sculpted onto the android skin.
Headless, after Tools/blender_extremities.py:

  /Applications/Blender.app/Contents/MacOS/Blender -b <android_skin.blend> \
      --python Tools/blender_torso.py -- <out.fbx> [<preview_dir>]

The remeshed skin is a smooth tube between ribs and hips. This adds what a woman's torso
shows (see Docs/CHARACTER_DIRECTION.md, "Middle body"):
- first, the hips: the remesh seams are relaxed and pelvis/thigh weights blended over a
  wide band, so the leg meets the body in a curve and not a hinge.
- always there: a navel (vertical oval under a small hood, in a shallow dish), a faint
  midline above it, the rounded low belly below it, the spinal furrow (deepest in the lumbar
  curve) between two soft columns, and the two dimples over the pelvis.
- shape keys, driven by sliders: Fat (a soft layer over belly, flanks and low back), AbsLines (midline, the rectus' side lines, the
  lines from hip bone to pubis), AbsPack (the three cross lines above the navel and the
  blocks between them), ThighGap (inner thighs pulled apart, stance unchanged).
The belly is subdivided first so the grooves have vertices to live on.
Everything is in cm, character facing -Y. Writes <out>.fbx and <out>.blend.
"""
import os
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
OUT = args[0]
PREVIEW = args[1] if len(args) > 1 else None

LOOK = dict(
    navel_z=106.5,       # height of the navel: xiphoid 127, pubis 90, upper part 1.25x the lower
    navel_depth=1.1,     # the pit
    navel_w=0.5,        # its half width and half height (a vertical oval)
    navel_h=1.0,
    hood=0.14,           # the lip over its top
    dish=0.22,           # the shallow hollow around it
    midline=0.12,        # midline groove that is always there, above the navel
    low_belly=2.0,       # the low belly: a soft rounded pad below the navel (subumbilical fat;
                         # Motus Woman and real anatomy carry it, abs never segment below the navel)
    furrow=1.3,         # spinal furrow at its deepest (lumbar)
    furrow_upper=0.2,    # and between the shoulder blades
    columns=0.45,        # the two muscle columns beside it
    dimple=0.6,          # dimples over the pelvis
    dimple_x=4.6, dimple_z=102.5,
    fat=2.0,             # Fat: how far the belly comes out at full
    lines=0.8,          # AbsLines: midline depth; the side lines are a bit shallower
    rectus=6.3,          # half width of the rectus (where its side line runs) above the navel
    pack=0.35,            # AbsPack: depth of the cross lines and height of the blocks
    gap=1.6,             # ThighGap: how far each inner thigh moves
    weld=0.16,           # seam slivers shorter than this are welded away
    seam_passes=14,      # smoothing passes that relax the remesh seams, knees to ribs
    key_smooth=12,       # passes smoothing the earlier keys' offsets after the sliver weld
    joint_passes=90,     # weight-diffusion passes across the hip (about 3 cm of blend)
    mons=0.55,           # the pubic mound: how far it stands out
    cleft=0.22,          # the shallow line at its lower end
    groin=0.09,          # the soft crease from hip bone to pubis, always there
    clavicle=0.3,       # landmarks (cm): the collarbone ridge
    clavicle_hollow=0.16,  # and the hollow above it
    notch=0.4,          # the dip between the collarbones' inner ends
    deltoid=0.35,        # the shoulder cap
    asis=0.2,            # front hip points
    kneecap=0.4,
    knee_hollow=0.14,
    ankle_knob=0.25,
    achilles=0.22,       # the grooves either side of the tendon
)

skin = next(o for o in bpy.data.objects if o.type == "MESH")
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
ZN = LOOK["navel_z"]

# the mannequin's stored normals no longer match the remeshed surface (they show as crinkled
# seams); drop them. Unreal computes its own on import.
bpy.context.view_layer.objects.active = skin
if skin.data.has_custom_normals:
    bpy.ops.mesh.customdata_custom_splitnormals_clear()

# ---- give the belly enough vertices ---------------------------------------------------------
bm = bmesh.new()
bm.from_mesh(skin.data)
before = len(bm.verts)


def refine(inside, cuts, facing=-1):
    bm.faces.ensure_lookup_table()
    edges = {e for f in bm.faces if f.normal.y * facing > 0.15 and inside(f.calc_center_median()) for e in f.edges}
    bmesh.ops.subdivide_edges(bm, edges=list(edges), cuts=cuts, use_grid_fill=True)


# the remesh seams are rows of sliver triangles: weld them away first (knees to ribs, not the hands)
dl = bm.verts.layers.deform.active
okg = {i for i, vg in enumerate(skin.vertex_groups)
       if vg.name == "pelvis" or vg.name.startswith(("spine_", "thigh_", "calf_"))}
weld = [v for v in bm.verts if 40 < v.co.z < 131 and all(gi in okg or w < 0.01 for gi, w in v[dl].items())]
bmesh.ops.remove_doubles(bm, verts=weld, dist=LOOK["weld"])
print("slivers welded:", before, "->", len(bm.verts))
bm.normal_update()
refine(lambda c: abs(c.x) < 13.5 and 90 < c.z < 130, 1)
bm.normal_update()
refine(lambda c: abs(c.x) < 4.5 and 82 < c.z < 95, 1)
bm.normal_update()
refine(lambda c: abs(c.x) < 8 and 98 < c.z < 128, 1, facing=1)       # the lumbar furrow and dimples
bm.normal_update()
refine(lambda c: (c.x / 2.6) ** 2 + ((c.z - ZN) / 3.0) ** 2 < 1, 2)
bm.normal_update()
refine(lambda c: abs(c.x) < 16 and 141 < c.z < 153, 1)                 # collarbones and the notch
bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 4])
bm.to_mesh(skin.data)
bm.free()
print("belly refined:", before, "->", len(skin.data.vertices))

# ---- fields ---------------------------------------------------------------------------------
mesh = skin.data
n = len(mesh.vertices)
P = np.empty(n * 3)
mesh.vertices.foreach_get("co", P)
P = P.reshape(-1, 3)
Nn = np.empty(n * 3)
mesh.vertices.foreach_get("normal", Nn)
Nn = Nn.reshape(-1, 3)
E = np.empty(len(mesh.edges) * 2, dtype=np.int32)
mesh.edges.foreach_get("vertices", E)
E = E.reshape(-1, 2)

# ---- hips: relax the remesh seams, then blend pelvis and thighs over a wide band ------------
# The remesh left crinkled seam lines (groin, hips, belly, thighs) and the weights step from
# pelvis to thigh within a centimetre there, so a raised leg folded like a hinge.
names = [vg.name for vg in skin.vertex_groups]
BODY = [i for i, nm in enumerate(names) if nm == "pelvis" or nm.startswith(("spine_", "thigh_", "calf_"))]
col = {gi: c for c, gi in enumerate(BODY)}
W = np.zeros((n, len(BODY)))
other = np.zeros(n)
for v in mesh.vertices:
    for ge in v.groups:
        if ge.group in col:
            W[v.index, col[ge.group]] = ge.weight
        else:
            other[v.index] += ge.weight
body = other < 0.01                     # not the hands hanging beside the hips


def diffuse(A, mask, passes, lam=None):
    """Neighbour averaging on the masked vertices; lam = (l, m) makes it Taubin (no shrink)."""
    idx = np.nonzero(mask > 1e-3)[0]
    loc = -np.ones(n, dtype=np.int64)
    loc[idx] = np.arange(len(idx))
    e = E[(loc[E[:, 0]] >= 0) & (loc[E[:, 1]] >= 0)]
    i0, i1 = loc[e[:, 0]], loc[e[:, 1]]
    deg = np.maximum(np.bincount(i0, minlength=len(idx)) + np.bincount(i1, minlength=len(idx)), 1)[:, None]
    m = mask[idx][:, None]
    a = A[idx].copy()
    for it in range(passes):
        acc = np.zeros_like(a)
        for c in range(a.shape[1]):
            acc[:, c] = np.bincount(i0, a[i1, c], len(idx)) + np.bincount(i1, a[i0, c], len(idx))
        f = 1.0 if lam is None else lam[it % 2]
        a += m * f * (acc / deg - a)
    out = A.copy()
    out[idx] = a
    return out


zz = P[:, 2]
# seam vertices: where slivers meet big triangles (longest edge over shortest > 4), plus 3 rings
el = np.linalg.norm(P[E[:, 0]] - P[E[:, 1]], axis=1)
lo, hi = np.full(n, 1e9), np.zeros(n)
for k in (0, 1):
    np.minimum.at(lo, E[:, k], el)
    np.maximum.at(hi, E[:, k], el)
seam = (hi / lo > 4).astype(float)
for _ in range(3):
    grow = seam.copy()
    np.maximum.at(grow, E[:, 0], seam[E[:, 1]])
    np.maximum.at(grow, E[:, 1], seam[E[:, 0]])
    seam = grow
region = body * np.clip((zz - 40) / 6, 0, 1) * np.clip((131 - zz) / 6, 0, 1)
relaxed = diffuse(P, region, 6, lam=(0.55, -0.58))                  # a light pass everywhere
relaxed = diffuse(relaxed, region * (0.15 + 0.85 * seam), LOOK["seam_passes"])   # and a real one on the seams
relax = relaxed - P
print("seams relaxed: %d seam verts, mean move %.3f, max %.2f" % (
    int((seam * region > 0.5).sum()), np.linalg.norm(relax, axis=1)[region > 0.5].mean(), np.linalg.norm(relax, axis=1).max()))
P = relaxed

joint = body * np.clip((zz - 72) / 8, 0, 1) * np.clip((110 - zz) / 8, 0, 1)
W2 = diffuse(W, joint, LOOK["joint_passes"])
W2 /= np.maximum(W2.sum(1, keepdims=True), 1e-6)
changed = np.nonzero((np.abs(W2 - W).max(1) > 1e-3) & (joint > 1e-3))[0]
for c, gi in enumerate(BODY):
    vg = skin.vertex_groups[gi]
    for i in changed:
        w = float(W2[i, c])
        if w > 0.004:
            vg.add([int(i)], w, "REPLACE")
        elif W[i, c] > 0:
            vg.remove([int(i)])
print("hip weights blended on", len(changed), "verts")

for _ in range(16):                     # smooth the normals: the remesh left rough seams, and pushing
    acc = Nn.copy()                     # along those tears the surface
    np.add.at(acc, E[:, 0], Nn[E[:, 1]])
    np.add.at(acc, E[:, 1], Nn[E[:, 0]])
    Nn = acc / np.linalg.norm(acc, axis=1, keepdims=True)
x, y, z = P.T
ax = np.abs(x)
sgn = np.where(x >= 0, 1.0, -1.0)


def ramp(v, a, b):
    t = np.clip((v - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def band(v, lo, hi, soft):
    return ramp(v, lo - soft, lo + soft) * (1 - ramp(v, hi - soft, hi + soft))


def g(d, s):
    return np.exp(-(d / s) ** 2)


torso = (ax < 17).astype(float)
front = ramp(-Nn[:, 1], 0.1, 0.5) * torso * band(z, 86, 134, 2)
back = ramp(Nn[:, 1], 0.1, 0.5) * torso * band(z, 92, 142, 2)
IN = np.array([0.0, 1.0, 0.0])          # into the belly


def along(f):                           # out along the surface normal
    return Nn * f[:, None]


pit = g(np.hypot(x / LOOK["navel_w"], (z - ZN) / LOOK["navel_h"]), 1.0) * front
xs = LOOK["rectus"] - np.clip((ZN - z) / (ZN - 92.0), 0, 1) * 2.8      # the rectus narrows to the pubis

# always there
base = np.zeros((n, 3))
base += IN * (pit * LOOK["navel_depth"])[:, None]
base += IN * (g(np.hypot(x / 3.0, (z - ZN) / 3.5), 1.0) * front * LOOK["dish"])[:, None]
base += along(g(np.hypot(x / 0.9, (z - ZN - 1.15) / 0.45), 1.0) * front * LOOK["hood"])
base -= along(g(x, 1.3) * band(z, ZN + 1.5, 127, 2) * front * LOOK["midline"])
base += along(g(np.hypot(x / 9.5, (z - 99.5) / 7.0), 1.0) * front * (1 - pit) * LOOK["low_belly"])
furrow = LOOK["furrow_upper"] * band(z, 104, 141, 3) + (LOOK["furrow"] - LOOK["furrow_upper"]) * band(z, 104, 122, 4)
base -= along(g(x, 1.8) * furrow * back)
base += along(g(ax - 3.6, 2.0) * band(z, 104, 124, 4) * back * LOOK["columns"])
base -= along(g(np.hypot(ax - LOOK["dimple_x"], z - LOOK["dimple_z"]), 1.1) * back * LOOK["dimple"])

# landmarks: what an anime figure's sculptor keys the anatomy on, crisp and small
# (Docs/Research/ANATOMY.md, "Surface landmarks"). Positions from the skeleton, in cm.
# (masks and pushes on smoothed normals: the raw ones crinkled the fine shapes)
deg_l = np.zeros(n)
np.add.at(deg_l, E[:, 0], 1.0)
np.add.at(deg_l, E[:, 1], 1.0)
deg_l = np.maximum(deg_l, 1.0)
Nl = Nn.copy()
for _ in range(12):
    acc = np.zeros_like(Nl)
    np.add.at(acc, E[:, 0], Nl[E[:, 1]])
    np.add.at(acc, E[:, 1], Nl[E[:, 0]])
    Nl = 0.5 * Nl + 0.5 * acc / deg_l[:, None]
Nl /= np.maximum(np.linalg.norm(Nl, axis=1, keepdims=True), 1e-6)


def out(f):
    return Nl * f[:, None]


facing_front = ramp(-Nl[:, 1], 0.05, 0.45)
facing_back = ramp(Nl[:, 1], 0.1, 0.5)
facing_out = ramp(Nl[:, 0] * sgn, 0.2, 0.6)        # the side of a limb away from the middle
facing_in = ramp(-Nl[:, 0] * sgn, 0.2, 0.6)
# collarbones: a fine ridge from the notch out to the shoulder, rising a little, with the
# soft hollow above it; the notch itself between their inner ends
c0, c1 = np.array([1.9, 146.6]), np.array([14.2, 148.4])
q = np.stack([ax, z], 1) - c0
t = np.clip(q @ (c1 - c0) / ((c1 - c0) @ (c1 - c0)), 0, 1)
clav_d = np.linalg.norm(q - t[:, None] * (c1 - c0), axis=1)
upper_chest = band(z, 140, 156, 1.5) * (ax < 17)
base += out(g(clav_d, 1.0) * facing_front * upper_chest * LOOK["clavicle"])
above = np.linalg.norm(np.stack([ax, z], 1) - (c0 + t[:, None] * (c1 - c0) + np.array([0.0, 2.3])), axis=1)
base -= out(g(above, 1.3) * band(t, 0.15, 0.8, 0.12) * facing_front * upper_chest * LOOK["clavicle_hollow"])
base -= out(g(np.hypot(x / 1.3, (z - 147.6) / 1.1), 1.0) * facing_front * LOOK["notch"])
# the shoulder cap: the deltoid's round over the joint, on the outside of the arm
base += out(g(np.hypot((ax - 18.3) / 2.6, (z - 139.5) / 4.0), 1.0) * facing_out * LOOK["deltoid"])
# the front hip points (ASIS), where the abs' V lines start
base += out(g(np.hypot(ax - 10.6, z - 101.0), 1.2) * facing_front * LOOK["asis"])
# knees: the kneecap, with the soft hollows either side below it
leg = (ax > 6) & (ax < 20)
knee = leg * band(z, 40, 60, 1)
base += out(g(np.hypot((ax - 13.0) / 2.2, (z - 51.0) / 2.6), 1.0) * facing_front * knee * LOOK["kneecap"])
base -= out(g(np.hypot((np.abs(ax - 13.0) - 2.9) / 1.0, (z - 47.0) / 1.6), 1.0) * facing_front * knee * LOOK["knee_hollow"])
# ankles: the inner knob sits higher than the outer; the Achilles stands between two grooves
ankle = leg * band(z, 3, 16, 1)
base += out(g(np.hypot(z - 9.6, Nl[:, 1] * 2.0), 1.0) * facing_in * ankle * (ax < 14.5) * LOOK["ankle_knob"])
base += out(g(np.hypot(z - 8.2, Nl[:, 1] * 2.0), 1.0) * facing_out * ankle * (ax > 14.5) * LOOK["ankle_knob"])
achilles = facing_back * ankle * band(z, 5, 14, 1.5)
base -= out(g(np.abs(ax - 14.7) - 1.6, 0.6) * achilles * LOOK["achilles"])

# Fat: belly out (most below the navel), flanks and low back fuller, the navel sinks deeper into it
soft = along(g(np.hypot(x / 9.5, (z - 100.0) / 9.0), 1.0) * front * (1 - pit) * LOOK["fat"])
soft += along(g(np.hypot(x / 8.0, (z - 116.0) / 6.0), 1.0) * front * LOOK["fat"] * 0.3)
soft += along(g(np.hypot((ax - 12.0) / 4.0, (z - 108.0) / 6.0), 1.0) * torso * 0.8)
soft += along(g(np.hypot((ax - 9.0) / 4.0, (z - 106.0) / 4.0), 1.0) * back * 0.6)

# AbsLines: midline, the rectus' side lines, hip bone to pubis, a flatter low belly
side = g(ax - xs, 1.0) * band(z, 95, 126, 2.5)
a_, b_ = np.array([10.5, 100.0]), np.array([3.5, 91.0])               # hip bone -> pubis, in x/z
q = np.stack([ax, z], 1) - a_
t = np.clip(q @ (b_ - a_) / ((b_ - a_) @ (b_ - a_)), 0, 1)
vee_d = np.linalg.norm(q - t[:, None] * (b_ - a_), axis=1)
vee = g(vee_d, 1.0)
# the pubic mound between the groin creases, with a shallow line where it turns under: a
# mannequin's amount of anatomy, so briefs and bodysuits sit on a correct shape
low = ramp(-Nn[:, 1], -0.35, 0.3) * torso * band(z, 82.5, 100, 1.5)
base += along(g(np.hypot(x / 3.4, (z - 89.5) / 4.2), 1.0) * low * LOOK["mons"])
base -= along(g(x, 0.5) * band(z, 83.5, 88.5, 1.2) * low * LOOK["cleft"])
base -= along(g(vee_d, 2.2) * band(z, 91, 100, 3) * front * LOOK["groin"])
mid = g(x, 1.0) * (band(z, ZN + 1.2, 127, 2) + 0.12 * band(z, 97, ZN - 1.5, 2)) * (1 - pit)   # linea alba fades below the navel
lines = -along((mid * LOOK["lines"] + side * LOOK["lines"] * 0.75 + vee * 0.22) * front)
lines += along(g(ax - xs / 2, xs / 2.6) * band(z, 97, 125, 3) * front * 0.18)

# AbsPack: three cross lines above the navel, blocks between them
inside = (1 - ramp(ax, xs - 1.6, xs + 0.6)) * ramp(ax, 0.2, 1.6)
cross = sum(g(z - zi - 0.5 * ax / xs, 1.3) for zi in (ZN + 1.0, ZN + 6.5, ZN + 12.5))
block = g(ax - xs / 2, xs / 2.4) * inside * band(z, ZN + 1, 126, 1.5) * (1 - np.clip(cross, 0, 1))   # no blocks below the navel
pack = -along((cross * inside * LOOK["pack"] + mid * 0.2 + side * 0.15) * front)
pack += along(block * front * LOOK["pack"])

# ThighGap: inner thighs apart; the midline of the crotch stays, so the V opens into an arch
inner = ramp(-Nn[:, 0] * sgn, -0.25, 0.8) * (1 - ramp(ax, 6.5, 12))
amount = LOOK["gap"] * ramp(z, 48, 80) * (1 - ramp(z, 85, 91)) * ramp(ax, 0.2, 3.5) * inner
gap = np.zeros((n, 3))
gap[:, 0] = sgn * amount

# ---- write: the base into every key, then the new keys -------------------------------------
keys = mesh.shape_keys.key_blocks
# welding the remesh slivers merged vertices whose offsets in the earlier keys differed (the
# bust's), leaving a scratchy step pattern where those shapes change fast (under and beside
# the bust): smooth each earlier key's offsets over the welded band
deg = np.zeros(n)
np.add.at(deg, E[:, 0], 1.0)
np.add.at(deg, E[:, 1], 1.0)
deg = np.maximum(deg, 1.0)
welded = ((P[:, 2] > 40) & (P[:, 2] < 131)).astype(float)


def smooth_offsets(d, passes):
    for _ in range(passes):
        acc = np.zeros_like(d)
        np.add.at(acc, E[:, 0], d[E[:, 1]])
        np.add.at(acc, E[:, 1], d[E[:, 0]])
        d = d + welded[:, None] * 0.5 * (acc / deg[:, None] - d)
    return d


rest = np.empty(n * 3)
keys[0].data.foreach_get("co", rest)
rest = rest.reshape(-1, 3)
for k in keys[1:]:
    co = np.empty(n * 3)
    k.data.foreach_get("co", co)
    d = co.reshape(-1, 3) - rest
    if np.abs(d).max() > 1e-4:
        k.data.foreach_set("co", (rest + smooth_offsets(d, LOOK["key_smooth"])).ravel())
for k in keys:
    co = np.empty(n * 3)
    k.data.foreach_get("co", co)
    k.data.foreach_set("co", (co.reshape(-1, 3) + relax + base).ravel())
mesh.vertices.foreach_set("co", (P + base).ravel())
for name, delta in (("Fat", soft), ("AbsLines", lines), ("AbsPack", pack), ("ThighGap", gap)):
    k = skin.shape_key_add(name=name, from_mix=False)
    k.data.foreach_set("co", (P + base + delta).ravel())
    print(name, "moves", int((np.linalg.norm(delta, axis=1) > 0.02).sum()), "verts, max", round(float(np.abs(delta).max()), 2))
mesh.update()
for poly in mesh.polygons:
    poly.use_smooth = True

# ---- preview --------------------------------------------------------------------------------
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
    scn.render.resolution_x, scn.render.resolution_y = 700, 1000
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    scn.collection.objects.link(cam)
    scn.camera = cam
    cam.data.lens = 70
    looks = (("base", {}), ("soft", {"Fat": 1}), ("toned", {"AbsLines": 0.7}),
             ("defined", {"AbsLines": 1, "AbsPack": 0.45}), ("sixpack", {"AbsLines": 1, "AbsPack": 1}),
             ("gap", {"ThighGap": 1}))
    views = (("front", (0, 0, 106), (-25, -115, 6)), ("back", (0, 0, 108), (25, 115, 6)),
             ("legs", (0, 0, 76), (0, -130, 0)))
    for look, weights in looks:
        for k in keys:
            k.value = weights.get(k.name, 0)
        for view, target, offset in views:
            wanted = {"front": look != "gap", "back": look in ("base", "soft"), "legs": look in ("base", "gap")}
            if not wanted[view]:
                continue
            cam.location = Vector(target) + Vector(offset)
            cam.rotation_euler = (Vector(target) - cam.location).to_track_quat("-Z", "Y").to_euler()
            scn.render.filepath = os.path.join(PREVIEW, f"torso_{look}_{view}.png")
            bpy.ops.render.render(write_still=True)
    for k in keys:
        k.value = 0
    bpy.data.objects.remove(cam, do_unlink=True)

# ---- export ---------------------------------------------------------------------------------
bpy.ops.wm.save_as_mainfile(filepath=os.path.splitext(OUT)[0] + ".blend")   # Tools/blender_outfits.py starts here
bpy.ops.object.select_all(action="DESELECT")
arm.select_set(True)
skin.select_set(True)
bpy.ops.export_scene.fbx(filepath=OUT, use_selection=True, object_types={"ARMATURE", "MESH"},
                         add_leaf_bones=False, use_armature_deform_only=False,
                         mesh_smooth_type="FACE", bake_anim=False, axis_forward="-Z", axis_up="Y")
print("wrote", OUT)
