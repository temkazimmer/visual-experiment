"""Fit the card hairstyles to the MetaHuman skull: one uniform scale and an offset per style.

  python3 Tools/ue.py -c "MESH='/Game/Character/MH/Built/MHC_Her/Face/SKM_MHC_Her_FaceMesh'; OUT='<project>/SourceArt/Character/MH/mh_head.fbx'; exec(open('<project>/Tools/mh_skin_export.py').read())"
  Blender -b --factory-startup --python Tools/mh_hair_fit.py -- <project>

The cards were grown on the old head (SourceArt/Character/android_skin.fbx, drawn at 0.935). Each
style's scalp cap and the card vertices next to the scalp are put on the MetaHuman head
(mh_head.fbx) by least squares: after `p -> s * p + t` in the head bone's space, a vertex should
clear the new scalp by s times what it cleared the old one. Prints both heads' measurements, each
style's fit and what the fit leaves (cap vertices under the skin, the worst clearance error).

  --save   writes the fits to Config/DefaultGame.ini, [VisexMHHairFit] (a line per style:
           scale,up,front,side in UE's head-bone axes; Blender's y is flipped). That section is the
           one place the numbers live: UVisexMHHairComponent reads it, no rebuild. To try numbers
           by hand in Play: visex.Body.MetaHuman.HairFit "0.95,-0.5,1.1,0".
  --write  also writes refitted cards (SourceArt/Character/MH/Hair/*_mh.fbx: cap and roots moved
           onto the new scalp, bones untouched) for Tools/mh_hair.py to import as SKM_Hair*_MH,
           which the game prefers when they exist. Only worth it on the final head.

Run again after every re-assembly or new head shape (re-export the head first).
"""
import os
import sys

import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

P = sys.argv[sys.argv.index("--") + 1]
WRITE = "--write" in sys.argv        # also write the refitted cards (SourceArt/Character/MH/Hair/*_mh.fbx)
SAVE = "--save" in sys.argv          # write the fits to Config/DefaultGame.ini
FITS = []
CAP_OVER = 0.08                      # cm the scalp cap stands over the MetaHuman skin (the old head: 0.05)
NEAR, FAR = 1.0, 6.0                 # cm from the cap: the full correction, fading to none (the lengths keep their fall)
STYLES = (("Long", "hair_cards.fbx"), ("Elf", "hair_elf.fbx"), ("Short", "hair_short.fbx"))


def load(fbx, keep=None, drop=None):
    """-> vertices in the head bone's space (cm), triangles, per-vertex material names."""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=fbx, colors_type="LINEAR")     # as blender_hair_cards.py writes them
    arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    head = arm.matrix_world @ arm.data.bones["head"].matrix_local
    unit = 100.0 if abs(head.translation.z) < 5 else 1.0      # the MetaHuman export is in metres
    inv = head.inverted()
    V, T, M, off = [], [], [], 0
    for o in bpy.data.objects:
        if o.type != "MESH":
            continue
        me = o.data
        me.calc_loop_triangles()
        names = [m.name if m else "" for m in me.materials] or [""]
        mw = inv @ o.matrix_world
        V.append(np.array([(mw @ v.co)[:] for v in me.vertices]))
        vm = [""] * len(me.vertices)
        for t in me.loop_triangles:
            n = names[t.material_index]
            for i in t.vertices:
                vm[i] = n
            if (keep and not any(k in n for k in keep)) or (drop and any(k in n for k in drop)):
                continue
            T.append([i + off for i in t.vertices])
        M += vm
        off += len(me.vertices)
    return np.concatenate(V) * head.to_scale()[0] * unit, T, np.array(M), head


def tree(V, T):
    return BVHTree.FromPolygons([Vector(v) for v in V], [tuple(t) for t in T])


def sdist(bvh, pts):
    """Signed distance to the surface, + outside."""
    out = np.zeros(len(pts))
    for i, p in enumerate(pts):
        loc, nrm, _, d = bvh.find_nearest(Vector(p))
        out[i] = d if (Vector(p) - loc).dot(nrm) >= 0 else -d
    return out


def fit(new, pts, d_old, iters=40):
    """Gauss-Newton on (s, tx, ty, tz): clearance(new, s p + t) = s * d_old."""
    x = np.array([0.94, 0.0, 0.0, 0.0])
    res = lambda x: sdist(new, pts * x[0] + x[1:]) - x[0] * d_old
    for _ in range(iters):
        r0 = res(x)
        J = np.zeros((len(pts), 4))
        for k, h in enumerate((0.01, 0.1, 0.1, 0.1)):
            dx = np.zeros(4)
            dx[k] = h
            J[:, k] = (res(x + dx) - r0) / h
        step = np.linalg.lstsq(J, -r0, rcond=None)[0]
        x = x + 0.7 * step
        if np.abs(step).max() < 1e-4:
            break
    x[3] = 0.0            # she is symmetric: no sideways offset
    return x, res(x)


