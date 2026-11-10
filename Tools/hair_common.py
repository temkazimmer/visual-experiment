"""Shared Blender helpers for the hair-card tools (research agent).

Loads the skin FBX, strips the old hair, and renders review views. Run under Blender:
  /opt/homebrew/bin/blender -b --python Tools/<script>.py -- <args>
"""
import math
import os
import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

SKIN_FBX = os.environ.get("HAIR_SKIN", "/Users/tema/Documents/Unreal Projects/visex/SourceArt/Character/android_skin.fbx")


def load_skin(fbx=SKIN_FBX):
    """Import the skin FBX, delete the old hair faces (material 'Hair'). Returns (skin, armature)."""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=fbx)
    skin = next(o for o in bpy.data.objects if o.type == "MESH")
    arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    names = [m.name if m else "" for m in skin.data.materials]
    hair_i = names.index("Hair")
    bpy.context.view_layer.objects.active = skin
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="DESELECT")
    skin.active_material_index = hair_i
    bpy.ops.object.material_slot_select()
    bpy.ops.mesh.delete(type="FACE")
    bpy.ops.object.mode_set(mode="OBJECT")
    return skin, arm


def world_verts(obj):
    me = obj.data
    co = np.zeros(len(me.vertices) * 3)
    me.vertices.foreach_get("co", co)
    return co.reshape(-1, 3)


def bvh_of(obj):
    me = obj.data
    me.calc_loop_triangles()
    co = [v.co.copy() for v in me.vertices]
    tris = [tuple(t.vertices) for t in me.loop_triangles]
    return BVHTree.FromPolygons(co, tris)


def setup_render(res=(1920, 1080), samples=48):
    scn = bpy.context.scene
    scn.render.resolution_x, scn.render.resolution_y = res
    scn.render.resolution_percentage = 100
    scn.render.engine = "CYCLES"
    scn.cycles.device = "CPU"
    scn.cycles.samples = samples
    scn.cycles.use_denoising = True
    scn.cycles.transparent_max_bounces = 128
    scn.cycles.max_bounces = 16
    scn.render.film_transparent = False
    w = bpy.data.worlds.new("w")
    w.use_nodes = True
    bg = w.node_tree.nodes["Background"]
    bg.inputs[0].default_value = (0.62, 0.40, 0.22, 1)   # dust haze
    bg.inputs[1].default_value = 0.8
    scn.world = w
    return scn


def lights(target=Vector((0, 0, 150))):
    """Low warm sun from front-left, cool fill from the right: the plaza's light."""
    for name, rot, energy, col in (
        ("key", (math.radians(62), 0, math.radians(-35)), 4.0, (1.0, 0.72, 0.45)),
        ("fill", (math.radians(75), 0, math.radians(140)), 1.2, (0.55, 0.65, 0.95)),
        ("rim", (math.radians(100), 0, math.radians(180)), 2.5, (1.0, 0.8, 0.6)),
    ):
        d = bpy.data.lights.new(name, "SUN")
        d.energy = energy
        d.color = col
        o = bpy.data.objects.new(name, d)
        o.rotation_euler = rot
        bpy.context.collection.objects.link(o)


def shoot(path, eye, target, lens=85):
    cam = bpy.data.cameras.new("cam")
    cam.lens = lens
    cam.clip_start, cam.clip_end = 1, 2000
    o = bpy.data.objects.new("cam", cam)
    bpy.context.collection.objects.link(o)
    o.location = eye
    d = Vector(target) - Vector(eye)
    o.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
    bpy.context.scene.camera = o
    bpy.context.scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(o)
