"""Tape-measure a skeletal mesh component: the numbers used to match the MetaHuman body
to the current one (Docs/AGENT_METAHUMAN.md, stage 1).

  python3 Tools/ue.py -c "WHO='play'; exec(open(unreal.Paths.project_dir() + 'Tools/mh_measure.py').read())"
  python3 Tools/ue.py -c "WHO='label:MH_Her'; OUT='mh'; exec(open(unreal.Paths.project_dir() + 'Tools/mh_measure.py').read())"

WHO = 'play'          the player's skin mesh in a running Play session (default sliders
                      applied by the body component and the anim instance), or
      'label:<name>'  an actor in the editor level by label or class name (first skeletal
                      component, or the one named COMP; ALSO=['Face'] adds a MetaHuman's
                      separate head to the height), or
      'asset:<path>'  a skeletal mesh asset in its reference pose (spawned, measured, removed).
Reads the posed, skinned vertices (Geometry Script), so bone scales and morphs count.
Circumferences are tape-style: the convex hull of the torso's cross-section.
All numbers in cm. With OUT='name' also writes Tools/.bridge/measure_<name>.json.
"""
import json
import math
import os
import unreal


def _hull_perimeter(pts):
    pts = sorted(set((round(x, 3), round(y, 3)) for x, y in pts))
    if len(pts) < 3:
        return 0.0
    def half(seq):
        h = []
        for p in seq:
            while len(h) >= 2 and ((h[-1][0] - h[-2][0]) * (p[1] - h[-2][1])
                                   - (h[-1][1] - h[-2][1]) * (p[0] - h[-2][0])) <= 0:
                h.pop()
            h.append(p)
        return h
    hull = half(pts)[:-1] + half(reversed(pts))[:-1]
    return sum(math.dist(hull[i], hull[(i + 1) % len(hull)]) for i in range(len(hull)))


def _slice(V, T, z):
    """Cross-section at height z: a list of segments ((x, y), (x, y))."""
    segs = []
    for a, b, c in T.get(math.floor(z), ()):
        za, zb, zc = V[a][2], V[b][2], V[c][2]
        if (za < z) == (zb < z) == (zc < z):
            continue
        p = []
        for i, j in ((a, b), (b, c), (c, a)):
            zi, zj = V[i][2], V[j][2]
            if (zi < z) != (zj < z):
                t = (z - zi) / (zj - zi)
                p.append((V[i][0] + t * (V[j][0] - V[i][0]), V[i][1] + t * (V[j][1] - V[i][1])))
        if len(p) == 2:
            segs.append((p[0], p[1]))
    return segs


def _loops(segs):
    """Group a slice's segments into connected loops (union-find on shared endpoints)."""
    parent = {}
    def find(k):
        while parent.setdefault(k, k) != k:
            parent[k] = parent[parent[k]]
            k = parent[k]
        return k
    key = lambda p: (round(p[0], 2), round(p[1], 2))
    for p, q in segs:
        parent[find(key(p))] = find(key(q))
    groups = {}
    for p, q in segs:
        groups.setdefault(find(key(p)), []).extend((p, q))
    return list(groups.values())


def girth(V, T, z, cx, reach, arms_free=False):
    """Tape around the biggest loop centred within `reach` cm of x = cx (0 if none).
    arms_free: only count heights where both arms are separate loops (below the armpits),
    so a pose with the arms down doesn't add them to the chest."""
    loops = [(_hull_perimeter(l), sum(p[0] for p in l) / len(l)) for l in _loops(_slice(V, T, z))]
    loops = [l for l in loops if l[0] > 12.0]
    if arms_free and len(loops) < 3:
        return 0.0
    near = [g for g, mx in loops if abs(mx - cx) <= reach]
    return max(near) if near else 0.0


def scan(V, T, z0, z1, cx, reach, pick, arms_free=False, step=0.25):
    """The largest (pick=max) or smallest (pick=min) girth between two heights."""
    best = None
    z = min(z0, z1)
    while z <= max(z0, z1):
        g = girth(V, T, z, cx, reach, arms_free)
        if g > 1.0 and (best is None or pick(g, best[0]) == g):
            best = (g, z)
        z += step
    return best or (0.0, z0)


