"""Outfit Kit: vectors drawn in Figma over the T-pose views -> pieces glued to her skin.

    blender -b --factory-startup --python Tools/outfit_kit/figma_import.py -- <Name>

Reads SourceArt/Character/OutfitKit/<Name>/figma.json: one row per drawn subpath,
[layer name, view f|s|b, closed 0|1, stroke weight px, fill [r,g,b,a] or 0, flat pixel coords],
pulled from the Figma frame 'outfit' (see Docs/OUTFIT_KIT.md). Every point is cast from its
view onto her T-posed default body, then carried by its skin vertices to her rest pose (the
numbering is the same), so the result is the same sketch.json/sketch.md/preview.png as the
Blender sketch export.

A layer name that doesn't parse ("Vector") is read from its style: a filled closed shape is a
panel (red -> Tartan, near-black -> Satin, white -> Lace when opaque, Sheer when see-through),
an open stroke is a strap as wide as its stroke (10 px = 1 cm).
"""
import json
import os
import sys

import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

sys.path.insert(0, os.path.dirname(__file__))
import sketch  # noqa: E402

ARGS = sys.argv[sys.argv.index("--") + 1:]
NAME = ARGS[0]
D = os.path.join(sketch.KIT, NAME)
VIEWS = json.load(open(os.path.join(sketch.KIT, "views", "views.json")))
PX, ZT = VIEWS["px_per_cm"], VIEWS["z_top"]
STEP_PX = 5.0          # resample every 0.5 cm before casting


def ray(view, u, v):
    """Pixel (u, v) of a view -> (origin, direction) of the ray into her body."""
    vw = VIEWS["views"][view]
    h = vw["h_range_cm"][0] + u / PX
    z = ZT - v / PX
    d, r = Vector(vw["toward_camera"]), Vector(vw["image_right"])
    return r * h + Vector((0, 0, z)) + d * 300, -d


def resample(P, closed):
    if len(P) < 2:
        return P
    Q = np.vstack([P, P[:1]]) if closed else P
    seg = np.linalg.norm(np.diff(Q, axis=0), axis=1)
    s = np.r_[0, np.cumsum(seg)]
    n = max(2, int(s[-1] / STEP_PX) + 1)
    t = np.linspace(0, s[-1], n, endpoint=not closed)
    return np.stack([np.interp(t, s, Q[:, k]) for k in range(2)], 1)


def classify(name, closed, weight, fill):
    base = name.split(" · ")[0].strip()
    meta = sketch.parse_layer(base)
    if meta["kind"] in ("panel", "strap", "taut", "hang", "seam", "point", "notes"):
        if meta["kind"] in ("strap", "taut") and not any(c.isdigit() for c in base):
            meta["width_cm"] = round(weight / PX, 2)
        return base, meta
    if closed and fill:
        r, g, b, a = fill
        slot = "Tartan" if r > 0.4 and g < 0.3 else "Satin" if max(r, g, b) < 0.2 else ("Lace" if a > 0.8 else "Sheer") \
            if min(r, g, b) > 0.8 else "Satin"
        return "panel %s bind" % slot, dict(kind="panel", slot=slot, bind=slot in ("Tartan", "Satin"))
    return "strap %.1f" % (weight / PX), dict(kind="strap", width_cm=round(weight / PX, 2), slot="Binding")


body, X, info, masks = sketch.build_body(pose="T")
me = body.data
TP = np.array([v.co[:] for v in me.vertices])                 # her T-posed skin
TRI = np.array([p.vertices[:] for p in me.polygons])
# arms and hands are left out: in the T-pose her arm points straight at the side camera and
# would catch every stroke drawn at shoulder height (no current outfit has sleeves drawn)
ARM = np.zeros(len(TP), bool)
ARM = masks.get("arms", ARM)
keep = [t for t in TRI.tolist() if not (ARM[t[0]] and ARM[t[1]] and ARM[t[2]])]
bvh = BVHTree.FromPolygons([Vector(q) for q in TP], keep)
TRI = np.array(keep)
GRAZE = 0.25          # a point whose skin faces away from the view more than this is left to another view
reg = np.zeros(len(TP), np.int32)
for i, k in enumerate(("leg_r", "leg_l", "torso")):
    reg[masks[k]] = 2 - i
