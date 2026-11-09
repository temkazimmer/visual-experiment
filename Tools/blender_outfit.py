"""Build a layered outfit on the android skin in Blender (headless). Outside the editor:

  /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup \
      --python Tools/blender_outfit.py -- <android_skin.fbx> <out.fbx> [<preview_dir>]

Everything is cut from the skin itself (SourceArt/Character/android_skin.fbx), so it fits
her exactly. Pieces sit at different offsets so the outfit has real depth:

  0.25 cm  sheer mesh bodysuit, stockings          (material slot Sheer)
  0.3-0.7  gloves, yoke, underbust band, belt, welts (slot Latex)
  ~1 cm    boning seams, garter straps, clips       (slot Latex / Trim)
  edges    piping tubes on every opening            (slot Trim)

Each vertex copies skin weights and the BustUp/BustDown/GlutesUp/GlutesDown deltas from
the nearest skin vertex, so animation and the body sliders drive the outfit too.
Tools/char_outfit.py imports the FBX into UE. Look values are in FIT at the top.
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree
from mathutils.kdtree import KDTree

FIT = dict(
    sheer=0.25, glove=0.28, stocking=0.22, yoke=0.55, band=0.65, belt=0.75, welt=0.5,
    seam_r=0.2, seam_off=0.45, pipe_r=0.22, strap_w=1.3, strap_off=0.9,
    yoke_drop=4.0,        # cm above the bust peak where the opaque yoke ends
    band_h=3.2,           # underbust band height
    belt_h=4.5, belt_above_pelvis=7.0,
    welt_h=6.0, welt_below_crotch=17.0,
    glove_from=0.42,      # 0 = shoulder, 1 = wrist: where the opera gloves start
)

args = sys.argv[sys.argv.index("--") + 1:]
SRC, OUT = args[0], args[1]
PREVIEW = args[2] if len(args) > 2 else None

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=SRC)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
skin = next(o for o in bpy.data.objects if o.type == "MESH")
for m in skin.modifiers:
    if m.type == "ARMATURE":
        m.show_viewport = False
SM = skin.matrix_world
SMI = SM.inverted()
me = skin.data
W = [SM @ v.co for v in me.vertices]
N = [(SM.to_3x3() @ v.normal).normalized() for v in me.vertices]
gidx = {g.name: g.index for g in skin.vertex_groups}
B = lambda n: arm.matrix_world @ arm.data.bones[n].head_local
fwd = (B("ball_l") - B("foot_l")) + (B("ball_r") - B("foot_r")); fwd.z = 0; fwd.normalize()
up = Vector((0, 0, 1))
side = up.cross(fwd).normalized()
pel = B("pelvis")
lat = lambda p: (p - pel).dot(side)
D = math.radians


def groups(*keys):
    return [g for g in gidx if any(k in g for k in keys)]


def wsum(i, names):
    ids = {gidx[n] for n in names if n in gidx}
    return sum(g.weight for g in me.vertices[i].groups if g.group in ids)


TORSO = groups("pelvis", "spine", "clavicle")
NECK = groups("neck")
HEAD = groups("head")
UPPER = groups("upperarm")
FORE = groups("lowerarm")
HAND = groups("hand", "thumb", "index", "middle", "ring", "pinky")
THIGH = groups("thigh")
LEG = groups("thigh", "calf", "foot", "ball")

# ---- landmarks --------------------------------------------------------------------------
idx = range(len(W))
Z = [p.z for p in W]


def warr(names):
    ids = {gidx[n] for n in names if n in gidx}
    return [sum(g.weight for g in v.groups if g.group in ids) for v in me.vertices]


wT, wTN, wTNT = warr(TORSO), warr(TORSO + NECK), warr(TORSO + NECK + THIGH)
wU, wARM, wLEG = warr(UPPER), warr(UPPER + FORE + HAND), warr(LEG)
neck_z = B("neck_01").z
chin_z = B("head").z - 4
bust_z = max((W[i].dot(fwd), W[i].z) for i in idx if 115 < W[i].z < 140 and abs(lat(W[i])) < 15)[1]
crotch_z = min(W[i].z for i in idx if abs(lat(W[i])) < 1.2 and 60 < W[i].z < pel.z)


def width_at(z):
    xs = [lat(W[i]) for i in idx if abs(W[i].z - z) < 0.6 and wsum(i, TORSO) > 0.5]
    return max(xs) - min(xs) if xs else 1e9


waist_z = min((width_at(z), z) for z in [pel.z + k for k in range(4, 26)])[1]
hip_half = max(abs(lat(W[i])) for i in idx if abs(W[i].z - (pel.z - 6)) < 0.8 and wsum(i, TORSO + THIGH) > 0.5)
welt_top = crotch_z - FIT["welt_below_crotch"]
print("landmarks: neck %.1f chin %.1f bust %.1f waist %.1f pelvis %.1f crotch %.1f welt %.1f" %
      (neck_z, chin_z, bust_z, waist_z, pel.z, crotch_z, welt_top))


# ---- builders ---------------------------------------------------------------------------
PIECES = []          # (bmesh, slot)
tri = bmesh.new(); tri.from_mesh(me); bmesh.ops.triangulate(tri, faces=tri.faces)
TRIS = [tuple(v.index for v in f.verts) for f in tri.faces]
tri.free()


def region(field, offset, slot):
    """The part of the skin where field(i) >= 0, cut cleanly along the zero line
    (triangles are clipped, not dropped) and pushed out along the normal."""
    val = [field(i) for i in idx]
    bm = bmesh.new(); made = {}

    def vert(a, b=None):
        key = a if b is None else (min(a, b), max(a, b))
        if key not in made:
            if b is None:
                p, n = W[a], N[a]
            else:
                t = val[a] / (val[a] - val[b])
                p, n = W[a].lerp(W[b], t), N[a].lerp(N[b], t).normalized()
            made[key] = bm.verts.new(p + n * offset)
        return made[key]

    for f in TRIS:
        if all(val[i] < 0 for i in f):
            continue
        poly = []
        for k in range(3):
            a, b = f[k], f[(k + 1) % 3]
            if val[a] >= 0:
                poly.append(vert(a))
            if (val[a] >= 0) != (val[b] >= 0):
                poly.append(vert(a, b))
        poly = [v for k, v in enumerate(poly) if v is not poly[k - 1]]
        if len(poly) >= 3:
            try:
                bm.faces.new(poly)
            except ValueError:
                pass
    PIECES.append((bm, slot))
    return bm


def edge_loops(bm, min_len=20.0):
    """Boundary edges of a piece chained into point lists: [(points, closed)]."""
    adj = {}
    for e in bm.edges:
        if e.is_boundary:
            a, b = e.verts
            adj.setdefault(a, []).append(b); adj.setdefault(b, []).append(a)
    seen, loops = set(), []
    for start in adj:
        if start in seen:
            continue
        chain, prev, cur = [start], None, start
        seen.add(start)
        while True:
            nxt = [n for n in adj[cur] if n is not prev and n not in seen]
            if not nxt:
                break
            prev, cur = cur, nxt[0]
            seen.add(cur); chain.append(cur)
        pts = [v.co.copy() for v in chain]
        if sum((a - b).length for a, b in zip(pts, pts[1:])) >= min_len:
            loops.append((pts, start in adj[chain[-1]]))
    return loops


def smooth_pts(pts, closed, iters=6):
    for _ in range(iters):
        n = len(pts)
        pts = [pts[i] if (not closed and i in (0, n - 1)) else
               (pts[(i - 1) % n] + pts[i] * 2 + pts[(i + 1) % n]) / 4 for i in range(n)]
    return pts


def resample(pts, closed, step):
    seq = pts + ([pts[0]] if closed else [])
    out = [seq[0]]; need = step
    for a, b in zip(seq, seq[1:]):
        L = (b - a).length; pos = 0.0
        while L - pos >= need:
            pos += need; out.append(a.lerp(b, pos / L)); need = step
        need -= L - pos
    if closed and len(out) > 2 and (out[-1] - out[0]).length < step * 0.5:
        out.pop()
    return out


kd = KDTree(len(W))
for i, p in enumerate(W):
    kd.insert(p, i)
kd.balance()
nearest_normal = lambda p: N[kd.find(p)[1]]


def tube(pts, r, slot, closed=False, sides=8, push=0.0):
    """A round cord along a point list (piping, boning)."""
    pts = smooth_pts(resample(pts, closed, 0.7), closed)
    n = len(pts)
    if n < 4:
        return
    bm = bmesh.new(); rings = []
    for i, p in enumerate(pts):
        a_, b_ = (pts[i - 1], pts[(i + 1) % n]) if closed else (pts[max(i - 1, 0)], pts[min(i + 1, n - 1)])
        t = (b_ - a_).normalized()
        out = nearest_normal(p)
        a = out - t * out.dot(t)
        if a.length < 1e-4:
            a = t.orthogonal()
        a.normalize(); b = t.cross(a)
        c = p + out * push
        rings.append([bm.verts.new(c + (a * math.cos(2 * math.pi * k / sides) + b * math.sin(2 * math.pi * k / sides)) * r)
                      for k in range(sides)])
    for i in range(n if closed else n - 1):
        r0, r1 = rings[i], rings[(i + 1) % n]
        for k in range(sides):
            bm.faces.new((r0[k], r0[(k + 1) % sides], r1[(k + 1) % sides], r1[k]))
    if not closed:
        bm.faces.new(rings[0][::-1]); bm.faces.new(rings[-1])
    PIECES.append((bm, slot))


def ribbon(pts, w, thick, slot):
    """A flat strap lying on the surface (garter straps)."""
    pts = smooth_pts(resample(pts, False, 0.8), False, 8)
    bm = bmesh.new(); rows = []
    for i, p in enumerate(pts):
        t = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
        n = nearest_normal(p); b = t.cross(n).normalized()
        rows.append([bm.verts.new(p + b * s * w / 2 + n * h) for s, h in ((-1, 0), (1, 0), (1, thick), (-1, thick))])
    for r0, r1 in zip(rows, rows[1:]):
        for k in range(4):
            bm.faces.new((r0[k], r0[(k + 1) % 4], r1[(k + 1) % 4], r1[k]))
    bm.faces.new(rows[0][::-1]); bm.faces.new(rows[-1])
    PIECES.append((bm, slot))
    return pts


def clip(center, n, t, size, slot):
    """A small piece of hardware: a bevelled block lying on the surface."""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    b = t.cross(n).normalized(); t = n.cross(b)
    M = Matrix((b * size[0], t * size[1], n * size[2])).transposed().to_4x4()
    M.translation = center
    bmesh.ops.transform(bm, matrix=M, verts=bm.verts)
    bmesh.ops.bevel(bm, geom=bm.edges[:], offset=min(size) * 0.22, segments=2, affect="EDGES")
    PIECES.append((bm, slot))


def bvh_of(sel):
    bm = bmesh.new(); bm.from_mesh(me)
    bm.verts.ensure_lookup_table()
    for v in bm.verts:
        v.co = W[v.index]
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not sel(v.index)], context="VERTS")
    t = BVHTree.FromBMesh(bm); bm.free(); return t


BODY_BVH = bvh_of(lambda i: True)
TORSO_BVH = bvh_of(lambda i: wARM[i] < 0.3)


def ray(bvh, c, z, theta, offset):
    d = fwd * math.cos(theta) + side * math.sin(theta)
    hit = bvh.ray_cast(Vector((c.x, c.y, z)) + d * 80, -d, 80)
    return hit[0] + hit[1] * offset if hit[0] else None


def surface_line(z0, z1, theta, offset, bvh=TORSO_BVH, step=0.8):
    """Points on the body at a fixed angle around the vertical axis, from z0 to z1."""
    n = max(2, int(abs(z1 - z0) / step))
    pts = [ray(bvh, pel, z0 + (z1 - z0) * k / n, theta, offset) for k in range(n + 1)]
    return [p for p in pts if p]


def piping(bm, r=None, slot="Trim", only=lambda pts: True):
    for pts, closed in edge_loops(bm):
        if only(pts):
            tube(pts, r or FIT["pipe_r"], slot, closed, push=0.12)


# ---- 1. sheer bodysuit: high neck, sleeveless, high-cut legs ------------------------------
def leg_cut(p):
    """Leg opening: at the crotch in the middle, rising to the hip bone at the sides."""
    a = min(1.0, abs(lat(p)) / hip_half)
    return crotch_z + (pel.z + 4 - crotch_z) * a ** 1.5


torso = lambda i: min(wTNT[i] - 0.5, 0.3 - wU[i])
body = region(lambda i: min(torso(i), chin_z - 1 - Z[i], Z[i] - leg_cut(W[i])), FIT["sheer"], "Sheer")
piping(body, FIT["pipe_r"] * 0.85, "Latex")

# ---- 2. opaque yoke: collar to just above the bust ----------------------------------------
yoke_lo = bust_z + FIT["yoke_drop"]
yoke = region(lambda i: min(wTN[i] - 0.5, 0.28 - wU[i], chin_z - 1.4 - Z[i],
                            Z[i] - yoke_lo + 2.5 * (abs(lat(W[i])) / 14.0) ** 2), FIT["yoke"], "Latex")
piping(yoke)

# ---- 3. underbust band, 4. garter belt ----------------------------------------------------
ub_top = bust_z - 7.5
band = region(lambda i: min(wT[i] - 0.5, 0.12 - wU[i], ub_top - Z[i], Z[i] - (ub_top - FIT["band_h"])), FIT["band"], "Latex")
piping(band)
belt_lo = pel.z + FIT["belt_above_pelvis"]      # high hip, like a garter belt
dip = lambda p: 2.2 * max(0.0, 1 - (abs(lat(p)) / 9.0) ** 2) * min(1.0, max(0.0, (p - pel).dot(fwd) / 4.0))   # soft V at the front
belt = region(lambda i: min(wT[i] - 0.5, belt_lo + FIT["belt_h"] - Z[i], Z[i] - belt_lo + dip(W[i])), FIT["belt"], "Latex")
piping(belt)

# ---- 5. boning: raised seams from the band down through the belt into the leg line --------
for th in (16, -16, 50, -50, 92, -92, 140, -140, 180):
    tube(surface_line(ub_top - FIT["band_h"] + 0.3, belt_lo + FIT["belt_h"] - 0.3, D(th), FIT["seam_off"]), FIT["seam_r"], "Latex")
# two seams up from the band over the bust to the yoke: they frame it
for th in (24, -24):
    tube(surface_line(ub_top, yoke_lo + 0.5, D(th), FIT["seam_off"]), FIT["seam_r"], "Latex")

# ---- 6. stockings and their welts ---------------------------------------------------------
stock = region(lambda i: min(wLEG[i] - 0.5, welt_top - 1 - Z[i]), FIT["stocking"], "Sheer")
welt = region(lambda i: min(wLEG[i] - 0.5, welt_top - Z[i], Z[i] - (welt_top - FIT["welt_h"])), FIT["welt"], "Latex")
piping(welt)
# back seam on each stocking
for s in (1, -1):
    leg_bvh = bvh_of(lambda i, s=s: wLEG[i] >= 0.5 and lat(W[i]) * s > 0)
    hipb, ankle = B("thigh_l" if s > 0 else "thigh_r"), B("foot_l" if s > 0 else "foot_r")
    if lat(hipb) * s < 0:
        hipb, ankle = B("thigh_r" if s > 0 else "thigh_l"), B("foot_r" if s > 0 else "foot_l")
    axis = lambda z, a=hipb, b=ankle: a.lerp(b, (a.z - z) / (a.z - b.z))
    pts = [ray(leg_bvh, axis(z), z, math.pi, FIT["stocking"] + 0.12)
           for z in [welt_top - FIT["welt_h"] - k * 0.8 for k in range(int((welt_top - FIT["welt_h"] - ankle.z - 3) / 0.8))]]
    tube([p for p in pts if p], 0.13, "Latex")

    # ---- 7. garter straps: belt -> welt, front and outer side, with clips -------------------
    for th_belt, th_leg in ((16 * s, 6 * s), (72 * s, 88 * s), (140 * s, 172 * s)):
        top = ray(TORSO_BVH, pel, belt_lo + 0.6, D(th_belt), FIT["belt"] + 0.2)
        bot = ray(leg_bvh, axis(welt_top - 2.2), welt_top - 2.2, D(th_leg), FIT["welt"] + 0.2)
        pts = []
        for k in range(49):      # taut: straight, lifted wherever the body bulges into it
            p = top.lerp(bot, k / 48)
            loc, nrm, _, _ = BODY_BVH.find_nearest(p)
            out = loc + nrm * FIT["strap_off"] * (0.35 + 0.65 * math.sin(math.pi * k / 48))
            pts.append(out if (p - loc).dot(nrm) < (out - loc).dot(nrm) else p)
        pts = ribbon(pts, FIT["strap_w"], 0.22, "Latex")
        t = (pts[-1] - pts[-5]).normalized()
        for at, size in ((pts[-2], (2.1, 2.6, 0.55)), (pts[1], (1.9, 1.3, 0.5)), (pts[len(pts) // 2], (1.7, 0.9, 0.45))):
            clip(at + nearest_normal(at) * 0.3, nearest_normal(at), t, size, "Trim")

# ---- 8. opera gloves ----------------------------------------------------------------------
sh = {s: B(n) for n in ("upperarm_l", "upperarm_r") for s in (1, -1) if lat(B(n)) * s > 0}
wr = {s: B(n) for n in ("hand_l", "hand_r") for s in (1, -1) if lat(B(n)) * s > 0}


def along_arm(i):
    s = 1 if lat(W[i]) > 0 else -1
    a, b = sh[s], wr[s]
    return (W[i] - a).dot(b - a) / (b - a).length_squared


glove = region(lambda i: min(wARM[i] - 0.5, (along_arm(i) - FIT["glove_from"]) * 30), FIT["glove"], "Latex")
piping(glove)
# a second cuff ring a little below the glove top
cuff = region(lambda i: min(wARM[i] - 0.5, (along_arm(i) - FIT["glove_from"] - 0.03) * 30,
                            (FIT["glove_from"] + 0.11 - along_arm(i)) * 30), FIT["glove"] + 0.3, "Latex")
piping(cuff)

# ---- assemble: one mesh, three material slots ----------------------------------------------
SLOTS = ["Sheer", "Latex", "Trim"]
out_bm = bmesh.new()
for bm, slot in PIECES:
    tmp = bpy.data.meshes.new("tmp"); bm.to_mesh(tmp); bm.free()
    start = len(out_bm.faces)
    out_bm.from_mesh(tmp); bpy.data.meshes.remove(tmp)
    out_bm.faces.ensure_lookup_table()
    for f in out_bm.faces[start:]:
        f.material_index = SLOTS.index(slot); f.smooth = True
om = bpy.data.meshes.new("SKM_Outfit"); out_bm.to_mesh(om); out_bm.free()
outfit = bpy.data.objects.new("SKM_Outfit", om)
bpy.context.collection.objects.link(outfit)
for s in SLOTS:
    om.materials.append(bpy.data.materials.new(s))

# weights and body-slider shapes come from the nearest skin (3 nearest, distance-weighted)
names = [g.name for g in skin.vertex_groups]
vgs = [outfit.vertex_groups.new(name=n) for n in names]
R3 = SM.to_3x3()
keys = me.shape_keys.key_blocks if me.shape_keys else []
basis = [v.co.copy() for v in me.vertices]
deltas = {k.name: [R3 @ (k.data[i].co - basis[i]) for i in idx] for k in keys[1:]}
near = []
for v in om.vertices:
    hits = kd.find_n(v.co, 3)
    ws = [1.0 / (d + 0.05) for _, _, d in hits]; tot = sum(ws)
    near.append([(i, w / tot) for (_, i, _), w in zip(hits, ws)])
for vi, hits in enumerate(near):
    acc = {}
    for i, w in hits:
        for g in me.vertices[i].groups:
            acc[g.group] = acc.get(g.group, 0.0) + g.weight * w
    top = sorted(acc.items(), key=lambda x: -x[1])[:4]
    tot = sum(w for _, w in top) or 1.0
    for g, w in top:
        vgs[g].add([vi], w / tot, "REPLACE")
outfit.shape_key_add(name="Basis", from_mix=False)
for name, dl in deltas.items():
    kb = outfit.shape_key_add(name=name, from_mix=False)
    for vi, hits in enumerate(near):
        d = Vector((0, 0, 0))
        for i, w in hits:
            d += dl[i] * w
        if d.length_squared > 1e-10:
            kb.data[vi].co = om.vertices[vi].co + d
print("outfit verts", len(om.vertices), "faces", len(om.polygons), "pieces", len(PIECES))

# ---- previews ------------------------------------------------------------------------------
if PREVIEW:
    os.makedirs(PREVIEW, exist_ok=True)

    def pmat(m, col, rough, coat=0.0, alpha=None, metal=0.0):
        m.use_nodes = True
        p = m.node_tree.nodes["Principled BSDF"]
        p.inputs["Base Color"].default_value = (*col, 1)
        p.inputs["Roughness"].default_value = rough
        p.inputs["Metallic"].default_value = metal
        p.inputs["Coat Weight"].default_value = coat
        p.inputs["Coat Roughness"].default_value = 0.06
        if alpha is not None:      # sheer: more opaque at grazing angles, like real mesh
            lw = m.node_tree.nodes.new("ShaderNodeLayerWeight"); lw.inputs["Blend"].default_value = 0.35
            mr = m.node_tree.nodes.new("ShaderNodeMapRange")
            mr.inputs["To Min"].default_value, mr.inputs["To Max"].default_value = alpha, 0.97
            m.node_tree.links.new(lw.outputs["Facing"], mr.inputs["Value"])
            m.node_tree.links.new(mr.outputs["Result"], p.inputs["Alpha"])
            for attr, val in (("surface_render_method", "BLENDED"), ("blend_method", "BLEND")):
                try:
                    setattr(m, attr, val)
                except Exception:
                    pass
    pmat(om.materials[0], (0.01, 0.01, 0.012), 0.6, alpha=0.38)
    pmat(om.materials[1], (0.012, 0.013, 0.018), 0.3, coat=1.0)
    pmat(om.materials[2], (0.75, 0.78, 0.82), 0.25, metal=1.0)
    sk = bpy.data.materials.new("Skin"); pmat(sk, (0.62, 0.36, 0.28), 0.28, coat=1.0)
    me.materials.clear(); me.materials.append(sk)
    for p in me.polygons:
        p.use_smooth = True
    scn = bpy.context.scene
    for eng in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT"):
        try:
            scn.render.engine = eng; break
        except TypeError:
            pass
    scn.render.resolution_x, scn.render.resolution_y = 1000, 1500
    world = bpy.data.worlds.new("Dust"); scn.world = world; world.use_nodes = True
    bg = world.node_tree.nodes["Background"]
    bg.inputs[0].default_value = (0.9, 0.42, 0.14, 1); bg.inputs[1].default_value = 0.7
    for name, d, e, col in (("Key", fwd * 0.5 + side * 0.7 + up * 0.35, 4.0, (1, 0.8, 0.6)),
                            ("Rim", -fwd * 0.8 - side * 0.5 + up * 0.3, 5.0, (0.7, 0.85, 1))):
        ld = bpy.data.lights.new(name, "SUN"); ld.energy = e; ld.color = col; ld.angle = 0.1
        lo = bpy.data.objects.new(name, ld); bpy.context.collection.objects.link(lo)
        lo.rotation_euler = (-d).normalized().to_track_quat("-Z", "Y").to_euler()
    cd = bpy.data.cameras.new("Cam"); cam = bpy.data.objects.new("Cam", cd)
    bpy.context.collection.objects.link(cam); scn.camera = cam
    shots = (("front", fwd * 0.9 + side * 0.42, 92, 560, 70), ("back", -fwd * 0.9 + side * 0.42, 92, 560, 70),
             ("torso", fwd * 0.85 + side * 0.5, 118, 250, 70), ("hip", fwd * 0.6 + side * 0.8, 82, 210, 70),
             ("back_hip", -fwd * 0.8 + side * 0.6, 84, 220, 70))
    for label, d, z, dist, lens in shots:
        cd.lens = lens
        tgt = Vector((pel.x, pel.y, z))
        cam.location = tgt + d.normalized() * dist
        cam.rotation_euler = (tgt - cam.location).to_track_quat("-Z", "Y").to_euler()
        scn.render.filepath = os.path.join(PREVIEW, label + ".png")
        bpy.ops.render.render(write_still=True)

# ---- export --------------------------------------------------------------------------------
outfit.modifiers.new("Armature", "ARMATURE").object = arm
bpy.data.objects.remove(skin, do_unlink=True)
for o in list(bpy.data.objects):
    if o.type not in ("ARMATURE", "MESH"):
        bpy.data.objects.remove(o, do_unlink=True)
bpy.ops.object.select_all(action="DESELECT")
arm.select_set(True); outfit.select_set(True)
bpy.context.view_layer.objects.active = arm
bpy.ops.export_scene.fbx(filepath=OUT, use_selection=True, object_types={"ARMATURE", "MESH"},
                         add_leaf_bones=False, use_armature_deform_only=False,
                         mesh_smooth_type="FACE", bake_anim=False, axis_forward="-Z", axis_up="Y")
print("wrote", OUT)
