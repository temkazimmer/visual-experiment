"""Add the clothes' fit morphs to the android skin (headless). Outside the editor:

  blender -b --factory-startup --python Tools/blender_skin_fit.py -- \
      SourceArt/Character/android_skin.fbx SourceArt/Character/Outfits <out.fbx> \
      [--preview=<dir> --outfit=Tartan]

Tools/blender_outfit.py --fit writes skinfit_<outfit>.npz next to each outfit: per skin vertex,
how the skin is pressed in under the garment and spills out past its edges (Fit_<O>, the seat)
and how that grows with each body slider past the design body (Fit_<O>_<Key>). This adds them
as shape keys, so Tools/char_skin.py imports them as morph targets with the rest; the C++
(UVisexBodyComponent::ApplyFit) drives the worn outfit's set on the skin and the outfit alike.

--preview renders the bust and hips with the outfit at the biggest body, fit off and on.
"""
import glob
import os
import sys

import bpy
import numpy as np

args = sys.argv[sys.argv.index("--") + 1:]
SRC, FIT_DIR, OUT = args[0], args[1], args[2]
opt = dict(a[2:].split("=", 1) for a in args[3:] if a.startswith("--") and "=" in a)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=SRC)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
skin = next(o for o in bpy.data.objects if o.type == "MESH")
me = skin.data
nv = len(me.vertices)

# blender_outfit.py fits to the skin without her hair: the same vertices, in the same order
import bmesh
hair = [i for i, m in enumerate(me.materials) if m and m.name.lower().startswith("hair")]
hb = bmesh.new()
hb.from_mesh(me)
lay = hb.verts.layers.int.new("orig")
for v in hb.verts:
    v[lay] = v.index
if hair:      # exactly the cut blender_outfit.py makes
    bmesh.ops.delete(hb, geom=[f for f in hb.faces if f.material_index in hair], context="FACES")
    bmesh.ops.delete(hb, geom=[v for v in hb.verts if not v.link_faces], context="VERTS")
KEEP = np.array([v[lay] for v in hb.verts])
hb.free()
INV = np.linalg.inv(np.array(skin.matrix_world)[:3, :3]).T     # world cm -> mesh space
if not me.shape_keys:
    skin.shape_key_add(name="Basis", from_mix=False)
basis = np.empty(nv * 3)
me.shape_keys.key_blocks[0].data.foreach_get("co", basis)
basis = basis.reshape(-1, 3)

added = []
for path in sorted(glob.glob(os.path.join(FIT_DIR, "skinfit_*.npz"))):
    data = np.load(path)
    for name in data.files:
        d = data[name]
        if len(d) != len(KEEP):
            raise RuntimeError("%s: %d verts, the skin has %d without hair: rebuild the outfit" % (path, len(d), len(KEEP)))
        full = np.zeros((nv, 3))
        full[KEEP] = d @ INV
        kb = me.shape_keys.key_blocks.get(name) or skin.shape_key_add(name=name, from_mix=False)
        kb.data.foreach_set("co", (basis + full).ravel())
        added.append(name)
print("fit morphs", added)

