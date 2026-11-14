"""Shape the MetaHuman head onto her current head (stage 6: "match her current face", ears included).

  Blender -b --factory-startup --python Tools/mh_face_fit.py -- <project> [--identity] [--views]

Reads  SourceArt/Character/android_skin.fbx       her current skin (the head is drawn at 0.935 x 1.04)
       SourceArt/Character/MH/mh_head.fbx         the assembled MetaHuman face mesh (Tools/mh_skin_export.py)
       SourceArt/Character/MH/Face/landmarks.json matching points on both heads (see LANDMARKS below)
Writes SourceArt/Character/MH/Face/face_head.fbx, face_eye_l.fbx, face_eye_r.fbx, face_teeth.fbx:
       the MetaHuman's own topology and UVs with every vertex moved onto her head, as static meshes
       for `import_from_template` (matched by UV, alignment NONE: they sit where the head sits on the
       MetaHuman body). Tools/mh_face.py imports them and fits the Creator's face to them.

--identity writes the MetaHuman head unmoved (a round-trip test of the import path).
--views    writes orthographic pictures of both heads with a cm grid to Saved/MH/face/views (to place
           landmarks by eye); a normal run writes the same pictures of the fitted head ('fit_*').
--look <head.fbx> <name>  the same pictures of another MetaHuman face mesh (what the Creator made).

Everything here is in Blender's frame in cm: x to her left, -y is forward, z up.
"""
import json
import os
import sys

import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

argv = sys.argv[sys.argv.index("--") + 1:]
P = argv[0]
IDENTITY = "--identity" in argv
VIEWS = "--views" in argv
SRC = os.path.join(P, "SourceArt", "Character")
OUT = os.path.join(SRC, "MH", "Face")
HEAD_SCALE = 0.935 * 1.04      # her actor scale (166 / 177.5) times the Head slider's default
os.makedirs(OUT, exist_ok=True)


def load(fbx):
    """World-space vertices in cm, triangles, per-triangle material name, per-corner UVs, bones."""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=fbx)
    ob = max((o for o in bpy.data.objects if o.type == "MESH"), key=lambda o: len(o.data.vertices))
    me = ob.data
    V = np.empty(len(me.vertices) * 3, np.float64)
    me.vertices.foreach_get("co", V)
    V = V.reshape(-1, 3)
    M = np.array(ob.matrix_world)
    V = V @ M[:3, :3].T + M[:3, 3]
    unit = 100.0 if V[:, 2].max() < 10.0 else 1.0          # the MetaHuman export comes in metres
    V *= unit
    me.calc_loop_triangles()
    n = len(me.loop_triangles)
    T = np.empty(n * 3, np.int64)
    me.loop_triangles.foreach_get("vertices", T)
    T = T.reshape(-1, 3)
    L = np.empty(n * 3, np.int64)
    me.loop_triangles.foreach_get("loops", L)
    mi = np.empty(n, np.int64)
    me.loop_triangles.foreach_get("material_index", mi)
    mats = [m.name for m in me.materials]
    UV = np.empty(len(me.loops) * 2, np.float64)
    me.uv_layers[0].data.foreach_get("uv", UV)
    UV = UV.reshape(-1, 2)[L].reshape(-1, 3, 2)
    arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    bones = {b.name: np.array(arm.matrix_world @ b.head_local) * unit for b in arm.data.bones}
    return V, T, [mats[i] for i in mi], UV, bones


PARTS = {"head": "Face_Skin_Baked_LOD0", "teeth": "Teeth", "eye_l": "EyeL", "eye_r": "EyeR"}


def part(T, mat, key):
    """Triangle rows of one part of the MetaHuman face mesh, and its vertex ids."""
    rows = np.array([i for i, m in enumerate(mat) if PARTS[key] in m])
    return rows, np.unique(T[rows])


def write_static(name, V, T, UV, rows):
    """One part as a static mesh FBX in metres, the MetaHuman's vertex order and UVs kept."""
    ids = np.unique(T[rows])
    remap = -np.ones(len(V), np.int64)
    remap[ids] = np.arange(len(ids))
    bpy.ops.wm.read_factory_settings(use_empty=True)
    me = bpy.data.meshes.new(name)
    me.from_pydata((V[ids] / 100.0).tolist(), [], remap[T[rows]].tolist())
    me.uv_layers.new(name="DiffuseUV")
    me.uv_layers[0].data.foreach_set("uv", UV[rows].reshape(-1))
    me.update()
    assert len(me.polygons) == len(rows) and len(me.loops) == 3 * len(rows)
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    for p in me.polygons:
        p.use_smooth = True
    bpy.ops.object.select_all(action="DESELECT")
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    out = os.path.join(OUT, name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=out, use_selection=True, object_types={"MESH"}, mesh_smooth_type="FACE",
                             bake_anim=False, axis_forward="-Z", axis_up="Y")
    print("FIT wrote %s: %d vertices, %d triangles" % (out, len(ids), len(rows)))


