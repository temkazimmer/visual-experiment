"""Create BP_MenuDirector (Blueprint child of the C++ AVisexMenuDirector) and place it
in Lvl_Dust, wired to MenuCam and the MenuModel stand-in (see menu_shot.py).
Run live: python3 Tools/ue.py Tools/add_menu.py
"""
import unreal

PATH, NAME = "/Game/VisEx/Menu", "BP_MenuDirector"
bp = unreal.load_asset(PATH + "/" + NAME)
if not bp:
    f = unreal.BlueprintFactory()
    f.set_editor_property("parent_class", unreal.VisexMenuDirector)
    bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, PATH, unreal.Blueprint, f)
    unreal.EditorAssetLibrary.save_asset(PATH + "/" + NAME)

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
by = {a.get_actor_label(): a for a in eas.get_all_level_actors()}
d = by.get("MenuDirector")
if not d:
    d = eas.spawn_actor_from_object(bp, by["MenuCam"].get_actor_location())
    d.set_actor_label("MenuDirector")
d.set_editor_property("menu_camera", by["MenuCam"])
d.set_editor_property("stand_in", by["MenuModel"])
# menu look lives in a Blueprint child of the C++ widget, so it can be restyled there
WPATH, WNAME = PATH, "WBP_Menu"
wbp = unreal.load_asset(WPATH + "/" + WNAME)
if not wbp:
    wf = unreal.WidgetBlueprintFactory()
    wf.set_editor_property("parent_class", unreal.VisexMenuWidget)
    wbp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(WNAME, WPATH, unreal.WidgetBlueprint, wf)
cdo = unreal.get_default_object(wbp.generated_class())
for k, v in (("item_size", 58), ("idle_opacity", 0.62), ("tagline_size", 26)):
    cdo.set_editor_property(k, v)
unreal.EditorAssetLibrary.save_asset(WPATH + "/" + WNAME)
d.panel.set_editor_property("widget_class", wbp.generated_class())
d.set_editor_property("panel_yaw", 34.0)
print("MenuDirector placed:", d.get_class().get_name(), "widget", wbp.get_name())
