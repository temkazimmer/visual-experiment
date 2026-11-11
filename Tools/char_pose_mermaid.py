"""Mermaid sit with a peace sign: she kneels, slides her hips to her left onto the ground,
props herself on the left hand and holds a V beside the right eye. Idempotent.

  python3 Tools/ue.py Tools/char_pose_mermaid.py     # builds three assets, then shoots a strip

Writes /Game/Characters/Poses/A_Pose_Mermaid (the held pose), A_Pose_Mermaid_In (stand to
sit) and A_Pose_Mermaid_Out (sit to stand). The pose wheel plays _In, holds its last frame,
and plays _Out when she lets go (VisexPoseWheel.cpp).

Every frame is solved, not blended: a handful of key poses hold numbers in mesh space
(X = her left, Y = forward, Z = up, ground at Z = 0), a spline runs through them, and each
frame gets its legs from two-bone IK, so a planted foot or knee stays planted.
The maths kit and the hand helpers come from char_pose_peace.py.
Globals: BUILD=False to only shoot, SHOTS=[seconds into _In] or [] to skip.
"""
import math
import os
import shutil
import unreal

BUILD = globals().get("BUILD", True)
SHOTS = globals().get("SHOTS", [0.0, 0.45, 0.8, 1.15, 1.5, 1.9])
TOOLS = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools")
kit = open(os.path.join(TOOLS, "char_pose_peace.py")).read().split("# ---- the pose ----")[0]
exec(compile(kit, "char_pose_peace.py", "exec"), globals())

OUT_NAME = "A_Pose_Mermaid"
FPS = 30
BASE = {b: (local[b][0], local[b][1], local[b][2]) for b in bones}

def reset():
    for b in bones:
        local[b] = list(BASE[b])
    _cache.clear()

def slerp(a, b, t):
    d = sum(x * y for x, y in zip(a, b))
    if d < 0:
        b, d = tuple(-c for c in b), -d
    if d > 0.9995:
        return qnorm(tuple(x + (y - x) * t for x, y in zip(a, b)))
    th = math.acos(d)
    return qnorm(tuple((x * math.sin((1 - t) * th) + y * math.sin(t * th)) / math.sin(th) for x, y in zip(a, b)))

def limb(up, lo, end, target, pole_pt):
    """Two-bone chain: put `end` at target, the middle joint toward pole_pt."""
    s = pos(up)
    l1, l2 = length(sub(pos(lo), s)), length(sub(pos(end), pos(lo)))
    to = sub(target, s)
    d = max(min(length(to), (l1 + l2) * 0.999), abs(l1 - l2) * 1.01)
    u = norm(to)
    a = (l1 * l1 - l2 * l2 + d * d) / (2 * d)
    hgt = math.sqrt(max(l1 * l1 - a * a, 0.0))
    joint = add(add(s, mul(u, a)), mul(norm(perp(sub(pole_pt, s), u)), hgt))
    n0 = hinge(up)                                          # the knee's own axis (char_pose_peace.py)
    n1 = cross(sub(joint, s), sub(add(s, mul(u, d)), joint))
    rotate(up, align2(sub(pos(lo), s), n0, sub(joint, s), n1))
    aim_at(lo, end, add(s, mul(u, d)))

FOOT = {s: length(sub(pos("ball_" + s), pos("foot_" + s))) for s in "lr"}
def toe_dir(s): return norm(sub(pos("ball_" + s), pos("foot_" + s)))
SOLE = {s: rot(qinv(world("foot_" + s)[1]), norm(perp(neg(Z), toe_dir(s)))) for s in "lr"}
ARM = [b for b in bones if any(p in ("clavicle_l", "clavicle_r") for p in
                               [str(n) for n in unreal.AnimationLibrary.find_bone_path_to_root(idle, b)][1:])]
