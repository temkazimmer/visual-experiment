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
    bust_radius=9.0, bust_up=3.2, bust_down=-1.6,      # cm
    glute_radius=10.5, glute_up=2.6, glute_down=-1.4,   # cm
)

args = sys.argv[sys.argv.index("--") + 1:]
SRC, OUT = args[0], args[1]
PREVIEW = args[2] if len(args) > 2 else None

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=SRC)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
src = next(o for o in bpy.data.objects if o.type == "MESH")
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

# ---- 4. shape keys -------------------------------------------------------------------------
me = skin.data
me.calc_normals_split() if hasattr(me, "calc_normals_split") else None
W = [skin.matrix_world @ v.co for v in me.vertices]
N = [(skin.matrix_world.to_3x3() @ v.normal).normalized() for v in me.vertices]
inv = skin.matrix_world.inverted().to_3x3()
M = 0.01   # metres per cm


def peak(zlo, zhi, direction, lat_lo, lat_hi, sign):
    """The vertex reaching furthest along direction in a band on one side."""
    best, best_d = None, -1e9
    for i, p in enumerate(W):
        lat = (p - bones["pelvis"]).dot(side) * sign
        if zlo <= p.z <= zhi and lat_lo * M <= lat <= lat_hi * M:
            d = p.dot(direction)
            if d > best_d:
                best, best_d = p, d
    return best


chest_lo, chest_hi = bones["spine_03"].z, bones["clavicle_l"].z
hip_lo, hip_hi = bones["thigh_l"].z - 0.16, bones["pelvis"].z + 0.04
def front_point(z, lateral, direction, sign):
    """Surface point furthest along direction near a given height and side offset."""
    best, best_d = None, -1e9
    for p in W:
        lat = (p - bones["pelvis"]).dot(side) * sign
        if abs(p.z - z) < 0.015 and abs(lat - lateral) < 0.015:
            d = p.dot(direction)
            if d > best_d:
                best, best_d = p, d
    return best


shoulder = abs((bones["upperarm_l"] - bones["pelvis"]).dot(side))
hip = abs((bones["thigh_l"] - bones["pelvis"]).dot(side))
z_bust = peak(chest_lo, chest_hi, fwd, 0, 20, 1).z
bust = [front_point(z_bust, shoulder * 0.52, fwd, s) for s in (1, -1)]
glutes = [front_point((bones["thigh_l"].z + bones["pelvis"].z) * 0.5 - 0.02, hip * 0.95, -fwd, s) for s in (1, -1)]
print("bust centres", [tuple(round(c, 3) for c in b) for b in bust])
print("glute centres", [tuple(round(c, 3) for c in g) for g in glutes])


def add_key(name, centres, radius_cm, amount_cm, direction, lift):
    key = skin.shape_key_add(name=name, from_mix=False)
    r = radius_cm * M
    for i, p in enumerate(W):
        if N[i].dot(direction) <= 0.0:
            continue
        w = 0.0
        for c in centres:
            d = (p - c).length
            if d < r:
                t = 1.0 - d / r
                w = max(w, t * t * (3.0 - 2.0 * t))          # smoothstep falloff
        if w <= 0.0:
            continue
        facing = min(1.0, N[i].dot(direction) * 1.4)
        offset = (direction * amount_cm + Vector((0, 0, lift * amount_cm))) * M * w * facing
        key.data[i].co = me.vertices[i].co + inv @ offset


skin.shape_key_add(name="Basis", from_mix=False)
add_key("BustUp", bust, SKIN["bust_radius"], SKIN["bust_up"], fwd, -0.25)
add_key("BustDown", bust, SKIN["bust_radius"], SKIN["bust_down"], fwd, 0.0)
add_key("GlutesUp", glutes, SKIN["glute_radius"], SKIN["glute_up"], -fwd, 0.15)
add_key("GlutesDown", glutes, SKIN["glute_radius"], SKIN["glute_down"], -fwd, 0.0)

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
    target = bones["spine_02"]
    for label, d in (("front", fwd), ("side", side), ("back", -fwd)):
        cam.location = target + d * 4.2 + Vector((0, 0, 0.05))
        look = (target - cam.location).to_track_quat("-Z", "Y")
        cam.rotation_euler = look.to_euler()
        for key_name, val in (("base", 0.0), ("up", 1.0)):
            for kb in me.shape_keys.key_blocks[1:]:
                kb.value = val if kb.name.endswith("Up") else 0.0
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
bpy.ops.object.select_all(action="DESELECT")
arm.select_set(True)
skin.select_set(True)
bpy.ops.export_scene.fbx(filepath=OUT, use_selection=True, object_types={"ARMATURE", "MESH"},
                         add_leaf_bones=False, use_armature_deform_only=False,
                         mesh_smooth_type="FACE", bake_anim=False, axis_forward="-Z", axis_up="Y")
print("wrote", OUT)
