# The MetaHuman body as a skin source for Tools/blender_outfit.py (--body=mh). exec'd by it right
# after the FBX import, so `arm`, `skin`, `np`, `bpy`, `bmesh` and later globals are in scope.
#
# The export from UE (Tools/mh_skin_export.py) differs from android_skin.fbx in four ways:
#   - metres under a 0.01 empty: flattened here, so world units are cm like the old skin
#   - split along UV seams (32,334 vertices for 30,455): welded, or every seam would be cut as an edge
#   - ~0.7 cm between vertices (the old skin: 0.27): each triangle is split in four, so the script's
#     smoothing passes and cut edges work at the scale they were tuned for. The new vertices lie on
#     the skin's own triangles and take their corners' weights
#   - no shape keys: the bust is found from the surface (mh_bust), there is no design morph set
#
# Kit outfits (openings drawn in Figma over the old body's views) are transferred by landmark:
# mh_kit_tpose() puts this body in the views' T-pose and warps it onto the old body's heights,
# widths and depths (OutfitKit/views/ref_profile.json, written by `--kit-ref` on the old skin), so a
# shape drawn at the old waist lands on this waist.

_empty = arm.parent
if _empty is not None:
    arm.parent = None
    arm.matrix_world = Matrix.Identity(4)
    bpy.data.objects.remove(_empty, do_unlink=True)
arm.scale = (1.0, 1.0, 1.0)
bpy.context.view_layer.update()

_bm = bmesh.new()
_bm.from_mesh(skin.data)
_n0 = len(_bm.verts)
bmesh.ops.remove_doubles(_bm, verts=_bm.verts, dist=1e-4)
_n1 = len(_bm.verts)
bmesh.ops.triangulate(_bm, faces=_bm.faces)
bmesh.ops.subdivide_edges(_bm, edges=_bm.edges, cuts=1, use_grid_fill=True)
bmesh.ops.triangulate(_bm, faces=_bm.faces)
# the two body vertices each new vertex was put between (itself twice for the body's own)
_bm.verts.index_update()
MH_PARENTS = np.zeros((len(_bm.verts), 2), int)
for _v in _bm.verts:
    _base = [e.other_vert(_v) for e in _v.link_edges if e.other_vert(_v).index < _n1]
    if _v.index < _n1 or len(_base) < 2:
        MH_PARENTS[_v.index] = _v.index
        continue
    _pair = min(((a, b) for i, a in enumerate(_base) for b in _base[i + 1:]), key=lambda ab: ((ab[0].co + ab[1].co) * 0.5 - _v.co).length)
    MH_PARENTS[_v.index] = (_pair[0].index, _pair[1].index)
_bm.to_mesh(skin.data)
_bm.free()
skin.data.update()
print("mh skin: %d vertices welded to %d, split to %d" % (_n0, _n1, len(skin.data.vertices)))
MH_BASE_VERTS = _n1      # bmesh keeps the welded vertices first: 0.._n1-1 are the body's own


def mh_bust():
    """Stands in for the Bust morph's reach: 1 at the apex (the most forward chest skin on each
    side), falling to 0 at 9 cm from it."""
    out = np.zeros(nv)
    z0, z1 = Bn["spine_03"][2], Bn["clavicle_l"][2]
    for s in (1, -1):
        i = np.nonzero((LAT * s > 3.0) & (Z > z0) & (Z < z1) & (UPPERARM < 0.3))[0]
        apex = X[i[np.argmax(FW[i])]]
        d = np.linalg.norm(X - apex, axis=1)
        out = np.maximum(out, np.where(LAT * s > 0, np.clip(1.0 - d / 9.0, 0.0, 1.0), 0.0))
    return out


def mh_tpose_points():
    """This body's vertices with both upper arms raised to horizontal (the kit views' pose)."""
    mod = next(m for m in skin.modifiers if m.type == "ARMATURE")
    mod.show_viewport = True
    Mi = arm.matrix_world.inverted().to_3x3()
    for s, sgn in (("l", 1.0), ("r", -1.0)):
        pb, lo = arm.pose.bones["upperarm_" + s], arm.pose.bones["lowerarm_" + s]
        cur = (lo.head - pb.head).normalized()
        tgt = (Mi @ Vector((sgn, 0.0, 0.0))).normalized()
        R = Matrix.Translation(pb.head) @ cur.rotation_difference(tgt).to_matrix().to_4x4() @ Matrix.Translation(-pb.head)
        pb.matrix = R @ pb.matrix
        bpy.context.view_layer.update()
    ev = skin.evaluated_get(bpy.context.evaluated_depsgraph_get())
    m = ev.to_mesh()
    P = np.array([skin.matrix_world @ v.co for v in m.vertices])
    ev.to_mesh_clear()
    for pb in arm.pose.bones:
        pb.matrix_basis = Matrix.Identity(4)
    mod.show_viewport = False
    bpy.context.view_layer.update()
    assert len(P) == nv
    return P


