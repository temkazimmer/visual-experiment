"""The flex layer's morph deltas for the MetaHuman body (stage 8): muscle bellies that the game
tenses by joint angle and load (UVisexMHBodyComponent::ApplyFlex). MetaHuman's own correctives keep
volume at the joints; nothing in it makes a belly stand out, so these do.

  Blender -b --factory-startup --python Tools/mh_flex_build.py -- [--preview=<dir>] [--scale=1.0]

In:  SourceArt/Character/MH/mh_skin.fbx              (topology, skin weights, reference joints)
     SourceArt/Character/MH/Morphs/mh_morphs.json    (the body's vertices in the mesh's own order)
     Saved/MH/morphs/var_musc0.json, var_musc1.json  (Creator variants: Tools/mh_morphs.py
                                                      STEP='variants', VARIANT='musc0' / 'musc1')
Out: the Flex* morphs added to mh_morphs.json (every other morph in it is left as it is). Then
     Tools/mh_morphs.py STEP='apply' puts them on the body mesh and the Silversuit re-cut carries
     them (Tools/mh_after_build.sh does both).

Where a delta comes from. The Creator's Muscularity axis (the same body solved at -1.0 and +0.5,
girths let go) says where a real body puts muscle. Per limb the variant's shift of each cross
section's centre is taken out (the solve also moves joints), what is left is read along the skin's
normal, and a soft window per muscle (along the bone, around it, and by skin weight so an arm's
window cannot reach the ribs) cuts one belly out of it. Half of each shape is that, half is a plain
bump of the window itself, so a belly reads as one form even where the variant is flat. Each is
then sized to a peak height in cm (PEAK below): a slim woman's, not a bodybuilder's.

Nothing moves near the neck opening (shared with the face mesh), the wrists or the ankles.
"""
import json
import os
import sys

import bpy
import numpy as np
from mathutils.kdtree import KDTree

ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
PROJECT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FBX = os.path.join(PROJECT, "SourceArt", "Character", "MH", "mh_skin.fbx")
MORPHS_JSON = os.path.join(PROJECT, "SourceArt", "Character", "MH", "Morphs", "mh_morphs.json")
WORK = os.path.join(PROJECT, "Saved", "MH", "morphs")
PREVIEW = next((a.split("=", 1)[1] for a in ARGS if a.startswith("--preview=")), None)
SCALE = float(next((a.split("=", 1)[1] for a in ARGS if a.startswith("--scale=")), 1.0))
FLIP = np.array([1.0, -1.0, 1.0])                  # Blender (x, y, z) <-> UE (x, -y, z)

# ---- the body: vertices in the mesh's own order (UE's frame, cm), triangles, weights, joints ------
data = json.load(open(MORPHS_JSON))
U = np.array(data["base"], float)
NV = len(U)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=FBX)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
skin = next(o for o in bpy.data.objects if o.type == "MESH")
bpy.context.view_layer.update()
_mw = np.array(skin.matrix_world)
_co = np.zeros(len(skin.data.vertices) * 3)
skin.data.vertices.foreach_get("co", _co)
XF = (_co.reshape(-1, 3) @ _mw[:3, :3].T + _mw[:3, 3]) * 100.0 * FLIP      # the FBX's vertices (split along UV seams)
_kd = KDTree(NV)
for _i in range(NV):
    _kd.insert(U[_i], _i)
_kd.balance()
_hit = [_kd.find(p) for p in XF.tolist()]
F2U = np.array([h[1] for h in _hit])               # FBX vertex -> the mesh's vertex
assert max(h[2] for h in _hit) < 0.02 and len(set(F2U.tolist())) == NV, "mh_skin.fbx is not the body mh_morphs.json was made on"
skin.data.calc_loop_triangles()
_t = np.zeros(len(skin.data.loop_triangles) * 3, int)
skin.data.loop_triangles.foreach_get("vertices", _t)
T = F2U[_t.reshape(-1, 3)]
T = T[(T[:, 0] != T[:, 1]) & (T[:, 1] != T[:, 2]) & (T[:, 2] != T[:, 0])]
_aw = np.array(arm.matrix_world)
BONE = {b.name: (np.array(b.head_local) @ _aw[:3, :3].T + _aw[:3, 3]) * 100.0 * FLIP for b in arm.data.bones}
GROUPS = [g.name for g in skin.vertex_groups]
W = np.zeros((NV, len(GROUPS)), np.float32)        # skin weights per vertex of the mesh
for _v in skin.data.vertices:
    for _g in _v.groups:
        W[F2U[_v.index], _g.group] = _g.weight
W /= np.maximum(W.sum(1, keepdims=True), 1e-9)


