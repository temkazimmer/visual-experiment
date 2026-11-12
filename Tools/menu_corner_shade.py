# Builds /Game/VisEx/UI/M_MenuCornerShade: a big, smooth post-process darkening from the screen's
# top-left corner, so the white menu reads on the bright haze (Tema's markup): an ellipse over the
# left ~60% of the frame that curves away and reaches zero before the character. Dithered, so the
# long gradient doesn't band. Runs before DOF, i.e. before the after-DOF translucency the menu
# panel draws in, so the text itself stays white. AVisexMenuDirector::SetCornerShade fades it by
# scaling the Strength parameter (keep that name).
#   python3 Tools/ue.py Tools/menu_corner_shade.py     (run with Play stopped: it saves)
import unreal

PATH, NAME = "/Game/VisEx/UI", "M_MenuCornerShade"
full = f"{PATH}/{NAME}"
mel = unreal.MaterialEditingLibrary
STRENGTH = globals().get("STRENGTH", 0.75)   # darkening at the corner itself
RADIUS_X = globals().get("RADIUS_X", 0.7)   # reach along the top, in screen widths
RADIUS_Y = globals().get("RADIUS_Y", 1.6)   # reach down the left side, in screen heights
INNER = globals().get("INNER", 0.3)         # flat-ish core; the feather runs from here to 1

m = unreal.load_asset(full) or unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    NAME, PATH, unreal.Material, unreal.MaterialFactoryNew())
mel.delete_all_material_expressions(m)
m.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
m.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_BEFORE_DOF)


def node(cls, x, y):
    return mel.create_material_expression(m, cls, x, y)


def scalar(name, v, x, y):
    n = node(unreal.MaterialExpressionScalarParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", v)
    return n


uv = node(unreal.MaterialExpressionScreenPosition, -900, 0)
size = node(unreal.MaterialExpressionViewSize, -900, 150)
code = node(unreal.MaterialExpressionCustom, -500, 0)
code.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
code.set_editor_property("code", """
float d = length(UV / float2(RX, RY));
float w = 1.0 - smoothstep(Inner, 1.0, d);
float2 px = UV * Size;
float n = frac(52.9829189 * frac(dot(px, float2(0.06711056, 0.00583715))));
return 1.0 - Strength * w * (1.0 + (n - 0.5) * 0.02);
""")
names = ["UV", "Size", "RX", "RY", "Inner", "Strength"]
ins = []
for nm in names:
    ci = unreal.CustomInput()
    ci.set_editor_property("input_name", nm)
    ins.append(ci)
code.set_editor_property("inputs", ins)
mel.connect_material_expressions(uv, "ViewportUV", code, "UV")
mel.connect_material_expressions(size, "", code, "Size")
mel.connect_material_expressions(scalar("RadiusX", RADIUS_X, -800, 300), "", code, "RX")
mel.connect_material_expressions(scalar("RadiusY", RADIUS_Y, -800, 380), "", code, "RY")
mel.connect_material_expressions(scalar("Inner", INNER, -800, 460), "", code, "Inner")
mel.connect_material_expressions(scalar("Strength", STRENGTH, -800, 540), "", code, "Strength")

scene = node(unreal.MaterialExpressionSceneTexture, -500, -250)
scene.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
mask_rgb = node(unreal.MaterialExpressionComponentMask, -300, -250)
for c in "rgb":
    mask_rgb.set_editor_property(c, True)
mel.connect_material_expressions(scene, "Color", mask_rgb, "")
out = node(unreal.MaterialExpressionMultiply, -100, -100)
mel.connect_material_expressions(mask_rgb, "", out, "A")
mel.connect_material_expressions(code, "", out, "B")
mel.connect_material_property(out, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(m)
print("saved", unreal.EditorAssetLibrary.save_asset(full), full)
