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
    n0 = cross(sub(pos(lo), s), sub(pos(end), pos(lo)))
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
        rotate(f, align2(toe_dir(s), rot(world(f)[1], SOLE[s]), toe, P["sole_" + s]))
    return min(pos("calf_l")[2], pos("calf_r")[2])

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
    w = P["arms"]
    if w > 0.001:
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
        for b in ARM:
            local[b][1] = slerp(BASE[b][1], local[b][1], w)
        _cache.clear()
    return lift

# ---- the key poses ----
STAND = dict(pelvis=I["pelvis"], roll=0.0, pitch=0.0, yaw=0.0,
             ball_l=I["ball_l"], toe_l=I_TOE["l"], sole_l=I_SOLE["l"], knee_l=I["calf_l"],
             ball_r=I["ball_r"], toe_r=I_TOE["r"], sole_r=I_SOLE["r"], knee_r=I["calf_r"],
             lean_left=0.0, lean_fwd=0.0, twist_left=0.0, head_tilt=0.0, head_down=0.0, head_left=0.0,
             arms=0.0, prop=(14.0, 4.0, 7.0),
             floor=5.5)   # the lowest a knee bone may go; the hips rise to keep it there
TUCK, HEEL = (0.0, 0.25, -1.0), (0.0, -1.0, -0.25)          # on the ball of the foot, heel up
# right knee down beside the left foot, left knee still up
HALF = dict(STAND, pelvis=(0.0, 10.0, 40.0), pitch=-6.0, yaw=-10.0,
            toe_r=TUCK, sole_r=HEEL, knee_r=(-15.0, 70.0, -10.0), knee_l=(17.0, 90.0, 50.0),
            lean_fwd=10.0, head_down=6.0)
# left foot drawn back: kneeling on both knees, hips above the heels
KNEEL = dict(HALF, pelvis=(2.0, 2.0, 30.0), pitch=0.0, yaw=-22.0,
             ball_l=(17.0, -10.0, 0.5), toe_l=TUCK, sole_l=HEEL, knee_l=(13.0, 70.0, -10.0),
             lean_fwd=4.0, head_down=2.0, arms=0.12)
# hips slide off to her left and down; shins sweep to her right, feet on their sides
ANKLE_L, TOE_L = (-25.8, 11.0, 5.5), (-0.95, -0.30, 0.0)
ANKLE_R, TOE_R = (-33.3, -9.8, 5.6), (-0.70, -0.70, -0.05)
SIT = dict(KNEEL, pelvis=(18.0, -13.0, 17.0), roll=28.0, pitch=8.0, yaw=0.0,
           ball_l=ball_from("l", ANKLE_L, TOE_L), toe_l=TOE_L, sole_l=(0.3, -0.9, 0.3), knee_l=(13.0, 27.0, 5.5),
           ball_r=ball_from("r", ANKLE_R, TOE_R), toe_r=TOE_R, sole_r=(0.5, -0.6, 0.6), knee_r=(-8.0, 24.0, 9.0),
           lean_left=-16.0, lean_fwd=4.0, twist_left=8.0, head_tilt=14.0, head_down=3.0, head_left=0.0, arms=1.0, floor=0.0)
SIT.update(globals().get("TWEAK", {}))

IN_KEYS = [(0.0, STAND), (0.60, HALF), (1.10, KNEEL), (1.90, SIT)]
OUT_KEYS = [(0.0, SIT), (0.75, KNEEL), (1.25, HALF), (1.85, STAND)]

