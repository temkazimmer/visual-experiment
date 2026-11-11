"""Toe bones for the android skin. Headless, on the finished skin FBX (after blender_face):

  /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup \
      --python Tools/blender_toes.py -- <android_skin.fbx> <makehuman base.obj> <out.fbx> [<preview_dir>]

The grafted MakeHuman feet have five real toes, but Quinn's skeleton stops at the ball, so
the toes could only move as one block. This adds two bones per toe under each ball bone,
MetaHuman names: bigtoe / indextoe / middletoe / ringtoe / littletoe, _01 (from the knuckle)
and _02 (to the tip). Joint places come from MakeHuman's toe joints, bent onto Quinn's foot
the same way blender_extremities did, then centred on the skin's own toe geometry. The
ball's weight on each toe moves to its bones, soft across the knuckle and the middle joint.
Quinn (the hidden pose leader) gets the same bones in UE (Tools/char_toes.py).
"""
import json
import os
import sys

import bpy
import numpy as np
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
SRC, MH, OUT = args[0], args[1], args[2]
PREVIEW = args[3] if len(args) > 3 else None

TOES = ["bigtoe", "indextoe", "middletoe", "ringtoe", "littletoe"]   # MakeHuman toe 1..5
LOOK = dict(
    knuckle_blend=0.9,   # cm each side of the knuckle over which ball weight hands to the toe
    joint_blend=0.45,    # cm each side of the middle joint
    tau=0.22,            # cm: how sharply a vertex belongs to its nearest toe (webs blend)
    slice=0.35,          # cm half-thickness of the slice used to centre a joint in its toe
)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=SRC)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
skin = next(o for o in bpy.data.objects if o.type == "MESH")
assert "bigtoe_01_l" not in arm.data.bones, "toe bones already there: run on the skin without them"
AW = np.array(arm.matrix_world)


def bone(n):
    return np.array(arm.matrix_world @ arm.data.bones[n].head_local)


# ---- MakeHuman's toe joints, in the skin's space (as in blender_extremities) ---------------
Vraw, groups, g = [], {}, None
for line in open(MH):
    if line.startswith("v "):
        Vraw.append([float(x) for x in line.split()[1:4]])
    elif line.startswith("g "):
        g = line.split()[1]
    elif line.startswith("f "):
        groups.setdefault(g, set()).update(int(t.split("/")[0]) - 1 for t in line.split()[1:])
Vraw = np.array(Vraw)
MV = np.stack([Vraw[:, 0], -Vraw[:, 2], Vraw[:, 1]], 1) * 10.0


def J(n):
    return MV[sorted(groups["joint-" + n])].mean(0)


MV[:, 2] -= J("ground")[2]

# ---- the skin: positions (world) and weights ------------------------------------------------
me = skin.data
SV = np.zeros(len(me.vertices) * 3)
me.vertices.foreach_get("co", SV)
M4 = np.array(skin.matrix_world)
SV = SV.reshape(-1, 3) @ M4[:3, :3].T + M4[:3, 3]
gidx = {vg.name: vg.index for vg in skin.vertex_groups}


def group_weights(name):
    w = np.zeros(len(me.vertices))
    gi = gidx.get(name)
    if gi is None:
        return w
    for v in me.vertices:
        for e in v.groups:
            if e.group == gi:
                w[v.index] = e.weight
    return w


def frame(o, b):
    f = b - o
    f[2] = 0
    f /= np.linalg.norm(f)
    z = np.array([0, 0, 1.0])
    return np.stack([f, np.cross(z, f), z], 1)


