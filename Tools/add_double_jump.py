"""Double jump for the Lvl_Dust player pawn (BP_ThirdPersonCharacter). Idempotent.

  python3 Tools/ue.py Tools/add_double_jump.py

Second-jump boost (2x height) lives in the Event Graph: OnJumped ->
JumpCurrentCount == 2 -> LaunchCharacter(0,0,778, Z override). Python can't
add graph nodes, so they're in Tools/double_jump_boost.t3d: copy, paste, compile.
"""
import unreal

BP_PATH = "/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"
JUMP_MAX_COUNT = 2      # ground jump + one air jump
JUMP_Z_VELOCITY = 550.0  # template: 500
AIR_CONTROL = 0.5        # template: 0.35; lets the second jump redirect

if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise RuntimeError("Play mode is running: stop PIE before saving assets")

bp = unreal.load_asset(BP_PATH)
cdo = unreal.get_default_object(bp.generated_class())
move = cdo.get_editor_property("character_movement")

cdo.set_editor_property("jump_max_count", JUMP_MAX_COUNT)
move.set_editor_property("jump_z_velocity", JUMP_Z_VELOCITY)
move.set_editor_property("air_control", AIR_CONTROL)

unreal.BlueprintEditorLibrary.compile_blueprint(bp)
unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)

cdo = unreal.get_default_object(bp.generated_class())
move = cdo.get_editor_property("character_movement")
print("JumpMaxCount", cdo.get_editor_property("jump_max_count"))
print("JumpZVelocity", move.get_editor_property("jump_z_velocity"))
print("AirControl", round(move.get_editor_property("air_control"), 3))
