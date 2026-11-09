"""Lingerie outfits fitted to the android's skin, in Blender (headless). Outside the editor:

  /Applications/Blender.app/Contents/MacOS/Blender -b <android_skin.blend> \
      --python Tools/blender_outfits.py -- <out_dir> [<preview_dir>]

android_skin.blend is written by Tools/blender_skin.py next to android_skin.fbx.

Every garment piece is a copy of the skin surface lifted a few millimetres off it, so it
keeps the skin's skin weights AND its shape keys: the body sliders (BustUp, GlutesUp) and
the jiggle (…Bounce…) move fabric and skin together. Skin-tight fabric is right for
lingerie; loose parts (skirts, hanging straps) are for cloth simulation in UE.

A piece is a coverage field in centimetres (positive inside, 0 on the edge) built from
body landmarks. It is written to the vertex colour (R); the fabric material cuts the edge
at 0.5, so edges run smoothly through triangles instead of along them.

Material slots (one fabric each; Tools/char_outfits.py makes the materials):
  Solid  satin straps, bands and cups      Sheer  fine see-through mesh
  Lace   patterned lace                    Net    fishnet
  Latex  glossy catsuit
"""
import math
import os
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
OUT_DIR = args[0]
PREVIEW = args[1] if len(args) > 1 else None
os.makedirs(OUT_DIR, exist_ok=True)

skin = bpy.data.objects["SKM_AndroidSkin"]
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
me = skin.data
nv = len(me.vertices)
MW = skin.matrix_world

P = np.array([tuple(MW @ v.co) for v in me.vertices])
NRM = np.array([tuple((MW.to_3x3() @ v.normal).normalized()) for v in me.vertices])
EDGES = np.array([tuple(e.vertices) for e in me.edges])
BONES = {b.name: np.array(tuple(arm.matrix_world @ b.head_local)) for b in arm.data.bones}
CM = (P[:, 2].max() - P[:, 2].min()) / 179.5     # scene units per cm (the skin is Quinn-tall)
X = (P - np.array((0.0, 0.0, 0.0))) / CM          # everything below in cm

B = {k: v / CM for k, v in BONES.items()}
FWD = (B["ball_l"] - B["foot_l"]) + (B["ball_r"] - B["foot_r"])
FWD[2] = 0
FWD /= np.linalg.norm(FWD)
UP = np.array((0.0, 0.0, 1.0))
LEFT = np.cross(UP, FWD)
if (B["thigh_l"] - B["pelvis"]).dot(LEFT) < 0:
    LEFT = -LEFT
LAT = (X - B["pelvis"]) @ LEFT          # + her left
FW = (X - B["pelvis"]) @ FWD            # + in front of the pelvis
Z = X[:, 2]
ALAT = np.abs(LAT)


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


NS = diffuse(NRM, 6)
NS /= np.linalg.norm(NS, axis=1, keepdims=True)


def group_weight(keys):
    idx = {g.index for g in skin.vertex_groups if any(k in g.name for k in keys)}
    w = np.zeros(nv)
    for v in me.vertices:
        w[v.index] = sum(g.weight for g in v.groups if g.group in idx)
    return w


ARMS = group_weight(("clavicle", "upperarm", "lowerarm", "hand", "thumb", "index", "middle", "ring", "pinky"))
UPPERARM = group_weight(("upperarm", "lowerarm", "hand", "thumb", "index", "middle", "ring", "pinky"))
HANDS = group_weight(("hand", "thumb", "index", "middle", "ring", "pinky"))
HEAD = group_weight(("head", "neck"))
LEGS = group_weight(("thigh", "calf", "foot", "ball"))

# ---- landmarks (cm, Quinn rest pose) --------------------------------------------------------
LM = dict(
    crotch=B["thigh_l"][2] - 10.0,
    hip_top=B["pelvis"][2] + 3.0,          # low-rise waistline of briefs
    waist=(B["spine_02"][2] + B["spine_03"][2]) * 0.5,
    bust=0.0, fold=0.0,                    # set from the bust anchors below
    hip_half=abs(B["thigh_l"].dot(LEFT) - B["pelvis"].dot(LEFT)) + 7.5,
    knee=B["calf_l"][2],
    thigh_mid=(B["thigh_l"][2] + B["calf_l"][2]) * 0.5,
    shoulder=B["clavicle_l"][2],
)


