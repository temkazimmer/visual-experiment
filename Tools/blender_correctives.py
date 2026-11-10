"""Arm-raise correctives for the android's skin (headless Blender, the last skin stage):

  blender -b <android_skin.blend from blender_face.py> \
      --python Tools/blender_correctives.py -- <out.fbx> [<preview_dir>]

It runs straight after blender_face.py: her hair is the separate card-hair mesh
(SKM_HairCards, put on by UVisexBodyComponent), so blender_hair.py is no longer in the chain.

Raising an arm with plain skin weights folds the armpit into a hard crease, stacks the
shoulder into a shelf and leaves the chest flat and the breast where it hung. For each arm:
pose it raised, relax the armpit, shoulder and side of the chest in that pose, lift and
stretch that side's pectoral and breast, then turn the difference back into rest space as a
morph (ArmUp_l / ArmUp_r). UVisexBodyComponent fades each in as that arm rises.

The arm's rise is measured as the angle between the upper arm and the torso's down axis
(pelvis to neck_01): 0 = hanging straight down, 180 = straight up. RAISE_AT is printed;
the C++ ramp ends there.
"""
import math
import os
import sys

import bpy
import numpy as np
from mathutils import Euler, Vector

LOOK = dict(
    raise_deg=140.0,   # how far the arm is lifted for the sculpt (about its local Y)
    reach=19.0,        # cm around the armpit and the shoulder top the relax reaches
    fold=250,          # plain Laplacian passes on the fold itself
    relax=150,         # Taubin iterations in the raised pose (the mesh is dense: ~0.3 cm edges)
    lift=2.6,          # cm the breast rides up with the arm
    pec_lift=1.6,      # cm the pectoral and upper chest stretch up
    thigh_deg=100.0,   # how far the thigh is folded up for the hip sculpt
    thigh_reach=16.0,  # cm around the groin fold the relax reaches
    thigh_fold=260,    # plain Laplacian passes on the fold
    thigh_volume=0.7,  # cm the thigh and glute swell in deep flexion
    thigh_open=1.0,    # cm each side of the groin fold steps back out
)

args = sys.argv[sys.argv.index("--") + 1:]
OUT = args[0]
PREVIEW = args[1] if len(args) > 1 else None

arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
skin = next(o for o in bpy.data.objects if o.type == "MESH" and o.data.shape_keys)
me = skin.data
keys = me.shape_keys.key_blocks
n = len(me.vertices)
for o in (skin, arm):   # the hair stage leaves both at identity; the maths below relies on it
    assert all(abs(o.matrix_world[i][j] - (i == j)) < 1e-4 for i in range(4) for j in range(4)), o.matrix_world


def key_co(name):
    a = np.empty(n * 3)
    keys[name].data.foreach_get("co", a)
    return a.reshape(-1, 3)


REST = key_co("Basis")
EDGES = np.empty(len(me.edges) * 2, dtype=np.int64)
me.edges.foreach_get("vertices", EDGES)
EDGES = EDGES.reshape(-1, 2)
deg = np.zeros(n)
np.add.at(deg, EDGES[:, 0], 1.0)
np.add.at(deg, EDGES[:, 1], 1.0)
deg = np.maximum(deg, 1.0)


def diffuse(a, iters):
    for _ in range(iters):
        acc = np.zeros_like(a)
        np.add.at(acc, EDGES[:, 0], a[EDGES[:, 1]])
        np.add.at(acc, EDGES[:, 1], a[EDGES[:, 0]])
        a = 0.5 * a + 0.5 * acc / (deg[:, None] if a.ndim == 2 else deg)
    return a


def taubin(X, mask, iters, lam=0.5, mu=-0.53):
    m = mask[:, None]
    for _ in range(iters):
        for k in (lam, mu):
            X = X + m * k * (diffuse(X, 1) * 2.0 - X - X)
    return X


# skin weights as a dense table over the bones that matter
names = [g.name for g in skin.vertex_groups]
W = np.zeros((n, len(names)))
for v in me.vertices:
    for g in v.groups:
        W[v.index, g.group] = g.weight
W /= np.maximum(W.sum(axis=1, keepdims=True), 1e-6)


def group(*parts):
    cols = [i for i, nm in enumerate(names) if any(p in nm for p in parts)]
    return W[:, cols].sum(axis=1) if cols else np.zeros(n)


