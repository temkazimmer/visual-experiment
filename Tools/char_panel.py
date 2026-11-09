"""The Character page's panel material: the engine's 3D-widget material, but drawn after
depth of field and without a depth test, so the text stays sharp and on top when the
camera goes in for a close-up with a long lens. Idempotent.

  python3 Tools/ue.py Tools/char_panel.py

AVisexBodyEditor loads /Game/Character/UI/M_BodyPanel when it exists.
"""
import unreal

SRC = "/Engine/EngineMaterials/Widget3DPassThrough"
DST = "/Game/Character/UI/M_BodyPanel"

if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise RuntimeError("Play mode is running: stop PIE before saving assets")

eal = unreal.EditorAssetLibrary
m = unreal.load_asset(DST) if eal.does_asset_exist(DST) else eal.duplicate_asset(SRC, DST)
if not isinstance(m, unreal.Material):
    raise RuntimeError("expected a Material, got %s" % type(m))
m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property("translucency_pass", unreal.MaterialTranslucencyPass.MTP_AFTER_DOF)
m.set_editor_property("disable_depth_test", True)
m.set_editor_property("two_sided", True)
unreal.MaterialEditingLibrary.recompile_material(m)
eal.save_loaded_asset(m)
print("panel material", m.get_path_name(), m.get_editor_property("blend_mode"), m.get_editor_property("translucency_pass"))
