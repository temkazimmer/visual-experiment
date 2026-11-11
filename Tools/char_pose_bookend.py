"""Bookend kick (bundle 03, Tema's tartan references 2026-11-11), a one-pose AnimSequence. Idempotent.

  python3 Tools/ue.py Tools/char_pose_bookend.py     # builds the asset, then shoots it in Tartan

The FK kit is char_pose_peace.py's (mesh space: X = her left, Y = forward, Z = up). All her
weight on the left leg, pelvis tipped forward so the small of the back arches and the seat
sits back; the right knee folded right back, shin rising behind her, foot pointed, sole up.
Chest lifted and turned to her left, chin over the left shoulder; left fingertips on the
tie knot, right hand resting on the front of the hip.
Globals: BUILD=False to only shoot, SHOTS=["front","side","back","face","threeq"] or [],
OUTFIT="Tartan" (or "" for none) for the shots.
"""
import math
import os
import shutil
import unreal

IDLE = "/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle"
OUT_DIR, OUT_NAME = "/Game/Characters/Poses", "A_Pose_Bookend"
BP_PATH = "/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"
BUILD = globals().get("BUILD", True)
SHOTS = globals().get("SHOTS", ["threeq", "side", "front", "back", "face"])
OUTFIT = globals().get("OUTFIT", "Tartan")

if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise RuntimeError("Play mode is running: stop PIE first")

# ---- small vector / quaternion kit (x, y, z, w), same algebra as FQuat ----
X, Y, Z = (1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0)
def add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def mul(a, s): return (a[0] * s, a[1] * s, a[2] * s)
def neg(a): return mul(a, -1.0)
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def length(a): return math.sqrt(dot(a, a))
def norm(a):
    n = length(a)
    return mul(a, 1.0 / n) if n > 1e-9 else (0.0, 0.0, 0.0)
def perp(a, n): return sub(a, mul(n, dot(a, n)))
def qnorm(q):
    n = math.sqrt(sum(c * c for c in q))
    return tuple(c / n for c in q)
def qmul(a, b):  # applies b first, then a
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)
def qinv(q): return (-q[0], -q[1], -q[2], q[3])
def rot(q, v):
    u = (q[0], q[1], q[2])
    t = mul(cross(u, v), 2.0)
    return add(add(v, mul(t, q[3])), cross(u, t))
def axis_angle(axis, deg):
    h = math.radians(deg) * 0.5
    a = norm(axis)
    return (a[0] * math.sin(h), a[1] * math.sin(h), a[2] * math.sin(h), math.cos(h))
def between(a, b):
    a, b = norm(a), norm(b)
    d = dot(a, b)
    if d < -0.9999:
        return axis_angle(cross(a, X) if abs(a[0]) < 0.9 else cross(a, Y), 180.0)
    c = cross(a, b)
    return qnorm((c[0], c[1], c[2], 1.0 + d))
def turn(a, b, deg):
    """Rotation that swings direction a toward direction b by deg."""
    return axis_angle(cross(a, b), deg)
def align2(p0, s0, p1, s1):
    """Rotation taking direction p0 onto p1, then twisting about p1 so s0 lines up with s1."""
    q1 = between(p0, p1)
    p1 = norm(p1)
    a, b = norm(perp(rot(q1, s0), p1)), norm(perp(s1, p1))
    q2 = axis_angle(p1, 180.0) if dot(a, b) < -0.9999 else between(a, b)
    return qmul(q2, q1)

# ---- read the idle pose ----
idle = unreal.load_asset(IDLE)
APE = unreal.AnimPoseExtensions
src = APE.get_anim_pose_at_frame(idle, 0, unreal.AnimPoseEvaluationOptions())
bones = [str(n) for n in APE.get_bone_names(src)]
local, parent = {}, {}
for b in bones:
    t = APE.get_bone_pose(src, b, unreal.AnimPoseSpaces.LOCAL)
    r = t.rotation
    local[b] = [(t.translation.x, t.translation.y, t.translation.z), qnorm((r.x, r.y, r.z, r.w)),
                (t.scale3d.x, t.scale3d.y, t.scale3d.z)]
    path = [str(n) for n in unreal.AnimationLibrary.find_bone_path_to_root(idle, b)]
    parent[b] = path[1] if len(path) > 1 else None

