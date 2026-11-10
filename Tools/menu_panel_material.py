"""Menu and Character panels: the engine's 3D-widget material, writing custom depth and
stencil 1 where the text is solid, so screen filters (M_Anime80s) can leave the UI alone.
The threshold (0.9) is above the menu shade's darkest (0.8), so only the text is masked:
the shade fades out with the filter on it, no edge.

  python3 Tools/ue.py Tools/menu_panel_material.py      (with Play stopped)

Makes /Game/VisEx/UI/M_MenuPanel (AVisexMenuDirector::PanelMaterial) and patches
/Game/Character/UI/M_BodyPanel (Tools/char_panel.py builds it).
"""
import unreal

if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise RuntimeError("Play mode is running: stop PIE before saving assets")

eal = unreal.EditorAssetLibrary
SRC = "/Engine/EngineMaterials/Widget3DPassThrough"
for dst, after_dof in (("/Game/VisEx/UI/M_MenuPanel", False), ("/Game/Character/UI/M_BodyPanel", True)):
    m = unreal.load_asset(dst) if eal.does_asset_exist(dst) else eal.duplicate_asset(SRC, dst)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("two_sided", True)
    if after_dof:
        m.set_editor_property("translucency_pass", unreal.MaterialTranslucencyPass.MTP_AFTER_DOF)
        m.set_editor_property("disable_depth_test", True)
    m.set_editor_property("allow_translucent_custom_depth_writes", True)
    m.set_editor_property("opacity_mask_clip_value", 0.9)
    unreal.MaterialEditingLibrary.recompile_material(m)
    eal.save_loaded_asset(m)
    print("panel material", dst)