mhV, mhT, mhMat, mhUV, mhBones = load(os.path.join(SRC, "MH", "mh_head.fbx"))
print("FIT MetaHuman head: %d vertices; head joint %s" % (len(mhV), np.round(mhBones["head"], 2)))
for key in PARTS:
    rows, ids = part(mhT, mhMat, key)
    print("FIT   %-6s %6d vertices %6d triangles  bbox %s %s" % (key, len(ids), len(rows), np.round(mhV[ids].min(0), 2), np.round(mhV[ids].max(0), 2)))
newV = mhV.copy()


def her_head():
    """Her current head as drawn in the game, placed so its head joint is on the MetaHuman's."""
    V, T, _, _, bones = load(os.path.join(SRC, "android_skin.fbx"))
    joint = bones["head"]
    keep = (V[T][:, :, 2] > joint[2] - 16.0).all(axis=1)          # the head and most of the neck
    T = T[keep]
    ids = np.unique(T)
    remap = -np.ones(len(V), np.int64)
    remap[ids] = np.arange(len(ids))
    return (V[ids] - joint) * HEAD_SCALE + mhBones["head"], remap[T]


SHOTS = [      # (tag, view, centre across, centre up, cm shown): across is x in front views, cm FORWARD in side views
    ("front", "front", 0.0, 4.0, 30.0), ("side", "side", 0.0, 4.0, 30.0),
    ("eyes", "front", 0.0, 5.0, 12.0), ("mouth", "front", 0.0, -1.5, 12.0),
    ("ear", "side", -3.0, 6.0, 14.0), ("earfront", "front", 8.0, 6.0, 12.0), ("profile", "side", 8.0, 1.0, 16.0),
]


def views(name, V, T, shots=SHOTS, px=1600):
    """Orthographic pictures with a cm grid whose origin is the head joint. Front views: x across
    (her left is to the right). Side views show her left side: the across label is cm forward."""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    me = bpy.data.meshes.new(name)
    me.from_pydata(((V - mhBones["head"]) / 100.0).tolist(), [], T.tolist())
    for q in me.polygons:
        q.use_smooth = True
    ob = bpy.data.objects.new(name, me)
    sc.collection.objects.link(ob)
    ob.color = (0.82, 0.74, 0.68, 1.0)
    sc.render.engine = "BLENDER_WORKBENCH"
    sh = sc.display.shading
    sh.light, sh.color_type, sh.show_cavity, sh.cavity_type = "STUDIO", "OBJECT", True, "BOTH"
    sh.curvature_ridge_factor = sh.curvature_valley_factor = 1.6
    sc.render.resolution_x = sc.render.resolution_y = px
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    cam.data.type, cam.data.clip_end = "ORTHO", 10.0
    up = np.array((0.0, 0.0, 1.0))
    for tag, view, ca, cz, size in shots:
        front = view == "front"
        A = np.array((1.0, 0.0, 0.0)) if front else np.array((0.0, 1.0, 0.0))        # the picture's right
        toward = np.array((0.0, -1.0, 0.0)) if front else np.array((1.0, 0.0, 0.0))  # where the camera stands
        rot = (np.pi / 2, 0.0, 0.0) if front else (np.pi / 2, 0.0, np.pi / 2)
        sign = 1.0 if front else -1.0                # side views are labelled in cm forward, which is -y
        ac = ca * sign
        step = 0.5 if size <= 8.0 else 1.0
        marks = []

        def add(centre, scale, colour):
            bpy.ops.mesh.primitive_cube_add(size=1.0)
            g = bpy.context.object
            g.location, g.scale, g.color = centre / 100.0, scale / 100.0, colour
            marks.append(g)

        def text(body, where):
            bpy.ops.object.text_add()
            t = bpy.context.object
            t.data.body, t.data.size = body, size * 0.00022
            t.rotation_euler, t.location, t.color = rot, where / 100.0, (1.0, 0.25, 0.1, 1.0)
            marks.append(t)

        n = int(size / step / 2) + 1
        for i in range(-n, n + 1):
            for vertical in (True, False):
                k = (round(ac / step) + i) * step if vertical else (round(cz / step) + i) * step
                major = abs(k / 5.0 - round(k / 5.0)) < 1e-6
                w = size * (0.0012 if major else 0.0005)
                colour = (0.1, 0.3, 0.9, 1.0) if major else (0.15, 0.15, 0.15, 1.0)
                if vertical:
                    add(A * k + up * cz + toward * 30.0, np.abs(A) * w + up * size + np.abs(toward) * w, colour)
                    text("%g" % (k * sign), A * (k + size * 0.004) + up * (cz - size * 0.49) + toward * 30.0)
                else:
                    add(A * ac + up * k + toward * 30.0, np.abs(A) * size + up * w + np.abs(toward) * w, colour)
                    text("%g" % k, A * (ac - size * 0.495) + up * (k + size * 0.004) + toward * 30.0)
        cam.location, cam.rotation_euler, cam.data.ortho_scale = (A * ac + up * cz) / 100.0 + toward, rot, size / 100.0
        sc.render.filepath = os.path.join(P, "Saved", "MH", "face", "views", "%s_%s.png" % (name, tag))
        bpy.ops.render.render(write_still=True)
        print("FIT view", sc.render.filepath)
        for g in marks:
            bpy.data.objects.remove(g, do_unlink=True)