_cache = {}
def world(b):
    if b is None:
        return ((0.0, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0))
    if b not in _cache:
        pp, pq = world(parent[b])
        _cache[b] = (add(pp, rot(pq, local[b][0])), qnorm(qmul(pq, local[b][1])))
    return _cache[b]
def pos(b): return world(b)[0]
def set_world_rot(b, q):
    local[b][1] = qnorm(qmul(qinv(world(parent[b])[1]), q))
    _cache.clear()
def rotate(b, q): set_world_rot(b, qmul(q, world(b)[1]))
def translate(b, d):
    local[b][0] = add(local[b][0], rot(qinv(world(parent[b])[1]), d))
    _cache.clear()
def aim(b, child, direction):
    rotate(b, between(sub(pos(child), pos(b)), direction))
def aim_at(b, child, point): aim(b, child, sub(point, pos(b)))

worst = max(length(sub(pos(b), (lambda v: (v.x, v.y, v.z))(APE.get_bone_pose(src, b, unreal.AnimPoseSpaces.WORLD).translation)))
            for b in bones)
print("FK check, worst error cm:", round(worst, 3))

# The toes come from Quinn's own reference, not the idle's: MM_Idle has no toe tracks, so it
# evaluates them against the skeleton's copy, which lags when char_toes.py moves Quinn's toe
# bones onto a re-weighted skin (character-toes e55d62a: up to 0.8 cm off). Every clip keys
# them, so a stale copy would pull her toes off the skin's joints in every pose.
_quinn = unreal.SkeletonModifier()
_quinn.set_skeletal_mesh(unreal.load_asset("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple"))
for b in bones:
    if "toe_0" in b:
        t = _quinn.get_bone_transform(b, False)
        local[b] = [(t.translation.x, t.translation.y, t.translation.z),
                    qnorm((t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w)),
                    (t.scale3d.x, t.scale3d.y, t.scale3d.z)]
_cache.clear()

# ---- hand helpers ----
def palm_normal_local(side):
    """Palm-facing direction in the hand's own frame, read off the idle pose (palms face the thighs)."""
    h = "hand_" + side
    n = norm(cross(sub(pos("index_01_" + side), pos(h)), sub(pos("pinky_01_" + side), pos(h))))
    if dot(n, sub(pos("pelvis"), pos(h))) < 0:
        n = neg(n)
    return rot(qinv(world(h)[1]), n)
PALM = {s: palm_normal_local(s) for s in "lr"}
def palm(side): return rot(world("hand_" + side)[1], PALM[side])
def fingers(side): return norm(sub(pos("middle_01_" + side), pos("hand_" + side)))
def across(side):  # index toward pinky, square to the fingers
    return norm(perp(sub(pos("pinky_01_" + side), pos("index_01_" + side)), fingers(side)))
def set_hand(side, finger_dir, palm_dir):
    rotate("hand_" + side, align2(fingers(side), palm(side), finger_dir, palm_dir))
def curl(side, finger, degs):
    """Bend each joint toward the palm by degs (01, 02, 03)."""
    for i, deg in enumerate(degs):
        b = "%s_0%d_%s" % (finger, i + 1, side)
        axis = cross(fingers(side), palm(side))
        rotate(b, axis_angle(axis, deg))
def straighten(side, finger, spread_deg):
    """Lay the finger out along the hand, fanned sideways by spread_deg (+ toward the pinky)."""
    d = rot(axis_angle(palm(side), 1.0), fingers(side))
    sign = 1.0 if dot(sub(d, fingers(side)), across(side)) > 0 else -1.0
    d = rot(axis_angle(palm(side), sign * spread_deg), fingers(side))
    aim("%s_01_%s" % (finger, side), "%s_02_%s" % (finger, side), d)
    aim("%s_02_%s" % (finger, side), "%s_03_%s" % (finger, side), d)
    set_world_rot("%s_03_%s" % (finger, side), world("%s_02_%s" % (finger, side))[1])
