# Builds /Game/VisEx/UI/M_MenuShade: the soft dark pool behind the menu text (UI material).
# An ellipse centred on the text, fading to nothing well inside the panel's edges.
#   python3 Tools/ue.py Tools/menu_shade_material.py     (run with Play stopped: it saves)
import unreal

PATH, NAME = "/Game/VisEx/UI", "M_MenuShade"
full = f"{PATH}/{NAME}"
mel = unreal.MaterialEditingLibrary
CENTER = globals().get("CENTER", (0.36, 0.31))   # UV of the text block's middle in the padded panel
RADIUS = globals().get("RADIUS", (0.46, 0.44))   # where the shade reaches zero
STRENGTH = globals().get("STRENGTH", 0.6)

# load first: create_asset on an existing name opens a modal Overwrite dialog
m = unreal.load_asset(full) or unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    NAME, PATH, unreal.Material, unreal.MaterialFactoryNew())
mel.delete_all_material_expressions(m)
m.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)


def node(cls, x, y):
    return mel.create_material_expression(m, cls, x, y)


def const2(v, x, y):
    n = node(unreal.MaterialExpressionConstant2Vector, x, y)
    n.set_editor_property("r", v[0]); n.set_editor_property("g", v[1])
    return n


uv = node(unreal.MaterialExpressionTextureCoordinate, -1000, 0)
sub = node(unreal.MaterialExpressionSubtract, -800, 0)
mel.connect_material_expressions(uv, "", sub, "A")
mel.connect_material_expressions(const2(CENTER, -1000, 150), "", sub, "B")
div = node(unreal.MaterialExpressionDivide, -650, 0)
mel.connect_material_expressions(sub, "", div, "A")
mel.connect_material_expressions(const2(RADIUS, -800, 150), "", div, "B")
length = node(unreal.MaterialExpressionLength, -500, 0)
mel.connect_material_expressions(div, "", length, "")
sat = node(unreal.MaterialExpressionSaturate, -380, 0)
mel.connect_material_expressions(length, "", sat, "")
inv = node(unreal.MaterialExpressionOneMinus, -280, 0)
mel.connect_material_expressions(sat, "", inv, "")
# smooth falloff: (1-d)^2 * (3 - 2(1-d))-ish via smoothstep on the inverse
smooth = node(unreal.MaterialExpressionSmoothStep, -150, 0)
mel.connect_material_expressions(inv, "", smooth, "Value")
smooth.set_editor_property("const_min", 0.0)
smooth.set_editor_property("const_max", 1.0)
amt = node(unreal.MaterialExpressionScalarParameter, -280, 150)
amt.set_editor_property("parameter_name", "Strength")
amt.set_editor_property("default_value", STRENGTH)
op = node(unreal.MaterialExpressionMultiply, 0, 0)
mel.connect_material_expressions(smooth, "", op, "A")
mel.connect_material_expressions(amt, "", op, "B")

black = node(unreal.MaterialExpressionConstant3Vector, 0, -150)
black.set_editor_property("constant", unreal.LinearColor(0, 0, 0, 1))
mel.connect_material_property(black, "", unreal.MaterialProperty.MP_FINAL_COLOR if hasattr(unreal.MaterialProperty, "MP_FINAL_COLOR") else unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)
mel.recompile_material(m)
print("saved", unreal.EditorAssetLibrary.save_asset(full), full)