I = {b: pos(b) for b in ("pelvis", "calf_l", "calf_r", "ball_l", "ball_r")}
I_TOE = {s: toe_dir(s) for s in "lr"}
I_SOLE = {s: norm(perp(neg(Z), I_TOE[s])) for s in "lr"}

def ball_from(side, ankle, toe): return add(ankle, mul(norm(toe), FOOT[side]))

# ---- the ankle's range ----
# The ankle hinges about an axis fixed in the shin. Taken from the rest pose, it lets a foot
# be measured as real joints are: flex (0 = the rest angle, + = pointed), side (toes turned
# in or out of the hinge's plane) and twist (sole rolled about the toes).
def _shin(s): return norm(sub(pos("foot_" + s), pos("calf_" + s)))
HINGE = {s: rot(qinv(world("calf_" + s)[1]), norm(cross(_shin(s), toe_dir(s)))) for s in "lr"}
def ankle(s, toe=None, sole=None):
    H = rot(world("calf_" + s)[1], HINGE[s]); sh = _shin(s); F = cross(H, sh)
    toe = norm(toe or toe_dir(s)); sole = norm(sole or rot(world("foot_" + s)[1], SOLE[s]))
    tp = norm(perp(toe, H))
    flex = math.degrees(math.atan2(dot(tp, sh), dot(tp, F)))
    side = math.degrees(math.asin(max(-1.0, min(1.0, dot(toe, H)))))
    n0 = norm(cross(toe, H)); sp = norm(perp(sole, toe))
    twist = math.degrees(math.atan2(dot(cross(n0, sp), toe), dot(n0, sp)))
    return flex, side, twist
ANKLE0 = {s: ankle(s) for s in "lr"}
# how far past rest a foot may go: past this the skin folds at the back of the ankle
ANKLE_RANGE = globals().get("ANKLE_RANGE", dict(point=25.0, flex=65.0, side=18.0, twist=22.0))
def ankle_limit(s, toe, sole):
    """The nearest foot direction (toe, sole) the ankle can really reach, for the shin as solved."""
    H = rot(world("calf_" + s)[1], HINGE[s]); sh = _shin(s); F = cross(H, sh)
    flex, side, twist = ankle(s, toe, sole)
    f0, _, t0 = ANKLE0[s]; R = ANKLE_RANGE
    flex = math.radians(min(max(flex, f0 - R["flex"]), f0 + R["point"]))
    side = math.radians(min(max(side, -R["side"]), R["side"]))
    twist = math.radians(min(max(twist, t0 - R["twist"]), t0 + R["twist"]))
    toe = norm(add(mul(add(mul(F, math.cos(flex)), mul(sh, math.sin(flex))), math.cos(side)), mul(H, math.sin(side))))
    n0 = norm(cross(toe, H))
    return toe, add(mul(n0, math.cos(twist)), mul(cross(toe, n0), math.sin(twist)))

# ---- the game's joint limits ----
# VisexAnimInstance clamps every joint to human range last (FVisexAnimProxy::Limits, the same
# table and maths as here). A key that asks for more is bent back in game, and the hand or head
# ends up somewhere other than where the script put it, so poses are checked against it here.
LIMITS = {
    "thigh": (-120, 30, -45, 45, -60, 60), "calf": (-10, 130, -12, 5, -30, 20), "foot": (-30, 50, -15, 25, -25, 38),
    "upperarm": (-60, 180, -180, 45, -80, 90), "lowerarm": (-44, 111, -30, 20, -80, 80), "hand": (-80, 75, -40, 50, -35, 135),
    "neck_01": (-30, 40, -25, 25, -40, 40), "head": (-30, 25, -23, 23, -40, 40),
    "spine_01": (-15, 30, -12, 12, -13, 13), "spine_02": (-8, 20, -10, 10, -13, 13), "spine_03": (-8, 20, -10, 10, -13, 13),
    "spine_04": (-8, 20, -10, 10, -13, 13), "spine_05": (-6, 12, -6, 6, -10, 10)}