def weight(*starts):
    """Summed weight of every joint whose name starts with one of these."""
    cols = [i for i, n in enumerate(GROUPS) if any(n.startswith(s) for s in starts)]
    return W[:, cols].sum(1)


E = np.unique(np.sort(np.concatenate([T[:, [0, 1]], T[:, [1, 2]], T[:, [2, 0]]]), axis=1), axis=0)
DEG = np.maximum(np.bincount(E[:, 0], minlength=NV) + np.bincount(E[:, 1], minlength=NV), 1).astype(float)


def blur(a, iters):
    """Diffuse a per-vertex array (n) or (n, 3) over the body's edges (about 0.7 cm a pass)."""
    a = np.asarray(a, float)
    for _ in range(iters):
        acc = np.zeros_like(a)
        np.add.at(acc, E[:, 0], a[E[:, 1]])
        np.add.at(acc, E[:, 1], a[E[:, 0]])
        a = 0.5 * a + 0.5 * acc / (DEG if a.ndim == 1 else DEG[:, None])
    return a


def smoothstep(x, a, b):
    t = np.clip((np.asarray(x, float) - a) / (b - a), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def normals(P):
    fn = np.cross(P[T[:, 1]] - P[T[:, 0]], P[T[:, 2]] - P[T[:, 0]])
    n = np.zeros_like(P)
    for k in range(3):
        np.add.at(n, T[:, k], fn)
    return n / np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-9)


N = normals(U)
if float(((U - U.mean(0)) * N).sum()) < 0:         # outward, whatever the import's winding
    N = -N
NS = blur(N, 4)
NS /= np.maximum(np.linalg.norm(NS, axis=1, keepdims=True), 1e-9)
print("mh flex: %d vertices, %d triangles, %d joints with weights" % (NV, len(T), len(GROUPS)))

# ---- the Creator's Muscularity axis, as height along the skin's normal --------------------------------
_v0 = np.array(json.load(open(os.path.join(WORK, "var_musc0.json")))["v"], float)
_v1 = np.array(json.load(open(os.path.join(WORK, "var_musc1.json")))["v"], float)
assert len(_v0) == NV and len(_v1) == NV, "the muscle variants are not this mesh: run Tools/mh_morphs.py STEP='variants' for musc0 and musc1"
DV = _v1 - _v0
FWD = np.array([0.0, 1.0, 0.0])                    # she faces +Y in UE's frame


class Seg:
    """A bone from joint a to joint b: for every vertex, t along it (0..1), its distance from the axis
    and the angle round it (0 in front of her, 90 on the outside, 180 behind, -90 inside)."""

    def __init__(self, a, b, side):
        A, B = BONE[a], BONE[b]
        self.L = float(np.linalg.norm(B - A))
        ax = (B - A) / self.L
        f = FWD - ax * (FWD @ ax)
        f /= np.linalg.norm(f)
        o = np.cross(ax, f)
        o = o if o[0] * side > 0 else -o
        rel = U - A
        self.t = rel @ ax / self.L
        rad = rel - np.outer(rel @ ax, ax)
        self.r = np.linalg.norm(rad, axis=1)
        self.th = np.degrees(np.arctan2(rad @ o, rad @ f))
        self.ax, self.f, self.o = ax, f, o

    def along(self, t0, t1, t2, t3):
        """0 before t0, 1 from t1 to t2, 0 after t3."""
        return smoothstep(self.t, t0, t1) * (1.0 - smoothstep(self.t, t2, t3))

    def around(self, centre, half, soft=0.5):
        """1 at the angle `centre`, 0 from `half` degrees off it; `soft` is the part of that taken to fade."""
        d = np.abs((self.th - centre + 180.0) % 360.0 - 180.0)
        return 1.0 - smoothstep(d, half * (1.0 - soft), half)

    def creator(self, gate):
        """The variant's height along the normal with each cross section's own shift taken out."""
        k = np.clip(((self.t + 0.3) / 1.6 * 24).astype(int), 0, 23)
        shift = np.zeros((24, 3))
        for i in range(24):
            m = (k == i) & (gate > 0.5)
            if m.sum() >= 8:
                shift[i] = DV[m].mean(0)
        for i in range(1, 24):                     # an empty slice takes its neighbour's
            if not shift[i].any():
                shift[i] = shift[i - 1]
        pos = np.clip((self.t + 0.3) / 1.6 * 24 - 0.5, 0, 23)
        lo = np.floor(pos).astype(int)
        hi = np.minimum(lo + 1, 23)
        c = shift[lo] * (1 - (pos - lo))[:, None] + shift[hi] * (pos - lo)[:, None]
        return ((DV - c) * N).sum(1)


