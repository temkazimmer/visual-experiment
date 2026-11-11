"""Photo sequence locomotion: the FemaleMovement walk, jog and run, baked onto her skeleton.

  python3 Tools/ue.py Tools/char_photoseq_clips.py

The pack is licensed and lives on its own SK_Quinn skeleton, so nothing references it at
runtime: every frame is read here and written into new clips on SK_Mannequin (rotations from
the pack, bone lengths hers, root and pelvis scaled by leg length so the stride stays true),
with the ik_foot bones put on the feet as char_pose_mermaid.py does. Root motion stays ON:
photo mode plays these on a slot and lets them carry her, so her feet never slide.

Writes /Game/Characters/PhotoSeq/A_PhotoSeq_Walk (Walk_F_Loop), _Jog (Jog_F_Loop) and
_Run (Run_F_Loop). Idempotent.
"""
import os
import unreal

if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise RuntimeError("Play mode is running: stop PIE first")
_tools = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools")
_kit = open(os.path.join(_tools, "char_pose_mermaid.py")).read().split("# ---- the seat: mermaid ----")[0]
BUILD, SHOTS = False, []
exec(compile(_kit, "char_pose_mermaid.py", "exec"), globals())

PHOTO_DIR = "/Game/Characters/PhotoSeq"
SRC = "/Game/FemaleMovement/Animation/"
CLIPS = {"A_PhotoSeq_Walk": SRC + "Walk/AS_F_Unarmed_Walk_F_Loop",
         "A_PhotoSeq_Jog": SRC + "Jog/AS_F_Unarmed_Jog_F_Loop",
         "A_PhotoSeq_Run": SRC + "Run/AS_F_Unarmed_Run_F_Loop"}
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
    out = write(name, frames, folder=PHOTO_DIR, save=False)
    out.set_editor_property("enable_root_motion", True)
    out.set_editor_property("force_root_lock", False)
    unreal.EditorAssetLibrary.save_loaded_asset(out, only_if_is_dirty=False)
    d = frames[-1]["root"][0][1] - frames[0]["root"][0][1]
    print("%s: %.2f s, %d frames, stride scale %.3f, %.0f cm/s, missing %s" % (name, T, n, scale, d / T, missing[:6]))
