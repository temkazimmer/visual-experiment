"""M_MenuPanelLens: M_MenuPanel plus a per-vertex lens compensation, for the Settings shot.

  python3 Tools/ue.py Tools/menu_panel_lens.py     # (re)builds /Game/VisEx/UI/M_MenuPanelLens

When the menu lens goes from focal f to K * f, a panel point lands on the same pixel if its depth
along the camera's forward axis is scaled by K. World Position Offset does exactly that per
vertex: WPO = (LensK - 1) * dot(P - LensCamPos, LensCamFwd) * LensCamFwd. The panel's quad becomes
the exact (sheared) image the old lens saw, whatever the widget draws on it. The menu director
sets the three parameters every frame (VisexMenuDirector.cpp, ApplyPanelLens); LensK = 1 is the
plain M_MenuPanel. M_MenuPanel itself is not touched. Idempotent: rebuilt from M_MenuPanel.
"""
import unreal

SRC, DST = "/Game/VisEx/UI/M_MenuPanel", "/Game/VisEx/UI/M_MenuPanelLens"
eal, mel = unreal.EditorAssetLibrary, unreal.MaterialEditingLibrary
if eal.does_asset_exist(DST):
    eal.delete_asset(DST)
m = eal.duplicate_asset(SRC, DST)

def node(cls, x, y):
    return mel.create_material_expression(m, cls, x, y)
pos = node(unreal.MaterialExpressionWorldPosition, -900, 900)
cam = node(unreal.MaterialExpressionVectorParameter, -900, 1000)
cam.set_editor_property("parameter_name", "LensCamPos")
fwd = node(unreal.MaterialExpressionVectorParameter, -900, 1150)
fwd.set_editor_property("parameter_name", "LensCamFwd")
fwd.set_editor_property("default_value", unreal.LinearColor(1, 0, 0, 0))
k = node(unreal.MaterialExpressionScalarParameter, -900, 1300)
k.set_editor_property("parameter_name", "LensK")
k.set_editor_property("default_value", 1.0)
rel = node(unreal.MaterialExpressionSubtract, -650, 950)
mel.connect_material_expressions(pos, "", rel, "A")
mel.connect_material_expressions(cam, "RGB", rel, "B")
depth = node(unreal.MaterialExpressionDotProduct, -450, 1000)
mel.connect_material_expressions(rel, "", depth, "A")
mel.connect_material_expressions(fwd, "RGB", depth, "B")
km1 = node(unreal.MaterialExpressionSubtract, -650, 1300)
km1.set_editor_property("const_b", 1.0)
mel.connect_material_expressions(k, "", km1, "A")
amt = node(unreal.MaterialExpressionMultiply, -300, 1100)
mel.connect_material_expressions(depth, "", amt, "A")
mel.connect_material_expressions(km1, "", amt, "B")
off = node(unreal.MaterialExpressionMultiply, -150, 1100)
mel.connect_material_expressions(amt, "", off, "A")
mel.connect_material_expressions(fwd, "RGB", off, "B")
mel.connect_material_property(off, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
mel.recompile_material(m)
eal.save_loaded_asset(m)
print("saved", DST, "params", mel.get_scalar_parameter_names(m), mel.get_vector_parameter_names(m))