REF_ROT = {}
for _b in bones:
    _k = _b[:-2] if _b[-2:] in ("_l", "_r") else _b
    if _k in LIMITS:
        _r = _quinn.get_bone_transform(_b, False).rotation
        REF_ROT[_b] = (_k, qnorm((_r.x, _r.y, _r.z, _r.w)))

def joint_angles(b):
    """(bend, side, twist) degrees of bone b off its reference, read as the game reads them."""
    d = qmul(qinv(REF_ROT[b][1]), local[b][1])
    if d[3] < 0:
        d = tuple(-v for v in d)
    n = math.sqrt(d[0] ** 2 + d[3] ** 2)
    tw = (d[0] / n, 0.0, 0.0, d[3] / n) if n > 1e-9 else (0.0, 0.0, 0.0, 1.0)
    sw = qmul(qinv(tw), d) if REF_ROT[b][0] == "hand" else qmul(d, qinv(tw))
    if sw[3] < 0:
        sw = tuple(-v for v in sw)
    twist = math.degrees(2 * math.atan2(tw[0], tw[3]))
    twist = twist - 360 if twist > 180 else twist + 360 if twist < -180 else twist
    ang = 2 * math.acos(max(-1.0, min(1.0, sw[3])))
    s = math.sin(ang / 2)
    ax = (sw[0] / s, sw[1] / s, sw[2] / s) if s > 1e-9 else (0.0, 0.0, 0.0)
    deg = math.degrees(ang)
    return deg * ax[2], deg * ax[1], twist

def _soft_max(x, hi, s=10.0):
    return x if x <= hi - s else hi - s + s * (1.0 - math.exp(-(x - (hi - s)) / s))
def soft_clamp(x, lo, hi):
    """The game's clamp: it starts giving 10 degrees before a limit, so a joint near one is pulled in too."""
    return -_soft_max(-_soft_max(x, hi), -lo)

def over_limits(slack=0.0):
    """[(bone, degrees the game moves it, (bend, side, twist))] for every joint the game will move, worst first."""
    out = []
    for b, (k, _) in REF_ROT.items():
        a = joint_angles(b)
        r = LIMITS[k]
        ex = max(abs(a[i] - soft_clamp(a[i], r[2 * i], r[2 * i + 1])) for i in range(3))
        if ex > slack:
            out.append((b, round(ex, 1), tuple(round(v) for v in a)))
    return sorted(out, key=lambda x: -x[1])

def game_limits(kinds):
    """Clamp these joints (LIMITS keys) as the game will, so what is solved after them, the
    hands on the ground or the head in a hand, lands where the game will draw it. Returns the
    rotations it replaced, to put back before the clip is written."""
    raw = {}
    for b, (k, ref) in REF_ROT.items():
        if k not in kinds:
            continue
        a = joint_angles(b)
        r = LIMITS[k]
        n = [soft_clamp(a[i], r[2 * i], r[2 * i + 1]) for i in range(3)]
        if max(abs(x - y) for x, y in zip(a, n)) < 0.02:
            continue
        ang = math.sqrt(n[0] ** 2 + n[1] ** 2)
        sw = axis_angle((0.0, n[1] / ang, n[0] / ang), ang) if ang > 0.01 else (0.0, 0.0, 0.0, 1.0)
        tw = axis_angle(X, n[2])
        raw[b] = tuple(local[b][1])
        local[b][1] = qnorm(qmul(ref, qmul(tw, sw) if k == "hand" else qmul(sw, tw)))
    _cache.clear()
    return raw
GAME_LIMITS = globals().get("GAME_LIMITS", True)
LEGS_J, TRUNK_J, ARMS_J = ("thigh", "calf", "foot"), ("spine_01", "spine_02", "spine_03", "spine_04", "spine_05", "neck_01", "head"), ("upperarm", "lowerarm", "hand")