def toe_joints(s):
    """Per toe: knuckle, middle joint, tip, bent onto Quinn's foot as blender_extremities did,
    then each joint centred in the skin's own toe (the graft was subdivided and relaxed)."""
    ma, mk_ = J("%s-ankle" % s), J("%s-knee" % s)
    raw = []
    for k in range(1, 6):
        js = [J("%s-toe-%d-%d" % (s, k, j)) for j in range(1, 5) if "joint-%s-toe-%d-%d" % (s, k, j) in groups]
        raw.append([js[0], js[1], js[-1]])            # knuckle, first middle joint, tip
    mball = np.mean([t[0] for t in raw], 0)
    qa, qb = bone("foot_" + s), bone("ball_" + s)
    R = frame(qa, qb) @ frame(ma, mball).T
    sc = np.linalg.norm(qb - qa) / np.linalg.norm(mball - ma)
    place = lambda X: (sc * (R @ (np.atleast_2d(X) - ma).T)).T + qa
    # the graft stood on her old sole: same lift (MakeHuman's foot, mapped, its lowest point)
    n_mh = (ma - mk_) / np.linalg.norm(ma - mk_)
    side = np.sign(ma[0])
    body = sorted(groups["body"])
    mfoot = MV[body][(np.sign(MV[body][:, 0]) == side) & ((MV[body] - ma) @ n_mh > -16.0) & (MV[body][:, 2] < 40)]
    ball_w = group_weights("ball_" + s)
    feet = SV[ball_w > 0.05]
    lift = feet[:, 2].min() - place(mfoot)[:, 2].min()
    toes = [place(np.array(t)) + np.array([0, 0, lift]) for t in raw]
    # centre: the skin's vertices in a thin slice across each toe at each joint
    near = SV[ball_w > 0.05]
    fixed = []
    for k, t in enumerate(toes):
        d = t[2] - t[0]
        d /= np.linalg.norm(d)
        js = []
        for j, p in enumerate(t[:2]):
            rel = near - p
            along = rel @ d
            radial = np.linalg.norm(rel - np.outer(along, d), axis=1)
            pick = (np.abs(along) < LOOK["slice"]) & (radial < 1.3)
            js.append(near[pick].mean(0) if pick.sum() > 8 else p)
        # tip: the far end of the skin along this toe
        rel = near - js[1]
        along = rel @ d
        radial = np.linalg.norm(rel - np.outer(along, d), axis=1)
        pick = (radial < 1.0) & (along > 0)
        tip = js[1] + d * (along[pick].max() if pick.any() else np.linalg.norm(t[2] - t[1]))
        tip[2] = js[1][2] + (tip - js[1])[2] * 0.5     # the tip bone ends at the toe's centre, not its pad
        fixed.append([js[0], js[1], tip])
        print("%s %-9s knuckle %s mid %s tip %s (moved %.2f %.2f cm)" % (
            s, TOES[k], np.round(js[0], 2), np.round(js[1], 2), np.round(tip, 2),
            np.linalg.norm(js[0] - t[0]), np.linalg.norm(js[1] - t[1])))
    return fixed, ball_w


def smooth01(a, b, x):
    u = np.clip((x - a) / (b - a), 0, 1)
    return u * u * (3 - 2 * u)


# ---- bones ----------------------------------------------------------------------------------
JOINTS = {s: toe_joints(s) for s in ("l", "r")}
inv = np.linalg.inv(AW)
loc = lambda p: Vector((inv @ np.append(p, 1.0))[:3])
bpy.context.view_layer.objects.active = arm
for o in bpy.data.objects:
    o.select_set(o is arm)
bpy.ops.object.mode_set(mode="EDIT")
eb = arm.data.edit_bones
for s in ("l", "r"):
    for k, (kn, mid, tip) in enumerate(JOINTS[s][0]):
        b1 = eb.new("%s_01_%s" % (TOES[k], s))
        b1.head, b1.tail = loc(kn), loc(mid)
        b1.parent = eb["ball_" + s]
        b1.use_connect = False
        b2 = eb.new("%s_02_%s" % (TOES[k], s))
        b2.head, b2.tail = loc(mid), loc(tip)
        b2.parent = b1
        b2.use_connect = True
        for b in (b1, b2):          # roll: the bone's Z is the foot's up, so toes curl about one axis
            b.align_roll(Vector((AW[:3, :3].T @ np.array([0, 0, 1.0]))))
bpy.ops.object.mode_set(mode="OBJECT")
print("bones", len(arm.data.bones))

