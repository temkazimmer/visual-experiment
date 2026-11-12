"""Her standing idle: contrapposto, one hand on the hip, chest up, a slow look around.

  python3 Tools/ue.py Tools/char_idle_proud.py

Writes the result into /Game/Characters/Mannequins/Anims/Unarmed/MM_Idle, so everything that
stands her still (the ABP's Idle state, the locomotion blend space at speed 0, Lvl_Dust) gets it.
The template idle is kept as MM_Idle_Template; the pose kit reads that one.

The body is FemaleEveryday mocap, baked onto her skeleton like char_photoseq_clips.py: the pack's
idles all start and end on one pose (weight on the left leg), so LookAround, Neutral_B and
WeightShift_Right chain into one 38 s loop with the feet planted by the mocap itself. On top:
  - the walk's proud posture (VisexAnimInstance Catwalk: spine back, gaze level) plus shoulders back;
  - one hand on the hip of the standing leg, placed where the pack's HandsOnHips puts it relative
    to the pelvis (two-bone reach, palm and fingers copied); it swaps sides with the weight shift;
  - a slow look-around on the neck and head between the clip's own glances.
Also writes A_Idle_Proud_Blend (the loop's first 2 s) for the blend space's speed-0 samples, so
the walk's normalised time isn't stretched over a 38 s idle.
"""
import math
import os
import unreal

if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise RuntimeError("Play mode is running: stop PIE first")
_tools = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools")
_kit = open(os.path.join(_tools, "char_pose_mermaid.py")).read().split("# ---- the seat: mermaid ----")[0]
BUILD, SHOTS = False, []
exec(compile(_kit, "char_pose_mermaid.py", "exec"), globals())

UNARMED = "/Game/Characters/Mannequins/Anims/Unarmed"
E = "/Game/FemaleEveryday/Volume1/Animations/AS_%s_Idle_F"
CHAIN = ["LookAround", "Neutral_B", "WeightShift_Right"]          # each starts and ends on the rest pose
POSTURE = globals().get("POSTURE", 4.0)       # deg the spine carries the chest back (the pack stands taller than the walk clip; this matches the walk)
SHOULDERS = globals().get("SHOULDERS", 9.0)   # deg the collarbones draw back
HIP_OUT = globals().get("HIP_OUT", {"l": -2.2, "r": -3.8})  # cm the hip hand sits further out than the pack's (measured: palm onto her skin)
SCAN = globals().get("SCAN", 1.0)             # share of the look-around


def pose_at(seq, t):
    opt = unreal.AnimPoseEvaluationOptions()
    opt.set_editor_property("extract_root_motion", False)
    p = APE.get_anim_pose_at_time(seq, t, opt)
    out = {}
    for n in APE.get_bone_names(p):
        tr = APE.get_bone_pose(p, n, unreal.AnimPoseSpaces.LOCAL)
        r = tr.rotation
        out[str(n)] = ((tr.translation.x, tr.translation.y, tr.translation.z), qnorm((r.x, r.y, r.z, r.w)))
    return out


_rest = None
def retarget(src):
    """The pack's rotations on her bones; root and pelvis translation scaled to her legs."""
    global _rest
    if _rest is None:
        _rest = BASE["pelvis"][0][2] / src["pelvis"][0][2]
    for b in bones:
        p, q, s = BASE[b]
        if b in src:
            q = src[b][1]
            if b in ("root", "pelvis"):
                p = tuple(v * _rest for v in src[b][0])
        local[b] = [p, q, s]
    _cache.clear()


def side_arm(s):
    return [b for b in ARM if b.endswith("_" + s)]


def hand_kids(s):
    h = "hand_" + s
    out = []
    for b in side_arm(s):
        x = parent[b]
        while x and x != h:
            x = parent[x]
        if x == h:
            out.append(b)
    return out


# ---- where the pack puts a hand on the hip, relative to the pelvis ----
_hips = unreal.load_asset(E % "HandsOnHips")
retarget(pose_at(_hips, 5.5))
HIP = {}
for s in "lr":
    P, Q = world("pelvis")
    iq = qinv(Q)
    HIP[s] = dict(at=rot(iq, sub(pos("hand_" + s), P)), q=qmul(iq, world("hand_" + s)[1]),
                  elbow=rot(iq, sub(pos("lowerarm_" + s), P)),
                  fingers={b: tuple(local[b][1]) for b in hand_kids(s)})
    print("hip hand %s: %.1f cm out, %.1f fwd, %.1f up from the pelvis" % (
        s, *sub(pos("hand_" + s), P)))


def proud():
    """The walk's posture held standing: chest back, gaze level, neck long, shoulders back."""
    for b in ("spine_02", "spine_03"):
        rotate(b, turn(Z, neg(Y), POSTURE * 0.5))
    rotate("neck_01", turn(Z, Y, POSTURE * 0.6))
    rotate("head", turn(Z, Y, POSTURE * 0.4))
    for s in "lr":
        c = "clavicle_" + s
        d = norm(sub(pos("upperarm_" + s), pos(c)))
        rotate(c, turn(d, neg(Y), SHOULDERS))
        rotate(c, turn(d, neg(Z), SHOULDERS * 0.3))