def _along(V, a, b, radius, lo=-1e9):
    """Extent of the skin along the axis a->b, within `radius` of that line: (min, max)."""
    ax = [b[i] - a[i] for i in range(3)]
    n = math.sqrt(sum(c * c for c in ax))
    ax = [c / n for c in ax]
    ts = []
    for v in V:
        d = [v[i] - a[i] for i in range(3)]
        t = sum(d[i] * ax[i] for i in range(3))
        if t > lo and sum(c * c for c in d) - t * t < radius * radius:
            ts.append(t)
    return min(ts), max(ts)


def read_mesh(comp):
    """World-space posed vertices and triangles of a skeletal mesh component."""
    dm = unreal.DynamicMesh()
    opts = unreal.GeometryScriptCopyMeshFromComponentOptions()
    ok = unreal.GeometryScript_SceneUtils.copy_mesh_from_component(comp, dm, opts, True)[2]
    if ok != unreal.GeometryScriptOutcomePins.SUCCESS:
        # a Leader Pose follower has no pose of its own: read its skinned render mesh instead
        lod = unreal.GeometryScriptMeshReadLOD()
        lod.set_editor_property("lod_type", unreal.GeometryScriptLODType.RENDER_DATA)
        opts.set_editor_property("requested_lod", lod)
        unreal.GeometryScript_SceneUtils.copy_mesh_from_component(comp, dm, opts, True)
    vl = unreal.GeometryScript_MeshQueries.get_all_vertex_positions(dm, False)[1]
    tl = unreal.GeometryScript_MeshQueries.get_all_triangle_indices(dm, False)[1]
    V = [(v.x, v.y, v.z) for v in unreal.GeometryScript_List.convert_vector_list_to_array(vl)]
    T = [(t.x, t.y, t.z) for t in unreal.GeometryScript_List.convert_triangle_list_to_array(tl)]
    # Drop what isn't skin: a MetaHuman body parks ~30 collapsed vertices on the component
    # origin (2.4 cm under her soles), and unused vertex ids read as (0, 0, 0) too.
    o = comp.get_world_location()
    bad = set(i for i, v in enumerate(V) if abs(v[0] - o.x) + abs(v[1] - o.y) + abs(v[2] - o.z) < 0.01)
    T = [t for t in T if not (t[0] in bad or t[1] in bad or t[2] in bad)]
    used = set(i for t in T for i in t)
    some = V[T[0][0]]
    V = [v if i in used else some for i, v in enumerate(V)]
    return V, T


BONES = ["pelvis", "spine_01", "spine_03", "spine_05", "neck_01", "head",
         "clavicle_l", "upperarm_l", "upperarm_r", "lowerarm_l", "hand_l", "middle_01_l",
         "middle_03_l", "thigh_l", "thigh_r", "calf_l", "foot_l", "ball_l"]


