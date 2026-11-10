"""Render the skin with its old hair stripped: front, side, back. Run: blender -b --python this -- <outdir>"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from mathutils import Vector
from hair_common import *

out = sys.argv[sys.argv.index("--") + 1]
os.makedirs(out, exist_ok=True)
skin, arm = load_skin()
co = world_verts(skin)
head = co[co[:, 2] > co[:, 2].max() - 40]
print("TOP", co[:, 2].max(), "head bbox", head.min(0), head.max(0))
m = bpy.data.materials.new("base")
m.diffuse_color = (0.55, 0.5, 0.45, 1)
skin.data.materials.clear()
skin.data.materials.append(m)
setup_render((1600, 900), 24)
lights()
top = co[:, 2].max()
tgt = (0, 0, top - 12)
for n, eye in (("front", (0, -90, top - 8)), ("side", (90, 0, top - 8)), ("back", (0, 90, top - 8))):
    shoot(os.path.join(out, "base_%s.png" % n), eye, tgt)