def surface_near(point, direction=None):
    """The skin vertex nearest to a point (optionally preferring one facing a direction)."""
    d = np.linalg.norm(X - point, axis=1)
    if direction is not None:
        d = d - 2.0 * np.clip(NS @ direction, 0, 1)
    return X[np.argmin(d + 100.0 * (UPPERARM > 0.3))]


# bust anchors: the front of the chest, as in Tools/blender_skin.py
shoulder_half = abs((B["upperarm_l"] - B["pelvis"]).dot(LEFT))
chest_z = B["spine_03"][2] + (B["clavicle_l"][2] - B["spine_03"][2]) * 0.40
ANCHOR = {}
for s in (1, -1):
    near = (np.abs(Z - chest_z) < 1.5) & (np.abs(LAT - s * shoulder_half * 0.5) < 1.5) & (ARMS < 0.3)
    i = np.nonzero(near)[0]
    ANCHOR[s] = X[i[np.argmax(FW[i])]]
LM["bust"] = ANCHOR[1][2]
LM["fold"] = LM["bust"] - 5.8


def frame(direction, s, yaw, pitch):
    out = LEFT * s
    f = direction * math.cos(math.radians(yaw)) + out * math.sin(math.radians(yaw))
    f = f * math.cos(math.radians(pitch)) - UP * math.sin(math.radians(pitch))
    f /= np.linalg.norm(f)
    u = UP - f * UP.dot(f)
    u /= np.linalg.norm(u)
    l = out - f * out.dot(f) - u * out.dot(u)
    return f, u, l / np.linalg.norm(l)


TORSO = (ARMS < 0.35) & (HEAD < 0.5)
LEG_ONLY = (LEGS > 0.6) & (Z < LM["crotch"] + 2)
NEG = -50.0


def where(mask, f):
    return np.where(mask, f, NEG)


# ---- pieces: coverage in cm ----------------------------------------------------------------
def cups(top=0.30, scale=1.0):
    """Teardrop cups over the bust; top cuts the upper pole (0 = at the apex, 1 = full)."""
    out = np.full(nv, NEG)
    for s in (1, -1):
        f, u, l = frame(FWD, s, 12, 6)
        D = X - ANCHOR[s]
        x, y = D @ l, D @ u
        rx = np.where(x > 0, 9.0, 6.0) * scale
        ry = np.where(y > 0, 10.0, 6.0) * scale
        r = np.sqrt((x / rx) ** 2 + (y / ry) ** 2)
        c = np.minimum((1.0 - r) * 7.0, top * 10.0 - y)
        out = np.maximum(out, where(TORSO & (NS @ FWD > -0.2) & (D @ f > -6), c))
    return out


def band(z, width, mask=TORSO):
    return where(mask, width * 0.5 - np.abs(Z - z))


def underband(width=1.8):
    return band(LM["fold"] - width * 0.5, width)


def path(points, width, mask=None):
    """A strap along points (cm) that hug the skin: distance to the polyline."""
    pts = []
    for a, b in zip(points[:-1], points[1:]):
        for t in np.linspace(0, 1, 16, endpoint=False):
            pts.append(surface_near(a + (b - a) * t))
    pts.append(surface_near(points[-1]))
    pts = np.array(pts)
    d = np.full(nv, 1e9)
    for a, b in zip(pts[:-1], pts[1:]):
        ab = b - a
        t = np.clip(((X - a) @ ab) / max(ab @ ab, 1e-6), 0, 1)
        d = np.minimum(d, np.linalg.norm(X - (a + t[:, None] * ab), axis=1))
    f = width * 0.5 - d
    return where(mask if mask is not None else (UPPERARM < 0.5) & (HEAD < 0.5), f)


def shoulder_straps(width=1.0, cup_top=0.30):
    out = np.full(nv, NEG)
    for s in (1, -1):
        f, u, l = frame(FWD, s, 12, 6)
        start = ANCHOR[s] + u * (cup_top * 10.0 - 0.5) + l * 0.5
        top = (B["clavicle_l" if s == 1 else "clavicle_r"] + B["upperarm_l" if s == 1 else "upperarm_r"]) * 0.5
        top = top + UP * 6.0
        back = np.array(start) - FWD * 30.0
        back[2] = LM["fold"]
        back = surface_near(back, -FWD)
        out = np.maximum(out, path([start, top, back], width))
    return out