# ---- weights: the ball's share on each toe moves to its bones -------------------------------
for s in ("l", "r"):
    joints, ball_w = JOINTS[s]
    idx = np.nonzero(ball_w > 1e-4)[0]
    P = SV[idx]
    D, T1, T2 = [], [], []
    for kn, mid, tip in joints:
        d1 = (mid - kn) / np.linalg.norm(mid - kn)
        d2 = (tip - mid) / np.linalg.norm(tip - mid)
        a1 = (P - kn) @ d1                            # along the toe from its knuckle
        a2 = (P - mid) @ d2                           # past the middle joint
        seg = lambda a, b: np.linalg.norm(P - (a + np.clip(((P - a) @ (b - a)) / ((b - a) @ (b - a)), 0, 1)[:, None] * (b - a)), axis=1)
        D.append(np.minimum(seg(kn, mid), seg(mid, tip)))
        T1.append(smooth01(-LOOK["knuckle_blend"], LOOK["knuckle_blend"], a1))
        T2.append(smooth01(-LOOK["joint_blend"], LOOK["joint_blend"], a2))
    D, T1, T2 = np.array(D), np.array(T1), np.array(T2)
    share = np.exp(-(D - D.min(0)) / LOOK["tau"])
    share /= share.sum(0)
    vgs = {}
    for k in range(5):
        for j in (1, 2):
            n = "%s_0%d_%s" % (TOES[k], j, s)
            vgs[n] = skin.vertex_groups.get(n) or skin.vertex_groups.new(name=n)
    moved = 0
    ball_vg = skin.vertex_groups["ball_" + s]
    for i, v in enumerate(idx):
        w = ball_w[v]
        toe_part = (share[:, i] * T1[:, i]).sum()
        if toe_part < 1e-3:
            continue
        moved += 1
        ball_vg.add([int(v)], float(w * (1 - toe_part)), "REPLACE")
        for k in range(5):
            wk = w * share[k, i] * T1[k, i]
            if wk < 1e-4:
                continue
            vgs["%s_01_%s" % (TOES[k], s)].add([int(v)], float(wk * (1 - T2[k, i])), "REPLACE")
            vgs["%s_02_%s" % (TOES[k], s)].add([int(v)], float(wk * T2[k, i]), "REPLACE")
    print(s, "toe vertices", moved, "of", len(idx), "on the ball")

bpy.context.view_layer.objects.active = skin
bpy.ops.object.mode_set(mode="WEIGHT_PAINT")
bpy.ops.object.vertex_group_limit_total(group_select_mode="ALL", limit=4)
bpy.ops.object.vertex_group_normalize_all(group_select_mode="ALL", lock_active=False)
bpy.ops.object.mode_set(mode="OBJECT")

# ---- preview: the left foot, big toe up, the others curled ------------------------------------
if PREVIEW:
    import math
    os.makedirs(PREVIEW, exist_ok=True)
    scn = bpy.context.scene
    scn.render.engine = "BLENDER_WORKBENCH"
    scn.display.shading.light = "STUDIO"
    scn.display.shading.color_type = "SINGLE"
    scn.display.shading.single_color = (0.8, 0.6, 0.5)
    scn.render.resolution_x, scn.render.resolution_y = 1200, 800
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    scn.collection.objects.link(cam)
    scn.camera = cam
    cam.data.lens = 70
    tgt = Vector(bone("ball_l")) + Vector((1.0, 0.0, 2.0))

    def shoot(label):
        for name, off in (("side", (55, -20, 12)), ("front", (8, -55, 18))):
            cam.location = tgt + Vector(off)
            cam.rotation_euler = (tgt - cam.location).to_track_quat("-Z", "Y").to_euler()
            scn.render.filepath = os.path.join(PREVIEW, "toes_%s_%s.png" % (label, name))
            bpy.ops.render.render(write_still=True)

    shoot("rest")
    pb = arm.pose.bones
    for k, n in enumerate(TOES):
        up = 30.0 if k == 0 else -18.0                # big toe lifts, the others curl
        for j in (1, 2):
            b = pb["%s_0%d_l" % (n, j)]
            b.rotation_mode = "XYZ"
            b.rotation_euler = (math.radians(up if j == 1 else up * 0.6), 0, 0)
    bpy.context.view_layer.update()
    shoot("posed")
    for b in pb:
        b.rotation_mode = "QUATERNION"
        b.rotation_quaternion = (1, 0, 0, 0)
        b.rotation_euler = (0, 0, 0)
    bpy.data.objects.remove(cam, do_unlink=True)

# ---- export (same settings as every skin stage) ----------------------------------------------
json.dump({s: {"%s_%s" % (TOES[k], s): [list(map(float, p)) for p in t] for k, t in enumerate(JOINTS[s][0])}
           for s in ("l", "r")}, open(os.path.splitext(OUT)[0] + "_toes.json", "w"), indent=1)
bpy.ops.object.select_all(action="DESELECT")
arm.select_set(True)
skin.select_set(True)
bpy.ops.export_scene.fbx(filepath=OUT, use_selection=True, object_types={"ARMATURE", "MESH"},
                         add_leaf_bones=False, use_armature_deform_only=False,
                         mesh_smooth_type="FACE", bake_anim=False, axis_forward="-Z", axis_up="Y")
print("wrote", OUT)