# the neck opening is shared with the face mesh, which gets no morphs: nothing moves near it
_cnt = {}
for _a, _b in np.sort(np.concatenate([T[:, [0, 1]], T[:, [1, 2]], T[:, [2, 0]]]), axis=1).tolist():
    _cnt[(_a, _b)] = _cnt.get((_a, _b), 0) + 1
RIM = sorted({v for e, c in _cnt.items() if c == 1 for v in e})
_kr = KDTree(len(RIM))
for _i, _v in enumerate(RIM):
    _kr.insert(U[_v], _i)
_kr.balance()
RIM_D = np.array([_kr.find(p)[2] for p in U.tolist()])
SEAM = smoothstep(RIM_D, 3.0, 8.0)
# hands and feet, with the wrist and ankle borders, stay as built
_HANDS = weight("hand_", "wrist_", "index_", "middle_", "ring_", "pinky_", "thumb_")
_FEET = weight("foot_", "ball_", "ankle_", "bigtoe_", "indextoe_", "middletoe_", "ringtoe_", "littletoe_")
KEEP = SEAM * (1.0 - smoothstep(blur(_HANDS + _FEET, 6), 0.02, 0.25))
print("mh flex: %d open-edge vertices (z %.1f..%.1f)" % (len(RIM), U[RIM, 2].min(), U[RIM, 2].max()))

MORPHS = {}
REPORT = []


def belly(name, window, creator, peak, lift=None, smooth=3):
    """One muscle: `window` (0..1 per vertex) cut out of the variant's height (half) and of a plain
    bump (half), sized so its highest point stands `peak` cm out. `lift`: a direction added to the
    normal (a tensed seat also rises)."""
    w = blur(np.clip(window, 0.0, 1.0) * KEEP, 2)
    inside = w > 0.3
    ref = float(np.average(np.clip(creator[inside], 0.0, None), weights=w[inside])) if inside.sum() > 10 else 0.0
    shape = w * (0.5 + 0.5 * np.clip(creator / ref, 0.0, 2.0)) if ref > 0.02 else w
    shape = blur(shape, smooth) * KEEP
    shape /= max(float(shape.max()), 1e-9)
    d = NS * shape[:, None]
    if lift is not None:
        d = d + np.asarray(lift)[None, :] * shape[:, None]
    d *= peak * SCALE / max(float(np.linalg.norm(d, axis=1).max()), 1e-9)
    MORPHS[name] = d
    m = np.linalg.norm(d, axis=1)
    REPORT.append((name, int((m > 0.003).sum()), float(m.max()), U[int(m.argmax())].round(1).tolist(), ref))


# ---- the muscles -------------------------------------------------------------------------------------
# peak height of each belly at weight 1, cm (a slim woman's; the game multiplies by visex.Body.MetaHuman.FlexScale)
PEAK = {"Biceps": 0.55, "Triceps": 0.40, "Forearm": 0.35, "Deltoid": 0.40, "Quad": 0.65, "Ham": 0.45,
        "Calf": 0.55, "Glute": 0.60, "Oblique": 0.30, "Lat": 0.40, "Abs": 0.40}
UP = np.array([0.0, 0.0, 1.0])

