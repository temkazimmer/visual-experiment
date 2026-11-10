"""Render saved hair_cards.blend at 4K for review. blender -b <hair_cards.blend> --python this -- <outdir> [view ...]"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy
from hair_common import shoot
args = sys.argv[sys.argv.index("--") + 1:]
out, views = args[0], (args[1:] or ["front", "front34"])
scn = bpy.context.scene
scn.render.resolution_x, scn.render.resolution_y = 3840, 2160
scn.cycles.samples = 48
top = max((bpy.data.objects["SKM_AndroidSkin"].matrix_world @ v.co).z for v in bpy.data.objects["SKM_AndroidSkin"].data.vertices[::50])
tz = top - 20
EYES = {"front": (0, -120, tz + 6), "front34": (85, -90, tz + 8), "side": (120, 0, tz + 6), "back": (0, 120, tz + 6),
        "back34": (-80, 95, tz + 8)}
for v in views:
    shoot(os.path.join(out, "hair4k_%s.png" % v), EYES[v], (0, 0, tz + 1), lens=55)