def _mh_interp(z, kz, kv):
    """np.interp that carries the end slopes on past the first and last knot."""
    out = np.interp(z, kz, kv)
    lo, hi = z < kz[0], z > kz[-1]
    out = np.where(lo, kv[0] + (z - kz[0]) * (kv[1] - kv[0]) / (kz[1] - kz[0]), out)
    return np.where(hi, kv[-1] + (z - kz[-1]) * (kv[-1] - kv[-2]) / (kz[-1] - kz[-2]), out)


def mh_kit_tpose(kit_dir):
    """T-pose vertices in the old body's frame: each height band (floor, knee, crotch, waist, bust,
    shoulder, neck, head) is mapped onto the old body's, widths by the silhouette's half-width
    there and depths by its front and back. Returns (points, heights map as (new z, old z))."""
    ref = _json.load(open(os.path.join(kit_dir, "views", "ref_profile.json")))
    tp = mh_tpose_points()
    mine = kit_profile(tp)
    kz, oz = np.array(mine["z"]), np.array(ref["z"])
    z = tp[:, 2]
    lat, fw = (tp - PEL) @ LEFT, (tp - PEL) @ FWD
    sx = np.interp(z, kz, np.array(ref["hw"]) / np.array(mine["hw"]))
    ff_n, fb_n = np.interp(z, kz, mine["front"]), np.interp(z, kz, mine["back"])
    ff_o, fb_o = np.interp(z, kz, ref["front"]), np.interp(z, kz, ref["back"])
    fw2 = fb_o + (fw - fb_n) * (ff_o - fb_o) / (ff_n - fb_n)
    out = np.array(ref["pel"])[None, :] + (lat * sx)[:, None] * np.array(ref["left"]) + fw2[:, None] * np.array(ref["fwd"])
    out[:, 2] = _mh_interp(z, kz, oz)
    print("kit transfer (cm, this body -> old):", {n: "%.1f->%.1f" % (a, b) for n, a, b in zip(mine["names"], kz, oz)},
          "width x", (np.array(ref["hw"]) / np.array(mine["hw"])).round(2).tolist())
    return out, (kz, oz)


COVER_MARGIN = 0.5   # cm of skin kept under every fabric edge, so an edge never shows a hole (at 1.0 the
                     # skin left under a hem could still come through it in a deep hip or wrist bend)
COVER_PX = 2048