# ---- one frame from its numbers ----
def legs(P, lift):
    reset()
    q = qmul(turn(X, neg(Z), P["roll"]), qmul(turn(Z, neg(Y), P["pitch"]), turn(Y, neg(X), P["yaw"])))
    rotate("pelvis", q)                                    # roll: left hip down, pitch: tilt back
    translate("pelvis", sub(add(P["pelvis"], (0.0, 0.0, lift)), pos("pelvis")))
    for s in "lr":
        toe = norm(P["toe_" + s])
        limb("thigh_" + s, "calf_" + s, "foot_" + s, sub(P["ball_" + s], mul(toe, FOOT[s])), P["knee_" + s])
        f = "foot_" + s
        toe, sole = ankle_limit(s, toe, norm(P["sole_" + s]))
        rotate(f, align2(toe_dir(s), rot(world(f)[1], SOLE[s]), toe, sole))
    if GAME_LIMITS:
        # The legs as the game will bend them. A thigh past range (the mermaid's crossed left thigh,
        # 49 deg of adduction) is held back there and swings the foot ~7 cm into the sand, and the
        # contact pass then lifts her whole body, hands and all, off the ground. So the shin is
        # re-aimed at the ankle's mark from the clamped knee and the foot set again; what the clip
        # keeps is these re-aimed rotations, which the game leaves alone.
        RAW.clear()
        held = game_limits(("thigh",))
        for s in "lr":
            if "thigh_" + s in held:
                toe = norm(P["toe_" + s])
                aim_at("calf_" + s, "foot_" + s, sub(P["ball_" + s], mul(toe, FOOT[s])))
                toe, sole = ankle_limit(s, toe, norm(P["sole_" + s]))
                f = "foot_" + s
                rotate(f, align2(toe_dir(s), rot(world(f)[1], SOLE[s]), toe, sole))
        for b in held:                                     # the thigh as asked: the game clamps it, once
            RAW[b] = held[b]
        RAW.update(game_limits(("calf", "foot")))
    return min(pos("calf_l")[2], pos("calf_r")[2])

def peace_arms(P):
    """The mermaid's hands: a V beside the right eye, the left hand flat on the ground."""
    head_turn = qmul(world("head")[1], qinv(head_rot0))
    # right arm: the V beside the eye, as in char_pose_peace.py
    reach("r", add(pos("head"), rot(head_turn, (-17.0, 9.0, 0.0))), (-0.6, 0.1, -1.0))
    set_hand("r", rot(head_turn, (0.30, 0.05, 0.95)), (0.0, 1.0, 0.1))
    share_twist("r")
    straighten("r", "index", -11.0)
    straighten("r", "middle", 9.0)
    curl("r", "ring", (75.0, 95.0, 60.0))
    curl("r", "pinky", (75.0, 95.0, 60.0))
    aim_at("thumb_01_r", "thumb_02_r", add(pos("middle_01_r"), mul(palm("r"), 3.0)))
    aim_at("thumb_02_r", "thumb_03_r", add(pos("ring_02_r"), mul(palm("r"), 1.0)))
    curl("r", "thumb", (0.0, 0.0, 25.0))
    # left arm: straight down to the ground beside the hip, fingers out
    sh = pos("upperarm_l")
    reach("l", (sh[0] + P["prop"][0], sh[1] + P["prop"][1], P["prop"][2]), (0.2, -1.0, 0.2))
    set_hand("l", (0.80, 0.55, -0.12), (0.10, 0.10, -1.0))
    share_twist("l")
    for f, s in (("index", -9.0), ("middle", -2.0), ("ring", 5.0), ("pinky", 13.0)):
        straighten("l", f, s)
ARMS = peace_arms   # a pose script swaps in its own