def reach(side, wrist, pole):
    """Two-bone arm: put the wrist at a point, elbow toward pole."""
    up, lo, h = "upperarm_" + side, "lowerarm_" + side, "hand_" + side
    s = pos(up)
    l1, l2 = length(sub(pos(lo), s)), length(sub(pos(h), pos(lo)))
    to = sub(wrist, s)
    d = min(length(to), (l1 + l2) * 0.999)
    u = norm(to)
    a = (l1 * l1 - l2 * l2 + d * d) / (2 * d)
    hgt = math.sqrt(max(l1 * l1 - a * a, 0.0))
    elbow = add(add(s, mul(u, a)), mul(norm(perp(pole, u)), hgt))
    n0 = hinge(up)                                          # the elbow's own axis, not the current bend plane
    n1 = cross(sub(elbow, s), sub(add(s, mul(u, d)), elbow))
    rotate(up, align2(sub(pos(lo), s), n0, sub(elbow, s), n1))
    aim_at(lo, h, add(s, mul(u, d)))
def aim_hinged(up, lo, end, lo_dir):
    """Point `lo` along lo_dir by bending about the joint's own hinge: `up` first turns about its
    axis so the hinge is normal to the bend plane, then `lo` aims. A knee turned in this way is hip
    rotation, not a sideways knee (Peace's free leg read 36 deg of knee valgus in the limits audit)."""
    u = norm(sub(pos(lo), pos(up)))
    n1 = norm(cross(u, norm(lo_dir)))
    h = norm(perp(hinge(up), u))
    ang = math.degrees(math.atan2(dot(cross(h, n1), u), dot(h, n1)))
    rotate(up, axis_angle(u, ang))
    aim(lo, end, lo_dir)
def hinge(up):
    """The joint axis below `up` (elbow, knee), in world space, as the idle pose has it. Two-bone IK
    turns the upper bone so this axis is normal to the new bend plane: the lower bone then bends
    about the real hinge instead of sideways (a knee bent 36 deg sideways showed in the limits audit)."""
    return rot(world(up)[1], HINGE_LOCAL[up])
HINGE_LOCAL = {}
for _up, _lo, _end in (("upperarm_l", "lowerarm_l", "hand_l"), ("upperarm_r", "lowerarm_r", "hand_r"),
                       ("thigh_l", "calf_l", "foot_l"), ("thigh_r", "calf_r", "foot_r")):
    HINGE_LOCAL[_up] = rot(qinv(world(_up)[1]), norm(cross(sub(pos(_lo), pos(_up)), sub(pos(_end), pos(_lo)))))