herV, herT = her_head()
print("FIT her head: %d vertices, bbox %s %s" % (len(herV), np.round(herV.min(0), 2), np.round(herV.max(0), 2)))
if VIEWS:
    rows, _ = part(mhT, mhMat, "head")
    views("mh", mhV, mhT[rows])
    views("her", herV, herT)
    sys.exit(0)

J = mhBones["head"]                      # every landmark below is in cm from this joint
FWD = np.array((0.0, -1.0, 0.0))


def bvh_of(V, T):
    return BVHTree.FromPolygons([Vector(v) for v in V], [tuple(int(i) for i in t) for t in T])


def front_hit(bvh, x, z):
    """Where a ray from in front of the face meets the surface at picture point (x, z)."""
    hit = bvh.ray_cast(Vector((J[0] + x, J[1] - 60.0, J[2] + z)), Vector((0.0, 1.0, 0.0)))
    assert hit[0] is not None, "no surface at front point (%.2f, %.2f)" % (x, z)
    return np.array(hit[0])


def side_hit(bvh, f, z, side):
    """Where a ray from her left (side = 1) or right (-1) meets the surface at side point (f forward, z)."""
    hit = bvh.ray_cast(Vector((J[0] + 60.0 * side, J[1] - f, J[2] + z)), Vector((-1.0 * side, 0.0, 0.0)))
    assert hit[0] is not None, "no surface at side point (%.2f, %.2f)" % (f, z)
    return np.array(hit[0])


def rim_point(V, f, z, side, reach=0.4):
    """A vertex on an ear's rim at side point (f, z): outline points have no ray hit."""
    d = np.hypot(V[:, 1] - (J[1] - f), V[:, 2] - (J[2] + z))
    near = np.where((d < reach) & (V[:, 0] * side > 4.0))[0]
    assert len(near), "no ear rim near side point (%.2f, %.2f)" % (f, z)
    return V[near[np.argmax(V[near, 0] * side - d[near])]]      # far out, and close to the point


def front_vertex(V, x, z, reach=0.1):
    """The foremost vertex at picture point (x, z): for points on the edge of an opening, where a ray
    could slip through."""
    near = np.where(np.hypot(V[:, 0] - J[0] - x, V[:, 2] - J[2] - z) < reach)[0]
    assert len(near), "no vertex at front point (%.2f, %.2f)" % (x, z)
    return V[near[np.argmin(V[near, 1])]]


def weld(V, T):
    """Vertices that share a position (UV seams) as one: ids per vertex, welded positions, triangles."""
    _, first, inv = np.unique(np.round(V, 4), axis=0, return_index=True, return_inverse=True)
    inv = inv.reshape(-1)
    return inv, V[first], inv[T]


def normals(V, T):
    n = np.zeros_like(V)
    fn = np.cross(V[T[:, 1]] - V[T[:, 0]], V[T[:, 2]] - V[T[:, 0]])
    for k in range(3):
        np.add.at(n, T[:, k], fn)
    return n / np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)


def edges_of(T):
    e = np.vstack((T[:, (0, 1)], T[:, (1, 2)], T[:, (2, 0)]))
    return np.unique(np.sort(e, axis=1), axis=0)


