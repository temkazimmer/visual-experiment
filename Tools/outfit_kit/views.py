"""Outfit Kit: her default body as three orthographic views for tracing in Figma.

    blender -b --factory-startup --python Tools/outfit_kit/views.py

Writes SourceArt/Character/OutfitKit/views/{front,side,back}.png and views.json. Every view is
10 px per cm, so a vector drawn on a view maps back onto her skin exactly (Tools/outfit_kit/figma_import.py).
"""
import json
import os
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(__file__))
import sketch  # noqa: E402  (her default body, the same as blender_outfit.py)

PX = 10.0                      # px per cm
Z0, Z1 = -5.0, 195.0           # cm, bottom and top of every view
VIEWS = {   # name: (camera position direction, image-right axis, horizontal range in cm)
    "front": ((0, -1, 0), (1, 0, 0), (-100.0, 100.0)),   # her front; her left on image right
    "side": ((1, 0, 0), (0, 1, 0), (-40.0, 40.0)),       # her left side; her front on image left
    "back": ((0, 1, 0), (-1, 0, 0), (-100.0, 100.0)),    # her back; her left on image left
}
OUT = os.path.join(sketch.KIT, "views")

ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUTFIT = ARGS[ARGS.index("outfit") + 1] if "outfit" in ARGS else None
body, X, info, masks = sketch.build_body(pose="T")
body.color = (0.80, 0.77, 0.74, 1)
suit = None
if OUTFIT:   # an existing outfit, posed on her default body like the views
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=os.path.join(sketch.ROOT, "SourceArt", "Character", "Outfits",
                                                   "outfit_%s.fbx" % OUTFIT.lower()))
    new = set(bpy.data.objects) - before
    suit = next(o for o in new if o.type == "MESH")
    sarm = next(o for o in new if o.type == "ARMATURE")
    sketch.t_pose(sarm)
    P = sketch.posed_points(suit, sketch.DESIGN)
    if suit.data.shape_keys:
        suit.shape_key_clear()
    suit.parent = None
    suit.modifiers.clear()
    suit.matrix_world = ((1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 0, 0, 1))
    suit.data.vertices.foreach_set("co", P.ravel())
    suit.data.update()
    for o in new:
        if o != suit:
            bpy.data.objects.remove(o)
    suit.color = (0.74, 0.77, 0.84, 1)


def dust_look():
    """Lvl_Dust in a box: orange haze sky, sand below (what the chrome reflects), a low warm sun."""
    sc.render.engine = "BLENDER_EEVEE"
    w = sc.world = sc.world or bpy.data.worlds.new("Dust")
    w.use_nodes = True
    nt = w.node_tree
    nt.nodes.clear()
    tc, sep = nt.nodes.new("ShaderNodeTexCoord"), nt.nodes.new("ShaderNodeSeparateXYZ")
    ramp, bg, out = nt.nodes.new("ShaderNodeValToRGB"), nt.nodes.new("ShaderNodeBackground"), nt.nodes.new("ShaderNodeOutputWorld")
    nt.links.new(tc.outputs["Generated"], sep.inputs[0])
    nt.links.new(sep.outputs["Z"], ramp.inputs[0])
    nt.links.new(ramp.outputs[0], bg.inputs[0])
    nt.links.new(bg.outputs[0], out.inputs[0])
    el = ramp.color_ramp.elements
    el[0].position, el[0].color = 0.40, (0.50, 0.30, 0.16, 1)        # sand floor
    el[1].position, el[1].color = 0.62, (1.5, 1.0, 0.62, 1)          # bright haze overhead
    e = el.new(0.495)
    e.color = (0.80, 0.55, 0.34, 1)                                   # dunes at the horizon (the backdrop)
    e = el.new(0.505)
    e.color = (0.95, 0.58, 0.32, 1)                                   # haze just above them
    bg.inputs[1].default_value = 1.0
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sc.collection.objects.link(sun)
    sun.data.energy, sun.data.color, sun.data.angle = 3.5, (1.0, 0.80, 0.58), 0.05
    sun.rotation_euler = Vector((0.55, -0.75, 0.35)).normalized().to_track_quat("Z", "Y").to_euler()
    chrome = bpy.data.materials.new("Chrome")
    chrome.use_nodes = True
    b = chrome.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (0.80, 0.83, 0.88, 1)
    b.inputs["Metallic"].default_value, b.inputs["Roughness"].default_value = 1.0, 0.13
    skin = bpy.data.materials.new("Skin")
    skin.use_nodes = True
    k = skin.node_tree.nodes["Principled BSDF"]
    k.inputs["Base Color"].default_value = (0.62, 0.40, 0.31, 1)
    k.inputs["Roughness"].default_value = 0.45
    for i in range(len(suit.data.materials)):
        suit.data.materials[i] = chrome
    body.data.materials.clear()
    body.data.materials.append(skin)
    sc.view_settings.view_transform = "Standard"
    sc.display.shading.background_type = "WORLD"
os.makedirs(OUT, exist_ok=True)
sc = bpy.context.scene
sc.render.engine = "BLENDER_WORKBENCH"
sh = sc.display.shading
sh.light, sh.color_type, sh.show_cavity = "STUDIO", "OBJECT", True
sh.show_specular_highlight = True
sh.cavity_type, sh.cavity_ridge_factor, sh.cavity_valley_factor = "BOTH", 1.0, 1.4
sh.single_color = (0.80, 0.77, 0.74)
sh.show_object_outline = False
sh.background_type, sh.background_color = "VIEWPORT", (1.0, 1.0, 1.0)
sc.render.film_transparent = False
sc.view_settings.view_transform = "Standard"
cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
sc.collection.objects.link(cam)
cam.data.type = "ORTHO"
sc.camera = cam
if suit:
    dust_look()
meta = dict(px_per_cm=PX, z_top=Z1, z_bottom=Z0, landmarks=info["landmarks"], views={})
for name, (d, right, (h0, h1)) in VIEWS.items():
    w, h = int((h1 - h0) * PX), int((Z1 - Z0) * PX)
    sc.render.resolution_x, sc.render.resolution_y = w, h
    cam.data.ortho_scale = max(h1 - h0, Z1 - Z0)
    d, r = Vector(d), Vector(right)
    centre = r * ((h0 + h1) / 2) + Vector((0, 0, (Z0 + Z1) / 2))
    cam.location = centre + d * 300
    cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
    cam.data.clip_end = 1000
    shots = [(name, False)] if not suit else [("%s_%s" % (name, OUTFIT.lower()), False),
                                                ("%s_%s_only" % (name, OUTFIT.lower()), True)]
    for fn, alone in shots:
        body.hide_render = alone                 # alone: the garment by itself (shop render)
        sc.render.film_transparent = alone
        sc.render.filepath = os.path.join(OUT, fn + ".png")
        bpy.ops.render.render(write_still=True)
    body.hide_render = False
    meta["views"][name] = dict(file=name + ".png", width=w, height=h, toward_camera=list(d), image_right=list(r),
                               h_range_cm=[h0, h1],
                               note="pixel (u, v) -> cm: along image_right = h0 + u / px, z = z_top - v / px")
if not OUTFIT:
  with open(os.path.join(OUT, "views.json"), "w") as f:
    json.dump(meta, f, indent=1)
print("KIT views ->", OUT)
