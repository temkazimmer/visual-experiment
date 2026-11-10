"""Long loose hair as cards, BG3-style (research agent). Headless:

  HAIR_SKIN=<android_skin.fbx> /opt/homebrew/bin/blender -b --python Tools/blender_hair_cards.py -- <outdir> [preview]

Strands first: guide curves grow from the scalp, fall under gravity and are pushed off the
body (head, ears, neck, shoulders), so the hair drapes instead of hovering. Each guide
becomes one arched ribbon card textured with the procedural strand atlas from
hair_textures.py. Layers (0 under .. 2 top) sit outward from each other; a few short
flyaways break the edge. All look values are in LOOK.
Writes <outdir>/hair_cards.blend, hair_atlas.png and, with "preview", four 4K-ready renders.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy
import numpy as np
from mathutils import Vector

import hair_textures
from hair_common import bvh_of, lights, load_skin, setup_render, shoot, world_verts

args = sys.argv[sys.argv.index("--") + 1:]
OUT = args[0]
PREVIEW = len(args) > 1 and args[1] == "preview"
os.makedirs(OUT, exist_ok=True)

LOOK = dict(
    seed=11,
    guides=900,            # strands that become cards
    part_x=1.6,            # the part, cm to her left of centre
    steps=22,              # points along a card
    length=dict(front=(32, 40), side=(40, 48), back=(48, 58)),
    stiff=(0.90, 0.76),    # how much a strand keeps its heading, root -> tip
    wave_amp=0.9,         # cm
    wave_len=13.0,         # cm
    card_w=(3.6, 5.4),     # cm, a clump card's width
    arch=0.20,             # how much a card bows out of flat, against its width
    margin=0.30,           # cm a card keeps off the body
    layer_margin=0.30,     # cm per layer outward
    face_halfw=9.2,        # cm: hair below the brow stays outside this, so the face shows
    ear_open=0.09,         # radians around each ear (seen from the head centre) hair goes round
    line_front=36.0, line_side=10.0, line_back=-34.0,   # hairline elevation (deg)
)
rng = np.random.default_rng(LOOK["seed"])

skin, arm = load_skin()
co = world_verts(skin)
top = co[:, 2].max()
head = co[(co[:, 2] > top - 30) & (np.abs(co[:, 0]) < 9)]
C = Vector((0.0, (head[:, 1].min() + head[:, 1].max()) / 2 + 0.5, top - 9.5))
bvh = bvh_of(skin)
# the elf ears: vertices that sit outside the skull (a ray from the head's centre meets the
# skull first). Hair ignores them, so it lies against the skull and the ears come through it.
from mathutils import Vector as _V
cand = np.where((co[:, 2] > C.z - 12) & (co[:, 2] < C.z + 10) & (np.abs(co[:, 0]) > 6.5) & (co[:, 1] > C.y - 5))[0]
ear = np.zeros(len(co), bool)
for i in cand:
    v = _V(co[i])
    dvec = v - C
    hit, _, _, dist = bvh.ray_cast(C, dvec.normalized(), dvec.length + 1.0)
    if hit is not None and dvec.length - dist > 0.6:
        ear[i] = True
print("ear verts", int(ear.sum()))
from mathutils.kdtree import KDTree
ear_pts = co[ear]
ear_kd = KDTree(len(ear_pts))
for i, v in enumerate(ear_pts):
    ear_kd.insert(v, i)
ear_kd.balance()
ear_dirs = KDTree(len(ear_pts))
for i, v in enumerate(ear_pts):
    ear_dirs.insert((Vector(v) - C).normalized(), i)
ear_dirs.balance()
_ea = np.degrees(np.arctan2(np.abs(ear_pts[:, 0] - C.x), -(ear_pts[:, 1] - C.y)))   # 0 front .. 180 back
EAR_AZ_FRONT = float(np.percentile(_ea, 3))
EAR_AZ_BACK = float(np.percentile(_ea, 97))
_eel = np.degrees(np.arcsin(np.clip((ear_pts[:, 2] - C.z) / np.linalg.norm(ear_pts - np.array(C), axis=1), -1, 1)))
EAR_EL_TOP = float(np.percentile(_eel, 97))
EAR_R = float(np.percentile(np.linalg.norm(ear_pts - np.array(C), axis=1), 97))
print("ear az", EAR_AZ_FRONT, EAR_AZ_BACK, "el top", EAR_EL_TOP, "r", EAR_R)
print("ear front az", EAR_AZ_FRONT)
me_ = skin.data
me_.calc_loop_triangles()
from mathutils.bvhtree import BVHTree
bvh_body = BVHTree.FromPolygons([tuple(c) for c in co],
                                [tuple(t.vertices) for t in me_.loop_triangles if not ear[list(t.vertices)].any()])


def direction(az, el):
    a, e = math.radians(az), math.radians(el)
    return Vector((math.sin(a) * math.cos(e), -math.cos(a) * math.cos(e), math.sin(e)))


def scalp(az, el):
    d = direction(az, el)
    hit, nrm, _, dist = bvh_body.ray_cast(C, d, 30.0)
    if not hit or dist > 13.0:                       # through an ear: bridge it
        h2, n2 = scalp(az, el + 12.0)
        return C + d * (h2 - C).length, d
    if nrm.dot(d) < 0:
        nrm = -nrm
    return hit, nrm


def hairline(az):
    a = abs(((az + 180) % 360) - 180)
    if a < 90:
        t = a / 90
        t = t * t * (3 - 2 * t)
        return LOOK["line_front"] + (LOOK["line_side"] - LOOK["line_front"]) * t
    t = (a - 90) / 90
    t = t * t * (3 - 2 * t)
    return LOOK["line_side"] + (LOOK["line_back"] - LOOK["line_side"]) * t


def blend(a, b, t):
    return a + (b - a) * t


def region_length(az):
    a = abs(((az + 180) % 360) - 180)               # 0 front .. 180 back
    L = LOOK["length"]
    lo, hi = (L["front"], L["side"]) if a < 90 else (L["side"], L["back"])
    t = (a % 90) / 90
    return rng.uniform(blend(lo[0], hi[0], t), blend(lo[1], hi[1], t))


def grow(az, el, layer, flyaway=False):
    """One guide: list of points, root first."""
    p0, n0 = scalp(az, el)
    margin = LOOK["margin"] + LOOK["layer_margin"] * layer
    p = p0 + n0 * (0.15 + margin * 0.6)
    L = region_length(az) * (rng.uniform(0.55, 0.85) if flyaway else 1.0)
    side = 1.0 if p.x - LOOK["part_x"] > 0 else -1.0
    fa = abs(((az + 180) % 360) - 180)
    sweep = Vector((side, 0.0, 0.0)) * (1.2 if fa < 60 else 0.0)
    # over the ear: swept back above it, so it falls behind the ear
    over_ear = EAR_AZ_FRONT - 4 <= fa <= EAR_AZ_BACK + 10 and el > EAR_EL_TOP - 25
    back = Vector((0.0, 1.1, 0.0)) if over_ear else Vector((0.0, 0.0, 0.0))
    want = Vector((0, 0, -1)) + sweep + back           # where the hair wants to go
    tang = want - n0 * want.dot(n0)                     # ...laid along the scalp
    if tang.length < 1e-3:
        tang = Vector((0, 0.3, -1)) - n0 * Vector((0, 0.3, -1)).dot(n0)
    lift = 0.18 + 0.25 * max(0.0, (el - 45.0) / 45.0)   # a little volume at the crown
    d = (tang.normalized() + n0 * lift).normalized()
    front_of_ear = fa < EAR_AZ_FRONT - 4
    dodged = False
    pts = [p.copy()]
    nsteps = LOOK["steps"]
    step = L / nsteps
    phase = math.radians(az) * 2.3 + rng.uniform(-0.5, 0.5)
    for i in range(1, nsteps + 1):
        t = i / nsteps
        s = blend(LOOK["stiff"][0], LOOK["stiff"][1], t)
        down = Vector((0, 0, -1))
        d = (d * s + down * (1 - s) * (1.0 + 0.5 * t)).normalized()
        wav = math.sin(phase + i * step / LOOK["wave_len"] * 6.283) * LOOK["wave_amp"] * min(1.0, t * 3)
        perp = d.cross(Vector((0, 0, 1)))
        if perp.length > 1e-4:
            d = (d + perp.normalized() * wav * 0.12).normalized()
        p = p + d * step
        # the face shows: from the brow down, hair in front of the face goes out to its sides
        hw = LOOK["face_halfw"] * (1.0 - 0.12 * max(0.0, min(1.0, (C.z - p.z) / 24.0)))
        if p.y < C.y - 1 and C.z - 26 < p.z < C.z + 3 and abs(p.x) < hw:
            sx = 1.0 if p.x >= 0 else -1.0
            p.x = sx * hw
            d = (d + Vector((sx * 0.6, 0.2, 0))).normalized()
        # the ears show: hair that would cross an ear goes round it, in front or behind
        for _ in range(30):
            rel = p - C
            q, _, dd = ear_dirs.find(rel.normalized())
            if dd >= LOOK["ear_open"] or rel.length > EAR_R + 6.0:
                break
            a_ = math.atan2(rel.x, -rel.y)
            back_v = Vector((math.cos(a_), math.sin(a_), 0.0)) * (1.0 if rel.x > 0 else -1.0)
            mv = -back_v if front_of_ear else back_v
            p = p + mv * 0.35
            hit, nrm, _, _ = bvh_body.find_nearest(p)      # ...along the head, not off it
            if hit is not None:
                p = hit + nrm * max(margin, min((p - hit).length, margin + 0.6))
            d = (d + mv * 0.25).normalized()
            dodged = True
        for _ in range(2):                           # keep off the body
            hit, nrm, _, dist = bvh_body.find_nearest(p)
            if hit is not None:
                v = p - hit
                if v.dot(nrm) < 0 or v.length < margin:
                    p = hit + nrm * margin
                    d = (d - nrm * min(0.0, d.dot(nrm))).normalized()
        pts.append(p.copy())
    return pts, dodged


# ---- guides ------------------------------------------------------------------------------
guides = []        # (pts, layer, flyaway, az)
for k in range(LOOK["guides"]):
    az = rng.uniform(0, 360)
    lo = hairline(az)
    u = rng.uniform(0, 1) ** 0.8
    el = lo + (88.0 - lo) * u
    fly = rng.uniform() < 0.07
    layer = 0 if u > 0.62 else int(rng.choice([0, 1, 2], p=[0.25, 0.4, 0.35]))
    if (az < 20 or az > 340) and el < 62:             # the fringe zone: top layer
        layer = 2
    pts, dodged = grow(az, el, layer, fly)
    guides.append((pts, layer, fly, az))
print("guides", len(guides))

# ---- cards -------------------------------------------------------------------------------
TILES = 8
V, F, UVs = [], [], []
n = LOOK["steps"] + 1
for pts, layer, fly, az in guides:
    P = [np.array(x) for x in pts]
    fa = abs(((az + 180) % 360) - 180)
    w0 = rng.uniform(*LOOK["card_w"]) * (0.45 if fly else 1.0) * (0.6 if fa < 45 else 1.0)
    base = len(V)
    prev_n = None
    tile = int(rng.integers(0, TILES // 2)) if layer == 0 else int(rng.integers(TILES // 2, TILES))
    flip = rng.uniform() < 0.5
    for i, p in enumerate(P):
        t = P[min(i + 1, n - 1)] - P[max(i - 1, 0)]
        t = t / (np.linalg.norm(t) + 1e-9)
        out = p - np.array([C.x, C.y, min(p[2], C.z)])     # away from the head's axis
        out = out / (np.linalg.norm(out) + 1e-9)
        nn = out - t * out.dot(t)
        if np.linalg.norm(nn) < 1e-4 and prev_n is not None:
            nn = prev_n
        nn = nn / (np.linalg.norm(nn) + 1e-9)
        prev_n = nn
        b = np.cross(t, nn)
        b = b / (np.linalg.norm(b) + 1e-9)
        s = i / (n - 1)
        w = w0 * (1.0 - 0.22 * s) * (0.55 + 0.45 * min(1.0, s * 6))
        _, _, de = ear_kd.find(Vector(p))
        w *= min(1.0, max(0.25, de / 2.5))            # cards narrow beside an ear
        for off in (-0.5, 0.0, 0.5):
            q = p + b * off * w + nn * (LOOK["arch"] * w * (1 - (2 * off) ** 2) * 0.5)
            V.append(tuple(q))
            uu = (off + 0.5) if not flip else (0.5 - off)
            UVs.append(((tile + 0.03 + 0.94 * uu) / TILES, s))
    for i in range(n - 1):
        for j in range(2):
            a = base + i * 3 + j
            F.append((a, a + 1, a + 4, a + 3))
print("cards", len(guides), "verts", len(V), "quads", len(F))

me = bpy.data.meshes.new("HairCards")
me.from_pydata(V, [], F)
me.update()
uv = me.uv_layers.new(name="UVMap")
for li, loop in enumerate(me.loops):
    uv.data[li].uv = UVs[loop.vertex_index]
for pg in me.polygons:
    pg.use_smooth = True
hair = bpy.data.objects.new("Hair", me)
bpy.context.collection.objects.link(hair)

atlas = os.path.join(OUT, "hair_atlas.png")
hair_textures.make_atlas(atlas, tiles=TILES)

# ---- preview material (the Unreal one is built separately) ----------------------------------
m = bpy.data.materials.new("HairPreview")
m.use_nodes = True
nt = m.node_tree
nt.nodes.clear()
tex = nt.nodes.new("ShaderNodeTexImage")
tex.image = bpy.data.images.load(atlas)
tex.image.colorspace_settings.name = "Non-Color"
sep = nt.nodes.new("ShaderNodeSeparateColor")
nt.links.new(tex.outputs["Color"], sep.inputs["Color"])
ramp = nt.nodes.new("ShaderNodeValToRGB")          # root -> tip colour
nt.links.new(sep.outputs["Blue"], ramp.inputs["Fac"])
ramp.color_ramp.elements[0].position = 0.0
ramp.color_ramp.elements[0].color = (0.004, 0.005, 0.010, 1)
ramp.color_ramp.elements[1].position = 1.0
ramp.color_ramp.elements[1].color = (0.035, 0.042, 0.075, 1)
var = nt.nodes.new("ShaderNodeMath")
var.operation = "MULTIPLY_ADD"
var.inputs[1].default_value = 0.9
var.inputs[2].default_value = 0.45
nt.links.new(sep.outputs["Red"], var.inputs[0])
mul = nt.nodes.new("ShaderNodeMix")
mul.data_type = "RGBA"
mul.blend_type = "MULTIPLY"
mul.inputs["Factor"].default_value = 1.0
nt.links.new(ramp.outputs["Color"], mul.inputs["A"])
nt.links.new(var.outputs["Value"], mul.inputs["B"])
bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
nt.links.new(mul.outputs["Result"], bsdf.inputs["Base Color"])
bsdf.inputs["Roughness"].default_value = 0.42
bsdf.inputs["Specular IOR Level"].default_value = 0.5
bsdf.inputs["Anisotropic"].default_value = 0.9
bsdf.inputs["Sheen Weight"].default_value = 0.2
tan = nt.nodes.new("ShaderNodeTangent")
tan.direction_type = "UV_MAP"
tan.uv_map = "UVMap"
nt.links.new(tan.outputs["Tangent"], bsdf.inputs["Tangent"])
nt.links.new(tex.outputs["Alpha"], bsdf.inputs["Alpha"])
outn = nt.nodes.new("ShaderNodeOutputMaterial")
nt.links.new(bsdf.outputs["BSDF"], outn.inputs["Surface"])
hair.data.materials.append(m)

sm = bpy.data.materials.new("skinprev")
sm.use_nodes = True
sm.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.42, 0.38, 0.34, 1)
sm.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 0.55
skin.data.materials.clear()
skin.data.materials.append(sm)

if PREVIEW:
    setup_render((1600, 900), 64)
    lights()
    tz = top - 22                                     # aim at the chin: head and shoulders in frame
    for nme, eye in (("front", (0, -150, tz + 6)), ("side", (150, 0, tz + 6)), ("back", (0, 150, tz + 6)),
                     ("front34", (105, -110, tz + 8))):
        shoot(os.path.join(OUT, "hair_%s.png" % nme), eye, (0, 0, tz), lens=60)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "hair_cards.blend"))