oldV, oldT, _, _ = load(P + "/SourceArt/Character/android_skin.fbx", drop=["Hair"])
mhV, mhT, _, _ = load(P + "/SourceArt/Character/MH/mh_head.fbx", keep=["Skin_Baked_LOD0"])
old, new = tree(oldV, oldT), tree(mhV, mhT)
print("crown over the head joint: old %.1f cm (drawn at 0.935: %.1f), MetaHuman %.1f" % (oldV[:, 0].max(), oldV[:, 0].max() * 0.935, mhV[:, 0].max()))


def measure(name, V, T, bvh, k):
    """Head numbers in cm at the size she is drawn (k), from the head joint: x up, front, side."""
    V = V * k
    up, front, side = V[:, 0], -V[:, 1], V[:, 2]
    crown = up.max()
    # the chin's underside on the middle line: down from the chin's most forward point until the
    # profile has gone 2 cm back under the jaw
    prof = {}
    for i in np.nonzero((np.abs(side) < 0.6) & (up > -12.0) & (up < -2.5) & (front > 0))[0]:
        b_ = int(np.floor(up[i] / 0.25))
        prof[b_] = max(prof.get(b_, -99.0), front[i])
    tipb = max((b_ for b_ in prof if b_ * 0.25 > -9.0), key=lambda b_: prof[b_])
    low = tipb
    while low - 1 in prof and prof[low - 1] >= prof[tipb] - 2.0:
        low -= 1
    chin = (low * 0.25, -prof[tipb])
    dome = up > crown - 6.0
    band = (np.abs(side) < 3.0) & (up > crown - 9.5) & (up < crown - 3.0)
    # the ears: thin skin out at the sides (a ray inward leaves the mesh again within 1.2 cm)
    nrm = {}
    for t in T:
        a, b, c = (Vector(V[i] / k) for i in t)
        n = (b - a).cross(c - a)
        for i in t:
            nrm[i] = nrm.get(i, Vector((0, 0, 0))) + n
    ear = []
    for i, n in nrm.items():
        if up[i] < -4 * k or up[i] > crown or not 5.5 * k < abs(side[i]) < 14 * k or n.length == 0:
            continue
        n = n.normalized()
        hit = bvh.ray_cast(Vector(V[i] / k) - n * 0.02, -n, 1.2 / k)
        if hit[0] is not None:
            ear.append(i)
    E = V[ear]
    dome[ear] = False      # pointed ears reach into the dome's height
    print("HEAD %-9s crown %.1f  chin %.1f (front %.1f)  height %.1f  skull width (top 6 cm, no ears) %.1f  depth %.1f (brow %.1f, back %.1f)" % (
        name, crown, chin[0], -chin[1], crown - chin[0], 2 * np.abs(side[dome]).max(), front[band].max() - front[band].min(), front[band].max(), front[band].min()))
    if len(E):
        tip = E[np.argmax(E[:, 0])]
        print("HEAD %-9s ears (%d thin vertices): up %.1f .. %.1f (length %.1f), front %.1f .. %.1f, out to %.1f from the middle; tip at up %.1f front %.1f side %.1f" % (
            name, len(E), E[:, 0].min(), E[:, 0].max(), E[:, 0].max() - E[:, 0].min(), (-E[:, 1]).min(), (-E[:, 1]).max(), np.abs(E[:, 2]).max(), tip[0], -tip[1], abs(tip[2])))


