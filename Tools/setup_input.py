"""Input actions + key mappings in IMC_Default. Idempotent.

  python3 Tools/ue.py Tools/setup_input.py

IA_Sprint: hold to run (keyboard Left Shift, PlayStation L2). The speed switch lives
in BP_ThirdPersonCharacter's Event Graph: Tools/sprint_nodes.t3d (Started -> 540,
Completed -> 190). Walk default is in tune_movement.py.
IA_Dash: burst in the facing direction (keyboard Q, PlayStation L1). Logic in
Tools/dash_nodes.t3d, set up by Tools/add_dash.py.
"""
import unreal

ACTION_DIR = "/Game/Input/Actions"
IMC_PATH = "/Game/Input/IMC_Default"
ACTIONS = {
    "IA_Sprint": ["LeftShift", "Gamepad_LeftTrigger"],  # L2
    "IA_Dash": ["Q", "Gamepad_LeftShoulder"],           # L1
}

if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise RuntimeError("Play mode is running: stop PIE before saving assets")

imc = unreal.load_asset(IMC_PATH)


def mappings():
    return imc.get_editor_property("default_key_mappings").get_editor_property("mappings")


for name, keys in ACTIONS.items():
    path = "%s/%s" % (ACTION_DIR, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        action = unreal.load_asset(path)
    else:
        action = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, ACTION_DIR, unreal.InputAction, unreal.DataAssetFactory())
    action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    unreal.EditorAssetLibrary.save_loaded_asset(action, only_if_is_dirty=False)

    have = {m.get_editor_property("key").export_text() for m in mappings()
            if m.get_editor_property("action") == action}
    for k in keys:
        key = unreal.Key()
        key.import_text(k)
        if key.export_text() not in have:
            imc.map_key(action, key)

unreal.EditorAssetLibrary.save_loaded_asset(imc, only_if_is_dirty=False)

for m in mappings():
    a = m.get_editor_property("action")
    if a and a.get_name() in ACTIONS:
        print("mapped", a.get_name(), "->", m.get_editor_property("key").export_text())
