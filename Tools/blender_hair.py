"""Hair for the android: short, dark, layered spikes (Tema's reference: 90s anime).
Headless, after Tools/blender_face.py:

  /Applications/Blender.app/Contents/MacOS/Blender -b <android_skin.blend> \
      --python Tools/blender_hair.py -- <out.fbx> [<preview_dir>]

The hair is solid locks, not strands: each one a flat blade that tapers to a point. They
grow from a cap on the scalp, start out along the head away from the crown, are lifted a
little, then fall; they never go inside the head, so they drape over the skull and part
around the ears. Three layers: a long under layer, a middle one with the fringe, and
short lifted ones at the crown.
The hair is joined to the skin as a second material slot ("Hair") and follows the head
bone, so nothing changes in the game code. Outfits should start from the blend that
blender_face.py wrote, which has no hair. Writes <out>.fbx and <out>.blend.
"""
import os
import sys
from math import cos, pi, radians, sin

import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

args = sys.argv[sys.argv.index("--") + 1:]
OUT = args[0]
PREVIEW = args[1] if len(args) > 1 else None

LOOK = dict(
    seed=7,
    cap=0.25,             # cm the cap sits off the scalp
    # the hairline, as elevation (degrees above the head's centre) by direction
    line_front=52.0, line_side=14.0, line_back=-30.0,
    side_width=3.7, sides=7, steps=20,
    tip_bulge=0.45,        # how much a lock fills out toward its end (0 = plain taper)
    tip_taper=0.82,       # where along the lock the final point starts (0..1)
    thickness=0.30,       # a lock's thickness against its width
    # layers: rows of locks. elevation band above the hairline (0 = on it, 1 = the crown),
    # how many around, length (cm), lift off the head (degrees), how fast they fall, clearance
    layers=(
        dict(name="under",  band=(0.03, 0.14), count=30, length=(17.0, 23.0), lift=5,  fall=0.34, clear=0.30),
        dict(name="middle", band=(0.26, 0.44), count=28, length=(18.0, 24.0), lift=10, fall=0.30, clear=0.70),
        dict(name="upper",  band=(0.52, 0.68), count=22, length=(17.0, 22.0), lift=14, fall=0.28, clear=1.05),
        dict(name="crown",  band=(0.80, 0.93), count=12, length=(15.0, 20.0), lift=18, fall=0.26, clear=1.35),
    ),
    fringe=0.22,          # the fringe's length against the rest (it stops at the eyes)
    flick=0.11,           # how much the tips turn outward
    wander=16.0,          # degrees a lock strays from straight down the head
)

rng = np.random.default_rng(LOOK["seed"])
skin = next(o for o in bpy.data.objects if o.type == "MESH")
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
me = skin.data
gi = skin.vertex_groups["head"].index
co = np.zeros(len(me.vertices) * 3)
me.vertices.foreach_get("co", co)
co = co.reshape(-1, 3)
hw = np.array([next((g.weight for g in v.groups if g.group == gi), 0.0) for v in me.vertices])
skull = co[(hw > 0.9) & (np.abs(co[:, 0]) < 7.5)]
top = skull[:, 2].max()
C = Vector((0.0, (skull[:, 1].min() + skull[:, 1].max()) / 2 + 1.0, top - 9.5))
# the ears are left out: locks pass over their roots and the points come through the hair
ear = np.zeros(len(me.vertices))
me.attributes["ear"].data.foreach_get("value", ear)
polys = [tuple(p.vertices) for p in me.polygons]
full = BVHTree.FromPolygons([tuple(c) for c in co], polys)
bvh = BVHTree.FromPolygons([tuple(c) for c in co], [p for p in polys if not any(ear[i] > 0.5 for i in p)])


def direction(az, el):
    """az 0 = the face (-Y), 90 = her left (+X), 180 = the back; el up from the horizon."""
    a, e = radians(az), radians(el)
    return Vector((sin(a) * cos(e), -cos(a) * cos(e), sin(e)))


def scalp(az, el):
    """The skull's surface in that direction, and its normal (cast from inside: ears don't count)."""
    d = direction(az, el)
    hit, nrm, _, _ = bvh.ray_cast(C, d, 30.0)
    if not hit or (hit - C).length > 13.0:                # through an ear's root: bridge it
        a, _ = scalp(az, el + 14.0)
        hit, nrm = C + d * (a - C).length, d
    assert hit, "no scalp at az %.0f el %.0f" % (az, el)
    if nrm.dot(d) < 0:
        nrm = -nrm
    return hit, nrm