def share_twist(side):
    """Spread the wrist's twist along the forearm so the skin doesn't candy-wrap. First, what the
    wrist can't carry goes to the forearm bone: the hand turns 35 deg one way and 135 the other off
    the reference (the forearm's 160 of pronation/supination, VisexAnimInstance's limits), and the
    mocap turns the lowerarm bone itself up to 60. The forearm turns about its own axis and the hand
    is put back where it was, so nothing moves but the forearm's skin (menu lean: 174 -> 125)."""
    h, lo = "hand_" + side, "lowerarm_" + side
    for b in (h, lo):
        if b not in REF_LOCAL:
            r = unreal.VisexAnimInstanceLibrary.ref_pose_local_rotation(idle.get_editor_property("skeleton"), b)
            REF_LOCAL[b] = (r.x, r.y, r.z, r.w)
    up = "upperarm_" + side
    hand_twist = lambda: twist_x(qmul(qinv(REF_LOCAL[h]), local[h][1]))
    flip = 1.0
    for _ in range(4):
        t = hand_twist()
        e = t - min(max(t, HAND_TWIST[0] + 10.0), HAND_TWIST[1] - 10.0)
        if abs(e) < 0.5:
            break
        keep = world(h)[1]
        lt = twist_x(qmul(qinv(REF_LOCAL[lo]), local[lo][1]))
        give = min(max(lt + e, -LOWERARM_TWIST), LOWERARM_TWIST) - lt     # what the forearm can take
        if abs(give) > 0.01:
            rotate(lo, axis_angle(rot(world(lo)[1], X), give))
            set_world_rot(h, keep)
        rest = e - give
        if abs(rest) < 0.5:
            break
        # what's left turns the whole arm about the shoulder-to-wrist line (the elbow swings, the
        # wrist and the hand stay), which is what a shoulder does when a wrist runs out of turn
        axis = norm(sub(pos(h), pos(up)))
        before = abs(hand_twist())
        rotate(up, axis_angle(axis, -flip * rest))
        set_world_rot(h, keep)
        if abs(hand_twist()) > before:                      # wrong way round: undo and go the other way
            rotate(up, axis_angle(axis, 2.0 * flip * rest))
            set_world_rot(h, keep)
            flip = -flip
    delta = qmul(local[h][1], qinv(IDLE_LOCAL[h]))          # in the forearm's frame
    axis = norm(local[h][0])                                # forearm's long axis
    p = dot((delta[0], delta[1], delta[2]), axis)
    tw = qnorm((axis[0] * p, axis[1] * p, axis[2] * p, delta[3]))
    if tw[3] < 0:
        tw = tuple(-c for c in tw)
    ang = 2.0 * math.degrees(math.atan2(math.sqrt(tw[0] ** 2 + tw[1] ** 2 + tw[2] ** 2), tw[3]))
    sgn = 1.0 if dot((tw[0], tw[1], tw[2]), axis) >= 0 else -1.0
    for b, share in (("lowerarm_twist_01_" + side, 0.7), ("lowerarm_twist_02_" + side, 0.35)):
        local[b][1] = qnorm(qmul(axis_angle(axis, sgn * ang * share), IDLE_LOCAL[b]))
    _cache.clear()

IDLE_LOCAL = {b: local[b][1] for b in bones}
def toe_dir0(s): return norm(sub(pos("ball_" + s), pos("foot_" + s)))
SOLE0 = {s: rot(qinv(world("foot_" + s)[1]), norm(perp(neg(Z), toe_dir0(s)))) for s in "lr"}   # the sole's normal, in the foot
def set_foot(s, toe, sole):
    """Point the foot's toes along `toe` with the sole facing `sole`: the ankle's roll is set, not
    left to a minimal rotation (Peace's kicked-back foot came out rolled 71 deg)."""
    f = "foot_" + s
    rotate(f, align2(toe_dir0(s), rot(world(f)[1], SOLE0[s]), norm(toe), norm(perp(sole, norm(toe)))))
REF_LOCAL = {}                                              # the skeleton's reference pose, read on demand
HAND_TWIST, LOWERARM_TWIST = (-35.0, 135.0), 75.0
def twist_x(q):
    """Turn about the bone's own X axis, as the C++ limits read it (swing-twist, degrees)."""
    if q[3] < 0:
        q = tuple(-c for c in q)
    return 2.0 * math.degrees(math.atan2(q[0], q[3]))
foot_l_at, foot_l_rot = pos("foot_l"), world("foot_l")[1]
head_rot0 = world("head")[1]

# ---- the pose ----
# hips: weight over the left foot, left hip up and out, tipped forward (the arch), seat back
rotate("pelvis", turn(X, Z, 7.0))
rotate("pelvis", turn(Z, Y, 11.0))
translate("pelvis", (4.5, -2.0, 0.0))
aim_at("thigh_l", "foot_l", foot_l_at)                      # standing leg stays planted
translate("pelvis", (0.0, 0.0, foot_l_at[2] - pos("foot_l")[2]))
aim_at("thigh_l", "foot_l", foot_l_at)
set_world_rot("foot_l", foot_l_rot)
# free leg: knee down beside the standing one, shin folded back and up, foot pointed
aim("thigh_r", "calf_r", (0.14, 0.16, -1.0))
aim_hinged("thigh_r", "calf_r", "foot_r", (-0.06, -1.0, 0.42))
set_foot("r", (0.0, -0.93, -0.30), (0.0, -0.32, 0.95))         # toes back and a little down, sole up

