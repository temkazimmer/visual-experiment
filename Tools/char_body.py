"""Body sliders on the player. Idempotent.

  python3 Tools/ue.py Tools/char_body.py

- Reparents ABP_Unarmed to UVisexAnimInstance (C++): the graph runs as before, then the
  body profile reshapes the bones (Source/visex/VisexAnimInstance.cpp).
- Adds UVisexBodyComponent ("Body") to BP_ThirdPersonCharacter: height, leg-length
  lift, morph targets, save/load. The body page (AVisexBodyEditor) edits it.
"""
import unreal

BP_PATH = "/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"
ABP_PATH = "/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"

if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise RuntimeError("Play mode is running: stop PIE before saving assets")

abp = unreal.load_asset(ABP_PATH)
gen = abp.generated_class()
if not unreal.MathLibrary.class_is_child_of(gen, unreal.VisexAnimInstance.static_class()):
    unreal.BlueprintEditorLibrary.reparent_blueprint(abp, unreal.VisexAnimInstance.static_class())
unreal.BlueprintEditorLibrary.compile_blueprint(abp)
unreal.EditorAssetLibrary.save_loaded_asset(abp)
print("ABP is VisexAnimInstance:", unreal.MathLibrary.class_is_child_of(abp.generated_class(), unreal.VisexAnimInstance.static_class()))

bp = unreal.load_asset(BP_PATH)
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
handles = sds.k2_gather_subobject_data_for_blueprint(bp)
by = {str(lib.get_variable_name(lib.get_data(h))): h for h in handles}
if "Body" not in by:
    root = handles[0]
    h, fail = sds.add_new_subobject(unreal.AddNewSubobjectParams(
        parent_handle=root, new_class=unreal.VisexBodyComponent, blueprint_context=bp))
    if not fail.is_empty():
        raise RuntimeError("add Body failed: %s" % fail)
    sds.rename_subobject(h, unreal.Text("Body"))
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
unreal.EditorAssetLibrary.save_loaded_asset(bp)
print("components:", [str(lib.get_variable_name(lib.get_data(h))) for h in sds.k2_gather_subobject_data_for_blueprint(bp)])