rows = json.load(open(os.path.join(D, "figma.json")))
pieces, missed = [], 0
for name, view, closed, weight, fill, flat in rows:
    view = {"f": "front", "s": "side", "b": "back"}[view]
    P = resample(np.array(flat, float).reshape(-1, 2), bool(closed))
    layer, meta = classify(name, bool(closed), weight, fill)
    pts, nrm, anc, rg = [], [], [], []
    for u, v in P:
        o, d = ray(view, u, v)
        loc, n, fi, _ = bvh.ray_cast(o, d, 600)
        if loc is None or n.dot(-d) < GRAZE:
            missed += 1
            continue
        # the side view can't tell depth: a stroke at her front edge would land on her flank. It
        # only supplies what faces the side camera more than the front or back one does; the
        # front and back views own the rest
        if view == "side" and n.dot(-d) < abs(n.y):
            missed += 1
            continue
        tri = TRI[fi]
        A, B, C = TP[tri]
        v0, v1, v2 = B - A, C - A, np.array(loc) - A
        d00, d01, d11, d20, d21 = v0 @ v0, v0 @ v1, v1 @ v1, v2 @ v0, v2 @ v1
        den = d00 * d11 - d01 * d01 or 1e-12
        bv, bw = (d11 * d20 - d01 * d21) / den, (d00 * d21 - d01 * d20) / den
        w = np.clip([1 - bv - bw, bv, bw], 0, 1)
        w /= w.sum()
        rest = w @ X[tri]                                       # the same skin point, rest pose
        rn = np.cross(X[tri[1]] - X[tri[0]], X[tri[2]] - X[tri[0]])
        pts.append(rest)
        nrm.append(rn / (np.linalg.norm(rn) or 1))
        anc.append([int(t) for t in tri] + [round(float(x), 4) for x in w])
        rg.append(int(reg[tri[int(np.argmax(w))]]))
    if not pts:
        continue
    pos = np.array(pts)
    if meta["kind"] == "point":
        pos, nrm, anc = pos.mean(0)[None], nrm[:1], anc[:1]
    seg = np.linalg.norm(np.diff(np.vstack([pos, pos[:1]]) if closed else pos, axis=0), axis=1) if len(pos) > 1 else np.zeros(1)
    rgb = [round(c, 3) for c in fill[:3]] if fill else [0.05, 0.05, 0.06]
    pieces.append(dict(id=len(pieces) + 1, layer=layer, name=name, view=view, **meta, closed=bool(closed),
                       symmetry="as drawn in Figma (%s view)" % view, length_cm=round(float(seg.sum()), 1),
                       warning=("jumps %.1f cm: part of it landed on another body part" % seg.max()) if seg.max() > 4.0 else None,
                       rgb=rgb, region=sketch.REGIONS[np.bincount(rg, minlength=4).argmax()],
                       points=np.round(pos, 3).tolist(), normals=np.round(nrm, 3).tolist(), anchors=anc))
print("KIT figma: %d pieces, %d points missed her body or grazed it (left to another view)" % (len(pieces), missed))
# back to her rest pose for the preview (what the builder sees)
me.vertices.foreach_set("co", X.ravel())
me.update()
body["kit"] = json.dumps(info)
out = dict(outfit=NAME, source="figma.json (Figma frame 'outfit', T-pose views)", units="cm",
           body="android_skin.fbx, default sliders (DESIGN), hair removed, rest pose",
           axes="forward -Y, her left +X, up +Z", anchors="[v0, v1, v2, w0, w1, w2]: skin vertex ids and barycentric weights",
           mirror=False, **info, pieces=pieces)
with open(os.path.join(D, "sketch.json"), "w") as f:
    json.dump(out, f, indent=1)
sketch.NAME = NAME
sketch.write_md(D, out)
sketch.preview(D, pieces)
print("KIT import ->", D)
