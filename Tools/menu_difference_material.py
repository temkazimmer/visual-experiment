# Builds /Game/VisEx/UI/M_MenuDifference: the main menu panel's material.
# The widget's text is drawn as the inverse of the scene behind it, a "difference" blend, so white
# text reads dark over bright sky and light over dark stone.
#   python3 Tools/ue.py Tools/menu_difference_material.py
import unreal

PATH, NAME = "/Game/VisEx/UI", "M_MenuDifference"
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

full = f"{PATH}/{NAME}"
# load first: create_asset on an existing name opens a modal Overwrite dialog and blocks the editor
m = unreal.load_asset(full) or tools.create_asset(NAME, PATH, unreal.Material, unreal.MaterialFactoryNew())
mel.delete_all_material_expressions(m)
m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property("two_sided", True)


def node(cls, x, y):
    return mel.create_material_expression(m, cls, x, y)


# the parameter names the widget component fills in
tex = node(unreal.MaterialExpressionTextureSampleParameter2D, -900, 0)
tex.set_editor_property("parameter_name", "SlateUI")
tex.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
tint = node(unreal.MaterialExpressionVectorParameter, -900, 300)
tint.set_editor_property("parameter_name", "TintColorAndOpacity")
tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
node(unreal.MaterialExpressionScalarParameter, -900, 500).set_editor_property("parameter_name", "OpacityFromTexture")

text = node(unreal.MaterialExpressionMultiply, -600, 0)          # text colour at its brightness
mel.connect_material_expressions(tex, "RGB", text, "A")
mel.connect_material_expressions(tint, "RGB", text, "B")

alpha = node(unreal.MaterialExpressionMultiply, -600, 250)
mel.connect_material_expressions(tex, "A", alpha, "A")
mel.connect_material_expressions(tint, "A", alpha, "B")

scene = node(unreal.MaterialExpressionSceneColor, -900, -300)
scene_rgb = node(unreal.MaterialExpressionComponentMask, -650, -300)
for c in "rgb":
    scene_rgb.set_editor_property(c, True)
mel.connect_material_expressions(scene, "", scene_rgb, "")

# difference with white, in display terms: the scene is scaled by its own white level
# (SceneWhite, HDR) and inverted, then lit at the text's brightness
white = node(unreal.MaterialExpressionScalarParameter, -650, -450)
white.set_editor_property("parameter_name", "SceneWhite")
white.set_editor_property("default_value", 12.0)
norm = node(unreal.MaterialExpressionDivide, -450, -300)
mel.connect_material_expressions(scene_rgb, "", norm, "A")
mel.connect_material_expressions(white, "", norm, "B")
sat = node(unreal.MaterialExpressionSaturate, -300, -300)
mel.connect_material_expressions(norm, "", sat, "")
inv = node(unreal.MaterialExpressionOneMinus, -180, -300)
mel.connect_material_expressions(sat, "", inv, "")
absd = node(unreal.MaterialExpressionMultiply, -50, -150)
mel.connect_material_expressions(inv, "", absd, "A")
mel.connect_material_expressions(text, "", absd, "B")

mel.connect_material_property(absd, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY)
mel.recompile_material(m)
unreal.EditorAssetLibrary.save_asset(full)
print("built", full)

# the panel uses this instance, so SceneWhite can be tuned without a recompile
MI = "MI_MenuDifference"
mi_full = f"{PATH}/{MI}"
mi = unreal.load_asset(mi_full) or \
    tools.create_asset(MI, PATH, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
unreal.MaterialEditingLibrary.set_material_instance_parent(mi, m)
unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mi, "SceneWhite", globals().get("WHITE", 12.0))
unreal.EditorAssetLibrary.save_asset(mi_full)
print("built", mi_full)