for side, S in ((1.0, "L"), (-1.0, "R")):
    s = "_l" if side > 0 else "_r"
    mine = smoothstep(U[:, 0] * side, 0.0, 1.5)                 # her own half (the thighs touch in the middle)

    def w_side(*starts):
        cols = [i for i, n in enumerate(GROUPS) if n.endswith(s) and any(n.startswith(p) for p in starts)]
        return W[:, cols].sum(1)

    # upper arm: biceps in front, triceps behind, the deltoid capping the shoulder
    ua = Seg("upperarm" + s, "lowerarm" + s, side)
    g_arm = smoothstep(w_side("upperarm_", "lowerarm_"), 0.35, 0.8)
    c_ua = ua.creator(g_arm)
    belly("FlexBiceps" + S, ua.along(0.20, 0.44, 0.58, 0.86) * ua.around(-10.0, 80.0) * g_arm, c_ua, PEAK["Biceps"])
    belly("FlexTriceps" + S, ua.along(0.12, 0.36, 0.52, 0.80) * ua.around(165.0, 85.0) * g_arm, c_ua, PEAK["Triceps"])
    g_sh = smoothstep(w_side("upperarm_", "clavicle_out_", "clavicle_scap_"), 0.35, 0.8)
    belly("FlexDeltoid" + S, ua.along(-0.30, -0.04, 0.18, 0.46) * ua.around(90.0, 135.0) * g_sh, ua.creator(g_sh), PEAK["Deltoid"])

    # forearm: the mass under the elbow, mostly on the outside and front; nothing in the last third
    fa = Seg("lowerarm" + s, "hand" + s, side)
    g_fa = smoothstep(w_side("lowerarm_", "upperarm_"), 0.35, 0.8)
    belly("FlexForearm" + S, fa.along(0.02, 0.20, 0.32, 0.64) * (0.55 + 0.45 * fa.around(45.0, 150.0, 1.0)) * g_fa, fa.creator(g_fa), PEAK["Forearm"])

    # thigh: the quadriceps in front with the teardrop over the knee's inside, the hamstrings behind
    th = Seg("thigh" + s, "calf" + s, side)
    g_th = smoothstep(w_side("thigh_", "calf_"), 0.35, 0.8) * mine
    c_th = th.creator(g_th)
    quad = np.maximum(th.along(0.12, 0.40, 0.66, 0.90) * th.around(15.0, 85.0),
                      0.75 * th.along(0.60, 0.74, 0.84, 0.95) * th.around(-45.0, 55.0))
    belly("FlexQuad" + S, quad * g_th, c_th, PEAK["Quad"])
    belly("FlexHam" + S, th.along(0.22, 0.46, 0.62, 0.88) * th.around(180.0, 70.0) * g_th, c_th, PEAK["Ham"])

    # calf: the gastrocnemius, high on the back of the shin; the Achilles and the ankle stay
    ca = Seg("calf" + s, "foot" + s, side)
    g_ca = smoothstep(w_side("calf_", "thigh_"), 0.35, 0.8) * mine
    belly("FlexCalf" + S, ca.along(0.05, 0.24, 0.36, 0.60) * ca.around(185.0, 95.0) * g_ca, ca.creator(g_ca), PEAK["Calf"])

    # seat: round the cheek's most backward point, off the cleft; a tensed glute also rises
    to = Seg("pelvis", "spine_05", side)
    g_to = smoothstep(weight("spine_", "pelvis", "clavicle_pec", "clavicle_scap") + w_side("thigh_"), 0.5, 0.9)
    c_to = to.creator(g_to)
    zp = BONE["pelvis"][2]
    pick = np.nonzero((U[:, 0] * side > 3.0) & (U[:, 2] > zp - 12.0) & (U[:, 2] < zp + 5.0))[0]
    apex = U[pick[np.argmin(U[pick, 1])]]
    dist = np.linalg.norm(U - apex, axis=1)
    glute = (1.0 - smoothstep(dist, 3.5, 11.0)) * smoothstep(-(N @ FWD), -0.2, 0.3) * smoothstep(U[:, 0] * side, 1.0, 3.5)
    belly("FlexGlute" + S, glute, c_to, PEAK["Glute"], lift=UP * 0.35, smooth=4)

    # flank: the oblique between ribs and hip bone, and the latissimus under the armpit
    g_flank = g_to * (1.0 - smoothstep(w_side("upperarm_"), 0.1, 0.4))
    belly("FlexOblique" + S, to.along(0.05, 0.20, 0.38, 0.56) * to.around(58.0, 46.0) * g_flank * mine, c_to, PEAK["Oblique"])
    belly("FlexLat" + S, to.along(0.42, 0.62, 0.84, 1.02) * to.around(125.0, 55.0) * g_flank * mine, c_to, PEAK["Lat"])

# abs: the shape sliders' own lines and pack (the old body's keys, carried over in stage 7b), as one tensing
if "AbsLines" in data["morphs"] and "AbsPack" in data["morphs"]:
    d = np.zeros((NV, 3))
    for key, k in (("AbsLines", 0.6), ("AbsPack", 1.0)):
        m = data["morphs"][key]
        d[np.array(m["i"], int)] += np.array(m["d"], float).reshape(-1, 3) * k
    # the lines run on up the breastbone: stop under the bust, or it reads as a dent between the breasts
    d *= (KEEP * (1.0 - smoothstep(U[:, 2], BONE["spine_04"][2] - 4.0, BONE["spine_04"][2] + 1.0)))[:, None]
    d *= PEAK["Abs"] * SCALE / max(float(np.linalg.norm(d, axis=1).max()), 1e-9)
    MORPHS["FlexAbs"] = d
    REPORT.append(("FlexAbs", int((np.linalg.norm(d, axis=1) > 0.003).sum()), float(np.linalg.norm(d, axis=1).max()),
                   U[int(np.linalg.norm(d, axis=1).argmax())].round(1).tolist(), 0.0))