def hairline(az):
    a = abs(((az + 180) % 360) - 180)                  # 0 front .. 180 back
    if a < 90:
        t = a / 90
        t = t * t * (3 - 2 * t)
        return LOOK["line_front"] + (LOOK["line_side"] - LOOK["line_front"]) * t
    t = (a - 90) / 90
    t = t * t * (3 - 2 * t)
    return LOOK["line_side"] + (LOOK["line_back"] - LOOK["line_side"]) * t


V, F, UV = [], [], []

# ---- the cap: the scalp itself, dark, so no skin shows between locks ----------------------
NA, NE = 72, 14
grid = {}
for i in range(NA):
    az = i * 360 / NA
    lo = hairline(az)
    for j in range(NE):
        el = lo + (88.0 - lo) * j / (NE - 1)
        p, n_ = scalp(az, el)
        grid[i, j] = len(V)
        V.append(p + n_ * LOOK["cap"] * min(1.0, 0.25 + j * 0.5))
        UV.append((i / NA, j / NE))
for i in range(NA):
    for j in range(NE - 1):
        F.append((grid[i, j], grid[(i + 1) % NA, j], grid[(i + 1) % NA, j + 1], grid[i, j + 1]))
F.append(tuple(grid[i, NE - 1] for i in range(NA))[::-1])


# ---- locks --------------------------------------------------------------------------------
def lock(root, nrm, heading, length, width, lift, fall, clear):
    """A blade from root: along heading, lifted off the head, then falling. Returns the path."""
    v = (heading * cos(radians(lift)) + nrm * sin(radians(lift))).normalized()
    step = length / LOOK["steps"]
    p = root.copy()
    path = [(p.copy(), nrm.copy())]
    for k in range(LOOK["steps"]):
        t = (k + 1) / LOOK["steps"]
        v = (v + Vector((0, 0, -fall))).normalized()
        p = p + v * step
        near, n2, _, _ = bvh.find_nearest(p)
        out = p - near
        gap = out.length if out.dot(n2) > 0 else -out.length
        want = clear * (1 - 0.5 * t)
        if gap < want:                                    # keep off the head
            p = near + n2 * want
            v = (v - n2 * min(v.dot(n2), 0.0)).normalized()
        if t > 0.6:
            v = (v + n2 * LOOK["flick"]).normalized()
        path.append((p.copy(), n2.copy()))
    for _ in range(3):                                    # no kinks where it met the head
        pts = [q for q, _ in path]
        for k in range(1, len(path) - 1):
            path[k] = ((pts[k - 1] + pts[k] * 2 + pts[k + 1]) / 4, path[k][1])
    return path


def blade(path, width):
    base = len(V)
    ns = LOOK["sides"]
    n = len(path) - 1
    for k, (p, nrm) in enumerate(path):
        t = k / n
        tan = (path[min(k + 1, n)][0] - path[max(k - 1, 0)][0]).normalized()
        side = tan.cross(nrm)
        if side.length < 1e-4:
            side = tan.orthogonal()
        side.normalize()
        up = side.cross(tan).normalized()
        fill = 1 + LOOK["tip_bulge"] * t * t * (3 - 2 * t)
        pt = max(0.0, (t - LOOK["tip_taper"]) / (1 - LOOK["tip_taper"]))
        w = width * min(1.0, 0.55 + 2.2 * t) * fill * (1 - pt ** 1.6) / 2
        if k == n:
            V.append(p)
            UV.append((0.5, 1.0))
            break
        for s_ in range(ns):
            a = 2 * pi * s_ / ns
            # a lens: wide and flat, with a soft ridge on the outer face
            off = side * (cos(a) * w) + up * (sin(a) * w * LOOK["thickness"] * (0.6 + 0.4 * fill) * (1.25 if sin(a) > 0 else 0.6))
            V.append(p + off)
            UV.append((s_ / ns, t))
    for k in range(n - 1):
        for s_ in range(ns):
            a, b = base + k * ns + s_, base + k * ns + (s_ + 1) % ns
            F.append((a, b, b + ns, a + ns))
    tipv = base + n * ns
    for s_ in range(ns):
        F.append((base + (n - 1) * ns + s_, base + (n - 1) * ns + (s_ + 1) % ns, tipv))
    F.append(tuple(base + s_ for s_ in range(ns))[::-1])