def brief(back="thong", rise=0.0, cut=1.6):
    """Front panel up to a low-rise waist with high-cut legs; back as thong or cheeky."""
    top = LM["hip_top"] + rise
    a = np.clip(ALAT / LM["hip_half"], 0, 1)
    z_leg = LM["crotch"] + (top - LM["crotch"]) * a ** cut
    front = np.minimum(top - Z, Z - z_leg)
    if back == "thong":
        t = np.clip((Z - LM["crotch"]) / (top - LM["crotch"]), 0, 1)
        w = 0.6 + 5.5 * t ** 2
        rear = np.minimum(top - Z, w - ALAT)
        # the waist string joining front and back
        rear = np.maximum(rear, np.minimum(0.6 - np.abs(Z - (top - 0.6)), 30.0))
    else:   # cheeky: the leg line rises steeply over the cheek
        z_back = LM["crotch"] + (top - LM["crotch"]) * np.clip(a * 1.15, 0, 1) ** 0.7
        rear = np.minimum(top - Z, Z - z_back + 2.0)
    f = np.where(FW > 0, front, rear)
    region = TORSO & (Z > LM["crotch"] - 4) & (Z < top + 2) & ~((LEGS > 0.9) & (Z < LM["crotch"] - 1))
    return where(region, f)


def stockings(top=None):
    top = top if top is not None else LM["thigh_mid"] + 5.0
    return where((UPPERARM < 0.3) & (Z < LM["crotch"]), top - Z), top


def lace_top(top, width=5.0):
    return band(top - width * 0.5, width, (UPPERARM < 0.3) & (Z < LM["crotch"]))


def suspenders(belt_z, stock_top, width=1.2, back=True):
    out = np.full(nv, NEG)
    for s in (1, -1):
        thigh = B["thigh_l" if s == 1 else "thigh_r"]
        knee = B["calf_l" if s == 1 else "calf_r"]
        for side in ((1, -1) if back else (1,)):
            lat = 9.5 if side == 1 else 8.0
            p0 = B["pelvis"] + LEFT * s * lat + FWD * side * 20
            p0[2] = belt_z
            k = (stock_top - knee[2]) / (thigh[2] - knee[2])
            leg = knee + (thigh - knee) * k
            p1 = leg + FWD * side * 20 + LEFT * s * (1.0 if side == 1 else -0.5)
            p1[2] = stock_top - 1.0
            out = np.maximum(out, path([surface_near(p0, FWD * side), surface_near(p1, FWD * side)], width,
                                       (UPPERARM < 0.3)))
    return out


def thigh_band(z, width=1.5):
    return band(z, width, (UPPERARM < 0.3) & (Z < LM["crotch"] - 1))


def bodysuit(neck=None):
    neck = neck if neck is not None else LM["bust"] + 8.0
    f_top = neck - Z
    f_arm = (0.30 - ARMS) * 25.0
    legs = brief(back="cheeky", rise=30.0)
    a = np.clip(ALAT / LM["hip_half"], 0, 1)
    z_leg = LM["crotch"] + (LM["hip_top"] + 2 - LM["crotch"]) * a ** 1.6
    f_leg = np.where(FW > 0, Z - z_leg, Z - (LM["crotch"] + (LM["hip_top"] - LM["crotch"]) * np.clip(a * 1.15, 0, 1) ** 0.7) + 2.0)
    f = np.minimum(np.minimum(f_top, f_arm), f_leg)
    return where((HEAD < 0.5) & (Z > LM["crotch"] - 4), f)


def catsuit():
    f = np.minimum(LM["shoulder"] + 5.0 - Z + (1 - HEAD) * 0, (0.25 - HANDS) * 25.0)
    f = np.minimum(f, (0.5 - HEAD) * 30.0)
    # cutouts: side waist ovals and a window between the breasts
    for s in (1, -1):
        side_pt = surface_near(B["pelvis"] + LEFT * s * 30 + UP * (LM["waist"] - B["pelvis"][2] + 1), LEFT * s)
        r = np.sqrt(((Z - side_pt[2]) / 7.5) ** 2 + ((FW - (side_pt - B["pelvis"]).dot(FWD)) / 4.0) ** 2)
        r = np.where((LAT * s) > 0, r, 9.0)
        f = np.minimum(f, (r - 1.0) * 4.0)
    rw = np.abs(LAT) / 4.0 + np.abs(Z - (LM["bust"] + 1.0)) / 7.0
    rw = np.where(FW > 0, rw, 9.0)
    f = np.minimum(f, (rw - 1.0) * 4.0)
    return f