def mh_cover_mask(name, covers):
    """cover_<name>.png beside the FBX: the body's UV0, white where opaque fabric covers the skin
    by more than COVER_MARGIN. The body's material clips the skin there (Tools/mh_outfit.py), so
    nothing can poke through the fabric however the correctives move it."""
    c = np.full(nv, NEG)
    for cover, slot in covers:
        if slot in OPAQUE:
            c = np.maximum(c, np.asarray(cover, float))
    val = np.clip((c - COVER_MARGIN) / 0.3 + 0.5, 0.0, 1.0)
    N = COVER_PX
    uv = np.empty(len(me.loops) * 2)
    me.uv_layers[0].data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    uv = (uv - np.floor(uv.min(0))) * N - 0.5              # the body's UVs sit in tile 1 (u 1..2); texel centres
    lv = np.empty(len(me.loops), int)
    me.loops.foreach_get("vertex_index", lv)
    tri_uv, tri_v = uv.reshape(-1, 3, 2), val[lv].reshape(-1, 3)
    img, filled = np.zeros((N, N)), np.zeros((N, N), bool)
    keep = np.nonzero(tri_v.max(1) > 0.0)[0]               # only triangles that hide something
    for t in keep.tolist():
        (ax, ay), (bx, by), (cx, cy) = tri_uv[t].tolist()
        x0, x1 = max(int(min(ax, bx, cx)) - 1, 0), min(int(max(ax, bx, cx)) + 2, N - 1)
        y0, y1 = max(int(min(ay, by, cy)) - 1, 0), min(int(max(ay, by, cy)) + 2, N - 1)
        den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
        if abs(den) < 1e-9 or x1 < x0 or y1 < y0:
            continue
        gx, gy = np.meshgrid(np.arange(x0, x1 + 1), np.arange(y0, y1 + 1))
        w0 = ((by - cy) * (gx - cx) + (cx - bx) * (gy - cy)) / den
        w1 = ((cy - ay) * (gx - cx) + (ax - cx) * (gy - cy)) / den
        w2 = 1.0 - w0 - w1
        inside = (w0 >= -0.35) & (w1 >= -0.35) & (w2 >= -0.35)      # a texel past the edge, for filtering
        v = np.clip(w0, 0, 1) * tri_v[t, 0] + np.clip(w1, 0, 1) * tri_v[t, 1] + np.clip(w2, 0, 1) * tri_v[t, 2]
        sub, fs = img[y0:y1 + 1, x0:x1 + 1], filled[y0:y1 + 1, x0:x1 + 1]
        sub[inside] = np.where(fs[inside], np.maximum(sub[inside], v[inside]), v[inside])
        fs[inside] = True
    out = bpy.data.images.new("cover_" + name, N, N, alpha=False, is_data=True)
    out.pixels.foreach_set(np.repeat(np.clip(img, 0, 1)[:, :, None], 4, 2).astype(np.float32).ravel())
    out.filepath_raw = os.path.join(OUT_DIR, "cover_%s.png" % name.lower())
    out.file_format = "PNG"
    out.save()
    print("cover mask", out.filepath_raw, "hidden skin %.0f%%" % (100.0 * (val > 0.5).mean()))


def _mh_test_poses():
    """(label, bone, joint, 3x3 rotation): one joint bent hard, everything below it rigid with it."""
    out = []
    pos = lambda n: np.array(arm.matrix_world @ arm.data.bones[n].head_local)
    def rot(axis, deg):
        a = np.asarray(axis, float) / np.linalg.norm(axis)
        K = np.array([[0, -a[2], a[1]], [a[2], 0, -a[0]], [-a[1], a[0], 0]])
        r = math.radians(deg)
        return np.eye(3) + math.sin(r) * K + (1 - math.cos(r)) * K @ K
    for s in ("l", "r"):
        d = pos("hand_" + s) - pos("lowerarm_" + s)
        u = np.cross(d, (0.0, 0.0, 1.0))
        v = np.cross(d, u)
        u, v = u / np.linalg.norm(u), v / np.linalg.norm(v)
        for k in range(8):               # the hand bent 100 degrees every 45 degrees round the forearm
            ax = math.cos(k * math.pi / 4) * u + math.sin(k * math.pi / 4) * v
            out.append(("wrist_%s %3d" % (s, k * 45), "hand_" + s, pos("hand_" + s), rot(ax, 100)))
        for deg in (100, -100, 125, -125, 140, -140):
            for out_ in (0, 25, -25, 50, -50):     # a sit, the knee out, the knee across
                out.append(("hip_%s flex %+d out %+d" % (s, deg, out_), "thigh_" + s, pos("thigh_" + s), rot(LEFT, deg) @ rot(FWD, out_)))
    # only the two joints whose hems the tour fails: a hem sat down where the game never folds it
    # (waist, shoulder, ankle) would lose its standing edge for nothing. Add a joint here when the
    # tour's poke line names a hem on it.
    return out