def profile_points(bvh, name):
    """Midline landmarks read off the face's profile (a ray per 0.02 cm of height at x = 0): the same
    definition on both heads, so they pair up without anyone placing them."""
    zs = np.arange(-7.0, 9.0, 0.02)
    ys = np.array([front_hit(bvh, 0.0, z)[1] - J[1] for z in zs])
    sm = np.convolve(ys, np.ones(7) / 7.0, mode="same")

    def walk(i, step, want_max, least=0.04):
        """The next local extreme of depth from sample i (max = a recess, min = a point that stands out)."""
        best = i
        while 0 < i + step < len(sm) - 1:
            i += step
            better = sm[i] > sm[best] if want_max else sm[i] < sm[best]
            if better:
                best = i
            elif abs(sm[i] - sm[best]) > least:
                return best
        return best

    band = np.where((zs > 1.0) & (zs < 4.0))[0]             # full lips can stand further out than the nose
    prn = band[np.argmin(sm[band])]
    sn = walk(prn, -1, True, 0.1)
    ls = walk(sn, -1, False)
    sto = walk(ls, -1, True)
    li = walk(sto, -1, False)

    def chin(angle):
        """The profile's furthest point in a direction between forward (0) and down (90 degrees): a
        receding chin has no point that stands out, so the chin is found by direction."""
        a = np.radians(angle)
        low = np.where((zs < zs[li] - 1.2) & (zs > zs[li] - 5.5) & (sm < sm[li] + 3.0))[0]
        return low[np.argmax(-sm[low] * np.cos(a) - zs[low] * np.sin(a))]

    pg, me = chin(25.0), chin(65.0)
    between = np.arange(pg + 1, li)
    chord = sm[pg] + (sm[li] - sm[pg]) * (zs[between] - zs[pg]) / (zs[li] - zs[pg])
    sl = between[np.argmax(sm[between] - chord)]
    nas = walk(prn, 1, True, 0.02)
    out = {"pronasale": prn, "subnasale": sn, "lip_upper": ls, "stomion": sto, "lip_lower": li,
           "sublabial": sl, "pogonion": pg, "menton": me, "nasion": nas}
    pts = {}
    for k, i in out.items():
        y = ys[i]
        if k == "stomion":              # a ray can pass between the lips: keep the point at the lips' touch
            y = min(y, min(ys[ls], ys[li]) + 0.45)
        pts[k] = J + np.array((0.0, y, zs[i]))
    print("FIT profile %-4s" % name, "  ".join("%s %.2f/%.2f" % (k[:6], -(v[1] - J[1]), v[2] - J[2]) for k, v in pts.items()))
    return pts

mhRows, mhIds = part(mhT, mhMat, "head")
herBVH = bvh_of(herV, herT)
mhBVH = bvh_of(mhV, mhT[mhRows])


def measure(name, V, T):
    """The head's tape, cm (the head joint is the origin; ears are what stands more than 7.2 cm out)."""
    r = V - J
    front = r[:, 1] < -3.0
    crown = r[np.abs(r[:, 0]) < 3.0, 2].max()
    top = r[r[:, 2] > crown - 6.0]
    pts = profile_points(bvh_of(V, T), name)
    p = {k: v - J for k, v in pts.items()}
    band = lambda lo, hi: 2.0 * np.abs(r[front & (r[:, 2] > lo) & (r[:, 2] < hi), 0]).max()
    ear = r[(np.abs(r[:, 0]) > 7.2) & (r[:, 2] < 12.0) & (r[:, 2] > 0.0)]
    row = dict(crown=crown, chin=p["menton"][2], height=crown - p["menton"][2], skull=2.0 * np.abs(top[:, 0]).max(),
               cheeks=band(3.5, 5.0), jaw=band(p["stomion"][2] - 0.3, p["stomion"][2] + 0.3),
               nose=-p["pronasale"][1], nose_z=p["pronasale"][2], mouth_z=p["stomion"][2], chin_fwd=-p["pogonion"][1],
               ear_top=ear[:, 2].max(), ear_back=ear[:, 1].max(), ear_out=np.abs(ear[:, 0]).max())
    print("FIT tape %-4s" % name, "  ".join("%s %.1f" % kv for kv in row.items()))
    return row


if "--look" in argv:
    # --look <head.fbx> <name>: the same pictures of any MetaHuman face mesh (Tools/mh_face.py MODE='export')
    path, tag = argv[argv.index("--look") + 1:][:2]
    lookV, lookT, lookMat, _, _ = load(path)
    show = np.array([("Skin" in m and "LOD0" in m) or any(k in m for k in ("Eye_Left", "Eye_Right", "EyeL_", "EyeR_")) for m in lookMat])
    skin = np.array([("Skin" in m and "LOD0" in m) for m in lookMat])
    measure(tag, lookV, lookT[skin])
    views(tag, lookV, lookT[show])
    sys.exit(0)

