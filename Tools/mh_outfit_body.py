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


COVER_MARGIN = 1.0   # cm of skin kept under every fabric edge, so an edge never shows a hole
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
