"""Menu walks: the FemaleMovement walk start, loop and stop, baked onto her skeleton.

  python3 Tools/ue.py Tools/char_menu_walk_clips.py   (or the pythonscript commandlet)

The pack is licensed and lives on its own SK_Quinn skeleton, so nothing references it at
runtime: every frame is read here and written into new clips on SK_Mannequin (rotations from
the pack, bone lengths hers, root and pelvis scaled by leg length so the stride stays true),
with the ik_foot bones put on the feet as char_pose_mermaid.py does. Root motion ON: the
menu director plays them as one root-motion montage (start, n x loop, stop) and the capsule
follows the clip, so a planted foot stays put through starts and stops (FOOT_IK.md).

Writes /Game/Characters/Menu/A_Menu_Walk_Start (Walk_F_Start), _Loop (Walk_F_Loop) and
_Stop (Walk_F_End). The loop and the stop share their first second (the stop starts where
the loop does). Idempotent; -clips=A,B bakes only those.
"""
import os
import unreal

HEADLESS = "-run=pythonscript" in unreal.SystemLibrary.get_command_line().lower()
if HEADLESS:
    # the pythonscript commandlet has no level editor: asking it about Play crashes the engine
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
SRC = "/Game/FemaleMovement/Animation/"
CLIPS = {"A_Menu_Walk_Start": SRC + "Walk/AS_F_Unarmed_Walk_F_Start",
         "A_Menu_Walk_Loop": SRC + "Walk/AS_F_Unarmed_Walk_F_Loop",
         "A_Menu_Walk_Stop": SRC + "Walk/AS_F_Unarmed_Walk_F_End"}
_only = [a.split("=", 1)[1] for a in unreal.SystemLibrary.get_command_line().split() if a.lower().startswith("-clips=")]
if _only:
    CLIPS = {k: v for k, v in CLIPS.items() if k in _only[0].split(",")}
MOVE = ("root", "pelvis")          # the only bones whose translation comes from the pack


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


def replant_root(frames):
    """The pack's root runs a little ahead of or behind the planted foot (1.5-2.5 cm a stance
    on the loop). Re-derive the root from the feet: each frame it moves by the opposite of
    what her grounded heels and balls move in root space, so a planted sole stays put."""
    pts = [("foot_l", 0), ("ball_l", 0), ("foot_r", 1), ("ball_r", 1)]
    n = len(frames)
    loc = []
    for fr in frames:
        RP, RQ = _frame_world(fr, "root")
        iq = qinv(RQ)
        loc.append([rot(iq, sub(_frame_world(fr, b)[0], RP)) for b, _ in pts])
    low = [min(loc[f][i][2] for f in range(n)) for i in range(len(pts))]
    root = [fr["root"][0] for fr in frames]
    out = [root[0]]
    for f in range(1, n):
        orig = sub(root[f], root[f - 1])
        num, den = [0.0, 0.0], 0.0
        for i in range(len(pts)):
            d = sub(loc[f][i], loc[f - 1][i])
            h = 0.5 * (loc[f][i][2] + loc[f - 1][i][2]) - low[i]
            spd = (((d[0] + orig[0]) ** 2 + (d[1] + orig[1]) ** 2) ** 0.5) * FPS
            w = max(0.0, 1.0 - h / 2.0) * max(0.0, 1.0 - spd / 60.0)
            num[0] -= w * d[0]
            num[1] -= w * d[1]
            den += w
        step = (num[0] / den, num[1] / den, orig[2]) if den > 0.3 else orig
        out.append(add(out[-1], step))
    for fr, p in zip(frames, out):
        fr["root"] = (p, fr["root"][1], fr["root"][2])
    return out[-1][1] - out[0][1], root[-1][1] - root[0][1]


REPLANT = ("A_Menu_Walk_Loop",)

for name, path in CLIPS.items():
    seq = unreal.load_asset(path)
    T = seq.get_play_length()
    n = int(round(T * FPS))
    first = pose_at(seq, 0.0)
    missing = [b for b in bones if b not in first]
    # leg length ratio: her pelvis height over the pack's, both at frame 0
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
        frames.append(fr)
    if name in REPLANT:
        print("%s: root re-planted, lap %.1f cm (pack %.1f)" % ((name,) + replant_root(frames)))
    out = write(name, frames, folder=PHOTO_DIR, save=False)
    out.set_editor_property("enable_root_motion", True)
    out.set_editor_property("force_root_lock", False)
    unreal.EditorAssetLibrary.save_loaded_asset(out, only_if_is_dirty=False)
    d = frames[-1]["root"][0][1] - frames[0]["root"][0][1]
    print("%s: %.2f s, %d frames, stride scale %.3f, %.0f cm/s, missing %s" % (name, T, n, scale, d / T, missing[:6]))
