"""Hero sand for the intro: a 1 cm grid draped on the real dune where she walks up the ridge.

A grid along her path (Tools/intro_params.py). The dune surface comes from SourceArt/VisEx/Intro/dune_heights.json (25 cm traces of
SM_Dunes, baked by the intro tooling), smoothed with Catmull-Rom. The scanned ripple
relief (height_2k.npy, from Tools/intro_sand_height.py) is carved in with the same
world mapping as M_SandCover (uv = world xy / 200 cm), so geometry and normal map agree.
Relief fades out toward the patch edges, which sit 4 mm above the dune.

Run: Blender -b -P Tools/intro_sand_mesh.py   ->  SourceArt/VisEx/Intro/SM_IntroSand.fbx
"""
import json
import os
import bpy
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DUNE = os.path.join(ROOT, "SourceArt/VisEx/Intro/dune_heights.json")
RIPPLE = os.path.join(ROOT, "SourceArt/Megascans/RippledSand_vd3lecfs/height_2k.npy")
OUT = os.path.join(ROOT, "SourceArt/VisEx/Intro/SM_IntroSand.fbx")

import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import intro_params as P            # the path, patch extent and grid step

CENTER = P.CENTER                   # actor location in UE (cm); vertices are relative
RIPPLE_SCALE = 1.0                  # x the scan's 4 cm relief
LIFT = 0.4                          # cm above the dune at the edges

def catmull(p0, p1, p2, p3, t):
    return 0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t ** 2
                  + (-p0 + 3 * p1 - 3 * p2 + p3) * t ** 3)


def dune_height(X, Y):
    d = json.load(open(DUNE))
    g = np.array(d["z"], dtype=np.float64)              # [row = y, col = x]
    s = float(d["step"])
    fx = (X - d["x0"]) / s
    fy = (Y - d["y0"]) / s
    ix = np.floor(fx).astype(int)
    iy = np.floor(fy).astype(int)
    tx, ty = fx - ix, fy - iy

    def at(r, c):
        return g[np.clip(r, 0, g.shape[0] - 1), np.clip(c, 0, g.shape[1] - 1)]
    rows = [catmull(at(iy + j, ix - 1), at(iy + j, ix), at(iy + j, ix + 1), at(iy + j, ix + 2), tx)
            for j in (-1, 0, 1, 2)]
    return catmull(rows[0], rows[1], rows[2], rows[3], ty)


def ripple_height(X, Y):
    h = np.load(RIPPLE).astype(np.float64)
    n = h.shape[0]
    u = (X / 200.0) % 1.0 * n                            # M_SandCover: uv8 = WP.xy / 200
    v = (Y / 200.0) % 1.0 * n
    i0, j0 = np.floor(v).astype(int), np.floor(u).astype(int)
    tv, tu = v - i0, u - j0
    i1, j1 = (i0 + 1) % n, (j0 + 1) % n
    i0, j0 = i0 % n, j0 % n
    return ((h[i0, j0] * (1 - tu) + h[i0, j1] * tu) * (1 - tv)
            + (h[i1, j0] * (1 - tu) + h[i1, j1] * tu) * tv)


def smooth01(a):
    a = np.clip(a, 0, 1)
    return a * a * (3 - 2 * a)


us = np.arange(P.PATCH_U[0], P.PATCH_U[1] + P.STEP / 2, P.STEP)
vs = np.arange(-P.PATCH_V, P.PATCH_V + P.STEP / 2, P.STEP)
U, Vv = np.meshgrid(us, vs)                              # [row = v, col = u]
X, Y = P.world(U, Vv)
fade_y = smooth01((P.PATCH_V - np.abs(Vv)) / (P.PATCH_V - P.CORE_V))
fade_x = smooth01((U - P.PATCH_U[0]) / 200.0) * smooth01((P.PATCH_U[1] - U) / 200.0)
fade = fade_y * fade_x
Z = dune_height(X, Y) + LIFT + fade * RIPPLE_SCALE * ripple_height(X, Y)
print("grid", X.shape, "z", float(Z.min()), float(Z.max()))

# Blender mesh in metres around CENTER; Blender y = -UE y (the FBX round trip flips it back)
ny, nx = Z.shape
V = np.stack([(X - CENTER[0]) / 100.0, -(Y - CENTER[1]) / 100.0, Z / 100.0], axis=-1).reshape(-1, 3)
r, c = np.meshgrid(np.arange(ny - 1), np.arange(nx - 1), indexing="ij")
a = (r * nx + c).ravel()
quads = np.stack([a, a + nx, a + nx + 1, a + 1], axis=-1)   # rows +v, cols +u: same handedness as x, y
nq = quads.shape[0]

bpy.ops.wm.read_factory_settings(use_empty=True)
me = bpy.data.meshes.new("SM_IntroSand")
me.vertices.add(V.shape[0])
me.vertices.foreach_set("co", V.astype(np.float32).ravel())
me.loops.add(nq * 4)
me.loops.foreach_set("vertex_index", quads.astype(np.int32).ravel())
me.polygons.add(nq)
me.polygons.foreach_set("loop_start", (np.arange(nq) * 4).astype(np.int32))
uv = me.uv_layers.new(name="UVMap")
UV = np.stack([X / 200.0, Y / 200.0], axis=-1).reshape(-1, 2)[quads.ravel()]
uv.data.foreach_set("uv", UV.astype(np.float32).ravel())
me.update()
ob = bpy.data.objects.new("SM_IntroSand", me)
bpy.context.scene.collection.objects.link(ob)
ob.select_set(True)
bpy.context.view_layer.objects.active = ob
bpy.ops.export_scene.fbx(filepath=OUT, use_selection=True, object_types={"MESH"},
                         mesh_smooth_type="OFF", axis_forward="-Z", axis_up="Y")
print("wrote", OUT, V.shape[0], "verts", nq, "quads")
