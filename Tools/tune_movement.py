"""Calmer ground movement for BP_ThirdPersonCharacter. Idempotent.

  python3 Tools/ue.py Tools/tune_movement.py
"""
import unreal

BP_PATH = "/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"
MOVE = {
    "max_walk_speed": 150.0,                # calm walk (calm_walk.py halves the walk anim rate); Shift runs at 540 (sprint_nodes.t3d)
    "max_acceleration": 800.0,              # template 1500: ~0.7s to full speed
    "braking_deceleration_walking": 900.0,  # template 2000: glides ~0.6s to a stop
    "ground_friction": 4.0,                 # template 8: softer direction changes
}
TURN_YAW = 300.0                            # template 500 deg/s

if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise RuntimeError("Play mode is running: stop PIE before saving assets")

bp = unreal.load_asset(BP_PATH)
move = unreal.get_default_object(bp.generated_class()).get_editor_property("character_movement")
for k, v in MOVE.items():
    move.set_editor_property(k, v)
move.set_editor_property("rotation_rate", unreal.Rotator(roll=0.0, pitch=0.0, yaw=TURN_YAW))

unreal.BlueprintEditorLibrary.compile_blueprint(bp)
unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)

move = unreal.get_default_object(bp.generated_class()).get_editor_property("character_movement")
for k in MOVE:
    print(k, move.get_editor_property(k))
print("rotation_rate.yaw", move.get_editor_property("rotation_rate").yaw)