# spine: undo the tilt higher up (an arch, not a stoop), lift and turn the chest to her left
for b in ("spine_01", "spine_02", "spine_03", "spine_04", "spine_05"):
    rotate(b, turn(Z, neg(Y), 14.0 / 5))
    rotate(b, turn(X, neg(Z), 11.0 / 5))
    rotate(b, turn(Y, X, 14.0 / 5))
# head: chin over the left shoulder, tipped a little toward it, chin up a touch
for b, share in (("neck_01", 0.25), ("neck_02", 0.25), ("head", 0.5)):
    rotate(b, turn(Y, X, 26.0 * share))
    rotate(b, turn(Z, X, 7.0 * share))
    rotate(b, turn(Y, Z, 4.0 * share))
head_turn = qmul(world("head")[1], qinv(head_rot0))
chest = world("spine_05")[1]

# left hand: fingertips on the tie knot, elbow down and out
knot = add(pos("neck_01"), rot(chest, (-1.0, 10.0, -13.0)))     # fingers on the tie, a hand below the knot
reach("l", add(knot, rot(chest, (5.5, 3.5, -5.0))), (0.8, 0.2, -1.0))
set_hand("l", rot(chest, (-0.75, 0.15, 0.65)), rot(chest, (0.0, -1.0, 0.0)))
share_twist("l")
curl("l", "index", (10.0, 20.0, 10.0))
curl("l", "middle", (18.0, 28.0, 14.0))
curl("l", "ring", (26.0, 34.0, 18.0))
curl("l", "pinky", (32.0, 38.0, 20.0))

# right hand: resting flat on the front of the hip, elbow back
hip = add(pos("thigh_r"), (-9.0, 4.0, 9.0))                 # the outside of the hip bone
reach("r", add(hip, (-1.5, 1.5, 6.0)), (-0.8, -0.7, 0.0))
set_hand("r", (0.35, 0.45, -1.0), (0.85, -0.35, 0.0))
share_twist("r")
for f, s in (("index", -5.0), ("middle", 0.0), ("ring", 4.0), ("pinky", 8.0)):
    straighten("r", f, s)
curl("r", "index", (6.0, 8.0, 4.0))
curl("r", "middle", (8.0, 10.0, 5.0))

# ---- write the asset ----
path = OUT_DIR + "/" + OUT_NAME
if BUILD:
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        seq = unreal.load_asset(path)
    else:
        factory = unreal.AnimSequenceFactory()
        factory.set_editor_property("target_skeleton", idle.get_editor_property("skeleton"))
        seq = unreal.AssetToolsHelpers.get_asset_tools().create_asset(OUT_NAME, OUT_DIR, unreal.AnimSequence, factory)
    # ik_foot_l/r on her own feet: the Anim Blueprint's foot IK solves the legs to those bones,
    # so left at the idle's stance they pulled her feet back to it (as write() does for the clips)
    if "ik_foot_root" in local:
        RP, RQ = world("ik_foot_root")
        for s in "lr":
            FP, FQ = world("foot_" + s)
            local["ik_foot_" + s] = [rot(qinv(RQ), sub(FP, RP)), qnorm(qmul(qinv(RQ), FQ)), local["ik_foot_" + s][2]]
        _cache.clear()
    c = seq.controller
    c.open_bracket(unreal.Text("Bookend pose"), False)
    c.remove_all_bone_tracks(False)
    c.set_frame_rate(unreal.FrameRate(30, 1), False)
    c.set_number_of_frames(unreal.FrameNumber(1), False)
    for b in bones:
        p, q, s = local[b]
        c.add_bone_curve(b, False)
        c.set_bone_track_keys(b, [unreal.Vector(*p)] * 2, [unreal.Quat(*q)] * 2, [unreal.Vector(*s)] * 2, False)
    c.close_bracket(False)
    unreal.EditorAssetLibrary.save_loaded_asset(seq, only_if_is_dirty=False)
    back = APE.get_anim_pose_at_frame(seq, 0, unreal.AnimPoseEvaluationOptions())
    got = APE.get_bone_pose(back, "hand_r", unreal.AnimPoseSpaces.WORLD).translation
    print("saved", path, "hand_r read back", [round(v, 1) for v in (got.x, got.y, got.z)],
          "wanted", [round(v, 1) for v in pos("hand_r")])

