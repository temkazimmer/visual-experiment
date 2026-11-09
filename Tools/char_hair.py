"""The hair's material, on the skin mesh's second slot ("Hair", from Tools/blender_hair.py).

  python3 Tools/ue.py Tools/char_hair.py      # after Tools/char_skin.py

M_Hair: blue-black, glossy enough to catch the sun as a band across each lock, with a cool
rim so the silhouette reads against the orange dust. Values in HAIR; rebuilt each run.
"""
import unreal

HAIR = dict(
    color=(0.010, 0.013, 0.024),
    rim=(0.05, 0.08, 0.16),      # added at grazing angles
    rim_power=3.5,
    roughness=0.34,
    specular=0.55,
    anisotropy=0.55,
)
DIR = "/Game/Character/Android"
if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise RuntimeError("Play mode is running: stop PIE first")

mel = unreal.MaterialEditingLibrary
MP = unreal.MaterialProperty
path = DIR + "/M_Hair"
m = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else \
    unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_Hair", DIR, unreal.Material, unreal.MaterialFactoryNew())
mel.delete_all_material_expressions(m)


def vec(name, v, y):
    n = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -700, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", unreal.LinearColor(v[0], v[1], v[2], 1))
    return n


def num(name, v, y):
    n = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -700, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", v)
    return n


fres = mel.create_material_expression(m, unreal.MaterialExpressionFresnel, -700, 150)
fres.set_editor_property("exponent", HAIR["rim_power"])
rim = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -450, 100)
mel.connect_material_expressions(vec("Rim", HAIR["rim"], 0), "", rim, "A")
mel.connect_material_expressions(fres, "", rim, "B")
add = mel.create_material_expression(m, unreal.MaterialExpressionAdd, -250, -100)
mel.connect_material_expressions(vec("Color", HAIR["color"], -200), "", add, "A")
mel.connect_material_expressions(rim, "", add, "B")
mel.connect_material_property(add, "", MP.MP_BASE_COLOR)
mel.connect_material_property(num("Roughness", HAIR["roughness"], 300), "", MP.MP_ROUGHNESS)
mel.connect_material_property(num("Specular", HAIR["specular"], 380), "", MP.MP_SPECULAR)
mel.connect_material_property(num("Anisotropy", HAIR["anisotropy"], 460), "", MP.MP_ANISOTROPY)
m.set_editor_property("two_sided", True)
mel.recompile_material(m)
unreal.EditorAssetLibrary.save_loaded_asset(m)

skin = unreal.load_asset(DIR + "/SKM_AndroidSkin")
mats = list(skin.get_editor_property("materials"))
names = [str(x.get_editor_property("material_slot_name")) for x in mats]
assert "Hair" in names, "no Hair slot on the skin: %s (run blender_hair.py, then char_skin.py)" % names
for x in mats:
    slot = str(x.get_editor_property("material_slot_name"))
    x.set_editor_property("material_interface", m if slot == "Hair" else unreal.load_asset(DIR + "/M_AerogelShell"))
skin.set_editor_property("materials", mats)
unreal.EditorAssetLibrary.save_loaded_asset(skin)
print("slots", names)