def build(P):
    lift = 0.0
    if legs(P, 0.0) < P["floor"]:                             # a knee went under: raise the hips until it rests
        lo, hi = 0.0, 70.0
        for _ in range(14):
            lift = (lo + hi) * 0.5
            if legs(P, lift) < P["floor"]:
                lo = lift
            else:
                hi = lift
        legs(P, hi)
        lift = hi
    for b in ("spine_01", "spine_02", "spine_03", "spine_04", "spine_05"):
        rotate(b, turn(X, neg(Z), P["lean_left"] / 5))
        rotate(b, turn(Z, Y, P["lean_fwd"] / 5))
        rotate(b, turn(Y, X, P["twist_left"] / 5))
    for b, share in (("neck_01", 0.25), ("neck_02", 0.25), ("head", 0.5)):
        rotate(b, turn(Z, neg(X), P["head_tilt"] * share))  # toward the V
        rotate(b, turn(Y, neg(Z), P["head_down"] * share))
        rotate(b, turn(Y, X, P["head_left"] * share))
    if GAME_LIMITS:
        RAW.update(game_limits(TRUNK_J))                   # the trunk first: the hands are solved onto it as drawn
    w = P["arms"]
    if w > 0.001:
        ARMS(P)
        for b in ARM:
            local[b][1] = slerp(BASE[b][1], local[b][1], w)
        _cache.clear()
    if GAME_LIMITS:
        want = {s: pos("middle_01_" + s) for s in "lr"}
        OVER[:] = over_limits(0.5)
        arms = game_limits(ARMS_J)
        for s in "lr":                                     # how far the game's limits move each hand off its mark
            ARM_MISS[s] = length(sub(pos("middle_01_" + s), want[s]))
        GAME_POSE.clear()
        GAME_POSE.update({b: tuple(local[b][1]) for b in bones})
        # The clip keeps the asked-for rotations: the game clamps them itself, once. Clamped here
        # too, they would be softened twice. The arms were solved onto the clamped trunk, so in
        # game, where the trunk is clamped the same way, the hands land where they were put.
        for b, q in list(arms.items()) + list(RAW.items()):
            local[b][1] = q
        _cache.clear()
    return lift
OVER, ARM_MISS, RAW, GAME_POSE = [], {"l": 0.0, "r": 0.0}, {}, {}

# Lead and follow. Between two keys every number used to move on the same clock, which reads
# as a figure being posed. In a real move the hands and the gaze go first, the trunk next, and
# the hips settle last, so each part of a key is read a little ahead of (+) or behind (-) the
# clock. Clips are padded by PAD at both ends so the offsets stay inside the rest keys.
LEAD = {"plant": 0.30, "hip": 0.30, "prop": 0.30, "arms": 0.30,
        "head_tilt": 0.20, "head_down": 0.20, "head_left": 0.20,
        "lean_left": 0.10, "lean_fwd": 0.10, "twist_left": 0.10,
        "pelvis": -0.20, "roll": -0.20, "pitch": -0.20, "yaw": -0.20}
PAD = 0.30

def sample(keys, t):
    """Cubic Hermite through the keys, at rest at both ends, each part on its own clock."""
    ts = [k[0] for k in keys]
    def slope(name, c, j):
        if j == 0 or j == len(keys) - 1:
            return 0.0
        get = lambda k: keys[k][1][name] if c is None else keys[k][1][name][c]
        return (get(j + 1) - get(j - 1)) / (ts[j + 1] - ts[j - 1])
    def one(name, c):
        tn = min(max(t + LEAD.get(name, 0.0), 0.0), ts[-1])
        i = max(j for j in range(len(ts) - 1) if ts[j] <= tn + 1e-9)
        t0, t1 = ts[i], ts[i + 1]
        x = min(max((tn - t0) / (t1 - t0), 0.0), 1.0)
        h00, h10, h01, h11 = 2 * x ** 3 - 3 * x ** 2 + 1, x ** 3 - 2 * x ** 2 + x, -2 * x ** 3 + 3 * x ** 2, x ** 3 - x ** 2
        a = keys[i][1][name] if c is None else keys[i][1][name][c]
        b = keys[i + 1][1][name] if c is None else keys[i + 1][1][name][c]
        return h00 * a + h10 * (t1 - t0) * slope(name, c, i) + h01 * b + h11 * (t1 - t0) * slope(name, c, i + 1)
    out = {}
    for name, v in keys[0][1].items():
        out[name] = one(name, None) if isinstance(v, float) else tuple(one(name, c) for c in range(3))
    out["arms"] = min(max(out["arms"], 0.0), 1.0)
    return out

