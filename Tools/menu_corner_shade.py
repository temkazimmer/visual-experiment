# Builds /Game/VisEx/UI/M_MenuCornerShade: a post-process darkening from the screen's top-left
# corner, very soft, so the white menu text reads on bright sky. Runs before DOF, i.e. before
# the after-DOF translucency the menu panel draws in, so the text itself stays white.
#   python3 Tools/ue.py Tools/menu_corner_shade.py     (run with Play stopped: it saves)
import unreal

PATH, NAME = "/Game/VisEx/UI", "M_MenuCornerShade"
full = f"{PATH}/{NAME}"
mel = unreal.MaterialEditingLibrary
RADIUS = globals().get("RADIUS", 1.5)      # reach, in screen heights from the corner
STRENGTH = globals().get("STRENGTH", 0.93)   # darkening at the corner itself
POWER = globals().get("POWER", 1.25)         # falloff shape: lower = softer, wider

m = unreal.load_asset(full) or unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    NAME, PATH, unreal.Material, unreal.MaterialFactoryNew())
mel.delete_all_material_expressions(m)
m.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
m.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_BEFORE_DOF)


def node(cls, x, y):
    return mel.create_material_expression(m, cls, x, y)


def scalar(name, v, x, y):
    n = node(unreal.MaterialExpressionScalarParameter, x, y)
    n.set_editor_property("parameter_name", name); n.set_editor_property("default_value", v)
    return n


scene = node(unreal.MaterialExpressionSceneTexture, -300, -200)
scene.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)

uv = node(unreal.MaterialExpressionScreenPosition, -1200, 0)
size = node(unreal.MaterialExpressionViewSize, -1200, 150)
aspect = node(unreal.MaterialExpressionDivide, -1050, 150)          # (w/h, 1)
mel.connect_material_expressions(size, "", aspect, "A")
mask_y = node(unreal.MaterialExpressionComponentMask, -1150, 250)
mask_y.set_editor_property("g", True)
mel.connect_material_expressions(size, "", mask_y, "")
mel.connect_material_expressions(mask_y, "", aspect, "B")
scaled = node(unreal.MaterialExpressionMultiply, -900, 0)             # corner-relative, in screen heights
mel.connect_material_expressions(uv, "ViewportUV", scaled, "A")
mel.connect_material_expressions(aspect, "", scaled, "B")
dist = node(unreal.MaterialExpressionLength, -780, 0)
mel.connect_material_expressions(scaled, "", dist, "")
over = node(unreal.MaterialExpressionDivide, -660, 0)
mel.connect_material_expressions(dist, "", over, "A")
mel.connect_material_expressions(scalar("Radius", RADIUS, -800, 120), "", over, "B")
sat = node(unreal.MaterialExpressionSaturate, -560, 0)
mel.connect_material_expressions(over, "", sat, "")
inv = node(unreal.MaterialExpressionOneMinus, -470, 0)
mel.connect_material_expressions(sat, "", inv, "")
smooth = node(unreal.MaterialExpressionSmoothStep, -380, 0)              # no edge anywhere
smooth.set_editor_property("const_min", 0.0); smooth.set_editor_property("const_max", 1.0)
mel.connect_material_expressions(inv, "", smooth, "Value")
pw = node(unreal.MaterialExpressionPower, -260, 0)
mel.connect_material_expressions(smooth, "", pw, "Base")
mel.connect_material_expressions(scalar("Power", POWER, -380, 120), "", pw, "Exp")
amt = node(unreal.MaterialExpressionMultiply, -140, 0)
mel.connect_material_expressions(pw, "", amt, "A")
mel.connect_material_expressions(scalar("Strength", STRENGTH, -260, 160), "", amt, "B")
keep = node(unreal.MaterialExpressionOneMinus, -30, 0)
mel.connect_material_expressions(amt, "", keep, "")
mask_rgb = node(unreal.MaterialExpressionComponentMask, -150, -200)
for c in "rgb":
    mask_rgb.set_editor_property(c, True)
mel.connect_material_expressions(scene, "Color", mask_rgb, "")
out = node(unreal.MaterialExpressionMultiply, 100, -100)
mel.connect_material_expressions(mask_rgb, "", out, "A")
mel.connect_material_expressions(keep, "", out, "B")
mel.connect_material_property(out, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(m)
print("saved", unreal.EditorAssetLibrary.save_asset(full), full)