# hair faces (material 1) and the head stay put
hair = np.zeros(n, dtype=bool)
for p in me.polygons:
    if p.material_index == 1:
        hair[list(p.vertices)] = True
HEADW = group("head", "neck_02")

bust_d = np.linalg.norm(key_co("BustUp") - REST, axis=1)
bust_m = np.clip(bust_d / max(bust_d.max(), 1e-6), 0, 1)

B = {b.name: Vector(b.head_local) for b in arm.data.bones}
torso_up = (B["neck_01"] - B["pelvis"]).normalized()


def rise_deg(pose):
    """Angle of the upper arm from hanging straight down, in the torso's frame."""
    out = {}
    for s in ("l", "r"):
        a = pose.bones["upperarm_" + s].head
        b = pose.bones["lowerarm_" + s].head
        d = (b - a).normalized()
        out[s] = 180.0 - math.degrees(math.acos(max(-1.0, min(1.0, d.dot(torso_up)))))
    return out


def evaluated():
    dg = bpy.context.evaluated_depsgraph_get()
    ev = skin.evaluated_get(dg)
    m = ev.to_mesh()
    a = np.empty(n * 3)
    m.vertices.foreach_get("co", a)
    ev.to_mesh_clear()
    return a.reshape(-1, 3)


def deform_3x3():
    """Per-vertex linear part of the skinning (object space = world here)."""
    mats = np.zeros((len(names), 3, 3))
    for i, nm in enumerate(names):
        pb = arm.pose.bones.get(nm)
        if pb:
            M = pb.matrix @ arm.data.bones[nm].matrix_local.inverted()
            mats[i] = np.array(M.to_3x3())
        else:
            mats[i] = np.eye(3)
    return np.einsum("vb,bij->vij", W, mats)


for kb in keys:
    kb.value = 0.0
for pb in arm.pose.bones:
    pb.rotation_mode = "XYZ"
    pb.rotation_euler = Euler((0, 0, 0))
bpy.context.view_layer.update()
rest_rise = rise_deg(arm.pose)
print("rest rise", {k: round(v, 1) for k, v in rest_rise.items()})

UP = np.array((0.0, 0.0, 1.0))
made = {}
for s in ("l", "r"):
    sign = 1.0 if B["upperarm_" + s].x > 0 else -1.0
    pb = arm.pose.bones["upperarm_" + s]
    pb.rotation_euler = Euler((0, -math.radians(LOOK["raise_deg"]), 0))
    bpy.context.view_layer.update()
    raised = rise_deg(arm.pose)[s]
    P = evaluated()
    # the armpit: just under the shoulder joint, in the raised pose
    joint = np.array(arm.pose.bones["upperarm_" + s].head)
    pit = joint - np.array((sign * 2.0, 0.0, 6.0))
    centres = (pit,                                            # the hollow, at the back
               joint + np.array((-sign * 2.0, -7.0, -4.0)),    # the front fold, chest into arm
               joint + np.array((-sign * 3.0, 0.0, 6.0)))      # the shoulder shelf
    dist = np.min([np.linalg.norm(P - c, axis=1) for c in centres], axis=0)
    near = np.clip(1.0 - dist / LOOK["reach"], 0, 1)
    on_side = np.clip(sign * P[:, 0] / 4.0, 0, 1)                      # this side only
    upper = group("upperarm_" + s, "clavicle_" + s, "spine_05", "spine_04", "spine_03")
    far_arm = group("lowerarm_" + s, "hand_" + s, "thumb", "index", "middle", "ring", "pinky")
    mask = near * on_side * np.clip(upper * 2.5, 0, 1) * np.clip(1.0 - far_arm * 3.0, 0, 1)
    mask *= (~hair) * np.clip(1.0 - HEADW * 3.0, 0, 1)
    mask = diffuse(mask, 4)
    # the fold is the skin lying on itself: plain Laplacian smoothing opens it (and hollows
    # the pit a little, as it should), then Taubin evens out the wider shoulder
    Q = taubin(P, np.clip(mask * 1.6, 0, 1) ** 2, LOOK["fold"], mu=0.0)
    Q = taubin(Q, mask, LOOK["relax"])
    # the chest on that side stretches up with the arm, the breast rides up
    side_chest = np.clip(sign * P[:, 0] / 3.0, 0, 1) * (~hair)
    chest_band = np.clip(1.0 - np.abs(P[:, 2] - (joint[2] - 14.0)) / 16.0, 0, 1)
    front = np.clip(-(P[:, 1] - B["spine_05"].y) / 6.0, 0, 1)
    pec = diffuse(side_chest * chest_band * front * np.clip(1.0 - far_arm * 3.0, 0, 1), 6)
    Q = Q + UP[None, :] * (LOOK["pec_lift"] * pec + LOOK["lift"] * bust_m ** 0.8 * side_chest)[:, None]
    moved = Q - P
    # back into rest space through the inverse of each vertex's skinning
    Mv = deform_3x3()
    live = np.linalg.norm(moved, axis=1) > 1e-4
    rest_delta = np.zeros_like(moved)
    rest_delta[live] = np.linalg.solve(Mv[live], moved[live][:, :, None])[:, :, 0]
    k = skin.shape_key_add(name="ArmUp_" + s, from_mix=False)
    k.data.foreach_set("co", (REST + rest_delta).ravel())
    made[s] = (raised, k)
    print("ArmUp_%s: raise %.1f deg, moves %d verts, max %.2f cm" % (
        s, raised, int(live.sum()), float(np.linalg.norm(rest_delta, axis=1).max())))
    pb.rotation_euler = Euler((0, 0, 0))
    bpy.context.view_layer.update()