def mh_poke_sim(label, oc, src, top, tw, edist):
    """The game's poke check without the game: each test pose, every fabric vertex cut from the skin
    against the plane of the body triangle it lies over (the body is drawn with its own vertices; a
    fabric vertex between two of them carries their blended weights and leaves that plane when the
    two move differently). Linear skinning only, no RigLogic correctives. Prints, per pose, the
    fabric within 0.8 cm of a cut edge that ends up under the skin, as the tour's poke line does."""
    fab = np.nonzero((src[:, 0] >= 0) & (edist < NO_SEAM))[0]
    a, b, t = src[fab, 0].astype(int), src[fab, 1].astype(int), src[fab, 2][:, None]
    tris_of = {}
    for k, f in enumerate(T.tolist()):
        for i in range(3):
            tris_of.setdefault((min(f[i], f[i - 1]), max(f[i], f[i - 1])), []).append(k)
            tris_of.setdefault((f[i], f[i]), []).append(k)
    near = [tris_of.get((min(i, j), max(i, j)), []) for i, j in zip(a.tolist(), b.tolist())]
    pa, pb = MH_PARENTS[:, 0], MH_PARENTS[:, 1]

    def heights(flat, xd):
        """Lowest height of each fabric vertex over the planes of the body triangles under it."""
        fn = np.cross(flat[T[:, 1]] - flat[T[:, 0]], flat[T[:, 2]] - flat[T[:, 0]])
        fn /= np.maximum(np.linalg.norm(fn, axis=1, keepdims=True), 1e-9)
        on = flat[a] * (1 - t) + flat[b] * t
        h = np.full(len(fab), 9.0)
        for i, ks in enumerate(near):
            if ks:
                h[i] = ((xd[i] - on[i]) * fn[ks]).sum(1).min()
        return h

    rest = heights(X, oc[fab])
    sign = 1.0 if np.median(rest[rest < 9]) > 0 else -1.0
    panel = (rest * sign > 0.05) & (rest < 9)          # not a rim's foot, which sits on the skin
    hem = panel & (edist[fab] < 0.8)
    gi = {n: i for i, n in enumerate(gnames)}
    worst = 0.0
    for name, bone, J, R in _mh_test_poses():
        sub = np.zeros(len(gnames), bool)
        for bn in [bone] + [c.name for c in arm.data.bones[bone].children_recursive]:
            if bn in gi:
                sub[gi[bn]] = True
        move = lambda P_, s_: P_ + s_[:, None] * ((P_ - J) @ R.T + J - P_)
        Xd = move(X, WM[:, sub].sum(1).astype(float))
        flat = (Xd[pa] + Xd[pb]) * 0.5                 # where the drawn skin is: on the body's own triangles
        depth = -sign * heights(flat, move(oc[fab], (tw[fab] * sub[top[fab]]).sum(1)))
        d_hem = np.where(hem, depth, 0.0)
        worst = max(worst, d_hem.max())
        if os.environ.get("MH_DEBUG") and d_hem.max() > 0.1:
            i = int(np.argmax(d_hem))
            sk = WM[:, sub].sum(1)
            fs = float((tw[fab[i]] * sub[top[fab[i]]]).sum())
            c = sorted(set(MH_PARENTS[a[i]].tolist() + MH_PARENTS[b[i]].tolist()))
            print("   worst: src %d-%d t %.2f rest height %.2f edge dist %.2f; fabric weight in the bent part %.3f (untruncated %.3f); body corners %s weights %s, apart %s cm" % (
                a[i], b[i], float(t[i, 0]), rest[i] * sign, edist[fab[i]], fs, sk[a[i]] * (1 - t[i, 0]) + sk[b[i]] * t[i, 0], c,
                np.round(sk[c], 3).tolist(), np.round([np.linalg.norm(X[c[j]] - X[c[j - 1]]) for j in range(len(c))], 2).tolist()))
        print("pokesim %s %-22s at a hem: %4d under (%3d past 0.1), deepest %.2f cm; elsewhere deepest %.2f" % (
            label, name, int((d_hem > 0.02).sum()), int((d_hem > 0.1).sum()), d_hem.max(), np.where(panel & ~hem, depth, 0.0).max()))
    print("pokesim %s worst at a hem %.2f cm (%d fabric vertices, %d at a hem)" % (label, worst, int(panel.sum()), int(hem.sum())))
    return worst