# ---- the outfits ---------------------------------------------------------------------------
belt_z = LM["waist"] - 3.0
st_f, st_top = stockings()
OUTFITS = {
    "Noir": [   # sheer bodysuit, satin underband and straps, sheer thigh-highs
        ("Sheer", 0.12, bodysuit()),
        ("Solid", 0.30, underband(1.8)),
        ("Solid", 0.32, shoulder_straps(1.0, 0.55)),
        ("Sheer", 0.10, st_f),
        ("Lace", 0.16, lace_top(st_top, 6.0)),
    ],
    "Garter": [   # lace balconette, cheeky lace briefs, garter belt, stockings
        ("Lace", 0.22, cups(0.30)),
        ("Solid", 0.30, underband(1.5)),
        ("Solid", 0.32, shoulder_straps(0.9, 0.30)),
        ("Lace", 0.18, brief("cheeky")),
        ("Lace", 0.24, band(belt_z, 7.0)),
        ("Solid", 0.34, suspenders(belt_z - 3.0, st_top)),
        ("Sheer", 0.10, st_f),
        ("Lace", 0.16, lace_top(st_top, 5.0)),
    ],
    "Cocoa": [   # satin bra, thong, garter belt and thigh straps
        ("Solid", 0.24, cups(0.35)),
        ("Solid", 0.30, underband(1.6)),
        ("Solid", 0.32, shoulder_straps(1.0, 0.35)),
        ("Solid", 0.20, brief("thong")),
        ("Solid", 0.26, band(belt_z, 2.5)),
        ("Solid", 0.30, suspenders(belt_z, LM["thigh_mid"] + 9.0, 1.0, back=False)),
        ("Solid", 0.28, thigh_band(LM["thigh_mid"] + 9.0, 1.6)),
    ],
    "Fishnet": [   # satin bra and thong over fishnet tights
        ("Solid", 0.24, cups(0.25)),
        ("Solid", 0.30, underband(1.4)),
        ("Solid", 0.32, shoulder_straps(0.8, 0.25)),
        ("Solid", 0.26, brief("thong", rise=4.0)),
        ("Net", 0.12, where((UPPERARM < 0.3) & (Z < LM["hip_top"] + 4), LM["hip_top"] + 4 - Z)),
    ],
    "Catsuit": [   # glossy full-body suit with side and chest cutouts
        ("Latex", 0.15, catsuit()),
    ],
}

EDGE = 0.5   # cm over which coverage goes from 0 to 1 (the material cuts at 0.5)
SLOTS = ["Solid", "Sheer", "Lace", "Net", "Latex"]


def make_piece(name, slot, thickness, f):
    alpha = np.clip(0.5 + f / (2 * EDGE), 0, 1)
    obj = skin.copy()
    obj.data = skin.data.copy()
    obj.name = name
    bpy.context.collection.objects.link(obj)
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.verts.ensure_lookup_table()
    keep = alpha > 0.5
    dead = [fc for fc in bm.faces if not any(keep[v.index] for v in fc.verts)]
    bmesh.ops.delete(bm, geom=dead, context="FACES")
    loose = [v for v in bm.verts if not v.link_faces]
    bmesh.ops.delete(bm, geom=loose, context="VERTS")
    inv = obj.matrix_world.inverted().to_3x3()
    bm.to_mesh(obj.data)
    bm.free()
    # vertex indices changed: match by position to the skin (exact copies)
    me2 = obj.data
    co = np.array([tuple(v.co) for v in me2.vertices])
    skin_co = np.array([tuple(v.co) for v in me.vertices])
    from mathutils.kdtree import KDTree
    kd = KDTree(nv)
    for i, c in enumerate(skin_co):
        kd.insert(c, i)
    kd.balance()
    idx = np.array([kd.find(c)[1] for c in co])
    lift = np.array([tuple(inv @ Vector(tuple(NS[i] * thickness * CM))) for i in idx])
    if me2.shape_keys:
        for kb in me2.shape_keys.key_blocks:
            arr = np.zeros(len(kb.data) * 3)
            kb.data.foreach_get("co", arr)
            arr = arr.reshape(-1, 3) + lift
            kb.data.foreach_set("co", arr.ravel())
    arr = np.zeros(len(me2.vertices) * 3)
    me2.vertices.foreach_get("co", arr)
    me2.vertices.foreach_set("co", (arr.reshape(-1, 3) + lift).ravel())
    col = me2.color_attributes.new("Coverage", "BYTE_COLOR", "POINT")
    a = alpha[idx]
    rgba = np.stack([a, a, a, np.ones_like(a)], axis=1).ravel()
    col.data.foreach_set("color", rgba)
    me2.materials.clear()
    me2.materials.append(bpy.data.materials.get(slot) or bpy.data.materials.new(slot))
    return obj