print("RAISE_AT", round(min(v[0] for v in made.values()), 1), "REST_RISE", round(min(rest_rise.values()), 1))

# ---- hip flexion (seated, kneeling, lying): ThighUp_l / ThighUp_r ---------------------------
# Folding the thigh up collapses the crotch crease and flattens the thigh and glute. In the
# flexed pose: open the groin fold, then give the thigh and glute back some volume.


def flex_deg(pose, s):
    """Thigh lifted from hanging straight down, in the torso's frame (0 = standing)."""
    a = pose.bones["thigh_" + s].head
    b = pose.bones["calf_" + s].head
    d = (b - a).normalized()
    return 180.0 - math.degrees(math.acos(max(-1.0, min(1.0, d.dot(torso_up)))))


thigh_made = {}
for s in ("l", "r"):
    sign = 1.0 if B["thigh_" + s].x > 0 else -1.0
    pb = arm.pose.bones["thigh_" + s]
    # pick the local axis and sign that swings the knee forward (-Y)
    best = None
    for axis in range(3):
        for sg in (1.0, -1.0):
            e = [0.0, 0.0, 0.0]
            e[axis] = sg * math.radians(LOOK["thigh_deg"])
            pb.rotation_euler = Euler(e)
            bpy.context.view_layer.update()
            knee = arm.pose.bones["calf_" + s].head
            hip = arm.pose.bones["thigh_" + s].head
            score = -(knee.y - hip.y) - abs(knee.x - hip.x) * 2.0
            if best is None or score > best[0]:
                best = (score, e)
    pb.rotation_euler = Euler(best[1])
    bpy.context.view_layer.update()
    flexed = flex_deg(arm.pose, s)
    P = evaluated()
    joint = np.array(arm.pose.bones["thigh_" + s].head)
    centres = (joint + np.array((-sign * 2.0, -7.0, -2.0)),      # the front fold, belly onto thigh
               joint + np.array((-sign * 7.0, -3.0, -8.0)))      # the inner groin
    dist = np.min([np.linalg.norm(P - c, axis=1) for c in centres], axis=0)
    near = np.clip(1.0 - dist / LOOK["thigh_reach"], 0, 1)
    side = np.clip(0.5 + sign * P[:, 0] / 6.0, 0, 1)
    mine = group("thigh_" + s, "pelvis", "spine_01")
    lower = group("calf_" + s, "foot_" + s, "ball_" + s)
    mask = near * side * np.clip(mine * 2.0, 0, 1) * np.clip(1.0 - lower * 3.0, 0, 1) * (~hair)
    mask = diffuse(mask, 4)
    Q = taubin(P, np.clip(mask * 1.6, 0, 1) ** 2, LOOK["thigh_fold"], mu=0.0)
    Q = taubin(Q, mask, LOOK["relax"])
    # volume: the thigh and glute swell a little instead of flattening
    NRM = np.empty(n * 3)
    dg = bpy.context.evaluated_depsgraph_get()
    ev = skin.evaluated_get(dg)
    m = ev.to_mesh()
    m.vertices.foreach_get("normal", NRM)
    ev.to_mesh_clear()
    NRM = NRM.reshape(-1, 3)
    upper_thigh = group("thigh_" + s) * np.clip(1.0 - lower * 4.0, 0, 1)
    glute = group("pelvis") * np.clip(sign * P[:, 0] / 4.0, 0, 1) * np.clip((P[:, 1] - joint[1]) / 6.0, 0, 1)
    swell = diffuse(np.clip(upper_thigh + glute, 0, 1) * (~hair), 6) * (1 - np.clip(mask * 2.0, 0, 1))
    Q = Q + NRM * (LOOK["thigh_volume"] * swell)[:, None]
    # the belly and the thigh pass through each other in the fold: each side steps back out
    # along its own normal, so the crease opens into a line instead of a jagged cut
    fold = diffuse(np.clip(near * 1.4, 0, 1) ** 2 * side * (~hair), 3)
    Q = Q + NRM * (LOOK["thigh_open"] * fold)[:, None]
    moved = Q - P
    Mv = deform_3x3()
    live = np.linalg.norm(moved, axis=1) > 1e-4
    rest_delta = np.zeros_like(moved)
    rest_delta[live] = np.linalg.solve(Mv[live], moved[live][:, :, None])[:, :, 0]
    k = skin.shape_key_add(name="ThighUp_" + s, from_mix=False)
    k.data.foreach_set("co", (REST + rest_delta).ravel())
    thigh_made[s] = flexed
    print("ThighUp_%s: flex %.1f deg, moves %d verts, max %.2f cm" % (
        s, flexed, int(live.sum()), float(np.linalg.norm(rest_delta, axis=1).max())))
    pb.rotation_euler = Euler((0, 0, 0))
    bpy.context.view_layer.update()