def mh_arm_along(s):
    """Per skin vertex, cm along the forearm from the wrist joint of side s ("l"/"r"), + toward the
    fingers; and the furthest up the forearm that the hand's bones still pull on the body's own
    vertices (a triangle that spans it bends with the wrist)."""
    pos = lambda n: np.array(arm.matrix_world @ arm.data.bones[n].head_local)
    d = pos("hand_" + s) - pos("lowerarm_" + s)
    d /= np.linalg.norm(d)
    along = (X - pos("hand_" + s)) @ d
    side = G("lowerarm_" + s, "hand_" + s, "wrist_inner_" + s, "wrist_outer_" + s, "lowerarm_twist_01_" + s, "lowerarm_twist_02_" + s) > 0.5
    base = np.arange(len(X)) < MH_BASE_VERTS
    reach = along[side & base & (HANDS > 0.02)].min()
    if os.environ.get("MH_DEBUG"):
        for th in (0.02, 0.1, 0.25, 0.62, 0.9):
            m = side & base & (HANDS > th)
            print("mh wrist_%s: hand weight > %.2f reaches %.2f cm up the forearm (%d body vertices)" % (s, th, -along[m].min(), int(m.sum())))
        ring = side & base & (np.abs(along + 3.0) < 3.0)
        e = EDGES[ring[EDGES[:, 0]] & ring[EDGES[:, 1]] & (EDGES[:, 0] < MH_BASE_VERTS) & (EDGES[:, 1] < MH_BASE_VERTS)]
        print("mh wrist_%s: %d split edges within 6 cm above the joint, median %.2f cm" % (s, len(e), np.median(np.linalg.norm(X[e[:, 0]] - X[e[:, 1]], axis=1)) if len(e) else 0))
    return np.where(side, along, 99.0), reach


MH_HEM_TOL = 0.02      # cm a hem may end up under a folded neighbour (the tour fails past 0.1)
MH_HEM_MIN = 0.04      # cm: a hem is never put closer to the skin than this
_MH_SAFE = []
MH_DROPPED = []       # per panel cut: (slot, offset, hem vertices sat down, most)


def mh_safe_height():
    """Per skin vertex, the height (cm) up to which fabric over it stays out of the skin in every
    test pose. Fabric rides the skin vertex under it, so its offset turns with that vertex's blend
    of bones, while the body triangle on the far side of a crease turns with its own: in a hard
    fold (a bent wrist, the hip crease in a sit) that triangle comes over the fabric, the further
    under the higher the fabric stands (stage 3's two failing hems: the deepest vertices are over
    the body's own vertices, not the split ones). So a hem is safe below a height, not above one.
    Includes what a split vertex adds by leaving its triangle's plane."""
    if _MH_SAFE:
        return _MH_SAFE[0]
    pa, pb = MH_PARENTS[:, 0], MH_PARENTS[:, 1]
    gi = {n: i for i, n in enumerate(gnames)}
    fn0 = np.cross(X[T[:, 1]] - X[T[:, 0]], X[T[:, 2]] - X[T[:, 0]])
    sign = 1.0 if np.median((fn0 * NRM[T[:, 0]]).sum(1)) > 0 else -1.0
    H = np.full(nv, 9.0)
    for name, bone, J, R in _mh_test_poses():
        sub = np.zeros(len(gnames), bool)
        for bn in [bone] + [c.name for c in arm.data.bones[bone].children_recursive]:
            if bn in gi:
                sub[gi[bn]] = True
        s = WM[:, sub].sum(1).astype(float)[:, None]
        Xd = X + s * ((X - J) @ R.T + J - X)
        flat = (Xd[pa] + Xd[pb]) * 0.5
        fn = np.cross(flat[T[:, 1]] - flat[T[:, 0]], flat[T[:, 2]] - flat[T[:, 0]]) * sign
        fn /= np.maximum(np.linalg.norm(fn, axis=1, keepdims=True), 1e-9)
        Bn = (1 - s) * NRM + s * (NRM @ R.T)
        for k in range(3):
            v = T[:, k]
            c = ((Xd[v] - flat[v]) * fn).sum(1)
            g = (Bn[v] * fn).sum(1)
            ok = np.where(g < -1e-3, np.maximum(MH_HEM_TOL + c, 0.0) / np.maximum(-g, 1e-3), np.where(c < -MH_HEM_TOL, 0.0, 9.0))
            np.minimum.at(H, v, ok)
    # a hem's height should not jump from vertex to vertex: the lowest of each neighbourhood, eased
    for _ in range(2):
        low = H.copy()
        np.minimum.at(low, EDGES[:, 0], H[EDGES[:, 1]])
        np.minimum.at(low, EDGES[:, 1], H[EDGES[:, 0]])
        H = low
    H = np.minimum(H, diffuse(np.minimum(H, 1.0), 2))
    _MH_SAFE.append(H)
    print("mh safe hem height: %d of %d skin vertices under 0.25 cm, %d under 0.10" % (int((H < 0.25).sum()), nv, int((H < 0.10).sum())))
    return H
