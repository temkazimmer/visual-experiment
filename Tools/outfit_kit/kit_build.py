# Outfit Kit builder. Runs inside Tools/blender_outfit.py (exec'd after OUTFITS is defined, so
# region, tube, X, NV, nearest, UPPERARM and the rest are in scope). Every folder
# SourceArt/Character/OutfitKit/<Name>/ with a figma.json becomes outfit "Kit<Name>".
#
# Panels (filled closed shapes) are not traced as 3D outlines. Each skin vertex is looked at from
# the view that sees it most head-on (front, back, her left side, or the side view mirrored for
# her right); if it falls inside a drawn shape there, it is covered. The coverage field is the
# signed distance to the drawn edge in that view (cm), so region() cuts exactly along what was
# drawn. Shapes of one fabric (slot + bind) share one field; region() splits it into pieces.
# Open strokes (straps) come from sketch.json (figma_import.py): 3D lines on her skin, laid as
# flat bound tapes.
import json as _json

_KIT = os.path.join(os.path.dirname(os.path.abspath(SRC)), "OutfitKit")     # next to android_skin.fbx
_OFFSET = {"Tartan": 0.30, "Satin": 0.45, "Lace": 0.30, "Sheer": 0.24, "Latex": 0.30, "Nylon": 0.22,
           "Net": 0.22, "Jersey": 0.30}
_FACE = 0.15         # a vertex must face a view at least this much to be judged from it


def _views():
    vj = _json.load(open(os.path.join(_KIT, "views", "views.json")))
    tp = np.load(os.path.join(_KIT, "views", "tpose.npy")).astype(float)
    assert len(tp) == nv, "tpose.npy has %d vertices, the skin %d: re-run Tools/outfit_kit/views.py" % (len(tp), nv)
    # T-pose vertex normals
    fn = np.cross(tp[T[:, 1]] - tp[T[:, 0]], tp[T[:, 2]] - tp[T[:, 0]])
    vn = np.zeros_like(tp)
    for k in range(3):
        np.add.at(vn, T[:, k], fn)
    vn /= np.maximum(np.linalg.norm(vn, axis=1, keepdims=True), 1e-9)
    arms = UPPERARM >= 0.5
    keep = T[~(arms[T[:, 0]] & arms[T[:, 1]] & arms[T[:, 2]])]
    bvh = BVHTree.FromPolygons([Vector(q) for q in tp], keep.tolist())
    return vj, tp, vn, bvh


def _uv(vj, name, P):
    """T-pose points -> pixel (u, v) in a view."""
    vw = vj["views"][name]
    r = np.array(vw["image_right"])
    return np.stack([(P @ r - vw["h_range_cm"][0]) * vj["px_per_cm"], (vj["z_top"] - P[:, 2]) * vj["px_per_cm"]], 1)


def _sd_polys(Q, polys):
    """Signed distance (px, + inside) from points Q to the union of polygons."""
    best = np.full(len(Q), -1e9)
    for poly in polys:
        A, B = poly, np.roll(poly, -1, 0)
        inside = np.zeros(len(Q), bool)
        d = np.full(len(Q), 1e18)
        for a, b in zip(A, B):
            cross = ((a[1] > Q[:, 1]) != (b[1] > Q[:, 1])) & \
                    (Q[:, 0] < (b[0] - a[0]) * (Q[:, 1] - a[1]) / ((b[1] - a[1]) or 1e-9) + a[0])
            inside ^= cross
            ab = b - a
            t = np.clip(((Q - a) @ ab) / max(ab @ ab, 1e-9), 0, 1)
            d = np.minimum(d, np.linalg.norm(Q - (a + t[:, None] * ab), axis=1))
        best = np.maximum(best, np.where(inside, d, -d))
    return best


def _visible(bvh, P, toward, ids):
    """Of the vertices ids (T-pose points P), which ones the view along -toward actually sees."""
    t = Vector(toward)
    ok = np.zeros(len(ids), bool)
    for k, i in enumerate(ids):
        p = Vector(P[i])
        hit = bvh.ray_cast(p + t * 300, -t, 400)
        ok[k] = hit[0] is not None and (hit[0] - p).length < 0.6
    return ok