# ---- shoot it on a temporary character ----
if SHOTS:
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    by = {a.get_actor_label(): a for a in eas.get_all_level_actors()}
    stand = by["MenuModel"]
    root = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools")
    capture_src = open(os.path.join(root, "capture.py")).read()
    bp = unreal.load_asset(BP_PATH)
    hh = unreal.get_default_object(bp.generated_class()).get_editor_property("capsule_component").get_unscaled_capsule_half_height()
    loc = stand.get_actor_location() + unreal.Vector(0, 0, hh)
    char = eas.spawn_actor_from_class(bp.generated_class(), loc,
                                      unreal.Rotator(roll=0, pitch=0, yaw=stand.get_actor_rotation().yaw + 90.0))
    for comp in char.get_components_by_class(unreal.SkeletalMeshComponent):
        if comp.get_class().get_name() != "VisexShellComponent":
            body = comp
            comp.override_animation_data(unreal.load_asset(path))
    dress = None
    if OUTFIT:
        dress = eas.spawn_actor_from_class(unreal.SkeletalMeshActor, body.get_world_location(), body.get_world_rotation())
        dc = dress.skeletal_mesh_component
        dc.set_skinned_asset_and_update(unreal.load_asset("/Game/Character/Clothes/SKM_Outfit_%s" % OUTFIT))
        dc.set_collision_profile_name("NoCollision")
        dc.set_leader_pose_component(body, True)      # pose and morphs follow her body, as in the game
        dc.set_editor_property("bounds_scale", 3.0)
        body.set_editor_property("bounds_scale", 3.0)
        dress.attach_to_component(body, "", unreal.AttachmentRule.SNAP_TO_TARGET, unreal.AttachmentRule.SNAP_TO_TARGET,
                                  unreal.AttachmentRule.SNAP_TO_TARGET, False)
        # the outfit is cut on her default body: give it the body's morph values, or the cups sit inside her bust
        for mt in dc.skeletal_mesh_asset.get_editor_property("morph_targets"):
            n = mt.get_name()
            dc.set_morph_target(n, body.get_morph_target(n), True)
        print("dress morphs", {mt.get_name(): round(body.get_morph_target(mt.get_name()), 2)
                               for mt in dc.skeletal_mesh_asset.get_editor_property("morph_targets") if body.get_morph_target(mt.get_name())})
    was_hidden = stand.get_editor_property("hidden")
    stand.set_actor_hidden_in_game(True)
    stand.set_is_temporarily_hidden_in_editor(True)
    f, r, up = char.get_actor_forward_vector(), char.get_actor_right_vector(), unreal.Vector(0, 0, 1)
    to_world = body.get_world_transform()
    def at(bone): return to_world.transform_location(unreal.Vector(*pos(bone)))
    mid, head, hand = at("spine_01"), at("head") + up * 8, at("middle_01_r")
    views = {
        "front": (mid + f * 560 - up * 15, mid - up * 12, 30.0),
        "side": (mid + f * 400 + r * 400 - up * 15, mid - up * 12, 30.0),
        "back": (mid - f * 560 - up * 15, mid - up * 12, 30.0),
        "face": (head + f * 150 + up * 4, head - up * 6, 30.0),
        "hand": (hand + f * 60, hand, 30.0),
        "threeq": (mid + f * 470 + r * -300 - up * 10, mid - up * 10, 30.0),
    }
    try:
        for shot in SHOTS:
            cam, tgt, fov = views[shot]
            g = {"__name__": "__bridge__", "VIEW": ((cam.x, cam.y, cam.z), (tgt.x, tgt.y, tgt.z), fov)}
            exec(compile(capture_src, "capture.py", "exec"), g)
            dst = os.path.join(root, ".bridge", "bookend_%s.png" % shot)
            shutil.move(os.path.join(root, ".bridge", "capture.png"), dst)
            print("shot", dst)
    finally:
        if dress:
            eas.destroy_actor(dress)
        eas.destroy_actor(char)
        stand.set_actor_hidden_in_game(was_hidden)
        stand.set_is_temporarily_hidden_in_editor(False)
