"""Calm walk at 150 cm/s. Idempotent.

  python3 Tools/ue.py Tools/calm_walk.py

The walk clips cover 300 cm/s, so in BS_Idle_Walk_Run they move to Speed=150 and
play at half rate (no foot sliding). Walk speed default comes from tune_movement.py;
the Shift-release node in the Event Graph must also say 150.
"""
import unreal

BS_PATH = "/Game/Characters/Mannequins/Anims/Unarmed/BS_Idle_Walk_Run"
WALK_SPEED = 150.0
WALK_RATE = WALK_SPEED / 300.0

if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise RuntimeError("Play mode is running: stop PIE before saving assets")

bs = unreal.load_asset(BS_PATH)
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

for s in unreal.load_asset(BS_PATH).get_editor_property("sample_data"):
    if s.get_editor_property("animation").get_name() == "MF_Unarmed_Walk_Fwd":
        print("Walk_Fwd", s.get_editor_property("sample_value").y, s.get_editor_property("rate_scale"))