def kit_outfit(name):
    d = os.path.join(_KIT, name)
    rows = _json.load(open(os.path.join(d, "figma.json")))
    sk = _json.load(open(os.path.join(d, "sketch.json")))
    vj, tp, vn, bvh = _views()
    # the four ways a vertex can be seen: (view image, direction toward the camera, mirrored?)
    seen = [("front", np.array((0., -1., 0.)), False), ("back", np.array((0., 1., 0.)), False),
            ("side", np.array((1., 0., 0.)), False), ("side", np.array((-1., 0., 0.)), True)]
    groups = {}
    for nm, view, closed, weight, fill, flat in rows:
        if not (closed and fill):
            continue
        r, g, b, a = fill
        base = nm.split(" · ")[0].lower()
        slot = next((s for s in SLOTS if s.lower() in base), None) or \
            ("Tartan" if r > 0.4 and g < 0.3 else "Satin" if max(r, g, b) < 0.2 else ("Lace" if a > 0.8 else "Sheer"))
        bind = "bind" in base or ("panel" not in base and slot in ("Tartan", "Satin"))
        key = (slot, bind)
        poly = np.array(flat, float).reshape(-1, 2)
        if view == "s":
            # a shape the side view sees edge-on (the front panel's or thong's silhouette) is a thin
            # sliver: it belongs to the front or back view, and filled it would put fabric on her hip
            e = np.roll(poly, -1, 0) - poly
            area = 0.5 * abs(np.sum(poly[:, 0] * np.roll(poly[:, 1], -1) - np.roll(poly[:, 0], -1) * poly[:, 1]))
            if 2 * area / max(np.linalg.norm(e, axis=1).sum(), 1e-6) < 12.0:      # mean thickness < 1.2 cm
                print("KIT %s: side-view sliver '%s' left to front/back" % (name, nm))
                continue
        groups.setdefault(key, {}).setdefault({"f": "front", "s": "side", "b": "back"}[view], []).append(poly)
    for (slot, bind), by_view in groups.items():
        field = np.full(nv, NEG)
        best = np.full(nv, -1.0)
        for vname, toward, mirror in seen:
            if vname not in by_view:
                continue
            P = tp * np.array((-1.0, 1.0, 1.0)) if mirror else tp
            N = vn * np.array((-1.0, 1.0, 1.0)) if mirror else vn
            f = N @ (toward * np.array((-1.0, 1.0, 1.0)) if mirror else toward)
            cand = f > _FACE
            if vname == "side":
                cand &= f >= np.abs(N[:, 1])
            Q = _uv(vj, vname, P)
            polys = by_view[vname]
            lo, hi = np.min([p.min(0) for p in polys], 0) - 60, np.max([p.max(0) for p in polys], 0) + 60
            cand &= (Q[:, 0] > lo[0]) & (Q[:, 0] < hi[0]) & (Q[:, 1] > lo[1]) & (Q[:, 1] < hi[1])
            ids = np.nonzero(cand & (f > best))[0]
            if not len(ids):
                continue
            # occlusion is checked on the real (unmirrored) body from the real camera side
            vis = _visible(bvh, tp, toward, ids)          # toward is the real camera side, mirrored or not
            ids = ids[vis]
            sd = _sd_polys(Q[ids], polys) / vj["px_per_cm"]
            field[ids] = sd
            best[ids] = f[ids]
        # skin no view sees head-on (the top of the foot, under the bust) takes its neighbours' value
        known = best >= 0
        near = (field > -6.0) | ~known
        for _ in range(80):
            todo = ~known & near
            if not todo.any():
                break
            acc, cnt = np.zeros(nv), np.zeros(nv)
            for a_, b_ in ((EDGES[:, 0], EDGES[:, 1]), (EDGES[:, 1], EDGES[:, 0])):
                m_ = known[b_]
                np.add.at(acc, a_[m_], field[b_[m_]])
                np.add.at(cnt, a_[m_], 1.0)
            grow = todo & (cnt > 0)
            if not grow.any():
                break
            field[grow] = acc[grow] / cnt[grow]
            known |= grow
        print("KIT %s %s%s: %d vertices inside" % (name, slot, " bind" if bind else "", int((field > 0).sum())))
        region(field, _OFFSET.get(slot, 0.3), slot, bind=("Binding", 0.55) if bind else None, min_loop=4.0)
    # straps: 3D lines from the import; side-view copies of a line the front or back already has are dropped
    lines = [p for p in sk["pieces"] if p["kind"] in ("strap", "taut") and not p["closed"]]
    fb = [np.array(p["points"]) for p in lines if p["view"] != "side"]
    fbk = KDTree(sum(len(q) for q in fb) or 1)
    i = 0
    for q in fb:
        for pt in q:
            fbk.insert(Vector(pt), i)
            i += 1
    fbk.balance()
    for p in lines:
        P = np.array(p["points"])
        if p["view"] == "side" and i:
            P = P[[fbk.find(Vector(q))[2] > 1.5 for q in P]]
        runs, cur = [], [P[0]] if len(P) else []
        for a, b in zip(P[:-1], P[1:]):
            if np.linalg.norm(b - a) > 2.0:
                runs.append(cur)
                cur = []
            cur.append(b)
        if cur:
            runs.append(cur)
        for run in runs:
            if len(run) < 3:
                continue
            pts = [V3(q) + NV[nearest(V3(q))] * (0.2 + float(BRIDGE[nearest(V3(q))])) for q in run]
            tube(pts, 0.0, "Binding", flat=(p.get("width_cm", 1.0), 0.14), step=0.35)


for _d in sorted(os.listdir(_KIT)) if os.path.isdir(_KIT) else []:
    if os.path.exists(os.path.join(_KIT, _d, "figma.json")) and os.path.exists(os.path.join(_KIT, _d, "sketch.json")):
        OUTFITS["Kit" + _d] = (lambda n: (lambda: kit_outfit(n)))(_d)