def neck_centre(bvh):
    """Front-to-back middle of the neck on the midline, 6.5 to 9 cm under the head joint (cm forward)."""
    mids = []
    for z in np.arange(-9.0, -6.4, 0.5):
        front = bvh.ray_cast(Vector((J[0], J[1] - 60.0, J[2] + z)), Vector((0.0, 1.0, 0.0)))[0]
        back = bvh.ray_cast(Vector((J[0], J[1] + 60.0, J[2] + z)), Vector((0.0, -1.0, 0.0)))[0]
        mids.append(-(front[1] + back[1]) / 2.0 + J[1])
    return float(np.mean(mids))


# The two skeletons put the head joint at different depths in the neck (hers sits further forward in
# it), so joints together would hang her face a centimetre behind the MetaHuman's neck. Slide her head
# until the necks agree, then read every landmark (the side pictures were made before the slide).
SLIDE = neck_centre(mhBVH) - neck_centre(herBVH)
herV[:, 1] -= SLIDE
herBVH = bvh_of(herV, herT)
print("FIT her head slid %.2f cm forward: neck over neck" % SLIDE)
herProfile = profile_points(herBVH, "her")
mhProfile = profile_points(mhBVH, "mh")
MARKS = json.load(open(os.path.join(OUT, "landmarks.json")))
src, dst, names = [], [], []        # landmark pairs: on the MetaHuman, on her
for k in mhProfile:
    src.append(mhProfile[k]); dst.append(herProfile[k]); names.append(k)
for side in (1.0, -1.0):
    tag = "_l" if side > 0 else "_r"
    for k, v in MARKS["front"].items():
        if k.startswith("eye_"):
            continue                # the eyes are placed as a whole, below
        src.append(front_hit(mhBVH, v["mh"][0] * side, v["mh"][1]))
        dst.append(front_hit(herBVH, v["her"][0] * side, v["her"][1]))
        names.append(k + tag)
    for k, v in MARKS["rim"].items():
        src.append(rim_point(mhV[mhIds], v["mh"][0], v["mh"][1], side))
        dst.append(rim_point(herV, v["her"][0] + SLIDE, v["her"][1], side))
        names.append(k + tag)
    for k, v in MARKS["side"].items():
        src.append(side_hit(mhBVH, v["mh"][0], v["mh"][1], side))
        dst.append(side_hit(herBVH, v["her"][0] + SLIDE, v["her"][1], side))
        names.append(k + tag)
for n, a, b in zip(names, src, dst):
    print("FIT mark %-16s mh %s -> her %s  moves %.2f" % (n, np.round(a - J, 2), np.round(b - J, 2), np.linalg.norm(b - a)))


def boundary_loops(T):
    """Open edges of a welded mesh, grouped into loops (lists of vertex ids)."""
    e = np.vstack((T[:, (0, 1)], T[:, (1, 2)], T[:, (2, 0)]))
    key, count = np.unique(np.sort(e, axis=1), axis=0, return_counts=True)
    link = {}
    for a, b in key[count == 1]:
        link.setdefault(int(a), []).append(int(b))
        link.setdefault(int(b), []).append(int(a))
    loops, seen = [], set()
    for start in link:
        if start in seen:
            continue
        loop, stack = [], [start]
        while stack:
            v = stack.pop()
            if v in seen:
                continue
            seen.add(v)
            loop.append(v)
            stack.extend(link[v])
        loops.append(np.array(loop))
    return loops


# the head on its own, UV seams closed: W are its vertices, WT its triangles, wid maps a head vertex to W
wid, W, WT = weld(mhV[mhIds], np.searchsorted(mhIds, mhT[mhRows]))
E = edges_of(WT)
loops = boundary_loops(WT)
for lp in loops:
    print("FIT open loop: %4d vertices around %s" % (len(lp), np.round(W[lp].mean(0) - J, 2)))


LID = 0.12       # cm of closed lid over the eyeball: her lids are shut, the MetaHuman's eye opens under them
EYE_PTS = ("eye_in", "eye_out", "eye_top", "eye_bot")