def clip(keys):
    frames = []
    for f in range(int(round((keys[-1][0] + 2 * PAD) * FPS)) + 1):
        build(sample(keys, f / FPS - PAD))
        frames.append({b: (local[b][0], local[b][1], local[b][2]) for b in bones})
    return frames


def _frame_world(fr, b):
    chain = []
    while b:
        chain.append(b)
        b = parent[b]
    P, Q = (0.0, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0)
    for x in reversed(chain):
        p, q, _ = fr[x]
        P, Q = add(P, rot(Q, p)), qnorm(qmul(Q, q))
    return P, Q


def ik_feet(fr):
    """Put ik_foot_l/r on the frame's own feet. The anim blueprint's foot IK rig solves the legs
    to those bones, so left at the idle stance they pinned her feet there whatever the clip did."""
    if "ik_foot_root" not in fr:
        return
    RP, RQ = _frame_world(fr, "ik_foot_root")
    iq = qinv(RQ)
    for s in "lr":
        FP, FQ = _frame_world(fr, "foot_" + s)
        fr["ik_foot_" + s] = (rot(iq, sub(FP, RP)), qnorm(qmul(iq, FQ)), fr["ik_foot_" + s][2])


def write(name, frames, folder=OUT_DIR, save=True):
    frames = [dict(fr) for fr in frames]
    for fr in frames:
        ik_feet(fr)
    path = folder + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        seq = unreal.load_asset(path)
    else:
        factory = unreal.AnimSequenceFactory()
        factory.set_editor_property("target_skeleton", idle.get_editor_property("skeleton"))
        seq = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.AnimSequence, factory)
    if len(frames) == 1:
        frames = frames * 2
    c = seq.controller
    c.open_bracket(unreal.Text(name), False)
    c.remove_all_bone_tracks(False)
    c.set_frame_rate(unreal.FrameRate(FPS, 1), False)
    c.set_number_of_frames(unreal.FrameNumber(len(frames) - 1), False)
    for b in bones:
        ps, qs, ss, last = [], [], [], None
        for fr in frames:
            p, q, s = fr[b]
            if last and sum(x * y for x, y in zip(last, q)) < 0:
                q = tuple(-v for v in q)                    # keep neighbours on the same side
            last = q
            ps.append(unreal.Vector(*p)); qs.append(unreal.Quat(*q)); ss.append(unreal.Vector(*s))
        c.add_bone_curve(b, False)
        c.set_bone_track_keys(b, ps, qs, ss, False)
    c.close_bracket(False)
    if save:
        unreal.EditorAssetLibrary.save_loaded_asset(seq, only_if_is_dirty=False)
        print("saved", path, "%.2f s" % seq.get_play_length())
    return seq