def measure(comp, also=()):
    V, tris = read_mesh(comp)
    raw = {b: comp.get_socket_location(b) for b in BONES}
    # her own frame: x across (left thigh to right thigh), y front-back, z up from the floor
    ax = raw["thigh_r"] - raw["thigh_l"]
    n = math.hypot(ax.x, ax.y)
    cx, sx = ax.x / n, ax.y / n
    o = raw["pelvis"]
    floor = min(v[2] for v in V)
    def local(x, y, z):
        dx, dy = x - o.x, y - o.y
        return (dx * cx + dy * sx, -dx * sx + dy * cx, z - floor)
    V = [local(*v) for v in V]
    B = {b: local(p.x, p.y, p.z) for b, p in raw.items()}
    T = {}                                  # triangles binned by the cm of height they span
    for t in tris:
        zs = [V[i][2] for i in t]
        for k in range(math.floor(min(zs)), math.floor(max(zs)) + 1):
            T.setdefault(k, []).append(t)
    d = lambda a, b: math.dist(B[a], B[b])
    m = {"vertices": len(V), "triangles": len(tris)}
    m["height"] = max(v[2] for v in V)
    for other in also:                      # a MetaHuman's head is a separate mesh
        m["height"] = max(m["height"], max(v[2] for v in read_mesh(other)[0]) - floor)
    m["shoulder_joints"] = d("upperarm_l", "upperarm_r")
    near = [v for v in V if v[2] > B["upperarm_l"][2] - 3
            and min(math.dist(v, B["upperarm_l"]), math.dist(v, B["upperarm_r"])) < 12]
    lo, hi = _along(near, B["upperarm_l"], B["upperarm_r"], 12)
    m["shoulder_width"] = hi - lo
    # crotch: going down from the pelvis, the first height where the trunk splits into two legs
    zc = B["pelvis"][2] + 4
    while zc > B["calf_l"][2]:
        if sum(1 for l in _loops(_slice(V, T, zc)) if _hull_perimeter(l) > 30.0) >= 2:
            break
        zc -= 0.25
    m["inseam"] = zc
    top = B["upperarm_l"][2]
    m["bust"], m["bust_z"] = scan(V, T, B["spine_03"][2], top, 0, 9, max, True)
    m["waist"], m["waist_z"] = scan(V, T, B["pelvis"][2] + 2, m["bust_z"] - 6, 0, 9, min)
    m["underbust"] = scan(V, T, m["bust_z"] - 0.07 * m["height"], m["bust_z"] - 0.03 * m["height"], 0, 9, min, True)[0]
    # hip: fullest tape around the seat, from just under the hip joints up. Not down to the
    # crotch: there the tape would span the gap between the thighs in a legs-apart pose.
    m["hip"], m["hip_z"] = scan(V, T, B["thigh_l"][2] - 4, B["pelvis"][2] + 6, 0, 9, max)
    m["thigh"] = girth(V, T, zc - 3.0, B["thigh_l"][0], 7)
    m["calf"] = scan(V, T, B["foot_l"][2] + 12, B["calf_l"][2] - 4, B["calf_l"][0], 7, max)[0]
    m["neck"] = scan(V, T, B["neck_01"][2] + 1, B["head"][2] - 1, B["neck_01"][0], 6, min)[0]
    m["upper_arm_len"] = d("upperarm_l", "lowerarm_l")
    m["forearm_len"] = d("lowerarm_l", "hand_l")
    m["arm_shoulder_to_wrist"] = m["upper_arm_len"] + m["forearm_len"]
    m["hand_len"] = _along(V, B["hand_l"], B["middle_01_l"], 3.0, 0.0)[1]
    m["thigh_len"] = d("thigh_l", "calf_l")
    m["shin_len"] = d("calf_l", "foot_l")
    m["hip_joint_height"] = B["thigh_l"][2]
    m["knee_height"] = B["calf_l"][2]
    m["ankle_height"] = B["foot_l"][2]
    # straightened: what a stadiometer would read if she stood square (pose-independent legs)
    m["height_straight"] = m["ankle_height"] + m["shin_len"] + m["thigh_len"] + m["height"] - B["thigh_l"][2]
    heel = (B["foot_l"][0], B["foot_l"][1], 0.0)
    toe = (B["ball_l"][0], B["ball_l"][1], 0.0)
    low = [(v[0], v[1], 0.0) for v in V if v[2] < B["foot_l"][2]]
    lo, hi = _along(low, heel, toe, 7.0)
    m["foot_len"] = hi - lo
    m["head_and_neck"] = m["height"] - B["neck_01"][2]
    return {k: (round(v, 2) if isinstance(v, float) else v) for k, v in m.items()}


def _component(who):
    """(component, cleanup) for WHO."""
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    want = globals().get("COMP")
    if who == "play":
        w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        actor = unreal.GameplayStatics.get_player_character(w, 0)
        want = want or "Skin"
    elif who.startswith("label:"):
        actor = next(a for a in eas.get_all_level_actors()
                     if who[6:] in (a.get_actor_label(), a.get_class().get_name()))
    else:
        actor = eas.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(0, 0, 0))
        actor.skeletal_mesh_component.set_skinned_asset_and_update(unreal.load_asset(who[6:]))
        return actor.skeletal_mesh_component, lambda: eas.destroy_actor(actor)
    comps = actor.get_components_by_class(unreal.SkeletalMeshComponent)
    named = [c for c in comps if want and c.get_name() == want]
    print("components:", [(c.get_name(), c.get_skinned_asset().get_name() if c.get_skinned_asset() else None) for c in comps])
    global ALSO_COMPS
    ALSO_COMPS = [c for c in comps if c.get_name() in globals().get("ALSO", ())]
    return (named or comps)[0], lambda: None


if "WHO" in globals():
    _comp, _done = _component(WHO)
    try:
        RESULT = measure(_comp, globals().get('ALSO_COMPS', ()))
    finally:
        _done()
    print("mesh:", _comp.get_skinned_asset().get_name(), "scale", _comp.get_world_scale())
    for _k, _v in RESULT.items():
        print("  %-22s %s" % (_k, _v))
    if globals().get("OUT"):
        _p = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()),
                          "Tools", ".bridge", "measure_%s.json" % OUT)
        json.dump(RESULT, open(_p, "w"), indent=1)
        print("wrote", _p)
