"""Menu step-back: the FemaleMovement backward walk start and stop, baked onto her skeleton.

  python3 Tools/ue.py Tools/char_menu_walk_back_clips.py   (or the pythonscript commandlet)

The walk back from a page to the column is two steps: she steps back with her left, the right
closes, and she sinks into the lean (no turning round, Docs/Research/FOOT_IK.md). The pack's
backward start steps back with the right, so it's mirrored here (left and right swapped, across
her mesh's X); its stop closes with the right, as the lean wants. As char_menu_walk_clips.py:
her bone lengths, root and pelvis scaled by leg length, ik_foot on the feet, root motion ON.

Writes /Game/Characters/Menu/A_Menu_WalkB_Start (Walk_B_Start, mirrored) and A_Menu_WalkB_Stop
(Walk_B_End). The start's first step is stretched (WARP) so out and back match. Idempotent.
"""
import os
import unreal

HEADLESS = "-run=pythonscript" in unreal.SystemLibrary.get_command_line().lower()
if HEADLESS:
    class _NoPlay:
        def is_in_play_in_editor(self): return False
    _get = unreal.get_editor_subsystem
    unreal.get_editor_subsystem = lambda c: _NoPlay() if c is unreal.LevelEditorSubsystem else _get(c)
if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise RuntimeError("Play mode is running: stop PIE first")
_tools = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools")
_kit = open(os.path.join(_tools, "char_pose_mermaid.py")).read().split("# ---- the seat: mermaid ----")[0]
BUILD, SHOTS = False, []
exec(compile(_kit, "char_pose_mermaid.py", "exec"), globals())

PHOTO_DIR = "/Game/Characters/Menu"
SRC = "/Game/FemaleMovement/Animation/Walk/"
CLIPS = {"A_Menu_WalkB_Start": (SRC + "AS_F_Unarmed_Walk_B_Start", True),
         "A_Menu_WalkB_Stop": (SRC + "AS_F_Unarmed_Walk_B_End", False)}
MOVE = ("root", "pelvis")

# the mirror: each bone takes its partner's turn away from the reference pose, reflected across
# X (her mesh's side to side), so it works whatever axes the left and right bones carry
_ref_pose = APE.get_reference_pose(idle.get_editor_property("skeleton"))
REF = {}
for b in bones:
    t = APE.get_bone_pose(_ref_pose, b, unreal.AnimPoseSpaces.LOCAL)
    r = t.rotation
    REF[b] = ((t.translation.x, t.translation.y, t.translation.z), qnorm((r.x, r.y, r.z, r.w)), (1.0, 1.0, 1.0))
REF_W = {b: _frame_world(REF, b)[1] for b in bones}


def partner(b):
    m = b[:-2] + ("_r" if b.endswith("_l") else "_l") if b[-2:] in ("_l", "_r") else b
    return m if m in REF else b


def mq(q): return (q[0], -q[1], -q[2], q[3])


def mirror(fr):
    W = {b: _frame_world(fr, b)[1] for b in bones}
    NW = {b: qnorm(qmul(qmul(mq(W[partner(b)]), qinv(mq(REF_W[partner(b)]))), REF_W[b])) for b in bones}
    out = {}
    for b in bones:
        p, q, s = fr[b]
        par = parent[b]
        pq_new = NW[par] if par else (0.0, 0.0, 0.0, 1.0)
        pq_old = W[par] if par else (0.0, 0.0, 0.0, 1.0)
        if b in MOVE:
            c = rot(pq_old, p)
            p = rot(qinv(pq_new), (-c[0], c[1], c[2]))
        out[b] = (p, qnorm(qmul(qinv(pq_new), NW[b])), s)
    return out


# her first step back is a little longer than the pack's, so the walk back lands where the walk
# out (lean-up, two steps forward) left from: the root goes EXTRA cm further back over the left
# foot's swing (A..B s) while the planted right leg is re-solved to keep its foot where it was,
# then lets go of it as it lifts (C..D s)
WARP = {"A_Menu_WalkB_Start": dict(extra=8.5, a=0.9, b=1.5, c=1.55, d=1.95)}


def smooth(x):
    x = min(max(x, 0.0), 1.0)
    return x * x * (3.0 - 2.0 * x)


def warp(frames, extra, a, b, c, d):
    worst = 0.0
    for f, fr in enumerate(frames):
        t = f / FPS
        o = (0.0, -extra * smooth((t - a) / (b - a)), 0.0)
        w = 1.0 - smooth((t - c) / (d - c))
        for k in bones:
            local[k] = list(fr[k])
        _cache.clear()
        target, knee, turn = pos("foot_r"), pos("calf_r"), world("foot_r")[1]
        local["root"][0] = add(local["root"][0], o)
        _cache.clear()
        if w > 0.0 and o[1] != 0.0:
            goal = add(target, mul(o, 1.0 - w))
            limb("thigh_r", "calf_r", "foot_r", goal, add(knee, o))
            set_world_rot("foot_r", turn)
            worst = max(worst, length(sub(pos("foot_r"), goal)))
        frames[f] = {k: tuple(local[k]) for k in bones}
    print("warp: %.1f cm further back, planted foot off by at most %.2f cm" % (extra, worst))
    return frames


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


for name, (path, flip) in CLIPS.items():
    seq = unreal.load_asset(path)
    T = seq.get_play_length()
    n = int(round(T * FPS))
    first = pose_at(seq, 0.0)
    scale = local["pelvis"][0][2] / first["pelvis"][0][2] if abs(first["pelvis"][0][2]) > 1 else 1.0
    frames = []
    for f in range(n + 1):
        src = pose_at(seq, min(f / FPS, T))
        fr = {}
        for b in bones:
            p, q, s = local[b]
            if b in src:
                q = src[b][1]
                if b in MOVE:
                    p = tuple(v * scale for v in src[b][0])
            fr[b] = (p, q, s)
        frames.append(mirror(fr) if flip else fr)
    if name in WARP:
        frames = warp(frames, **WARP[name])
    out = write(name, frames, folder=PHOTO_DIR, save=False)
    out.set_editor_property("enable_root_motion", True)
    out.set_editor_property("force_root_lock", False)
    unreal.EditorAssetLibrary.save_loaded_asset(out, only_if_is_dirty=False)
    d = frames[-1]["root"][0][1] - frames[0]["root"][0][1]
    print("%s: %.2f s, %d frames, stride scale %.3f, root %.1f cm%s" % (name, T, n, scale, d, ", mirrored" if flip else ""))
