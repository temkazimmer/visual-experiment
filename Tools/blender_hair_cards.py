"""Long loose hair as cards, BG3-style (research agent). Headless:

  HAIR_STYLE=long|elf HAIR_SKIN=<android_skin.fbx> /opt/homebrew/bin/blender -b --python Tools/blender_hair_cards.py -- <outdir> [preview] [rig]

Strands first: guide curves grow from the scalp, fall under gravity and are pushed off the
body (head, ears, neck, shoulders), so the hair drapes instead of hovering. Each guide
becomes one arched ribbon card textured with the procedural strand atlas from
hair_textures.py. Layers (0 under .. 2 top) sit outward from each other; a few short
flyaways break the edge. All look values are in LOOK.
Writes <outdir>/hair_cards.blend, hair_atlas.png and, with "preview", four 4K-ready renders;
with "rig", hair_cards.fbx (long) or hair_elf.fbx (elf), each with its own skeleton.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy
import numpy as np
from mathutils import Vector

import hair_textures
from hair_common import bvh_of, lights, load_skin, setup_render, shoot, world_verts

args = sys.argv[sys.argv.index("--") + 1:]
OUT = args[0]
PREVIEW = len(args) > 1 and args[1] == "preview"
os.makedirs(OUT, exist_ok=True)

LOOK = dict(
    style=os.environ.get("HAIR_STYLE", "long"),   # "long": loose and straight; "elf": half-up (Tema: Lord of the Rings elves)
    tie_el=6.0,            # elf: degrees above the ear tops where the gathered hair is tied at the back
    fall=(54, 62),         # elf: cm the gathered hair falls from the tie
    card_w_gather=(2.2, 3.4),
    card_w_tie=(1.1, 1.1),
    seed=11,
    guides=1700,           # strands that become cards (dense: a full head of hair, no gaps)
    part_x=1.6,            # the part, cm to her left of centre
    steps=30 if os.environ.get("HAIR_STYLE", "long") == "elf" else 22,   # points along a card (the tied hair runs further)
    length=dict(front=(48, 56), side=(60, 68), back=(66, 76), side_lock=(54, 62)),   # mid-back; locks to the chest
    fringe=140,            # guides in the fringe (long style only; the elf style has a centre part)
    fringe_az=34.0,        # degrees either side of the face the fringe spans
    fringe_el=72.0,        # how far back on the head the fringe grows from
    brow=1.8,              # cm ABOVE the head centre where the fringe is cut: on the brows, eyes clear
    brow_jag=1.2,          # cm of unevenness in that cut
    wisps=0.16,            # share of fringe strands that run long, down between and past the eyes
    wisp_cut=-3.0,         # ...to here (cm from the head centre; the eyes sit at about -2)
    side_locks=55,         # long style; the elf style keeps one thin lock a side (elf_locks)
    elf_locks=10,         # guides per face-framing lock
    side_az=(40.0, 66.0),  # where the locks grow, degrees round from the face
    stiff=(0.80, 0.50),    # how much a strand keeps its heading, root -> tip (low: falls straight)
    wave_amp=0.06,         # cm: straight, a hint of life
    wave_len=13.0,         # cm
    card_w_back=(4.0, 6.0),  # cm, card widths by zone
    card_w_fringe=(1.3, 2.1),
    card_w_side=(1.8, 2.8),  # long style
    card_w_elflock=(1.2, 1.9),
    arch=0.05,             # how much a card bows out of flat, against its width
    margin=0.15,           # cm a card keeps off the head
    body_clear=1.0,        # cm off the shoulders and back (clothes agent: straps, binding)
    layer_margin=0.20,     # cm per layer outward: some body, still not a helmet
    back_body=0.35,        # cm more per layer at the back and sides: a fuller back mass
    face_halfw=8.6,        # cm: hair below the brow stays outside this, so the face shows
    ear_open=0.09,         # radians around each ear (seen from the head centre) hair goes round
    line_front=36.0, line_side=-14.0, line_back=-34.0,   # hairline elevation (deg)
    cap_lift=0.05,         # cm the cap sits off the scalp (under the lowest cards)
    cap_fade=7.0,          # degrees over which the cap fades in from the hairline
)
rng = np.random.default_rng(LOOK["seed"])
FBX_NAME = "hair_elf.fbx" if LOOK["style"] == "elf" else "hair_cards.fbx"

skin, arm = load_skin()
co = world_verts(skin)
top = co[:, 2].max()
head = co[(co[:, 2] > top - 30) & (np.abs(co[:, 0]) < 9)]
C = Vector((0.0, (head[:, 1].min() + head[:, 1].max()) / 2 + 0.5, top - 9.5))
bvh = bvh_of(skin)
# the elf ears: vertices that sit outside the skull (a ray from the head's centre meets the
# skull first). Hair ignores them, so it lies against the skull and the ears come through it.
from mathutils import Vector as _V
cand = np.where((co[:, 2] > C.z - 12) & (co[:, 2] < C.z + 10) & (np.abs(co[:, 0]) > 6.5) & (co[:, 1] > C.y - 5))[0]
ear = np.zeros(len(co), bool)
_att = skin.data.attributes.get("ear")          # blender_face.py marks the ears
if _att is not None and _att.domain == "POINT":
    _v = np.zeros(len(co))
    _att.data.foreach_get("value", _v)
    ear = _v > 0.5
    cand = []
for i in cand:
    v = _V(co[i])
    dvec = v - C
    hit, _, _, dist = bvh.ray_cast(C, dvec.normalized(), dvec.length + 1.0)
    if hit is not None and dvec.length - dist > 0.6:
        ear[i] = True
print("ear verts", int(ear.sum()))
from mathutils.kdtree import KDTree
ear_pts = co[ear]
ear_kd = KDTree(len(ear_pts))
for i, v in enumerate(ear_pts):
    ear_kd.insert(v, i)
ear_kd.balance()
ear_dirs = KDTree(len(ear_pts))
for i, v in enumerate(ear_pts):
    ear_dirs.insert((Vector(v) - C).normalized(), i)
ear_dirs.balance()
_ea = np.degrees(np.arctan2(np.abs(ear_pts[:, 0] - C.x), -(ear_pts[:, 1] - C.y)))   # 0 front .. 180 back
EAR_AZ_FRONT = float(np.percentile(_ea, 3))
EAR_AZ_BACK = float(np.percentile(_ea, 97))
_eel = np.degrees(np.arcsin(np.clip((ear_pts[:, 2] - C.z) / np.linalg.norm(ear_pts - np.array(C), axis=1), -1, 1)))
EAR_EL_TOP = float(np.percentile(_eel, 97))
EAR_R = float(np.percentile(np.linalg.norm(ear_pts - np.array(C), axis=1), 97))
print("ear az", EAR_AZ_FRONT, EAR_AZ_BACK, "el top", EAR_EL_TOP, "r", EAR_R)
print("ear front az", EAR_AZ_FRONT)
me_ = skin.data
me_.calc_loop_triangles()
from mathutils.bvhtree import BVHTree
bvh_body = BVHTree.FromPolygons([tuple(c) for c in co],
                                [tuple(t.vertices) for t in me_.loop_triangles if not ear[list(t.vertices)].any()])


def direction(az, el):
    a, e = math.radians(az), math.radians(el)
    return Vector((math.sin(a) * math.cos(e), -math.cos(a) * math.cos(e), math.sin(e)))


def scalp(az, el):
    d = direction(az, el)
    hit, nrm, _, dist = bvh_body.ray_cast(C, d, 30.0)
    if not hit or dist > 13.0:                       # through an ear: bridge it
        h2, n2 = scalp(az, el + 12.0)
        return C + d * (h2 - C).length, d
    if nrm.dot(d) < 0:
        nrm = -nrm
    return hit, nrm


def hairline(az):
    a = abs(((az + 180) % 360) - 180)
    if a < 90:
        t = a / 90
        t = t * t * (3 - 2 * t)
        return LOOK["line_front"] + (LOOK["line_side"] - LOOK["line_front"]) * t
    t = (a - 90) / 90
    t = t * t * (3 - 2 * t)
    return LOOK["line_side"] + (LOOK["line_back"] - LOOK["line_side"]) * t


def blend(a, b, t):
    return a + (b - a) * t


def region_length(az):
    a = abs(((az + 180) % 360) - 180)               # 0 front .. 180 back
    L = LOOK["length"]
    lo, hi = (L["front"], L["side"]) if a < 90 else (L["side"], L["back"])
    t = (a % 90) / 90
    return rng.uniform(blend(lo[0], hi[0], t), blend(lo[1], hi[1], t))


def grow(az, el, layer, zone, flyaway=False):
    """One guide, root first, resampled to LOOK["steps"] + 1 points.

    zone: "fringe" falls forward over the forehead and is cut at the brow; "side" is a
    face-framing lock that hangs in front of the ear; "back" is combed behind the ears.
    """
    p0, n0 = scalp(az, el)
    margin = LOOK["margin"] + LOOK["layer_margin"] * layer
    if zone == "back":
        margin += LOOK["back_body"] * layer                  # the back reads fuller; the front stays sleek
    p = p0 + n0 * (0.15 + margin * 0.6)
    fa = abs(((az + 180) % 360) - 180)
    side = 1.0 if p.x - LOOK["part_x"] > 0 else -1.0
    if zone == "fringe":
        L = 40.0                                       # stops at the brow instead
        want = Vector((p.x * 0.04, -0.75, -1.0))
    elif zone == "side":
        L = rng.uniform(*LOOK["length"]["side_lock"])
        want = Vector((0.0, 0.12, -1.0))          # hangs straight beside the face, not over the eye
    else:
        L = region_length(az)
        over_ear = EAR_AZ_FRONT - 25 <= fa <= EAR_AZ_BACK + 10 and EAR_EL_TOP - 25 < el < EAR_EL_TOP + 40
        behind = 0.25 if fa > 70 else 0.0               # falls behind the shoulders
        want = Vector((side * (0.15 if fa < 60 else 0.0), (0.7 if over_ear else 0.0) + behind, -1.0))
    if flyaway:
        L *= rng.uniform(0.55, 0.85)
    tang = want - n0 * want.dot(n0)                     # laid along the scalp
    if tang.length < 1e-3:
        tang = Vector((0, 0.3, -1)) - n0 * Vector((0, 0.3, -1)).dot(n0)
    lift = 0.05 + 0.07 * max(0.0, (el - 45.0) / 45.0)   # a little body at the crown, still sleek
    if zone == "fringe":
        lift = 0.04                                    # the fringe lies on the head
    d = (tang.normalized() + n0 * lift).normalized()
    front_of_ear = zone != "back"
    z_cut = C.z + LOOK["brow"] + rng.uniform(-LOOK["brow_jag"], LOOK["brow_jag"])
    wisp = zone == "fringe" and rng.uniform() < LOOK["wisps"]
    if wisp:
        z_cut = C.z + LOOK["wisp_cut"] + rng.uniform(-1.0, 1.0)
    if zone == "fringe":
        z_cut = min(z_cut, p.z - rng.uniform(2.5, 4.0))   # rooted low at the sides: no stubs, a bit longer there
    pts = [p.copy()]
    step = 0.6
    travelled = 0.0
    phase = math.radians(az) * 2.3 + rng.uniform(-0.5, 0.5)
    while travelled < L:
        t = travelled / L
        s = blend(LOOK["stiff"][0], LOOK["stiff"][1], t) ** (step / 1.9)
        d = (d * s + Vector((0, 0, -1)) * (1 - s) * (1.0 + 0.5 * t)).normalized()
        wav = math.sin(phase + travelled / LOOK["wave_len"] * 6.283) * LOOK["wave_amp"] * min(1.0, t * 3)
        perp = d.cross(Vector((0, 0, 1)))
        if perp.length > 1e-4:
            d = (d + perp.normalized() * wav * 0.04).normalized()
        p = p + d * step
        travelled += step
        if zone == "fringe" and p.z < z_cut:
            break
        # the face shows: from the brow down, hair in front of the face goes out to its sides
        hw = LOOK["face_halfw"] * (1.0 - 0.12 * max(0.0, min(1.0, (C.z - p.z) / 24.0)))
        if zone != "fringe" and p.y < C.y - 1 and C.z - 26 < p.z < C.z + 3 and abs(p.x) < hw:
            sx = 1.0 if p.x >= 0 else -1.0
            p.x = sx * hw
            d = (d + Vector((sx * 0.12, 0.3, -0.4))).normalized()   # on down past the face, not sideways
        # the ears show: side locks pass in front of them, everything else behind
        for _ in range(30):
            rel = p - C
            q, _, dd = ear_dirs.find(rel.normalized())
            if dd >= LOOK["ear_open"] or rel.length > EAR_R + 6.0:
                break
            a_ = math.atan2(rel.x, -rel.y)
            back_v = Vector((math.cos(a_), math.sin(a_), 0.0)) * (1.0 if rel.x > 0 else -1.0)
            mv = -back_v if front_of_ear else back_v
            p = p + mv * 0.25
            hit, nrm, _, _ = bvh_body.find_nearest(p)      # ...along the head, not off it
            if hit is not None:
                p = hit + nrm * max(margin, min((p - hit).length, margin + 0.6))
            d = (d + mv * 0.25).normalized()
        for _ in range(2):                           # keep off the body
            hit, nrm, _, dist = bvh_body.find_nearest(p)
            if hit is not None:
                v = p - hit
                m_here = max(margin, LOOK["body_clear"]) if p.z < C.z - 22 else margin
                if v.dot(nrm) < 0 or v.length < m_here:
                    p = hit + nrm * m_here
                    d = (d - nrm * min(0.0, d.dot(nrm))).normalized()
        pts.append(p.copy())
    # resample to an even number of points along the path
    P = np.array([tuple(x) for x in pts])
    seg = np.r_[0, np.cumsum(np.linalg.norm(np.diff(P, axis=0), axis=1))]
    if seg[-1] < 1e-3:
        P = np.vstack([P, P[-1] + np.array([0, 0, -1.0])])
        seg = np.r_[0, 1.0]
    tt = np.linspace(0, seg[-1], LOOK["steps"] + 1)
    R = np.stack([np.interp(tt, seg, P[:, k]) for k in range(3)], 1)
    return [Vector(r) for r in R]


# ---- guides ------------------------------------------------------------------------------
def runs_level(pts, n=4):
    """True if the strand runs level for n steps by the head (caught going round an ear): it
    would render as a horizontal spike sticking out of the silhouette."""
    P = np.array([tuple(x) for x in pts])
    d = np.diff(P, axis=0)
    L = np.linalg.norm(d, axis=1) + 1e-9
    flat = (np.abs(d[:, 2]) / L < 0.3) & (P[1:, 2] > C.z - 20)
    run = 0
    for f in flat:
        run = run + 1 if f else 0
        if run >= n:
            return True
    return False


def resample(pts):
    P = np.array([tuple(x) for x in pts])
    seg = np.r_[0, np.cumsum(np.linalg.norm(np.diff(P, axis=0), axis=1))]
    tt = np.linspace(0, seg[-1], LOOK["steps"] + 1)
    return [Vector(r) for r in np.stack([np.interp(tt, seg, P[:, k]) for k in range(3)], 1)]


def keep_off(p, margin):
    for _ in range(2):
        hit, nrm, _, _ = bvh_body.find_nearest(p)
        if hit is not None:
            v = p - hit
            m_here = max(margin, LOOK["body_clear"]) if p.z < C.z - 22 else margin
            if v.dot(nrm) < 0 or v.length < m_here:
                p = hit + nrm * m_here
    return p


EL_TIE = EAR_EL_TOP + LOOK["tie_el"]
_pT, _nT = scalp(180.0, EL_TIE)
TIE = _pT + _nT * 1.4                  # the knot at the back of the head
FALL = Vector((0.0, 0.35, -1.0)).normalized()


def gathered(az, el, layer):
    """Combed from the root along the head, over the ear, back to the tie; then it falls."""
    a0 = signed_az(az)
    side = 1.0 if a0 >= 0 else -1.0
    a1 = side * 180.0
    margin = 0.2 + 0.15 * layer
    pts = []
    K = 26
    dr0 = direction(az, el)
    dr1 = direction(a1 - side * 8.0, EL_TIE)                # into the knot from its own side
    # from a centre part: out sideways and down past a point above the ear, then back to the knot
    # (Tema's references: parted, swept behind the ears). Higher roots pass higher, so the
    # layers lie over each other instead of bunching into one rope.
    uu = max(0.0, min(1.0, (el - 20.0) / 68.0))
    drW = direction(side * 88.0, EAR_EL_TOP + 8.0 + 30.0 * uu)
    for k in range(K):
        t = k / (K - 1)
        dd = (dr0 * (1 - t) ** 2 + drW * 2 * t * (1 - t) * 1.25 + dr1 * t ** 2).normalized()
        e = math.degrees(math.asin(max(-1.0, min(1.0, dd.z))))
        a = math.degrees(math.atan2(dd.x, -dd.y))
        if EAR_AZ_FRONT - 22 <= abs(a) <= EAR_AZ_BACK + 18:
            e = max(e, EAR_EL_TOP + 7.0)           # over the ears: they show below the gathered hair
        p_, n_ = scalp(a, e)
        q = p_ + n_ * margin
        if t > 0.8:                                  # draw in to the knot
            u = (t - 0.8) / 0.2
            q = q.lerp(TIE + Vector((side * 0.6, 0, 0)), u * u)
        pts.append(q)
    p = TIE + Vector((rng.uniform(-1.3, 1.3), rng.uniform(-0.3, 0.3), rng.uniform(-0.8, 0.8)))
    d = FALL.copy()
    L = rng.uniform(*LOOK["fall"])
    step, travelled = 0.6, 0.0
    while travelled < L:
        t = travelled / L
        s_ = blend(0.85, 0.55, t) ** (step / 1.9)
        d = (d * s_ + Vector((0, 0, -1)) * (1 - s_) * (1.0 + 0.5 * t)).normalized()
        p = keep_off(p + d * step, 0.6)
        travelled += step
        pts.append(p.copy())
    return resample(pts)


def tie_ring():
    """A band of hair wrapped round the knot."""
    ax = FALL
    u = ax.cross(Vector((1, 0, 0))).normalized()
    v = ax.cross(u).normalized()
    return [TIE + (u * math.cos(t) + v * math.sin(t)) * 1.5 for t in np.linspace(0, 2 * math.pi, LOOK["steps"] + 1)]


def signed_az(az):
    return ((az + 180) % 360) - 180


guides = []        # (pts, layer, flyaway, az, zone)
dropped = 0
if LOOK["style"] == "elf":
    for k in range(LOOK["guides"]):
        az = rng.uniform(0, 360)
        lo = hairline(az)
        u = rng.uniform(0, 1) ** 0.8
        el = lo + (88.0 - lo) * u
        fa = abs(signed_az(az))
        layer = 0 if u > 0.62 else int(rng.choice([0, 1, 2], p=[0.25, 0.4, 0.35]))
        if fa < 100:                                  # the front and the sides: tied back
            sa = signed_az(az)
            guides.append((gathered(az, el, layer), layer, False, (1 if sa >= 0 else -1) * rng.uniform(150, 180), "gather"))
            continue
        fly = rng.uniform() < 0.04
        pts = grow(az, el, layer, "back", fly)       # the back: loose, down her back
        if runs_level(pts):
            dropped += 1
            continue
        guides.append((pts, layer, fly, az, "back"))
    for sgn in (1, -1):                               # one thin lock a side, in front of the ear
        for k in range(LOOK["elf_locks"]):
            az = (sgn * rng.uniform(44, 58)) % 360
            el = rng.uniform(EAR_EL_TOP - 4, EAR_EL_TOP + 6)
            guides.append((grow(az, el, 2, "side"), 2, False, az, "side"))
    # no band round the knot: the loose back hair covers it, and a band only poked out behind
else:
    for k in range(LOOK["guides"]):                       # the main mass, combed behind the ears
        az = rng.uniform(0, 360)
        lo = hairline(az)
        u = rng.uniform(0, 1) ** 0.8
        el = lo + (88.0 - lo) * u
        fa = abs(((az + 180) % 360) - 180)
        if fa < LOOK["fringe_az"] and el < LOOK["fringe_el"]:
            continue                                      # the fringe has its own guides
        if LOOK["fringe_az"] <= fa < EAR_AZ_FRONT - 2 and el < EAR_EL_TOP + 8:   # temples: they frame the face, never sweep back over the ear
            if rng.uniform() < 0.5:
                guides.append((grow(az, el, 1, "side"), 1, False, az, "side"))
            continue
        fly = rng.uniform() < 0.04
        layer = 0 if u > 0.62 else int(rng.choice([0, 1, 2], p=[0.25, 0.4, 0.35]))
        pts = grow(az, el, layer, "back", fly)
        if runs_level(pts):
            dropped += 1
            continue
        guides.append((pts, layer, fly, az, "back"))
    for k in range(LOOK["fringe"]):                       # the fringe, to the brow
        az = rng.uniform(-LOOK["fringe_az"], LOOK["fringe_az"]) % 360
        el = rng.uniform(hairline(az) + 4, LOOK["fringe_el"])
        guides.append((grow(az, el, 2, "fringe"), 2, False, az, "fringe"))
    for sgn in (1, -1):                                   # two face-framing side locks
        for k in range(LOOK["side_locks"]):
            az = (sgn * rng.uniform(*LOOK["side_az"])) % 360
            el = rng.uniform(hairline(az) + 2, hairline(az) + 22)
            guides.append((grow(az, el, 2, "side"), 2, False, az, "side"))
print("guides", len(guides), "dropped (ran level round an ear)", dropped)

# ---- cards -------------------------------------------------------------------------------
TILES = 8
V, F, UVs = [], [], []
n = LOOK["steps"] + 1
CARDS = []         # (first vertex, zone, az, path) per card, for the rig
for pts, layer, fly, az, zone in guides:
    P = [np.array(x) for x in pts]
    CARDS.append((len(V), zone, az, np.array(P)))
    wkey = "card_w_elflock" if (zone == "side" and LOOK["style"] == "elf") else "card_w_" + zone
    w0 = rng.uniform(*LOOK[wkey]) * (0.45 if fly else 1.0)
    if zone == "fringe" and P[-1][2] < C.z - 0.5:
        w0 *= 0.5                                     # the long wisps are a few strands each
    base = len(V)
    prev_n = None
    # dense tiles under everything, and on the locks by her face and the fringe: on sparse tiles
    # those were mostly holes, and the sunlit skin behind showed through as beige (QA 0278)
    dense = layer == 0 or zone in ("side", "fringe", "tie", "gather")
    tile = int(rng.integers(0, TILES // 2)) if dense else int(rng.integers(TILES // 2, TILES))
    flip = rng.uniform() < 0.5
    for i, p in enumerate(P):
        t = P[min(i + 1, n - 1)] - P[max(i - 1, 0)]
        t = t / (np.linalg.norm(t) + 1e-9)
        out = p - np.array([C.x, C.y, min(p[2], C.z)])     # away from the head's axis
        out = out / (np.linalg.norm(out) + 1e-9)
        nn = out - t * out.dot(t)
        if np.linalg.norm(nn) < 1e-4 and prev_n is not None:
            nn = prev_n
        nn = nn / (np.linalg.norm(nn) + 1e-9)
        prev_n = nn
        b = np.cross(t, nn)
        b = b / (np.linalg.norm(b) + 1e-9)
        s = i / (n - 1)
        w = w0 * (1.0 - 0.22 * s) * (0.55 + 0.45 * min(1.0, s * 6))
        _, _, de = ear_kd.find(Vector(p))
        w *= min(1.0, max(0.25, de / 2.5))            # cards narrow beside an ear
        for off in (-0.5, 0.0, 0.5):
            q = p + b * off * w + nn * (LOOK["arch"] * w * (1 - (2 * off) ** 2) * 0.5)
            V.append(tuple(q))
            uu = (off + 0.5) if not flip else (0.5 - off)
            UVs.append(((tile + 0.03 + 0.94 * uu) / TILES, s))
    for i in range(n - 1):
        for j in range(2):
            a = base + i * 3 + j
            F.append((a, a + 1, a + 4, a + 3))
print("cards", len(guides), "verts", len(V), "quads", len(F))

# ---- the cap --------------------------------------------------------------------------------
# A shell on the scalp, hair-coloured, fading out at the hairline: the part and any gap between
# cards show hair, not bright skin (studios darken the scalp under card hair; Docs/Research/HAIR.md).
# Its UV.x carries the fade (0 at the hairline .. 1 inside); it gets its own material slot.
N_CARD_VERTS, N_CARD_FACES = len(V), len(F)
_tri = np.array([t.vertices[:] for t in me_.loop_triangles])
_rel = co - np.array(C)
_dist = np.linalg.norm(_rel, axis=1) + 1e-9
_el = np.degrees(np.arcsin(np.clip(_rel[:, 2] / _dist, -1, 1)))
_az = np.degrees(np.arctan2(_rel[:, 0], -_rel[:, 1]))
_line = np.array([hairline(a) for a in _az])
_fade = np.clip((_el - _line) / LOOK["cap_fade"], 0, 1)
_in = (_el > _line) & (_dist < 14.0) & ~ear
_keep = _tri[_in[_tri].all(axis=1)]
_fn = np.cross(co[_keep[:, 1]] - co[_keep[:, 0]], co[_keep[:, 2]] - co[_keep[:, 0]])
_vn = np.zeros_like(co)
for k in range(3):
    np.add.at(_vn, _keep[:, k], _fn)
_vn /= np.linalg.norm(_vn, axis=1, keepdims=True) + 1e-9
_out = (_vn * _rel).sum(1) < 0                       # normals out of the head
_vn[_out] *= -1
_ids = np.unique(_keep)
_map = {int(v): N_CARD_VERTS + i for i, v in enumerate(_ids)}
for v in _ids:
    V.append(tuple(co[v] + _vn[v] * LOOK["cap_lift"]))
    UVs.append((float(_fade[v]), 0.5))
for t in _keep:
    F.append(tuple(_map[int(v)] for v in t))
CAP_VERTS = list(range(N_CARD_VERTS, len(V)))
print("cap verts", len(CAP_VERTS), "tris", len(_keep))

me = bpy.data.meshes.new("HairCards")
me.from_pydata(V, [], F)
me.update()
uv = me.uv_layers.new(name="UVMap")
for li, loop in enumerate(me.loops):
    uv.data[li].uv = UVs[loop.vertex_index]
for pg in me.polygons:
    pg.use_smooth = True
    pg.material_index = 1 if pg.index >= N_CARD_FACES else 0
hair = bpy.data.objects.new("Hair", me)
bpy.context.collection.objects.link(hair)

atlas = os.path.join(OUT, "hair_atlas.png")
hair_textures.make_atlas(atlas, tiles=TILES)

# ---- preview material (the Unreal one is built separately) ----------------------------------
m = bpy.data.materials.new("HairPreview")
m.use_nodes = True
nt = m.node_tree
nt.nodes.clear()
tex = nt.nodes.new("ShaderNodeTexImage")
tex.image = bpy.data.images.load(atlas)
tex.image.colorspace_settings.name = "Non-Color"
sep = nt.nodes.new("ShaderNodeSeparateColor")
nt.links.new(tex.outputs["Color"], sep.inputs["Color"])
ramp = nt.nodes.new("ShaderNodeValToRGB")          # root -> tip colour
nt.links.new(sep.outputs["Blue"], ramp.inputs["Fac"])
ramp.color_ramp.elements[0].position = 0.0
ramp.color_ramp.elements[0].color = (0.004, 0.005, 0.010, 1)
ramp.color_ramp.elements[1].position = 1.0
ramp.color_ramp.elements[1].color = (0.035, 0.042, 0.075, 1)
var = nt.nodes.new("ShaderNodeMath")
var.operation = "MULTIPLY_ADD"
var.inputs[1].default_value = 0.9
var.inputs[2].default_value = 0.45
nt.links.new(sep.outputs["Red"], var.inputs[0])
mul = nt.nodes.new("ShaderNodeMix")
mul.data_type = "RGBA"
mul.blend_type = "MULTIPLY"
mul.inputs["Factor"].default_value = 1.0
nt.links.new(ramp.outputs["Color"], mul.inputs["A"])
nt.links.new(var.outputs["Value"], mul.inputs["B"])
bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
nt.links.new(mul.outputs["Result"], bsdf.inputs["Base Color"])
bsdf.inputs["Roughness"].default_value = 0.42
bsdf.inputs["Specular IOR Level"].default_value = 0.5
bsdf.inputs["Anisotropic"].default_value = 0.9
bsdf.inputs["Sheen Weight"].default_value = 0.2
tan = nt.nodes.new("ShaderNodeTangent")
tan.direction_type = "UV_MAP"
tan.uv_map = "UVMap"
nt.links.new(tan.outputs["Tangent"], bsdf.inputs["Tangent"])
nt.links.new(tex.outputs["Alpha"], bsdf.inputs["Alpha"])
outn = nt.nodes.new("ShaderNodeOutputMaterial")
nt.links.new(bsdf.outputs["BSDF"], outn.inputs["Surface"])
hair.data.materials.append(m)
capm = bpy.data.materials.new("HairCap")
capm.use_nodes = True
capm.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.008, 0.007, 0.006, 1)
capm.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 0.8
hair.data.materials.append(capm)

sm = bpy.data.materials.new("skinprev")
sm.use_nodes = True
sm.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.42, 0.38, 0.34, 1)
sm.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 0.55
skin.data.materials.clear()
skin.data.materials.append(sm)

if PREVIEW:
    setup_render((1600, 900), 64)
    lights()
    tz = top - 22                                     # aim at the chin: head and shoulders in frame
    for nme, eye in (("front", (0, -150, tz + 6)), ("side", (150, 0, tz + 6)), ("back", (0, 150, tz + 6)),
                     ("front34", (105, -110, tz + 8))):
        shoot(os.path.join(OUT, "hair_%s.png" % nme), eye, (0, 0, tz), lens=60)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "hair_cards.blend"))
if "rig" in args:

    # ---- the physics rig ----------------------------------------------------------------------
    # Hair is its own skeletal mesh: the character's bones from pelvis to head (same names, so the
    # hair's AnimBP can Copy Pose From Mesh) plus swaying chains under "head" for Kawaii Physics.
    CHAINS = dict(                       # zone -> (azimuths, bones per chain); az 0 = face, + = her left
        back=([105, 140, 168, -168, -140, -105], 5),
        side=([53, -53], 5),
        fringe=([-18, 0, 18], 2),
    )


    def az_diff(a, b):
        return abs(((a - b + 180) % 360) - 180)


    def signed_az(az):
        return ((az + 180) % 360) - 180


    chains = []                          # dict(name, zone, az, bones=[(head, tail)], f0)
    assign = []                          # per card: [(chain index, weight)]
    for zone, (azs, nb) in CHAINS.items():
        for k, a in enumerate(azs):
            chains.append(dict(zone=zone, az=a, nb=nb, members=[],
                               name="hair_%s%d" % (zone[0], k)))
    for ci_card, (first, zone, az, P) in enumerate(CARDS):
        if zone == "tie":
            assign.append([])                       # the knot rides the head
            continue
        cz = "back" if zone == "gather" else zone
        cand = [(az_diff(signed_az(az), c["az"]), i) for i, c in enumerate(chains) if c["zone"] == cz]
        cand.sort()
        if cz == "back" and len(cand) > 1:
            (d0, i0), (d1, i1) = cand[0], cand[1]
            w0 = d1 / (d0 + d1 + 1e-6)
            assign.append([(i0, w0), (i1, 1 - w0)])
        else:
            assign.append([(cand[0][1], 1.0)])
        if zone != "gather":                        # the chains follow the loose hair's path
            chains[assign[-1][0][0]]["members"].append(P)

    n_pts = LOOK["steps"] + 1
    used = {ci for a_ in assign for ci, _ in a_}
    keep_idx = [i for i, c in enumerate(chains) if c["members"] and i in used]
    remap = {old: new for new, old in enumerate(keep_idx)}
    chains = [chains[i] for i in keep_idx]           # no fringe in the elf style: no fringe chains
    assign = [[(remap[ci], w) for ci, w in a_ if ci in remap] for a_ in assign]
    for c in chains:
        M = np.mean(c["members"], axis=0) if c["members"] else None
        c["path"] = M
        # where the chain leaves the head: back and side hang from below the ears, the fringe
        # from where it lifts off the forehead
        start = n_pts // 3
        for i in range(n_pts):
            q = Vector(M[i])
            if c["zone"] == "fringe":
                hit, nrm, _, _ = bvh_body.find_nearest(q)
                if hit is not None and (q - hit).length > LOOK["margin"] + 0.9:
                    start = max(1, i - 1)
                    break
            elif q.z < C.z - (6.0 if c["zone"] == "side" else 9.0):
                start = i
                break
        start = min(start, n_pts - 3)
        c["f0"] = start / (n_pts - 1)
        seg = np.r_[0, np.cumsum(np.linalg.norm(np.diff(M[start:], axis=0), axis=1))]
        joints = [np.array([np.interp(t, seg, M[start:, k]) for k in range(3)])
                  for t in np.linspace(0, seg[-1], c["nb"] + 1)]
        c["bones"] = list(zip(joints[:-1], joints[1:]))

    skin_arm = arm
    bpy.ops.object.select_all(action="DESELECT")
    rig_data = skin_arm.data.copy()
    rig_obj = bpy.data.objects.new("HairRig", rig_data)
    bpy.context.collection.objects.link(rig_obj)
    bpy.context.view_layer.objects.active = rig_obj
    bpy.ops.object.mode_set(mode="EDIT")
    eb = rig_data.edit_bones
    keep = set()
    b = eb["head"]
    while b:
        keep.add(b.name)
        b = b.parent
    for b in list(eb):
        if b.name not in keep:
            eb.remove(b)
    for c in chains:
        parent = eb["head"]
        c["bone_names"] = []
        for j, (h, t) in enumerate(c["bones"]):
            nb_ = eb.new("%s_%02d" % (c["name"], j + 1))
            nb_.head, nb_.tail = Vector(h), Vector(t)
            nb_.roll = 0.0
            nb_.parent = parent
            nb_.use_connect = j > 0
            parent = nb_
            c["bone_names"].append(nb_.name)
    bpy.ops.object.mode_set(mode="OBJECT")

    # weights: each card follows its chain(s) by how far along the card a vertex is
    groups = {"head": hair.vertex_groups.new(name="head")}
    for c in chains:
        for bn in c["bone_names"]:
            groups[bn] = hair.vertex_groups.new(name=bn)
    per_card = n_pts * 3
    for ci_card, (first, zone, az, P) in enumerate(CARDS):
        # where this card leaves the head (its own, not its chain's: the tied hair runs further)
        cf0 = None
        if zone in ("back", "gather", "side"):
            lim = C.z - (6.0 if zone == "side" else 9.0)
            below = [i for i in range(n_pts) if P[i][2] < lim]
            cf0 = min(below[0] if below else n_pts - 3, n_pts - 3) / (n_pts - 1)
        if not assign[ci_card]:
            groups["head"].add([first + i for i in range(n_pts * 3)], 1.0, "REPLACE")
            continue
        for i in range(n_pts):
            f = i / (n_pts - 1)
            idx = [first + i * 3 + j for j in range(3)]
            wts = {}
            for ci, cw in assign[ci_card]:
                c = chains[ci]
                f0 = cf0 if cf0 is not None else c["f0"]
                g = (f - f0) / max(1e-6, 1 - f0) * c["nb"]
                if g <= 0:
                    h = min(1.0, max(0.0, 1.0 + g * 2))          # eases out of the head
                    wts["head"] = wts.get("head", 0) + cw * (1 - h * 0.0) * (1.0 if g < -0.5 else 1 - (g + 0.5))
                    if g > -0.5:
                        bn = c["bone_names"][0]
                        wts[bn] = wts.get(bn, 0) + cw * (g + 0.5)
                    continue
                k = min(int(g), c["nb"] - 1)
                fr = min(1.0, g - k)
                bn = c["bone_names"][k]
                if k + 1 < c["nb"]:
                    wts[bn] = wts.get(bn, 0) + cw * (1 - fr * 0.5)
                    bn2 = c["bone_names"][k + 1]
                    wts[bn2] = wts.get(bn2, 0) + cw * fr * 0.5
                else:
                    wts[bn] = wts.get(bn, 0) + cw
            tot = sum(wts.values()) or 1.0
            for bn, w in wts.items():
                groups[bn].add(idx, w / tot, "REPLACE")
    groups["head"].add(CAP_VERTS, 1.0, "REPLACE")       # the cap rides the head
    hair.parent = rig_obj
    mod = hair.modifiers.new("Armature", "ARMATURE")
    mod.object = rig_obj
    print("rig chains", len(chains), "bones", sum(c["nb"] for c in chains))

    # export: the hair and its rig only (the skin is not in this FBX)
    skin_name = skin.name
    bpy.data.objects.remove(skin, do_unlink=True)
    bpy.data.objects.remove(skin_arm, do_unlink=True)
    rig_obj.name = "root"                 # as in the skin chain: the armature node becomes "root"
    bpy.ops.object.select_all(action="DESELECT")
    hair.select_set(True)
    rig_obj.select_set(True)
    bpy.context.view_layer.objects.active = rig_obj
    bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, FBX_NAME), use_selection=True,
                             object_types={"ARMATURE", "MESH"}, add_leaf_bones=False,
                             use_armature_deform_only=False, mesh_smooth_type="FACE", bake_anim=False,
                             axis_forward="-Z", axis_up="Y")
    print("wrote", os.path.join(OUT, FBX_NAME))
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "hair_rig.blend"))