def shoot(prefix, frames, shots, views=("front", "side")):
    """Stills of `frames` at `shots` seconds, to Tools/.bridge/<prefix>_<view>_<i>.png."""
    if not shots:
        return
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    stand = {a.get_actor_label(): a for a in eas.get_all_level_actors()}["MenuModel"]
    capture_src = open(os.path.join(TOOLS, "capture.py")).read()
    bp = unreal.load_asset(BP_PATH)
    hh = unreal.get_default_object(bp.generated_class()).get_editor_property("capsule_component").get_unscaled_capsule_half_height()
    was_hidden = stand.get_editor_property("hidden")
    stand.set_actor_hidden_in_game(True)
    stand.set_is_temporarily_hidden_in_editor(True)
    SCRATCH = "/Game/Characters/Scratch"                    # never saved, removed below
    char, props = None, []
    try:
        for i, t in enumerate(shots):
            # nothing ticks in the editor world: a mesh shows the clip it is given first and
            # never updates after. So each moment is a one-frame scratch clip on a fresh character.
            still = write("A_Shot_%d" % i, [frames[min(int(round(t * FPS)), len(frames) - 1)]], SCRATCH, False)
            char = eas.spawn_actor_from_class(bp.generated_class(), stand.get_actor_location() + unreal.Vector(0, 0, hh),
                                              unreal.Rotator(roll=0, pitch=0, yaw=stand.get_actor_rotation().yaw + 90.0))
            body = char.get_editor_property("mesh")
            body.override_animation_data(still)
            props = []                                      # stand-ins (a column...), destroyed below
            for mesh_path, centre, axis, scale in globals().get("PROPS", []):
                xf = body.get_world_transform()
                at_w = xf.transform_location(unreal.Vector(*centre))
                ax_w = xf.transform_direction(unreal.Vector(*axis))
                prop = eas.spawn_actor_from_class(unreal.StaticMeshActor, at_w, unreal.MathLibrary.make_rot_from_z(ax_w))
                prop.static_mesh_component.set_static_mesh(unreal.load_asset(mesh_path))
                prop.set_actor_scale3d(unreal.Vector(*scale))
                props.append(prop)
            f, r, up = char.get_actor_forward_vector(), char.get_actor_right_vector(), unreal.Vector(0, 0, 1)
            ground = body.get_world_transform().transform_location(unreal.Vector(0, 0, 0))
            tgt = ground + up * 75 + f * 15
            zoom = 1.0
            if globals().get("FOCUS"):                      # a close-up on a point in mesh space
                tgt = body.get_world_transform().transform_location(unreal.Vector(*FOCUS))
                zoom = globals().get("ZOOM", 0.2)
            at = {"front": f * 640, "side": f * 450 - r * 450, "back": f * -640, "back3q": f * -450 - r * 450,
                  "left": r * -640, "right": r * 640, "front3qr": f * 450 + r * 450, "back3qr": f * -450 + r * 450,
                  "top": up * 520 + f * 160}
            views = {v: tgt + at[v] * zoom - up * (10 * zoom) for v in views}
            for name, cam in views.items():
                g = {"__name__": "__bridge__", "VIEW": ((cam.x, cam.y, cam.z), (tgt.x, tgt.y, tgt.z), 30.0)}
                exec(compile(capture_src, "capture.py", "exec"), g)
                dst = os.path.join(TOOLS, ".bridge", "%s_%s_%d.png" % (prefix, name, i))
                shutil.move(os.path.join(TOOLS, ".bridge", "capture.png"), dst)
            for prop in props:
                eas.destroy_actor(prop)
            props = []
            eas.destroy_actor(char)
            char = None
        print("shots", len(views) * len(shots))
    finally:
        if char:
            eas.destroy_actor(char)
        for prop in props:
            eas.destroy_actor(prop)
        for i in range(len(shots)):
            if unreal.EditorAssetLibrary.does_asset_exist(SCRATCH + "/A_Shot_%d" % i):
                unreal.EditorAssetLibrary.delete_asset(SCRATCH + "/A_Shot_%d" % i)
        stand.set_actor_hidden_in_game(was_hidden)
        stand.set_is_temporarily_hidden_in_editor(False)

# ---- the key poses ----
STAND = dict(pelvis=I["pelvis"], roll=0.0, pitch=0.0, yaw=0.0,
             ball_l=I["ball_l"], toe_l=I_TOE["l"], sole_l=I_SOLE["l"], knee_l=I["calf_l"],
             ball_r=I["ball_r"], toe_r=I_TOE["r"], sole_r=I_SOLE["r"], knee_r=I["calf_r"],
             lean_left=0.0, lean_fwd=0.0, twist_left=0.0, head_tilt=0.0, head_down=0.0, head_left=0.0,
             arms=0.0, prop=(14.0, 4.0, 7.0),
             floor=5.5)   # the lowest a knee bone may go; the hips rise to keep it there
