"""Build the android's skin mesh from Quinn in Blender (headless). Outside the editor:

  /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup \
      --python Tools/blender_skin.py -- <quinn.fbx> <out.fbx> [<preview_dir>]

Quinn is a mannequin made of separate panels with grooves between them. The skin has to
read like human skin, so:
1. voxel-remesh the body into one closed surface (gaps and seams fuse),
2. smooth the remaining grooves away (Laplacian, volume-preserving),
3. transfer Quinn's skin weights onto it (same skeleton, every animation works),
4. add shape keys BustUp/BustDown and GlutesUp/GlutesDown for the body sliders,
5. export with the armature; Tools/char_skin.py imports it as SKM_AndroidSkin.

Look values are in SKIN at the top.
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Vector

SKIN = dict(
    voxel=0.28,          # cm: fuses panel gaps; fine enough to keep the fingers
    coarse_voxel=1.0,    # cm: big enough to swallow the steps between panels
    coarse_smooth=40,
    smooth_iters=35,     # light pass after wrapping (hands and feet are masked out)
    smooth_factor=0.85,
    faces=80000,         # decimate target
    max_influences=4,
    # at slider 1 (morph weight 1): projection beyond the base body, cm
    bust_proj=9.0, bust_height=0.40, bust_spread=0.50, bust_bounce=2.4, bust_sway=2.0, bust_surge=1.5, bust_apart=2.4, bust_cleft=1.0,
    glute_proj=7.0, glute_height=1.0, glute_spread=0.60, glute_bounce=1.9,
)

args = sys.argv[sys.argv.index("--") + 1:]
SRC, OUT = args[0], args[1]
PREVIEW = args[2] if len(args) > 2 else None

CACHE = os.environ.get("SKIN_CACHE")   # a .blend of the smoothed skin; reused when it exists
if CACHE and os.path.exists(CACHE):
    bpy.ops.wm.open_mainfile(filepath=CACHE)
else:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=SRC)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
src = next(o for o in bpy.data.objects if o.type == "MESH" and o.name != "SKM_AndroidSkin")
for m in list(src.modifiers):
    if m.type == "ARMATURE":
        m.show_viewport = False   # rest pose while we work


def world(o, co):
    return o.matrix_world @ co


# ---- landmarks from the skeleton (world space, metres) --------------------------------
bones = {b.name: arm.matrix_world @ b.head_local for b in arm.data.bones}
fwd = (bones["ball_l"] - bones["foot_l"]) + (bones["ball_r"] - bones["foot_r"])   # feet splay; average
fwd.z = 0
fwd.normalize()
side = Vector((0, 0, 1)).cross(fwd).normalized()   # her left or right; we use both signs

# ---- 1. one closed surface ---------------------------------------------------------------
CACHED = "SKM_AndroidSkin" in bpy.data.objects
if CACHED:
    skin = bpy.data.objects["SKM_AndroidSkin"]
local_cm = 1.0 / (src.matrix_world.to_scale()[0] * 100.0)   # object units per cm


def build_skin():
  skin = src.copy()
  skin.data = src.data.copy()
  skin.name = "SKM_AndroidSkin"
  bpy.context.collection.objects.link(skin)
  skin.vertex_groups.clear()
  for m in list(skin.modifiers):
      skin.modifiers.remove(m)
  bpy.ops.object.select_all(action="DESELECT")
  skin.select_set(True)
  bpy.context.view_layer.objects.active = skin
  local_cm = 1.0 / (src.matrix_world.to_scale()[0] * 100.0)   # object units per cm
  skin.data.remesh_voxel_size = SKIN["voxel"] * local_cm
  skin.data.remesh_voxel_adaptivity = 0.0
  bpy.ops.object.voxel_remesh()

  # ---- 2. skin weights -----------------------------------------------------------------------
  for g in src.vertex_groups:
      skin.vertex_groups.new(name=g.name)
  dt = skin.modifiers.new("Weights", "DATA_TRANSFER")
  dt.object = src
  dt.use_vert_data = True
  dt.data_types_verts = {"VGROUP_WEIGHTS"}
  dt.vert_mapping = "POLYINTERP_NEAREST"
  dt.layers_vgroup_select_src = "ALL"
  dt.layers_vgroup_select_dst = "NAME"
  bpy.ops.object.modifier_apply(modifier=dt.name)
  bpy.ops.object.mode_set(mode="WEIGHT_PAINT")
  bpy.ops.object.vertex_group_smooth(group_select_mode="ALL", factor=0.5, repeat=2)
  bpy.ops.object.vertex_group_limit_total(group_select_mode="ALL", limit=SKIN["max_influences"])
  bpy.ops.object.vertex_group_normalize_all(group_select_mode="ALL", lock_active=False)
  bpy.ops.object.mode_set(mode="OBJECT")

  # ---- 3. smooth the grooves (not the hands and feet) ----------------------------------------
  mask = skin.vertex_groups.new(name="SmoothMask")
  keep = [g.index for g in skin.vertex_groups if any(k in g.name for k in ("hand", "thumb", "index", "middle", "ring", "pinky", "foot", "ball"))]
  for v in skin.data.vertices:
      held = sum(g.weight for g in v.groups if g.group in keep)
      mask.add([v.index], max(0.0, 1.0 - min(1.0, held * 1.5)), "REPLACE")
  # a coarse remesh swallows the steps between panels; the fine skin wraps onto it
  coarse = src.copy()
  coarse.data = src.data.copy()
  coarse.name = "Coarse"
  bpy.context.collection.objects.link(coarse)
  for m in list(coarse.modifiers):
      coarse.modifiers.remove(m)
  bpy.ops.object.select_all(action="DESELECT")
  coarse.select_set(True)
  bpy.context.view_layer.objects.active = coarse
  coarse.data.remesh_voxel_size = SKIN["coarse_voxel"] * local_cm
  coarse.data.remesh_voxel_adaptivity = 0.0
  bpy.ops.object.voxel_remesh()
  sm = coarse.modifiers.new("Smooth", "SMOOTH")
  sm.factor, sm.iterations = 0.8, SKIN["coarse_smooth"]
  bpy.ops.object.modifier_apply(modifier=sm.name)
  bpy.ops.object.select_all(action="DESELECT")
  skin.select_set(True)
  bpy.context.view_layer.objects.active = skin
  wrap = skin.modifiers.new("Wrap", "SHRINKWRAP")
  wrap.target = coarse
  wrap.wrap_method = "NEAREST_SURFACEPOINT"
  wrap.vertex_group = "SmoothMask"
  bpy.ops.object.modifier_apply(modifier=wrap.name)
  lap = skin.modifiers.new("Smooth", "SMOOTH")
  lap.factor = SKIN["smooth_factor"]
  lap.iterations = SKIN["smooth_iters"]
  lap.vertex_group = "SmoothMask"
  bpy.ops.object.modifier_apply(modifier=lap.name)
  bpy.data.objects.remove(coarse, do_unlink=True)
  skin.vertex_groups.remove(skin.vertex_groups["SmoothMask"])
  dec = skin.modifiers.new("Decimate", "DECIMATE")
  dec.ratio = min(1.0, SKIN["faces"] / max(1, len(skin.data.polygons)))
  bpy.ops.object.modifier_apply(modifier=dec.name)
  return skin


if not CACHED:
    skin = build_skin()
    if CACHE:
        bpy.ops.wm.save_as_mainfile(filepath=CACHE)

# ---- 4. shape keys -------------------------------------------------------------------------
# Bust and glutes are sculpted as anatomical volumes the skin wraps onto: for every vertex
# under a volume, march along its normal to the volume's surface. Breasts are teardrops
# (sloping upper pole, full rounded lower pole, a fold beneath, pointing slightly out and
# down); glutes are two rounded masses that slope into the lower back, round off at the
# bottom into the gluteal fold, with the cleft between them. A diffused copy of the push
# blends the edges into the body (smooth union).
import numpy as np

me = skin.data
nv = len(me.vertices)
MW = skin.matrix_world
P = np.array([tuple(MW @ v.co) for v in me.vertices])
NRM = np.array([tuple((MW.to_3x3() @ v.normal).normalized()) for v in me.vertices])
EDGES = np.array([tuple(e.vertices) for e in me.edges])
inv = MW.inverted().to_3x3()
M = 0.01   # metres per cm
UP = np.array((0.0, 0.0, 1.0))
FWD = np.array(tuple(fwd))
SIDE = np.array(tuple(side))
B = {k: np.array(tuple(v)) for k, v in bones.items()}
LEFT = 1.0 if (B["thigh_l"] - B["pelvis"]).dot(SIDE) > 0 else -1.0   # sign of her left along SIDE


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


def taubin(X, mask, iters, lam=0.5, mu=-0.53):
    """Smooth positions without shrinking; mask (0..1) says where."""
    m = mask[:, None]
    for _ in range(iters):
        for k in (lam, mu):
            X = X + m * k * (diffuse(X, 1) * 2.0 - X - X)   # diffuse(.,1) = (X + avg)/2
    return X


HANDFEET = None   # set below, after group_weight exists
NS = diffuse(NRM, 6)
NS /= np.linalg.norm(NS, axis=1, keepdims=True)


def group_weight(keys):
    idx = {g.index for g in skin.vertex_groups if any(k in g.name for k in keys)}
    w = np.zeros(nv)
    for v in me.vertices:
        w[v.index] = sum(g.weight for g in v.groups if g.group in idx)
    return w


ARMS = group_weight(("upperarm", "lowerarm", "hand", "thumb", "index", "middle", "ring", "pinky"))
LEGS_LOW = group_weight(("calf", "foot", "ball"))
HANDFEET = group_weight(("hand", "thumb", "index", "middle", "ring", "pinky", "foot", "ball"))
# the remesh leaves faint ripples on the torso: iron them out (not hands and feet)
P = taubin(P, np.clip(1.0 - HANDFEET * 2.0, 0, 1), 12)
for i, v in enumerate(me.vertices):
    v.co = inv @ Vector(tuple(P[i]))
me.update()
NRM = np.array([tuple((MW.to_3x3() @ v.normal).normalized()) for v in me.vertices])
NS = diffuse(NRM, 6)
NS /= np.linalg.norm(NS, axis=1, keepdims=True)


def frame(direction, lat, yaw, pitch):
    """Axes for a lobe: f points out of the body (turned yaw deg outward, pitch deg down)."""
    out = SIDE * lat
    f = direction * math.cos(math.radians(yaw)) + out * math.sin(math.radians(yaw))
    f = f * math.cos(math.radians(pitch)) - UP * math.sin(math.radians(pitch))
    f /= np.linalg.norm(f)
    u = UP - f * UP.dot(f)
    u /= np.linalg.norm(u)
    l = out - f * out.dot(f) - u * out.dot(u)
    l /= np.linalg.norm(l)
    return f, u, l


def lobe(anchor, axes, L):
    """Push (metres, along the smoothed normal) that wraps the skin onto one lobe."""
    f, u, l = axes
    C = anchor - f * L["sink"] * M
    rz_front = (L["proj"] + L["sink"]) * M

    def inside(Q):
        x, y, z = Q @ l, Q @ u, Q @ f
        rx = np.where(x > 0, L["lat"], L["med"]) * M
        ry = np.where(y > 0, L["up"], L["down"]) * M
        # upper pole slopes: less projection towards the top
        rz = np.where(z > 0, rz_front * (1.0 - L["slope"] * np.clip(y / (L["up"] * M), 0, 1)), L["back"] * M)
        return (x / rx) ** 2 + (y / ry) ** 2 + (z / rz) ** 2 < 1.0

    Q = P - C
    live = inside(Q) & (ARMS < 0.3) & (LEGS_LOW < 0.3) & (NS @ f > -0.1)
    push = np.zeros(nv)
    step = 0.1 * M
    for k in range(int((L["proj"] + 4) / 0.1)):
        if not live.any():
            break
        t = (k + 1) * step
        still = inside(Q[live] + NS[live] * t)
        idx = np.nonzero(live)[0]
        push[idx[~still]] = t
        live[idx[~still]] = False
    soft = diffuse(push, L["blend"])
    return diffuse(np.maximum(push, soft * L["fillet"]), 4)   # smooth union, then ease the seam


def relief(anchor, axes, L):
    """Glute push as a smooth relief: zero at the waist, rising to full projection, then a
    quick fall into the gluteal fold; full laterally, easing off into the cleft."""
    f, u, l = axes
    D = P - anchor
    x, y = (D @ l) / M, (D @ u) / M
    # rounded outline: an elliptical distance with a different reach per quadrant
    rx = np.where(x >= 0, L["lat"], L["med"])
    ry = np.where(y >= 0, L["up"], L["down"])
    r = np.clip(np.sqrt((x / rx) ** 2 + (y / ry) ** 2), 0, 1)
    upper = (1 - r ** 2) ** 2                 # long slope into the back and the hip
    lower = 1 - r ** L["fold"]                # stays full, then turns under into the fold
    w = np.clip(-y / (L["down"] * 0.5), 0, 1)
    V = upper * (1 - w) + lower * w
    H = 1.0
    facing = np.clip((NS @ f) / 0.6, 0, 1)
    keep = (ARMS < 0.3) & (LEGS_LOW < 0.3)
    return L["proj"] * M * V * H * facing * keep


def surface_point(z, lateral, direction):
    """Skin point furthest along direction at a height and side offset (metres)."""
    lat = (P - B["pelvis"]) @ SIDE
    near = (np.abs(P[:, 2] - z) < 0.015) & (np.abs(lat - lateral) < 0.015) & (ARMS < 0.3)
    i = np.nonzero(near)[0]
    return P[i[np.argmax(P[i] @ direction)]]


shoulder = abs((B["upperarm_l"] - B["pelvis"]).dot(SIDE))
hip = abs((B["thigh_l"] - B["pelvis"]).dot(SIDE))
chest_z = B["spine_03"][2] + (B["clavicle_l"][2] - B["spine_03"][2]) * SKIN["bust_height"]
glute_z = B["thigh_l"][2] + SKIN["glute_height"] * M

BUST = dict(sink=1.2, proj=SKIN["bust_proj"], med=6.5, lat=9.0, up=10.5, down=6.2, slope=0.55, back=6.0,
            blend=30, fillet=1.0)
GLUTE = dict(proj=SKIN["glute_proj"], med=8.0, lat=12.5, up=17.0, down=10.5, fold=3.0)

pushes = {}
for name, lat in (("L", LEFT), ("R", -LEFT)):
    a = surface_point(chest_z, lat * shoulder * SKIN["bust_spread"], FWD)
    pushes["Bust" + name] = lobe(a, frame(FWD, lat, 12, 6), BUST)
    g = surface_point(glute_z, lat * hip * SKIN["glute_spread"], -FWD)
    pushes["Glute" + name] = relief(g, frame(-FWD, lat, 10, -4), GLUTE)
    print(name, "bust anchor", a.round(3), "glute anchor", g.round(3))


def key_from_offsets(name, off):
    key = skin.shape_key_add(name=name, from_mix=False)
    for i in np.nonzero(np.abs(off).sum(axis=1) > 1e-6)[0]:
        key.data[i].co = me.vertices[i].co + inv @ Vector(tuple(off[i]))
    return key


skin.shape_key_add(name="Basis", from_mix=False)
bust_push = np.maximum(pushes["BustL"], pushes["BustR"])
glute_push = np.maximum(pushes["GluteL"], pushes["GluteR"])


def shaped(push, iters):
    """Offsets for a push, with the pushed surface relaxed so large moves stay smooth."""
    off = diffuse(NS * push[:, None], 6)
    mask = np.clip(diffuse(push, 6) / (0.4 * M), 0, 1)
    return taubin(P + off, mask, iters) - P


key_from_offsets("BustUp", shaped(bust_push, 25))
# the two reliefs meet at the top of the cleft in small domes (the clothes builder found
# them through the nylon's sheen): melt the push there into its surroundings
lat_g = (P - B["pelvis"]) @ SIDE
top_inner = np.clip((P[:, 2] - (glute_z - 7.0 * M)) / (5.0 * M), 0, 1) * np.clip(1.0 - np.abs(lat_g) / (11.0 * M), 0, 1)
top_inner = diffuse(top_inner, 6)
glute_push = glute_push * (1 - top_inner) + diffuse(glute_push, 80) * top_inner
key_from_offsets("GlutesUp", shaped(glute_push, 30))
# bounce: the soft tissue moves as a block, most where it is thickest
for part, proj, amp in (("Bust", SKIN["bust_proj"], SKIN["bust_bounce"]), ("Glute", SKIN["glute_proj"], SKIN["glute_bounce"])):
    for side_name in ("L", "R"):
        # the bust swings from its base: the tip travels, the root stays on the chest
        m = np.clip(pushes[part + side_name] / (proj * M), 0, 1) ** (1.3 if part == "Bust" else 0.7)
        m = diffuse(m, 3)
        if part == "Bust":
            out = SIDE * (LEFT if side_name == "L" else -LEFT)
            # sway out/in and surge forward/back, each with a little drop as it swings (an arc)
            for nm, d, a in (("Out", out, SKIN["bust_sway"]), ("In", -out, SKIN["bust_sway"]),
                             ("Fwd", FWD, SKIN["bust_surge"]), ("Back", -FWD, SKIN["bust_surge"])):
                key_from_offsets("BustSway%s_%s" % (side_name, nm), (d - UP * 0.25)[None, :] * (a * M) * m[:, None])
        key_from_offsets("%sBounce%s_Up" % (part, side_name), UP[None, :] * (amp * M) * m[:, None])
        key_from_offsets("%sBounce%s_Down" % (part, side_name), -UP[None, :] * (amp * M) * m[:, None])
# spacing: each breast slides outward as a block (turned a little outward too), opening
# the cleavage; the Bust spacing slider drives it, negative pulls them together
apart = np.zeros((nv, 3))
for side_name, lat in (("L", LEFT), ("R", -LEFT)):
    m = diffuse(np.clip(pushes["Bust" + side_name] / (SKIN["bust_proj"] * M), 0, 1) ** 0.6, 4)
    out = SIDE * lat
    apart += (out * 1.0 - FWD * 0.15)[None, :] * (SKIN["bust_apart"] * M) * m[:, None]
# ...and the fill the smooth union left between them sinks back, so a real cleavage opens
lat_c = (P - B["pelvis"]) @ SIDE
cleft = np.exp(-(lat_c / (3.0 * M)) ** 2) * SKIN["bust_cleft"]
apart += -NS * (bust_push * cleft)[:, None]
apart = diffuse(apart, 3)
key_from_offsets("BustApart", apart)
print("keys", [k.name for k in me.shape_keys.key_blocks])

# one material slot is enough for the skin
me.materials.clear()
mat = bpy.data.materials.new("M_Skin")
me.materials.append(mat)
print("skin verts", len(me.vertices), "faces", len(me.polygons), "groups", len(skin.vertex_groups))

# ---- previews ------------------------------------------------------------------------------
if PREVIEW:
    os.makedirs(PREVIEW, exist_ok=True)
    src.hide_render = True
    scn = bpy.context.scene
    scn.render.engine = "BLENDER_WORKBENCH"
    scn.render.resolution_x, scn.render.resolution_y = 900, 1200
    scn.display.shading.light = "STUDIO"
    scn.display.shading.color_type = "SINGLE"
    scn.display.shading.single_color = (0.85, 0.83, 0.80)
    scn.display.shading.show_cavity = False
    cam_data = bpy.data.cameras.new("Cam")
    cam_data.lens = 85
    cam = bpy.data.objects.new("Cam", cam_data)
    bpy.context.collection.objects.link(cam)
    scn.camera = cam
    target = (bones["spine_02"] + bones["pelvis"]) * 0.5
    for label, d in (("front", fwd), ("side", side), ("back", -fwd), ("q", (fwd + side).normalized())):
        cam.location = target + d * 2.6 + Vector((0, 0, 0.05))
        cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
        for key_name, val in (("base", 0.0), ("mid", 0.6), ("up", 1.0)):
            for kb in me.shape_keys.key_blocks[1:]:
                kb.value = val if kb.name in ("BustUp", "GlutesUp") else 0.0
            scn.render.filepath = os.path.join(PREVIEW, "%s_%s.png" % (label, key_name))
            bpy.ops.render.render(write_still=True)
    for kb in me.shape_keys.key_blocks[1:]:
        kb.value = 0.0

# ---- 5. export -----------------------------------------------------------------------------
skin.modifiers.new("Armature", "ARMATURE").object = arm
bpy.data.objects.remove(src, do_unlink=True)
for o in list(bpy.data.objects):
    if o.type not in ("ARMATURE", "MESH"):
        bpy.data.objects.remove(o, do_unlink=True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.splitext(OUT)[0] + ".blend")   # Tools/blender_outfits.py starts here
bpy.ops.object.select_all(action="DESELECT")
arm.select_set(True)
skin.select_set(True)
bpy.ops.export_scene.fbx(filepath=OUT, use_selection=True, object_types={"ARMATURE", "MESH"},
                         add_leaf_bones=False, use_armature_deform_only=False,
                         mesh_smooth_type="FACE", bake_anim=False, axis_forward="-Z", axis_up="Y")
print("wrote", OUT)