print("FLEX_AT", round(min(thigh_made.values()), 1))

# ---- preview: both arms up, without and with the correctives ---------------------------
if PREVIEW:
    os.makedirs(PREVIEW, exist_ok=True)
    for s in ("l", "r"):
        arm.pose.bones["upperarm_" + s].rotation_euler = Euler((0, -math.radians(LOOK["raise_deg"]), 0))
    keys["BustUp"].value = 0.7
    keys["GlutesUp"].value = 1.0
    scn = bpy.context.scene
    scn.render.engine = "BLENDER_WORKBENCH"
    scn.display.shading.light = "STUDIO"
    scn.display.shading.color_type = "SINGLE"
    scn.render.resolution_x, scn.render.resolution_y = 900, 1100
    cam_data = bpy.data.cameras.new("Cam")
    cam_data.lens = 70
    cam = bpy.data.objects.new("Cam", cam_data)
    scn.collection.objects.link(cam)
    scn.camera = cam
    t = Vector(B["spine_05"]) + Vector((0, 0, -4))
    for on in (0.0, 1.0):
        for s in ("l", "r"):
            keys["ArmUp_" + s].value = on
        for label, off in (("front", (0, -1, 0.05)), ("tq", (0.7, -0.75, 0.05)), ("side", (1, -0.1, 0.05))):
            cam.location = t + Vector(off).normalized() * 95.0
            cam.rotation_euler = (t - cam.location).to_track_quat("-Z", "Y").to_euler()
            scn.render.filepath = os.path.join(PREVIEW, "armup_%s_%s.png" % (("before", "after")[int(on)], label))
            bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(cam, do_unlink=True)
    for kb in keys:
        kb.value = 0.0
    for pb in arm.pose.bones:
        pb.rotation_euler = Euler((0, 0, 0))

# ---- export ------------------------------------------------------------------------------
bpy.ops.wm.save_as_mainfile(filepath=os.path.splitext(OUT)[0] + ".blend")
bpy.ops.object.select_all(action="DESELECT")
arm.select_set(True)
skin.select_set(True)
bpy.ops.export_scene.fbx(filepath=OUT, use_selection=True, object_types={"ARMATURE", "MESH"},
                         add_leaf_bones=False, use_armature_deform_only=False,
                         mesh_smooth_type="FACE", bake_anim=False, axis_forward="-Z", axis_up="Y")
print("wrote", OUT)
