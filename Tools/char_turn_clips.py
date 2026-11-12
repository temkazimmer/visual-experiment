"""Turn in place: the FemaleMovement TurnIP clips, baked onto her skeleton.

  python3 Tools/ue.py Tools/char_turn_clips.py

Same bake as char_photoseq_clips.py (pack rotations, her bone lengths, root and pelvis
translation scaled by leg length, ik_foot on the feet). Root motion ON: the root bone
carries the yaw, so a slot or montage that extracts root motion turns her without skating.

Writes /Game/Characters/Turn/A_Turn_180L, _180R, _90L, _90R. Idempotent.
"""
import os
import unreal

if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise RuntimeError("Play mode is running: stop PIE first")
_tools = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools")
_kit = open(os.path.join(_tools, "char_pose_mermaid.py")).read().split("# ---- the seat: mermaid ----")[0]
BUILD, SHOTS = False, []
exec(compile(_kit, "char_pose_mermaid.py", "exec"), globals())

TURN_DIR = "/Game/Characters/Turn"
SRC = "/Game/FemaleMovement/Animation/TurnInPlace/AS_F_Unarmed_TurnIP_"
CLIPS = {"A_Turn_" + k: SRC + k + "_01" for k in ("180L", "180R", "90L", "90R")}
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
    out = write(name, frames, folder=TURN_DIR, save=False)
    out.set_editor_property("enable_root_motion", True)
    out.set_editor_property("force_root_lock", False)
    unreal.EditorAssetLibrary.save_loaded_asset(out, only_if_is_dirty=False)
    a, b = frames[0]["root"], frames[-1]["root"]
    yaw = math.degrees(2 * math.atan2(b[1][2], b[1][3]) - 2 * math.atan2(a[1][2], a[1][3]))
    print("%s: %.2f s, %d frames, root yaw %.0f deg, root moves (%.1f, %.1f) cm, missing %s"
          % (name, T, n, yaw, b[0][0] - a[0][0], b[0][1] - a[0][1], missing[:6]))
