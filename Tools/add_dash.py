"""Dash (Q / PlayStation L1) for BP_ThirdPersonCharacter. Idempotent.

  python3 Tools/ue.py Tools/setup_input.py   # IA_Dash + keys
  python3 Tools/ue.py Tools/add_dash.py      # CanDash variable

Then paste Tools/jump_dash_nodes.t3d into the Event Graph: IA_Dash Started -> if CanDash ->
CanDash=false -> if falling: Z velocity 0 + GravityScale 0.2 -> play MM_Dash
on DefaultSlot -> Delay 0.6 -> GravityScale 1 -> Delay 0.6 -> CanDash=true.
MM_Dash carries root motion (~8.75 m over ~0.6 s) and ABP_Unarmed applies root
motion from montages, so the animation itself moves the character.
"""
import unreal

BP_PATH = "/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"

if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise RuntimeError("Play mode is running: stop PIE before saving assets")

bp = unreal.load_asset(BP_PATH)
cdo = unreal.get_default_object(bp.generated_class())
try:
    cdo.get_editor_property("CanDash")
except Exception:
    lib = unreal.BlueprintEditorLibrary
    lib.add_member_variable(bp, "CanDash", lib.get_basic_type_by_name("bool"))
    lib.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
cdo.set_editor_property("CanDash", True)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
print("CanDash", unreal.get_default_object(bp.generated_class()).get_editor_property("CanDash"))
