# The shape sliders' morph deltas for the MetaHuman body. exec'd by Tools/blender_outfit.py
# (--body=mh --mh-morphs) once its landmarks exist, so X, T, G, Bn, LM, LAT, PEL, LEFT, FWD,
# UPPERARM, kit_profile, mh_tpose_points, mh_kit_tpose, MH_BASE_VERTS and _KIT are in scope.
#
# In:  Saved/MH/morphs/base.json, var_<name>.json   (Tools/mh_morphs.py STEP='base' / 'variants')
#      SourceArt/Character/android_skin.fbx          (the current body's shape keys)
# Out: SourceArt/Character/MH/Morphs/mh_morphs.json  (deltas per body LOD 0 vertex, the mesh's own
#      vertex order and UE's frame, cm; Tools/mh_morphs.py STEP='apply' and the outfit build read it)
#
# Two sources. Where the Creator has a body control the shape is a real body's: one variant less
# another, kept inside the old slider's footprint and sized to it (the same root mean square), so a
# slider moves her as far as it does today. Where it has none (abs, the jiggle) the old shape key itself is
# carried over through the kit's landmark map (this body's T-pose warped onto the old body's).
from mathutils.interpolate import poly_3d_calc as _bary

_PROJ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_WORK = os.path.join(_PROJ, "Saved", "MH", "morphs")
_OLD = os.path.join(_PROJ, "SourceArt", "Character", "android_skin.fbx")
_OUT = os.path.join(_PROJ, "SourceArt", "Character", "MH", "Morphs", "mh_morphs.json")
_n1 = MH_BASE_VERTS

# ---- this body in the mesh's own vertex order (UE), and Blender's copy of it ---------------------
_base = _json.load(open(os.path.join(_WORK, "base.json")))
_U = np.array(_base["v"], float)
_UT = np.array(_base["t"], int)
assert len(_U) == _n1, "base.json has %d vertices, the skin %d: export both again" % (len(_U), _n1)
_FLIP = np.array([1.0, -1.0, 1.0])                 # UE (x, y, z) cm -> Blender (x, -y, z) cm
# (most of the body's own vertices are the split skin's first MH_BASE_VERTS, but not all: about 1,500
# on the soles land further up, so every vertex of the split skin is searched)
_kdb = KDTree(nv)
for _i in range(nv):
    _kdb.insert(X[_i], _i)
_kdb.balance()
_hits = [_kdb.find(_p) for _p in (_U * _FLIP).tolist()]
BL = np.array([h[1] for h in _hits])               # the mesh's vertex -> its vertex in Blender's split skin
_err = max(h[2] for h in _hits)
assert _err < 0.02 and len(set(BL.tolist())) == _n1, "skin FBX and base.json disagree (%.3f cm, %d of %d matched)" % (
    _err, len(set(BL.tolist())), _n1)
print("mh morphs: %d vertices matched to the mesh's order, worst %.4f cm" % (_n1, _err))

_E = np.unique(np.sort(np.concatenate([_UT[:, [0, 1]], _UT[:, [1, 2]], _UT[:, [2, 0]]]), axis=1), axis=0)
_DEGU = np.maximum(np.bincount(_E[:, 0], minlength=_n1) + np.bincount(_E[:, 1], minlength=_n1), 1).astype(float)


def _blur_u(a, iters):
    """Diffuse a per-vertex array (n) or (n, 3) over the body's edges."""
    a = np.asarray(a, float)
    for _ in range(iters):
        acc = np.zeros_like(a)
        np.add.at(acc, _E[:, 0], a[_E[:, 1]])
        np.add.at(acc, _E[:, 1], a[_E[:, 0]])
        a = 0.5 * a + 0.5 * acc / (_DEGU if a.ndim == 1 else _DEGU[:, None])
    return a