def tps2d(src, dst):
    """Thin-plate warp of a plane through matching points (rows of two coordinates)."""
    def U(r):
        return np.where(r > 1e-9, r * r * np.log(np.maximum(r, 1e-9)), 0.0)
    n = len(src)
    A = np.zeros((n + 3, n + 3))
    A[:n, :n] = U(np.linalg.norm(src[:, None] - src[None], axis=2))
    A[:n, n] = A[n, :n] = 1.0
    A[:n, n + 1:] = src
    A[n + 1:, :n] = src.T
    coef = np.linalg.solve(A, np.vstack((dst, np.zeros((3, 2)))))
    return lambda P: U(np.linalg.norm(P[:, None] - src[None], axis=2)) @ coef[:n] + coef[n] + P @ coef[n + 1:]


def angles(P, c):
    """A point seen from an eyeball's centre: (turn to her left, lift) in radians, and its distance."""
    q = P - c
    d = np.linalg.norm(q, axis=-1)
    return np.stack((np.arctan2(q[..., 0], -q[..., 1]), np.arcsin(np.clip(q[..., 2] / d, -1.0, 1.0))), axis=-1), d


def from_angles(a, d, c):
    return c + d[..., None] * np.stack((np.cos(a[..., 1]) * np.sin(a[..., 0]), -np.cos(a[..., 1]) * np.cos(a[..., 0]), np.sin(a[..., 1])), axis=-1)


EYES = {}
for side, key in ((1.0, "eye_l"), (-1.0, "eye_r")):
    ball = mhV[part(mhT, mhMat, key)[1]]
    c0 = (ball.max(0) + ball.min(0)) / 2.0
    radius = float(np.linalg.norm(ball - c0, axis=1).max())
    m = np.array([(MARKS["front"][k]["mh"][0] * side, MARKS["front"][k]["mh"][1]) for k in EYE_PTS])
    h = np.array([(MARKS["front"][k]["her"][0] * side, MARKS["front"][k]["her"][1]) for k in EYE_PTS])
    s = float(np.linalg.norm(h[1] - h[0]) / np.linalg.norm(m[1] - m[0]))
    # the eyeball: across, where it sat between the corners; up, in the middle of her lids' outline (the
    # iris then sits in the middle of the opening); deep, one lid's thickness behind her closed lid
    c1 = np.array((J[0] + (h[0, 0] + h[1, 0]) / 2.0 + s * (c0[0] - J[0] - (m[0, 0] + m[1, 0]) / 2.0), 0.0,
                   J[2] + (h[2, 1] + h[3, 1]) / 2.0))
    c1[1] = front_hit(herBVH, c1[0] - J[0], c1[2] - J[2])[1] + LID + s * radius
    # the opening's four points, as directions from the eyeball's centre: where they are on the
    # MetaHuman, and where her lids' outline puts them on the moved eyeball (same distance from it)
    a0, d0 = angles(np.array([front_vertex(mhV[mhIds], x, z) for x, z in m]), c0)
    lat = np.column_stack((J[0] + h[:, 0] - c1[0], J[2] + h[:, 1] - c1[2]))
    reach = np.minimum(np.linalg.norm(lat, axis=1), 0.97 * s * d0)
    lat *= (reach / np.linalg.norm(lat, axis=1))[:, None]
    depth = np.sqrt((s * d0) ** 2 - reach ** 2)
    a1, _ = angles(np.column_stack((c1[0] + lat[:, 0], c1[1] - depth, c1[2] + lat[:, 1])), c1)
    EYES[key] = dict(s=s, c0=c0, c1=c1, radius=radius, warp=tps2d(a0, a1))
    print("FIT %s: radius %.2f, scale %.3f, centre %s -> %s; opening %.2f x %.2f -> %.2f x %.2f cm" % (
        key, radius, s, np.round(c0 - J, 2), np.round(c1 - J, 2), abs(m[1, 0] - m[0, 0]), m[2, 1] - m[3, 1],
        abs(h[1, 0] - h[0, 0]), h[2, 1] - h[3, 1]))


def eyeball_move(P, key):
    e = EYES[key]
    return e["c1"] + e["s"] * (P - e["c0"])


def lid_move(P, key):
    """Skin that lies on the eyeball: each vertex keeps its distance from the eyeball's centre (so it
    still lies on it) and its direction is warped so the opening's outline lands on her lids' outline."""
    e = EYES[key]
    a, d = angles(P, e["c0"])
    return from_angles(e["warp"](a), e["s"] * d, e["c1"])


