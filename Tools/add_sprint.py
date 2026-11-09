"""Shift = run. Creates IA_Sprint (bool) and maps Left Shift in IMC_Default. Idempotent.

  python3 Tools/ue.py Tools/add_sprint.py

The speed switch lives in BP_ThirdPersonCharacter's Event Graph: paste
Tools/sprint_nodes.t3d (IA_Sprint Started -> MaxWalkSpeed 540, Completed -> 300).
Walk speed default (300 = MF_Unarmed_Walk_Fwd's own speed) is set in tune_movement.py.
"""
import unreal

ACTION_DIR, ACTION_NAME = "/Game/Input/Actions", "IA_Sprint"
IMC_PATH = "/Game/Input/IMC_Default"
KEY = "LeftShift"

if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise RuntimeError("Play mode is running: stop PIE before saving assets")

path = "%s/%s" % (ACTION_DIR, ACTION_NAME)
action = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
if action is None:
    action = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        ACTION_NAME, ACTION_DIR, unreal.InputAction, unreal.DataAssetFactory())
action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
unreal.EditorAssetLibrary.save_loaded_asset(action, only_if_is_dirty=False)

imc = unreal.load_asset(IMC_PATH)
key = unreal.Key()
key.import_text(KEY)
mapped = [m for m in imc.get_editor_property("default_key_mappings").get_editor_property("mappings")
          if m.get_editor_property("action") == action]
if not mapped:
    imc.map_key(action, key)
unreal.EditorAssetLibrary.save_loaded_asset(imc, only_if_is_dirty=False)

for m in imc.get_editor_property("default_key_mappings").get_editor_property("mappings"):
    if m.get_editor_property("action") == action:
        print("mapped", action.get_name(), "->", m.get_editor_property("key").export_text())