if "preview" in opt:
    out_dir, outfit = opt["preview"], opt.get("outfit", "Tartan")
    os.makedirs(out_dir, exist_ok=True)
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=os.path.join(FIT_DIR, "outfit_%s.fbx" % outfit.lower()))
    cloth = next(o for o in set(bpy.data.objects) - before if o.type == "MESH")
    for o in set(bpy.data.objects) - before:
        if o.type == "ARMATURE":
            cloth.parent = None
            for m in cloth.modifiers:
                m.show_viewport = m.show_render = False
            bpy.data.objects.remove(o, do_unlink=True)
    for m in skin.modifiers:
        m.show_viewport = m.show_render = False
    for p in me.polygons:
        p.use_smooth = True
    scn = bpy.context.scene
    scn.render.engine = "BLENDER_EEVEE"
    scn.render.resolution_x, scn.render.resolution_y = 1200, 1200
    skin_m = bpy.data.materials.new("PSkin")
    skin_m.use_nodes = True
    bsdf = skin_m.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (0.62, 0.40, 0.32, 1)
    bsdf.inputs["Roughness"].default_value = 0.45
    me.materials.clear()
    me.materials.append(skin_m)
    cl_m = bpy.data.materials.new("PCloth")
    cl_m.use_nodes = True
    cb = cl_m.node_tree.nodes["Principled BSDF"]
    cb.inputs["Base Color"].default_value = (0.35, 0.02, 0.03, 1)
    cb.inputs["Roughness"].default_value = 0.6
    cloth.data.materials.clear()
    cloth.data.materials.append(cl_m)
    world = bpy.data.worlds.new("W")
    scn.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.9, 0.55, 0.3, 1)
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.5
    from mathutils import Vector
    sun = bpy.data.objects.new("Key", bpy.data.lights.new("Key", "SUN"))
    sun.data.energy, sun.data.angle = 4.0, 0.05
    bpy.context.collection.objects.link(sun)
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    cam.data.lens = 85
    bpy.context.collection.objects.link(cam)
    scn.camera = cam
    # her front: the side the toes point to
    B = {b.name: arm.matrix_world @ b.head_local for b in arm.data.bones}
    fwd = ((B["ball_l"] - B["foot_l"]) + (B["ball_r"] - B["foot_r"]))
    fwd.z = 0
    fwd.normalize()
    left = Vector((0, 0, 1)).cross(fwd)
    if (B["thigh_l"] - B["pelvis"]).dot(left) < 0:
        left = -left
    sun.rotation_euler = (-(fwd * 0.6 + left * 0.6 + Vector((0, 0, 0.5)))).to_track_quat("-Z", "Y").to_euler()

    BIG = dict(BustUp=1.0, BustApart=1.097, GlutesUp=1.0, Fat=0.38, AbsLines=0.734, AbsPack=0.12,
               BustBounceL_Up=0.65, BustBounceR_Up=0.65)
    DESIGN = dict(BustUp=0.80, GlutesUp=0.5, Fat=0.38)
    for label, extra in (("big_bust", {}), ("big_bust_more", dict(BustUp=1.3, GlutesUp=1.3))):
        body = dict(BIG, **extra)
        for fit_on in (False, True):
            for ob in (skin, cloth):
                for kb in ob.data.shape_keys.key_blocks[1:]:
                    kb.slider_min, kb.slider_max = -2, 3
                    w = body.get(kb.name, 0.0)
                    if kb.name.startswith("Fit_"):
                        key = kb.name.split("_")[-1]
                        if kb.name == "Fit_" + outfit:
                            w = 1.0 if fit_on else 0.0
                        else:
                            w = max(0.0, body.get(key, 0.0) - DESIGN.get(key, 0.0)) if fit_on and kb.name.startswith("Fit_%s_" % outfit) else 0.0
                    kb.value = w
            for view, d, z, dist in (("front", fwd * 0.9 + left * 0.35, B["spine_04"].z, 105),
                                     ("side", fwd * 0.35 + left * 0.95, B["spine_04"].z, 105),
                                     ("hips", -fwd * 0.85 + left * 0.5, B["pelvis"].z - 6, 90)):
                tgt = Vector((B["pelvis"].x, B["pelvis"].y, z))
                cam.location = tgt + d.normalized() * dist
                cam.rotation_euler = (tgt - cam.location).to_track_quat("-Z", "Y").to_euler()
                scn.render.filepath = os.path.join(out_dir, "%s_%s_%s.png" % (label, view, "fit" if fit_on else "nofit"))
                bpy.ops.render.render(write_still=True)
    sys.exit(0)

bpy.ops.object.select_all(action="DESELECT")
arm.select_set(True)
skin.select_set(True)
bpy.ops.export_scene.fbx(filepath=OUT, use_selection=True, object_types={"ARMATURE", "MESH"},
                         add_leaf_bones=False, use_armature_deform_only=False,
                         mesh_smooth_type="FACE", bake_anim=False, axis_forward="-Z", axis_up="Y")
print("wrote", OUT)