def rbf(src, dst):
    """A smooth warp of space that carries each src point onto its dst point (kernel r, affine part)."""
    src, dst = np.asarray(src), np.asarray(dst)
    n = len(src)
    A = np.zeros((n + 4, n + 4))
    A[:n, :n] = np.linalg.norm(src[:, None] - src[None], axis=2)
    A[:n, n] = A[n, :n] = 1.0
    A[:n, n + 1:] = src
    A[n + 1:, :n] = src.T
    coef = np.linalg.solve(A + np.eye(n + 4) * 1e-9, np.vstack((dst, np.zeros((4, 3)))))

    def move(P):
        out = np.empty_like(P)
        for i in range(0, len(P), 4000):
            Q = P[i:i + 4000]
            out[i:i + 4000] = np.linalg.norm(Q[:, None] - src[None], axis=2) @ coef[:n] + coef[n] + Q @ coef[n + 1:]
        return out
    return move


def spread(points, count):
    """Indices of `count` points far from one another (farthest-point sampling)."""
    pick = [0]
    d = np.linalg.norm(points - points[0], axis=1)
    for _ in range(min(count, len(points)) - 1):
        pick.append(int(np.argmax(d)))
        d = np.minimum(d, np.linalg.norm(points - points[pick[-1]], axis=1))
    return np.array(pick)


# Skin that lies on an eyeball (lids, their edges, the socket behind) moves with its eye and is held
# there; a band around it is left to settle between the eye and her surface.
ON_EYE, NEAR_EYE = 0.18, 1.5
held = np.zeros(len(W), bool)
band = np.zeros(len(W), bool)
target = W.copy()
for key, e in EYES.items():
    d = np.linalg.norm(W - e["c0"], axis=1)
    on = d < e["radius"] + ON_EYE
    target[on] = lid_move(W[on], key)
    held |= on
    band |= (d < e["radius"] + NEAR_EYE) & ~on
    some = np.where(on)[0][spread(W[on], 40)]
    src.extend(W[some]); dst.extend(target[some]); names.extend([key] * len(some))
    print("FIT %s: %d vertices held on the eyeball, they move %.2f cm on average" % (key, on.sum(), np.linalg.norm(target[on] - W[on], axis=1).mean()))
# far below the face nothing moves: the lower neck and chest are the body's
low = np.where(W[:, 2] < J[2] - 15.5)[0]
anchors = low[spread(W[low], 30)]
src.extend(W[anchors]); dst.extend(W[anchors]); names.extend(["neck"] * len(anchors))
warp = rbf(src, dst)
X0 = warp(W)
print("FIT landmark warp: %d points; vertices move %.2f cm on average, %.2f at most" % (len(src), np.linalg.norm(X0 - W, axis=1).mean(), np.linalg.norm(X0 - W, axis=1).max()))

# Skin her surface has no match for (inside the mouth, the nostrils, the ear's folds) is found by a
# short ray along its own normal: if that hits the head again, it is in a hollow and only follows.
selfBVH = bvh_of(W, WT)
N0 = normals(W, WT)
hollow = np.zeros(len(W), bool)
for i in range(len(W)):
    hit = selfBVH.ray_cast(Vector(W[i] + N0[i] * 0.05), Vector(N0[i]), 2.5)
    hollow[i] = hit[0] is not None
for _ in range(2):                       # and the skin right at a hollow's mouth (nostril rims, the lips' line)
    grown = hollow.copy()
    grown[E[hollow[E[:, 1]], 0]] = True
    grown[E[hollow[E[:, 0]], 1]] = True
    hollow = grown
# The ears follow the landmark warp only: a human ear stretched to her ear's outline keeps its own
# folds, where settling it on her blade-like ear crumples them. An ear vertex is one in the ear's box
# with skin close behind it towards the middle of the head (the skull has the whole head behind it).
ear = np.zeros(len(W), bool)
rel = W - J
for i in np.where((np.abs(rel[:, 0]) > 4.5) & (np.abs(rel[:, 1]) < 3.0) & (rel[:, 2] > 0.0) & (rel[:, 2] < 8.0))[0]:
    inward = np.array((-np.sign(rel[i, 0]), 0.0, 0.0))
    hit = selfBVH.ray_cast(Vector(W[i] + inward * 0.05), Vector(inward), 4.0)
    ear[i] = hit[0] is not None
print("FIT ears: %d vertices follow the landmark warp only" % ear.sum())
free = ~held & ~band & ~hollow & ~ear & (W[:, 2] > J[2] - 16.0)
deg = np.bincount(E.ravel(), minlength=len(W)).astype(float)


def neighbour_mean(d):
    out = np.zeros_like(d)
    np.add.at(out, E[:, 0], d[E[:, 1]])
    np.add.at(out, E[:, 1], d[E[:, 0]])
    return out / deg[:, None]