TUCK, HEEL = (0.0, 0.25, -1.0), (0.0, -1.0, -0.25)          # on the ball of the foot, heel up
# The way down to the floor (and back up), shared by every floor pose. Not a squat: she steps
# back onto the ball of one foot, lowers that knee to the ground well behind her with the hips
# still high over the front heel, then brings the other knee down. Going up it runs backwards,
# the trunk leaning first (what rising people do), then the hips.
BACK = -36.0                                                  # how far the right foot steps back, cm
STEP = dict(STAND, pelvis=add(I["pelvis"], (0.0, -6.0, -8.0)), pitch=-3.0, yaw=-6.0,
            ball_r=add(I["ball_r"], (0.0, BACK, 0.0)), toe_r=TUCK, sole_r=HEEL, knee_r=(-14.0, 60.0, 30.0), knee_l=(17.0, 90.0, 50.0),
            lean_fwd=5.0, head_down=3.0)
# right knee on the ground behind her, left foot flat in front, hips over the front heel
HALF = dict(STEP, pelvis=(0.0, 1.0, 47.0), pitch=-6.0, yaw=-10.0,
            knee_r=(-15.0, 40.0, -10.0),
            lean_fwd=10.0, head_down=6.0)
# left foot drawn back beside the right: kneeling on both knees, hips above the heels
KNEEL = dict(HALF, pelvis=(2.0, 2.0, 30.0), pitch=0.0, yaw=-22.0,
             ball_r=add(I["ball_r"], (0.0, -10.0, 0.0)), knee_r=(-13.0, 70.0, -10.0),
             ball_l=(17.0, -10.0, 0.5), toe_l=TUCK, sole_l=HEEL, knee_l=(13.0, 70.0, -10.0),
             lean_fwd=4.0, head_down=2.0, arms=0.12)
# ---- the seat: mermaid ----
# hips slide off to her left and down; shins sweep to her right, feet on their sides
ANKLE_L, TOE_L = (-25.8, 11.0, 5.5), (-0.95, -0.30, 0.0)
ANKLE_R, TOE_R = (-33.3, -9.8, 5.6), (-0.70, -0.70, -0.05)
SIT = dict(KNEEL, pelvis=(18.0, -13.0, 17.0), roll=28.0, pitch=8.0, yaw=0.0,
           ball_l=ball_from("l", ANKLE_L, TOE_L), toe_l=TOE_L, sole_l=(0.3, -0.9, 0.3), knee_l=(13.0, 27.0, 5.5),
           ball_r=ball_from("r", ANKLE_R, TOE_R), toe_r=TOE_R, sole_r=(0.5, -0.6, 0.6), knee_r=(-8.0, 24.0, 9.0),
           lean_left=-16.0, lean_fwd=4.0, twist_left=8.0, head_tilt=14.0, head_down=3.0, head_left=0.0, arms=1.0, floor=0.0)
SIT.update(globals().get("TWEAK", {}))

IN_KEYS = [(0.0, STAND), (0.55, STEP), (1.25, HALF), (1.85, KNEEL), (2.65, SIT)]
OUT_KEYS = [(0.0, SIT), (0.75, KNEEL), (1.35, HALF), (2.00, STEP), (2.70, STAND)]

lift = build(SIT)
print("sit: hips lifted %.1f, knees L %s R %s, hand_l %s" % (
    lift, [round(v, 1) for v in pos("calf_l")], [round(v, 1) for v in pos("calf_r")], [round(v, 1) for v in pos("hand_l")]))
hold = [{b: (local[b][0], local[b][1], local[b][2]) for b in bones}]
frames_in = clip(IN_KEYS)
if BUILD:
    write(OUT_NAME, hold)
    write(OUT_NAME + "_In", frames_in)
    write(OUT_NAME + "_Out", clip(OUT_KEYS))
shoot("mermaid", frames_in, SHOTS)