for name, n, far, at, ref in REPORT:
    print("mh flex %-14s %5d vertices, %.2f cm at most at %s (the variant there: %.2f cm)" % (name, n, far, at, ref))
# the borders, read back: how close any delta comes to the neck opening, a hand or a foot
_all = np.max([np.linalg.norm(d, axis=1) for d in MORPHS.values()], axis=0)
_moved = _all > 0.003
print("mh flex: nearest moved vertex to the neck opening %.1f cm; most a hand or foot vertex moves %.3f cm" % (
    RIM_D[_moved].min(), float((_all * ((_HANDS + _FEET) > 0.5)).max())))

# ---- out ---------------------------------------------------------------------------------------------
if "--dry" not in ARGS:
    for k in [k for k in data["morphs"] if k.startswith("Flex")]:
        del data["morphs"][k]
    for k, d in MORPHS.items():
        i = np.nonzero(np.linalg.norm(d, axis=1) > 0.003)[0]
        data["morphs"][k] = {"i": i.tolist(), "d": np.round(d[i], 4).ravel().tolist()}
    json.dump(data, open(MORPHS_JSON, "w"), separators=(",", ":"))
    print("mh flex: %d flex morphs in %s (%d morphs, %.1f MB)" % (len(MORPHS), MORPHS_JSON, len(data["morphs"]), os.path.getsize(MORPHS_JSON) / 1e6))

# ---- preview: the body relaxed and with every flex morph at 1, under a raking light -----------------
if PREVIEW:
    os.makedirs(PREVIEW, exist_ok=True)
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    me = bpy.data.meshes.new("flex")
    me.from_pydata((U * FLIP * 0.01).tolist(), [], T[:, ::-1].tolist())
    me.update()
    for p in me.polygons:
        p.use_smooth = True
    ob = bpy.data.objects.new("flex", me)
    bpy.context.scene.collection.objects.link(ob)
    ob.shape_key_add(name="Basis")
    full = sum(MORPHS.values())
    for nm, d in (("Flex", full),) + tuple(MORPHS.items()):
        k = ob.shape_key_add(name=nm)
        k.data.foreach_set("co", ((U + d) * FLIP * 0.01).ravel())
    mat = bpy.data.materials.new("skin")
    mat.use_nodes = True
    bsdf = next(n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = (0.75, 0.6, 0.52, 1.0)
    bsdf.inputs["Roughness"].default_value = 0.38
    me.materials.append(mat)
    scn = bpy.context.scene
    scn.render.engine = "BLENDER_EEVEE"
    scn.render.resolution_x, scn.render.resolution_y = 1100, 1400
    scn.world = bpy.data.worlds.new("w")
    scn.world.use_nodes = True
    scn.world.node_tree.nodes["Background"].inputs[1].default_value = 0.35
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy = 4.0
    scn.collection.objects.link(sun)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    cam.data.lens = 85
    scn.collection.objects.link(cam)
    scn.camera = cam
    from mathutils import Vector
    # name: (look-at in UE cm, camera direction from it (UE frame), distance cm, light direction it comes from)
    VIEWS = {"arm_front": ((30, 0, 118), (0.5, 1, 0.1), 150, (1, 0.25, 0.3)), "arm_back": ((30, 0, 118), (0.5, -1, 0.1), 150, (-1, -0.2, 0.3)),
             "legs_front": ((0, 0, 55), (0.35, 1, 0.05), 300, (1, 0.2, 0.2)), "legs_back": ((0, 0, 60), (0.35, -1, 0.05), 300, (-1, -0.15, 0.2)),
             "legs_side": ((12, 0, 55), (1, 0.1, 0.05), 300, (0.2, -1, 0.2)),
             "torso_front": ((0, 0, 112), (0.4, 1, 0.05), 230, (1, 0.2, 0.25)), "torso_back": ((0, 0, 112), (-0.4, -1, 0.05), 230, (1, -0.2, 0.25))}
    for vn, (at, frm, dist, light) in VIEWS.items():
        at = Vector(np.array(at) * FLIP * 0.01)
        cam.location = at + Vector(np.array(frm) * FLIP).normalized() * dist * 0.01
        cam.rotation_euler = (at - cam.location).to_track_quat("-Z", "Y").to_euler()
        sun.rotation_euler = Vector(np.array(light) * FLIP).to_track_quat("Z", "Y").to_euler()
        for tag, val in (("0", 0.0), ("1", 1.0)):
            ob.data.shape_keys.key_blocks["Flex"].value = val
            scn.render.filepath = os.path.join(PREVIEW, "flex_%s_%s.png" % (vn, tag))
            bpy.ops.render.render(write_still=True)
    print("mh flex: previews in", PREVIEW)