def sample(keys, t):
    """Cubic Hermite through the keys, at rest at both ends."""
    ts = [k[0] for k in keys]
    i = max(j for j in range(len(ts) - 1) if ts[j] <= t + 1e-9)
    t0, t1 = ts[i], ts[i + 1]
    x = min(max((t - t0) / (t1 - t0), 0.0), 1.0)
    h00, h10, h01, h11 = 2 * x ** 3 - 3 * x ** 2 + 1, x ** 3 - 2 * x ** 2 + x, -2 * x ** 3 + 3 * x ** 2, x ** 3 - x ** 2
    def slope(name, c, j):
        if j == 0 or j == len(keys) - 1:
            return 0.0
        get = lambda k: keys[k][1][name] if c is None else keys[k][1][name][c]
        return (get(j + 1) - get(j - 1)) / (ts[j + 1] - ts[j - 1])
    def one(name, c):
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
    for f in range(int(round(keys[-1][0] * FPS)) + 1):
        build(sample(keys, f / FPS))
        frames.append({b: (local[b][0], local[b][1], local[b][2]) for b in bones})
    return frames

lift = build(SIT)
print("sit: hips lifted %.1f, knees L %s R %s, hand_l %s" % (
    lift, [round(v, 1) for v in pos("calf_l")], [round(v, 1) for v in pos("calf_r")], [round(v, 1) for v in pos("hand_l")]))
hold = [{b: (local[b][0], local[b][1], local[b][2]) for b in bones}]

def write(name, frames, folder=OUT_DIR, save=True):
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

frames_in = clip(IN_KEYS)
if BUILD:
    write(OUT_NAME, hold)
    write(OUT_NAME + "_In", frames_in)
    write(OUT_NAME + "_Out", clip(OUT_KEYS))

# ---- shoot moments of the sit-down on temporary characters ----
if SHOTS:
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    stand = {a.get_actor_label(): a for a in eas.get_all_level_actors()}["MenuModel"]
    capture_src = open(os.path.join(TOOLS, "capture.py")).read()
    bp = unreal.load_asset(BP_PATH)
    hh = unreal.get_default_object(bp.generated_class()).get_editor_property("capsule_component").get_unscaled_capsule_half_height()
    was_hidden = stand.get_editor_property("hidden")
    stand.set_actor_hidden_in_game(True)
    stand.set_is_temporarily_hidden_in_editor(True)
    SCRATCH = "/Game/Characters/Scratch"                    # never saved, removed below
    char = None
    try:
        for i, t in enumerate(SHOTS):
            # nothing ticks in the editor world: a mesh shows the clip it is given first and
            # never updates after. So each moment is a one-frame scratch clip on a fresh character.
            still = write("A_Shot_%d" % i, [frames_in[min(int(round(t * FPS)), len(frames_in) - 1)]], SCRATCH, False)
            char = eas.spawn_actor_from_class(bp.generated_class(), stand.get_actor_location() + unreal.Vector(0, 0, hh),
                                              unreal.Rotator(roll=0, pitch=0, yaw=stand.get_actor_rotation().yaw + 90.0))
            body = char.get_editor_property("mesh")
            body.override_animation_data(still)
            f, r, up = char.get_actor_forward_vector(), char.get_actor_right_vector(), unreal.Vector(0, 0, 1)
            ground = body.get_world_transform().transform_location(unreal.Vector(0, 0, 0))
            tgt = ground + up * 75 + f * 15
            views = globals().get("VIEWS", {"front": tgt + f * 640 - up * 10, "side": tgt + f * 450 - r * 450 - up * 10})
            for name, cam in views.items():
                g = {"__name__": "__bridge__", "VIEW": ((cam.x, cam.y, cam.z), (tgt.x, tgt.y, tgt.z), 30.0)}
                exec(compile(capture_src, "capture.py", "exec"), g)
                dst = os.path.join(TOOLS, ".bridge", "mermaid_%s_%d.png" % (name, i))
                shutil.move(os.path.join(TOOLS, ".bridge", "capture.png"), dst)
            eas.destroy_actor(char)
            char = None
        print("shots", len(views) * len(SHOTS))
    finally:
        if char:
            eas.destroy_actor(char)
        for i in range(len(SHOTS)):
            if unreal.EditorAssetLibrary.does_asset_exist(SCRATCH + "/A_Shot_%d" % i):
                unreal.EditorAssetLibrary.delete_asset(SCRATCH + "/A_Shot_%d" % i)
        stand.set_actor_hidden_in_game(was_hidden)
        stand.set_is_temporarily_hidden_in_editor(False)