def look(yaw, pitch):
    """Turn the gaze: + yaw toward her left, + pitch up; spread down the neck."""
    for b, share in (("spine_05", 0.15), ("neck_01", 0.25), ("neck_02", 0.25), ("head", 0.35)):
        rotate(b, turn(Y, X, yaw * share))
        rotate(b, turn(Y, Z, pitch * share))


def hip_hand(s, w):
    if w <= 0.001:
        return
    arm = side_arm(s)
    relaxed = {b: tuple(local[b][1]) for b in arm}
    P, Q = world("pelvis")
    H = HIP[s]
    at = add(P, rot(Q, H["at"]))
    out = norm(perp(sub(at, P), Z))
    at = add(at, mul(out, HIP_OUT[s]))
    reach(s, at, sub(add(P, rot(Q, H["elbow"])), pos("upperarm_" + s)))
    set_world_rot("hand_" + s, qmul(Q, H["q"]))
    for b, q in H["fingers"].items():
        local[b][1] = q
    _cache.clear()
    share_twist(s)
    for b in arm:
        local[b][1] = slerp(relaxed[b], local[b][1], w)
    _cache.clear()


def ramp(t, a, b):
    x = min(max((t - a) / (b - a), 0.0), 1.0)
    return x * x * (3 - 2 * x)


def keyed(keys, t):
    """Eased from key to key; two equal keys in a row hold."""
    for (t0, *a), (t1, *b) in zip(keys, keys[1:]):
        if t <= t1:
            x = ramp(t, t0, t1)
            return [u + (v - u) * x for u, v in zip(a, b)]
    return keys[-1][1:]


# ---- the timeline ----
SEGS = [(unreal.load_asset(E % n), n) for n in CHAIN]
STARTS, _t = [], 0.0
for seq, n in SEGS:
    STARTS.append(_t)
    _t += seq.get_play_length()
TOTAL = _t
W = STARTS[2]                      # WeightShift_Right: on the right leg ~2.5..9 s into it
GAZE = [(0.0, 0.0, 0.0), (12.5, 0.0, 0.0), (14.2, 32.0, 3.0), (17.0, 32.0, 3.0), (18.6, -6.0, 1.0),
        (19.6, -6.0, 1.0), (21.4, -30.0, 4.0), (23.8, -30.0, 4.0), (25.6, 0.0, 0.0), (28.4, 0.0, 1.0),
        (30.0, 24.0, 3.0), (33.0, 24.0, 3.0), (35.0, 0.0, 0.0), (TOTAL, 0.0, 0.0)]


def weights(t):
    wl = 1.0 - ramp(t, W + 0.6, W + 1.8) + ramp(t, W + 9.6, W + 10.8)
    wr = ramp(t, W + 2.2, W + 3.4) - ramp(t, W + 8.0, W + 9.2)
    return wl, wr


frames, pel_x = [], []
for (seq, n), t0 in zip(SEGS, STARTS):
    T = seq.get_play_length()
    count = int(round(T * FPS))
    last = (seq, n) == SEGS[-1]
    for f in range(count + (1 if last else 0)):
        tl = min(f / FPS, T)
        t = t0 + tl
        retarget(pose_at(seq, tl))
        proud()
        yaw, pitch = keyed(GAZE, t)
        look(yaw * SCAN, pitch * SCAN)
        wl, wr = weights(t)
        hip_hand("l", wl)
        hip_hand("r", wr)
        pel_x.append(pos("pelvis")[0])
        frames.append({b: (local[b][0], local[b][1], local[b][2]) for b in bones})
print("frames %d (%.1f s); pelvis x: %s" % (len(frames), TOTAL, " ".join("%.0f" % pel_x[i] for i in range(0, len(pel_x), 45))))

idle_seq = write("MM_Idle", frames, folder=UNARMED, save=False)
idle_seq.set_editor_property("enable_root_motion", False)
unreal.EditorAssetLibrary.save_loaded_asset(idle_seq, only_if_is_dirty=False)
blend = write("A_Idle_Proud_Blend", [frames[0]] * 31, folder=UNARMED)
bs = unreal.load_asset(UNARMED + "/BS_Idle_Walk_Run")
samples = list(bs.get_editor_property("sample_data"))
swapped = 0
for s in samples:
    a = s.get_editor_property("animation")
    if a and a.get_name() in ("MM_Idle", "A_Idle_Proud_Blend"):
        s.set_editor_property("animation", blend)
        swapped += 1
bs.set_editor_property("sample_data", samples)
unreal.VisexAnimInstanceLibrary.resample_blend_space(bs)
unreal.EditorAssetLibrary.save_loaded_asset(bs, only_if_is_dirty=False)
print("MM_Idle %.2f s; blend space speed-0 samples now A_Idle_Proud_Blend: %d" % (idle_seq.get_play_length(), swapped))
