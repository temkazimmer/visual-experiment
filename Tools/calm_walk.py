"""Calm, upright walk at 190 cm/s. Idempotent.

  python3 Tools/ue.py Tools/calm_walk.py

The walk clips cover 300 cm/s at rate 1. In BS_Idle_Walk_Run they sit exactly at the
walk speed (no idle mixed in) and play at WALK_RATE: a slower step rhythm, so the
clip's own speed is 300*0.75 = 225 and Stride Warping (ABP_Unarmed) only trims the
stride to ~85%. Heavier trimming bends the knees and drops the pelvis (sneaky look).
Walk speed itself: tune_movement.py and the Shift-release node (both 190).
"""
import unreal

BS_PATH = "/Game/Characters/Mannequins/Anims/Unarmed/BS_Idle_Walk_Run"
WALK_SPEED = 190.0
WALK_RATE = 0.75
SPEED_GRID = 60  # 10 cm/s cells on the 0..600 Speed axis, so 190 is a grid point

if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise RuntimeError("Play mode is running: stop PIE before saving assets")

bs = unreal.load_asset(BS_PATH)
params = bs.get_editor_property("blend_parameters")
speed = params[1]
speed.set_editor_property("grid_num", SPEED_GRID)
params[1] = speed
bs.set_editor_property("blend_parameters", params)

samples = bs.get_editor_property("sample_data")
for i in range(len(samples)):
    s = samples[i]  # a copy: modify, then write back by index
    if "_Walk_" in s.get_editor_property("animation").get_name():
        v = s.get_editor_property("sample_value")
        s.set_editor_property("sample_value", unreal.Vector(v.x, WALK_SPEED, 0.0))
        s.set_editor_property("rate_scale", WALK_RATE)
        samples[i] = s
bs.set_editor_property("sample_data", samples)  # triggers PostEditChange (grid rebuild)
unreal.EditorAssetLibrary.save_loaded_asset(bs, only_if_is_dirty=False)

bs = unreal.load_asset(BS_PATH)
print("Speed grid", bs.get_editor_property("blend_parameters")[1].get_editor_property("grid_num"))
for s in bs.get_editor_property("sample_data"):
    if s.get_editor_property("animation").get_name() == "MF_Unarmed_Walk_Fwd":
        print("Walk_Fwd", s.get_editor_property("sample_value").y, s.get_editor_property("rate_scale"))