measure("current", oldV, oldT, old, 0.935)
measure("MetaHuman", mhV, mhT, new, 1.0)
rng = np.random.default_rng(1)
for style, fbx in STYLES:
    V, _, M, head = load(P + "/SourceArt/Character/Hair/" + fbx)
    cap = V[M == "HairCap"]
    cards = V[(M != "HairCap") & (V[:, 0] > 2.0)]
    cards = cards[rng.choice(len(cards), min(len(cards), 12000), replace=False)]
    dc = sdist(old, cards)
    cards, dc = cards[np.abs(dc) < 1.5][:2500], dc[np.abs(dc) < 1.5][:2500]
    capS = cap[rng.choice(len(cap), min(len(cap), 2500), replace=False)]
    pts, d_old = np.concatenate([capS, cards]), np.concatenate([sdist(old, capS), dc])
    x, r = fit(new, pts, d_old)
    after = sdist(new, cap * x[0] + x[1:])
    before = sdist(old, cap)
    worst = cap[np.argmin(after)]
    print("%s: fit %.3f,%.2f,%.2f,0   (scale, up, front, side)" % (style, x[0], x[1], -x[2]))
    print("   residual rms %.2f cm, 95%% under %.2f, worst %.2f over %d points" % (np.sqrt((r ** 2).mean()), np.percentile(np.abs(r), 95), np.abs(r).max(), len(r)))
    print("   cap (%d vertices) over the scalp: old head median %.2f min %.2f; MetaHuman median %.2f min %.2f, %d under the skin, lowest at up %.1f front %.1f side %.1f"
          % (len(cap), np.median(before), before.min(), np.median(after), after.min(), int((after < 0).sum()), worst[0], -worst[1], worst[2]))
    FITS.append("%s=%.3f,%.2f,%.2f,0" % (style, x[0], x[1], -x[2]))
    if not WRITE:
        continue
    # ---- the refit: one scale cannot seat a cap that rides 0.05 cm over the scalp (half of it ends
    # up under the new skin). Every cap vertex is put CAP_OVER over the MetaHuman skin, and the cards
    # take the correction of the cap under them, fading out along their length. Written in the
    # asset's own space, so the runtime fit above still applies: fitted = s * (p + c / s) + t.
    s, t = x[0], x[1:]
    capF = cap * s + t
    corr = np.zeros_like(capF)
    for i, p in enumerate(capF):
        loc, nrm, _, _ = new.find_nearest(Vector(p))
        corr[i] = np.array(loc + nrm * CAP_OVER) - p
    from mathutils.kdtree import KDTree
    kd = KDTree(len(capF))
    for i, p in enumerate(capF):
        kd.insert(Vector(p), i)
    kd.balance()
    F = V * s + t
    is_cap = M == "HairCap"
    cap_i = np.cumsum(is_cap) - 1
    move = np.zeros_like(F)
    for i, p in enumerate(F):
        if is_cap[i]:
            move[i] = corr[cap_i[i]]
            continue
        near = kd.find_n(Vector(p), 8)
        d0 = near[0][2]
        if d0 >= FAR:
            continue
        w = np.array([1.0 / (d * d + 0.01) for _, _, d in near])
        c = (corr[[k for _, k, _ in near]] * w[:, None]).sum(0) / w.sum()
        u = min(1.0, max(0.0, (FAR - d0) / (FAR - NEAR)))
        move[i] = c * (u * u * (3.0 - 2.0 * u))
    F2 = F + move
    roots = (~is_cap) & (np.abs(sdist(new, F)) < 1.5)
    print("   refit: cap moved %.2f cm median, %.2f at most; cap under the skin %d -> %d; near-scalp card vertices under the skin %d -> %d of %d"
          % (np.median(np.linalg.norm(corr, axis=1)), np.linalg.norm(corr, axis=1).max(), int((after < 0).sum()),
             int((sdist(new, F2[is_cap]) < 0).sum()), int((sdist(new, F[roots]) < 0).sum()), int((sdist(new, F2[roots]) < 0).sum()), int(roots.sum())))
    # the scene still holds this style's import: move its vertices and write it out as it came in
    A = (F2 - t) / s
    assert abs(head.to_scale()[0] - 1.0) < 1e-6 and abs(head.translation.z) > 5, "the hair FBX is expected in cm at scale 1"
    off = 0
    arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    bpy.ops.object.select_all(action="DESELECT")
    for o in bpy.data.objects:
        if o.type != "MESH":
            continue
        back = o.matrix_world.inverted() @ head
        for v in o.data.vertices:
            v.co = back @ Vector(A[off + v.index])
        off += len(o.data.vertices)
        o.data.update()
        o.select_set(True)
    arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    out = os.path.join(P, "SourceArt", "Character", "MH", "Hair")
    os.makedirs(out, exist_ok=True)
    out = os.path.join(out, fbx.replace(".fbx", "_mh.fbx"))
    bpy.ops.export_scene.fbx(filepath=out, use_selection=True, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False,
                             use_armature_deform_only=False, mesh_smooth_type="FACE", bake_anim=False,
                             colors_type="LINEAR", axis_forward="-Z", axis_up="Y")      # as blender_hair_cards.py
    print("   wrote", out, "%.1f MB" % (os.path.getsize(out) / 1e6))

if SAVE:
    ini = os.path.join(P, "Config", "DefaultGame.ini")
    lines = open(ini).read().split("\n")
    if "[VisexMHHairFit]" in lines:          # replace the section
        a = lines.index("[VisexMHHairFit]")
        b = next((i for i in range(a + 1, len(lines)) if lines[i].startswith("[")), len(lines))
        while b > a + 1 and not lines[b - 1].strip():
            b -= 1
        lines[a:b] = []
    while lines and not lines[-1].strip():
        lines.pop()
    lines += ["", "[VisexMHHairFit]",
              "; her card hair on the MetaHuman head: scale,up,front,side per style (Tools/mh_hair_fit.py --save)"] + FITS + [""]
    open(ini, "w").write("\n".join(lines))
    print("saved", FITS, "to", ini)