PREVIEW_COLORS = dict(Solid=(0.02, 0.02, 0.025, 1), Sheer=(0.01, 0.01, 0.01, 1), Lace=(0.03, 0.025, 0.03, 1),
                      Net=(0.01, 0.01, 0.01, 1), Latex=(0.06, 0.03, 0.10, 1))
PREVIEW_READY = []


def preview_setup(scn):
    """EEVEE, a pale skin and one node material per fabric that cuts at coverage 0.5."""
    if PREVIEW_READY:
        return
    for engine in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE"):
        try:
            scn.render.engine = engine
            break
        except TypeError:
            pass
    scn.render.resolution_x, scn.render.resolution_y = 700, 1100
    world = bpy.data.worlds.new("W")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.35, 0.35, 0.38, 1)
    scn.world = world
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 3.0
    sun.rotation_euler = (math.radians(50), 0, math.radians(30))
    bpy.context.collection.objects.link(sun)
    sm = bpy.data.materials.new("PreviewSkin")
    sm.use_nodes = True
    sm.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.75, 0.62, 0.52, 1)
    skin.data.materials.clear()
    skin.data.materials.append(sm)
    for name, colr in PREVIEW_COLORS.items():
        m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
        m.use_nodes = True
        nt = m.node_tree
        bsdf = nt.nodes["Principled BSDF"]
        bsdf.inputs["Base Color"].default_value = colr
        bsdf.inputs["Roughness"].default_value = 0.35
        attr = nt.nodes.new("ShaderNodeAttribute")
        attr.attribute_name = "Coverage"
        gt = nt.nodes.new("ShaderNodeMath")
        gt.operation = "GREATER_THAN"
        gt.inputs[1].default_value = 0.5
        nt.links.new(attr.outputs["Fac"], gt.inputs[0])
        alpha = gt.outputs[0]
        if name in ("Sheer", "Net", "Lace"):   # see-through fabrics: let some skin show
            mul = nt.nodes.new("ShaderNodeMath")
            mul.operation = "MULTIPLY"
            mul.inputs[1].default_value = dict(Sheer=0.55, Net=0.45, Lace=0.75)[name]
            nt.links.new(alpha, mul.inputs[0])
            alpha = mul.outputs[0]
        nt.links.new(alpha, bsdf.inputs["Alpha"])
        for attr_name, val in (("surface_render_method", "DITHERED"), ("blend_method", "HASHED")):
            try:
                setattr(m, attr_name, val)
            except (AttributeError, TypeError):
                pass
    PREVIEW_READY.append(True)


bpy.context.view_layer.objects.active = skin
for out_name, pieces in OUTFITS.items():
    objs = []
    for k, (slot, t, f) in enumerate(pieces):
        objs.append(make_piece("%s_%d" % (out_name, k), slot, t, f))
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    if len(objs) > 1:
        bpy.ops.object.join()
    outfit = bpy.context.view_layer.objects.active
    outfit.name = "SKM_Outfit_" + out_name
    print(out_name, "verts", len(outfit.data.vertices), "slots", [m.name for m in outfit.data.materials])

    if PREVIEW:
        os.makedirs(PREVIEW, exist_ok=True)
        scn = bpy.context.scene
        preview_setup(scn)
        cam = scn.camera
        if cam is None:
            cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
            cam.data.lens = 70
            bpy.context.collection.objects.link(cam)
            scn.camera = cam
        for o in bpy.data.objects:
            if o.type == "MESH":
                o.hide_render = o not in (outfit, skin)
        target = Vector(tuple((B["spine_02"] * 0.4 + B["thigh_l"] * 0.6) * CM))
        target.x = 0
        for label, d in (("front", FWD), ("back", -FWD)):
            cam.location = target + Vector(tuple(d * 330 * CM)) + Vector((0, 0, 10 * CM))
            cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
            scn.render.filepath = os.path.join(PREVIEW, "%s_%s.png" % (out_name, label))
            bpy.ops.render.render(write_still=True)
        outfit.hide_render = True

    bpy.ops.object.select_all(action="DESELECT")
    arm.select_set(True)
    outfit.select_set(True)
    bpy.ops.export_scene.fbx(filepath=os.path.join(OUT_DIR, "outfit_%s.fbx" % out_name.lower()),
                             use_selection=True, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False,
                             use_armature_deform_only=False, mesh_smooth_type="FACE", bake_anim=False,
                             colors_type="LINEAR", axis_forward="-Z", axis_up="Y")
    outfit.hide_set(True)
print("done")