def settle(X, stiff, reach, sweeps=60):
    """One round: every free vertex looks for the nearest point of her surface that faces the same
    way; the pull is then smoothed over the mesh (stiff = how much neighbours count), so features
    slide as little as possible and the skin with no match follows its neighbours."""
    N = normals(X, WT)
    pull = np.zeros_like(X)
    got = np.zeros(len(X))
    for i in np.where(free)[0]:
        loc, nrm, _, dist = herBVH.find_nearest(Vector(X[i]), reach)
        if loc is not None and N[i] @ np.array(nrm) > 0.3:
            pull[i], got[i] = np.array(loc) - X[i], 1.0
    d = pull.copy()
    for _ in range(sweeps):
        d = (got[:, None] * pull + stiff * neighbour_mean(d)) / (got[:, None] + stiff)
        d[held] = 0.0
    left = np.linalg.norm(pull[got > 0] - d[got > 0], axis=1)
    return X + d, int(got.sum()), float(left.mean()), float(np.percentile(left, 99))


X = X0.copy()
X[held] = target[held]
for stiff, reach in ((8.0, 3.0), (4.0, 2.5), (2.0, 2.0), (1.0, 1.5), (0.5, 1.2), (0.25, 1.0), (0.1, 0.8)):
    X, n, mean, p99 = settle(X, stiff, reach)
    print("FIT settle stiff %.2f: %d of %d free vertices found her surface; still off it by %.3f cm on average, %.3f at 99%%" % (stiff, n, free.sum(), mean, p99))

# the neck: all hers above 7 cm under the joint, all the body's below 15 (a shorter fade leaves a
# step at the nape, where her neck is slimmer than the body's)
t = np.clip((W[:, 2] - (J[2] - 15.0)) / 8.0, 0.0, 1.0)
X = W + (t * t * (3.0 - 2.0 * t))[:, None] * (X - W)
# under the jaw and down the neck her mesh has small creases where its head was joined on: keep the
# shape there and lose the creases (each pass moves a vertex halfway to the middle of its neighbours)
soft = np.clip((J[2] - 4.5 - W[:, 2]) / 2.0, 0.0, 1.0) * (t > 0.0)
# and round the eyes, where the skin steps from the lids (deep, on the eyeball) up to her surface
# (shallow: her face is nearly flat beside the eyes): ease that step out over the band and a little past it
for e in EYES.values():
    d = np.linalg.norm(W - e["c0"], axis=1) - e["radius"]
    soft = np.maximum(soft, np.clip((NEAR_EYE + 1.0 - d) / 1.0, 0.0, 1.0) * (d > ON_EYE))
soft = soft * ~held
for _ in range(12):
    X = X + (0.5 * soft)[:, None] * (neighbour_mean(X) - X)
if not IDENTITY:
    newV[mhIds] = X[wid]
    for key in ("eye_l", "eye_r"):
        ids = part(mhT, mhMat, key)[1]
        newV[ids] = eyeball_move(mhV[ids], key)
    # teeth: behind the lips where the lips went, and as much narrower as her jaw is at the mouth
    sto = mhProfile["stomion"]
    near = int(np.argmin(np.linalg.norm(W - sto, axis=1)))
    jaw = [abs(side_hit(b, -(p["stomion"][1] - J[1]) - 3.0, p["stomion"][2] - J[2], 1.0)[0] - J[0]) for b, p in ((herBVH, herProfile), (mhBVH, mhProfile))]
    ts = jaw[0] / jaw[1]
    ids = part(mhT, mhMat, "teeth")[1]
    back = np.array((0.0, 0.4, 0.0))
    newV[ids] = X[near] + back + ts * (mhV[ids] - W[near] - back)
    print("FIT teeth: jaw half-width at the mouth %.2f (hers) / %.2f cm, scale %.3f, moved %s" % (jaw[0], jaw[1], ts, np.round(X[near] - W[near], 2)))
    move = np.linalg.norm(X - W, axis=1)
    print("FIT head: vertices moved %.2f cm on average, %.2f at most; %d held on the eyes, %d in hollows, %d settled on her surface" % (
        move.mean(), move.max(), held.sum(), (hollow & ~held).sum(), free.sum()))
    measure("her", herV, herT)
    measure("mh", mhV, mhT[mhRows])
    measure("fit", newV, mhT[mhRows])
    rows = np.concatenate([part(mhT, mhMat, k)[0] for k in ("head", "eye_l", "eye_r")])
    views("fit", newV, mhT[rows])
# FIT-BODY

for key in PARTS:
    rows, _ = part(mhT, mhMat, key)
    write_static("face_" + key, newV, mhT, mhUV, rows)