def _smoothstep(x, a, b):
    t = np.clip((np.asarray(x, float) - a) / (b - a), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


# the neck opening is shared with the face mesh, which gets no morphs: nothing moves near it
_cnt = {}
for _a, _b in np.sort(np.concatenate([_UT[:, [0, 1]], _UT[:, [1, 2]], _UT[:, [2, 0]]]), axis=1).tolist():
    _cnt[(_a, _b)] = _cnt.get((_a, _b), 0) + 1
_rim = sorted({v for e, c in _cnt.items() if c == 1 for v in e})
_kdr = KDTree(len(_rim))
for _i, _v in enumerate(_rim):
    _kdr.insert(_U[_v], _i)
_kdr.balance()
_rim_d = np.array([_kdr.find(_p)[2] for _p in _U.tolist()])
SEAM = _smoothstep(_rim_d, 2.0, 7.0)
print("mh morphs: %d open-edge vertices (z %.1f..%.1f); %d vertices within 7 cm are faded" % (
    len(_rim), _U[_rim, 2].min(), _U[_rim, 2].max(), int((SEAM < 1).sum())))

# ---- the old body's shape keys, carried over by landmark -----------------------------------------
_ref = _json.load(open(os.path.join(_KIT, "views", "ref_profile.json")))
_tp_old_frame, _ = mh_kit_tpose(_KIT)              # this body's T-pose, warped onto the old body's bands
_tpm = mh_tpose_points()
_mine = kit_profile(_tpm)
_kz, _oz = np.array(_mine["z"]), np.array(_ref["z"])
_z = _tpm[BL, 2]
# how many old cm one cm of this body is, across, front to back and up, at each vertex's height
_SX = np.interp(_z, _kz, np.array(_ref["hw"]) / np.array(_mine["hw"]))
_SY = (np.interp(_z, _kz, _ref["front"]) - np.interp(_z, _kz, _ref["back"])) / (np.interp(_z, _kz, _mine["front"]) - np.interp(_z, _kz, _mine["back"]))
_SZ = _mh_interp(_z + 0.5, _kz, _oz) - _mh_interp(_z - 0.5, _kz, _oz)
_OL, _OF, _OP = np.array(_ref["left"]), np.array(_ref["fwd"]), np.array(_ref["pel"])

_seen = set(bpy.data.objects)
bpy.ops.import_scene.fbx(filepath=_OLD)
_old = next(o for o in bpy.data.objects if o not in _seen and o.type == "MESH")
_ome = _old.data
_hair = [i for i, m in enumerate(_ome.materials) if m and m.name.lower().startswith("hair")]
if _hair:                                           # as blender_outfit.py does, so tpose.npy's numbering holds
    _hb = bmesh.new()
    _hb.from_mesh(_ome)
    bmesh.ops.delete(_hb, geom=[f for f in _hb.faces if f.material_index in _hair], context="FACES")
    bmesh.ops.delete(_hb, geom=[v for v in _hb.verts if not v.link_faces], context="VERTS")
    _hb.to_mesh(_ome)
    _hb.free()
    _ome.update()
_on = len(_ome.vertices)
_tpo = np.load(os.path.join(_KIT, "views", "tpose.npy")).astype(float)
assert len(_tpo) == _on, "tpose.npy has %d vertices, the old skin %d" % (len(_tpo), _on)
_OR = np.array(_old.matrix_world)[:3, :3]
_oco = vget(_ome.vertices, "co", _on)
OLD = {k.name: (vget(k.data, "co", _on) - _oco) @ _OR.T for k in _ome.shape_keys.key_blocks[1:]}
_ob = bmesh.new()
_ob.from_mesh(_ome)
bmesh.ops.triangulate(_ob, faces=_ob.faces)
_OT = np.array([[v.index for v in f.verts] for f in _ob.faces])
_ob.free()

# nearest point on the old T-pose, on the same side of her (an inner thigh must not read the other one's)
_olat = (_tpo - _OP) @ _OL
_cl = _olat[_OT].mean(1)
_bv = {0: BVHTree.FromPolygons(_tpo.tolist(), _OT.tolist()),
       1: BVHTree.FromPolygons(_tpo.tolist(), _OT[_cl > -0.3].tolist()),
       -1: BVHTree.FromPolygons(_tpo.tolist(), _OT[_cl < 0.3].tolist())}
_ot = {0: _OT, 1: _OT[_cl > -0.3], -1: _OT[_cl < 0.3]}
_tri = np.zeros((_n1, 3), int)
_bw = np.zeros((_n1, 3))
_far = np.zeros(_n1)
_tpb = _tp_old_frame[BL]
_mlat = (_tpb - _OP) @ _OL
for _i in range(_n1):
    _s = 0 if abs(_mlat[_i]) < 0.6 else (1 if _mlat[_i] > 0 else -1)
    _loc, _nrm, _fi, _dist = _bv[_s].find_nearest(Vector(_tpb[_i]))
    _tri[_i] = _ot[_s][_fi]
    _bw[_i] = _bary([Vector(_tpo[j]) for j in _tri[_i]], _loc)
    _far[_i] = _dist
_arm = G("upperarm", "lowerarm", "hand", "wrist", "index_", "middle_", "ring_", "pinky_", "thumb_")[BL]
_ARM = 1.0 - _smoothstep(_arm, 0.25, 0.6)           # the T-pose turns the arms: their deltas would point wrong
print("mh morphs: old skin %d vertices, %d keys; this body sits %.2f cm from it (median), %.2f (95%%), torso only" % (
    _on, len(OLD), np.median(_far[_ARM > 0.5]), np.percentile(_far[_ARM > 0.5], 95)))
_UP = np.array([0.0, 0.0, 1.0])


def old_delta(key):
    """An old shape key on this body: per vertex in the mesh's order, UE's frame, cm."""
    # (everything per vertex here is already in the mesh's order: it was read through BL)
    d = (OLD[key][_tri] * _bw[:, :, None]).sum(1)
    out = ((d @ _OL) / _SX)[:, None] * LEFT + ((d @ _OF) / _SY)[:, None] * FWD + (d[:, 2] / _SZ)[:, None] * _UP
    return out * _ARM[:, None] * _FLIP


# ---- the morphs ----------------------------------------------------------------------------------
def variant(a, b="base"):
    """Creator variant a less variant b, per vertex (the mesh's order, UE's frame)."""
    va = np.array(_json.load(open(os.path.join(_WORK, "var_%s.json" % a)))["v"], float)
    vb = np.array(_json.load(open(os.path.join(_WORK, "var_%s.json" % b)))["v"], float)
    assert len(va) == _n1 and len(vb) == _n1, "variant %s or %s is not this mesh" % (a, b)
    return va - vb


def footprint(old):
    """1 where the old slider moves the skin, fading out round it."""
    m = np.linalg.norm(old, axis=1)
    return np.clip(_blur_u(_smoothstep(m / max(m.max(), 1e-9), 0.05, 0.30), 6), 0.0, 1.0)


_vb = np.array(_json.load(open(os.path.join(_WORK, "var_base.json")))["v"], float)
print("mh morphs: the Creator's base variant against the assembled body: %.3f cm mean, %.3f max" % (
    np.linalg.norm(_vb - _U, axis=1).mean(), np.linalg.norm(_vb - _U, axis=1).max()))

_bu = np.linalg.norm(old_delta("BustUp"), axis=1)
APEX = [int(np.argmax(np.where(_U[:, 0] * sgn > 0, _bu, 0.0))) for sgn in (1.0, -1.0)]      # the bust's two tips (her left, her right)
_near_apex = np.min([np.linalg.norm(_U - _U[a], axis=1) for a in APEX], axis=0) < 3.0

# slider morph -> (old key(s) it stands for, with the weight one slider unit puts on them; Creator variant pair or None)
SHAPE = [
    ("Bust",      {"BustUp": 1.0},                                   ("base", "bust")),
    ("BustLift",  {"BustBounceL_Up": 1.3, "BustBounceR_Up": 1.3},    ("apex", "base")),
    ("BustApart", {"BustApart": 1.0},                                ("span", "base")),
    ("Glutes",    {"GlutesUp": 1.0},                                 ("hip", "base")),
    ("Fat",       {"Fat": 1.0},                                      ("fat1", "fat0")),
    ("ThighGap",  {"ThighGap": 1.0},                                 ("base", "thigh")),
    ("AbsLines",  {"AbsLines": 1.0},                                 None),
    ("AbsPack",   {"AbsPack": 1.0},                                  None),
]
ONLY_OLD = next((a.split("=", 1)[1].split(",") for a in args if a.startswith("--mh-old=")), ())    # --mh-old=Fat,Glutes: these from the old keys
BY_APEX = ("BustApart", "BustLift")
BY_PEAK = ("Fat",)
SPAN_IN = 2.0       # cm each bust tip moves toward the other at Bust spacing -1
MIN_COS = 0.35      # under this the variant is not the slider's shape: the old key is used instead
MORPHS = {}
for _name, _keys, _pair in SHAPE:
    _oldd = sum(old_delta(k) * w for k, w in _keys.items())
    _src = "old shape key"
    _d = _oldd * SEAM[:, None]
    if _pair and _name not in ONLY_OLD:
        _v = variant(*_pair) * (footprint(_oldd) * SEAM)[:, None]
        _dot, _vv, _oo = float((_oldd * _v).sum()), float((_v * _v).sum()), float((_oldd * _oldd).sum())
        _cos = _dot / max((_vv * _oo) ** 0.5, 1e-9)
        if abs(_cos) >= MIN_COS:
            # sized so the slider moves as much skin as it does today (the same root mean square);
            # a plain least-squares fit would shrink it by the cosine
            _k = (1.0 if _cos > 0 else -1.0) * (_oo / _vv) ** 0.5
            if _name in BY_PEAK:
                # as deep at its deepest as today's: a real body spreads fat wider than the old key's belly,
                # and sized by the mean it hardly showed
                _k *= float(np.linalg.norm(_oldd, axis=1).max() / (np.linalg.norm(_v, axis=1).max() * abs(_k)))
            if _name in BY_APEX:
                # the bust's tips travel as far as today's: the old key carves a cleavage between the breasts
                # (most of its movement), a real body moves each breast whole, and matched over the whole
                # footprint the two breasts ran into each other at the slider's end
                _k = float((_oldd[_near_apex] * _v[_near_apex]).sum() / (_v[_near_apex] * _v[_near_apex]).sum())
            _d = _v * _k
            _src = "Creator %s - %s, x%.2f, cos %.2f" % (_pair[0], _pair[1], _k, _cos)
        else:
            _src = "old shape key (Creator %s - %s reads differently: cos %.2f)" % (_pair[0], _pair[1], _cos)
    if _name == "BustApart" and _src.startswith("Creator"):
        # At today's travel (each tip 2.3 cm per unit, 5.6 cm inward at the slider's minimum) two real
        # breasts run through each other and come out as one cone. Sized so the minimum (-2.47 units at
        # the default bust: UVisexMHBodyComponent::ApplyMorphs) brings each tip SPAN_IN cm in, where
        # they meet. The slider's range is unchanged; its reach is about a third of the old body's.
        _d = _d * (SPAN_IN / (float(np.abs(_d[APEX][:, 0]).mean()) * 2.47))
        _src += ", then sized to %.1f cm in at the minimum" % SPAN_IN
        _m = np.linalg.norm(_d, axis=1)
        print("mh morph %-10s resized: %.2f cm at most" % (_name, _m.max()))
    MORPHS[_name] = _d
    if _name.startswith("Bust"):
        print("   at the bust's tips (x left, y front, z up; cm per unit): old %s, this %s" % (
            _oldd[APEX].round(2).tolist(), _d[APEX].round(2).tolist()))
    _m = np.linalg.norm(_d, axis=1)
    print("mh morph %-10s %-58s %5d vertices, %.2f cm at most at %s (old key: %.2f)" % (
        _name, _src, int((_m > 0.003).sum()), _m.max(), _U[_m.argmax()].round(1).tolist(), np.linalg.norm(_oldd, axis=1).max()))
for _k in sorted(OLD):
    if "Bounce" in _k or "Sway" in _k:
        MORPHS[_k] = _blur_u(old_delta(_k), 1) * SEAM[:, None]
        _m = np.linalg.norm(MORPHS[_k], axis=1)
        print("mh morph %-18s old shape key  %5d vertices, %.2f cm at most" % (_k, int((_m > 0.003).sum()), _m.max()))

_out = {"vertices": int(_n1), "base": np.round(_U, 3).tolist(), "check": [[int(i)] + _U[i].round(3).tolist() for i in range(0, _n1, 97)], "morphs": {}}
for _k, _d in MORPHS.items():
    _i = np.nonzero(np.linalg.norm(_d, axis=1) > 0.003)[0]
    _out["morphs"][_k] = {"i": _i.tolist(), "d": np.round(_d[_i], 4).ravel().tolist()}
os.makedirs(os.path.dirname(_OUT), exist_ok=True)
_json.dump(_out, open(_OUT, "w"), separators=(",", ":"))
print("mh morphs: wrote %d morphs to %s (%.1f MB)" % (len(MORPHS), _OUT, os.path.getsize(_OUT) / 1e6))