count = 0
for L in LOOK["layers"]:
    for i in range(L["count"]):
        az = (i + rng.uniform(-0.3, 0.3)) * 360 / L["count"] + (7 if L["name"] in ("middle", "crown") else 0)
        lo = hairline(az)
        el = lo + (88.0 - lo) * rng.uniform(*L["band"])
        root, nrm = scalp(az, el)
        # away from the crown, along the head
        lower, _ = scalp(az + rng.uniform(-1, 1) * LOOK["wander"], el - 6.0)
        heading = (lower - root)
        heading = (heading - nrm * heading.dot(nrm)).normalized()
        length = rng.uniform(*L["length"])
        a = abs(((az + 180) % 360) - 180)
        if a < 55 and L["name"] != "under":
            length *= LOOK["fringe"] + (1 - LOOK["fringe"]) * (a / 55) ** 2
        if a < 62 and L["name"] == "under":
            # no under layer over the forehead; at the temples it frames the face
            length *= 0.5 if a < 40 else 1.15
        width = LOOK["side_width"] * rng.uniform(0.8, 1.25)
        path = lock(root + nrm * LOOK["cap"], nrm, heading, length, width,
                    L["lift"] * rng.uniform(0.7, 1.3), L["fall"], L["clear"])
        blade(path, width)
        count += 1

hm = bpy.data.meshes.new("Hair")
hm.from_pydata([tuple(v) for v in V], [], F)
uvl = hm.uv_layers.new(name="UVMap")
for lp in hm.loops:
    uvl.data[lp.index].uv = UV[lp.vertex_index]
for p in hm.polygons:
    p.use_smooth = True
hair = bpy.data.objects.new("Hair", hm)
bpy.context.collection.objects.link(hair)
hair.vertex_groups.new(name="head").add(list(range(len(V))), 1.0, "REPLACE")
print("hair: %d locks, %d verts, %d faces" % (count, len(V), len(F)))

# ---- join: skin is slot 0, hair slot 1 ----------------------------------------------------
m_skin = bpy.data.materials.get("Skin") or bpy.data.materials.new("Skin")
m_hair = bpy.data.materials.new("Hair")
m_hair.diffuse_color = (0.02, 0.025, 0.045, 1)
me.materials.clear()
me.materials.append(m_skin)
for p in me.polygons:
    p.material_index = 0
hm.materials.append(m_hair)
for o in bpy.data.objects:
    o.select_set(False)
hair.select_set(True)
skin.select_set(True)
bpy.context.view_layer.objects.active = skin
bpy.ops.object.join()
assert [m.name for m in skin.data.materials] == ["Skin", "Hair"], [m.name for m in skin.data.materials]

# ---- preview ------------------------------------------------------------------------------
if PREVIEW:
    os.makedirs(PREVIEW, exist_ok=True)
    scn = bpy.context.scene
    scn.render.engine = "BLENDER_WORKBENCH"
    sh = scn.display.shading
    sh.light = "STUDIO"
    sh.color_type = "MATERIAL"
    m_skin.diffuse_color = (0.8, 0.6, 0.5, 1)
    sh.show_cavity = True
    sh.cavity_type = "BOTH"
    sh.show_shadows = True
    sh.show_specular_highlight = True
    m_hair.roughness = 0.35
    scn.display.light_direction = (0.6, 0.3, 0.74)
    scn.render.resolution_x, scn.render.resolution_y = 900, 1000
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    scn.collection.objects.link(cam)
    scn.camera = cam
    cam.data.lens = 85
    target = Vector((0, 0, 158))
    for label, offset in (("front", (0, -150, 0)), ("three_quarter", (90, -125, 4)), ("side", (150, 0, 0)),
                          ("back", (-50, 140, 8)), ("top", (20, -50, 90))):
        cam.location = target + Vector(offset)
        cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
        scn.render.filepath = os.path.join(PREVIEW, "hair_%s.png" % label)
        bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(cam, do_unlink=True)

# ---- export -------------------------------------------------------------------------------
bpy.ops.wm.save_as_mainfile(filepath=os.path.splitext(OUT)[0] + ".blend")
bpy.ops.object.select_all(action="DESELECT")
arm.select_set(True)
skin.select_set(True)
bpy.ops.export_scene.fbx(filepath=OUT, use_selection=True, object_types={"ARMATURE", "MESH"},
                         add_leaf_bones=False, use_armature_deform_only=False,
                         mesh_smooth_type="FACE", bake_anim=False, axis_forward="-Z", axis_up="Y")
print("wrote", OUT)
