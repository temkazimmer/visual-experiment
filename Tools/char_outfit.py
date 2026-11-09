"""Import an outfit (made by Tools/blender_outfit.py) and build its materials.

  python3 Tools/ue.py Tools/char_outfit.py      # OUTFIT_FBX / OUTFIT_NAME globals optional

- Imports the FBX as /Game/Character/Clothes/<NAME> on Quinn's skeleton, with the
  body-slider morphs (BustUp/BustDown, GlutesUp/GlutesDown).
- Builds three materials, one per slot: M_OutfitSheer (see-through mesh that closes up at
  grazing angles), M_OutfitLatex (black clear coat), M_OutfitTrim (metal piping, clips).
She wears it through the wardrobe row "Sheer" in FVisexBodyProfile::Outfits() (DRESS tab).
Look values are in LOOK.
"""
import os
import unreal

LOOK = dict(
    sheer_color=(0.012, 0.012, 0.016), sheer_opacity=0.66, sheer_edge=0.98, sheer_exp=1.5, sheer_rough=0.65,
    latex_color=(0.012, 0.013, 0.02), latex_rough=0.32, coat=1.0, coat_rough=0.06,
    trim_color=(0.78, 0.8, 0.84), trim_rough=0.28,
)
LOOK.update(globals().get("LOOK_OVERRIDE", {}))
NAME = globals().get("OUTFIT_NAME", "SKM_Outfit_Sheer")
FBX = globals().get("OUTFIT_FBX") or os.path.join(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "SourceArt", "Character", "Clothes", "outfit_sheer.fbx")
DIR = "/Game/Character/Clothes"
SKELETON = "/Game/Characters/Mannequins/Meshes/SK_Mannequin"

if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise RuntimeError("Play mode is running: stop PIE before importing")

mel = unreal.MaterialEditingLibrary
MP = unreal.MaterialProperty
tools = unreal.AssetToolsHelpers.get_asset_tools()


def material(name):
    path = DIR + "/" + name
    m = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else \
        tools.create_asset(name, DIR, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(m)
    m.set_editor_property("used_with_skeletal_mesh", True)
    m.set_editor_property("used_with_morph_targets", True)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("use_material_attributes", False)
    return m


def scalar(m, name, v, y):
    n = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -500, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", v)
    return n


def color(m, name, c, y):
    n = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -500, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", unreal.LinearColor(c[0], c[1], c[2], 1.0))
    return n


def done(m):
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    return m


# sheer: translucent, lit per pixel; the weave closes up where you look along the surface
m = material("M_OutfitSheer")
m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
mel.connect_material_property(color(m, "Color", LOOK["sheer_color"], -200), "", MP.MP_BASE_COLOR)
mel.connect_material_property(scalar(m, "Roughness", LOOK["sheer_rough"], -80), "", MP.MP_ROUGHNESS)
fr = mel.create_material_expression(m, unreal.MaterialExpressionFresnel, -500, 200)
mel.connect_material_expressions(scalar(m, "EdgeExponent", LOOK["sheer_exp"], 320), "", fr, "ExponentIn")
lerp = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -250, 120)
mel.connect_material_expressions(scalar(m, "Opacity", LOOK["sheer_opacity"], 60), "", lerp, "A")
mel.connect_material_expressions(scalar(m, "EdgeOpacity", LOOK["sheer_edge"], 120), "", lerp, "B")
mel.connect_material_expressions(fr, "", lerp, "Alpha")
mel.connect_material_property(lerp, "", MP.MP_OPACITY)
sheer = done(m)

m = material("M_OutfitLatex")
m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_CLEAR_COAT)
m.set_editor_property("use_material_attributes", True)   # Python has no clear-coat pins
attrs = mel.create_material_expression(m, unreal.MaterialExpressionMakeMaterialAttributes, -150, 0)
for pin, n in (("BaseColor", color(m, "Color", LOOK["latex_color"], -200)),
               ("Roughness", scalar(m, "Roughness", LOOK["latex_rough"], -80)),
               ("ClearCoat", scalar(m, "Coat", LOOK["coat"], 40)),
               ("ClearCoatRoughness", scalar(m, "CoatRoughness", LOOK["coat_rough"], 160))):
    if not mel.connect_material_expressions(n, "", attrs, pin):
        raise RuntimeError("pin " + pin)
mel.connect_material_property(attrs, "", MP.MP_MATERIAL_ATTRIBUTES)
latex = done(m)

m = material("M_OutfitTrim")
m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
mel.connect_material_property(color(m, "Color", LOOK["trim_color"], -200), "", MP.MP_BASE_COLOR)
mel.connect_material_property(scalar(m, "Roughness", LOOK["trim_rough"], -80), "", MP.MP_ROUGHNESS)
mel.connect_material_property(scalar(m, "Metallic", 1.0, 40), "", MP.MP_METALLIC)
trim = done(m)

if not globals().get("MATERIALS_ONLY"):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("create_physics_asset", False)
    ui.set_editor_property("skeleton", unreal.load_asset(SKELETON))
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    sk = ui.get_editor_property("skeletal_mesh_import_data")
    sk.set_editor_property("import_morph_targets", True)
    sk.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_COMPUTE_NORMALS)
    sk.set_editor_property("update_skeleton_reference_pose", False)
    sk.set_editor_property("use_t0_as_ref_pose", False)
    sk.set_editor_property("import_uniform_scale", float(globals().get("OUTFIT_SCALE", 0.01)))
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", FBX)
    task.set_editor_property("destination_path", DIR)
    task.set_editor_property("destination_name", NAME)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", ui)
    tools.import_asset_tasks([task])

mesh = unreal.load_asset(DIR + "/" + NAME)
if not mesh:
    raise RuntimeError("import failed: " + FBX)
by_slot = {"Sheer": sheer, "Latex": latex, "Trim": trim}
mats = list(mesh.get_editor_property("materials"))
for i, sm in enumerate(mats):
    slot = str(sm.get_editor_property("material_slot_name"))
    pick = next((v for k, v in by_slot.items() if k.lower() in slot.lower()), [sheer, latex, trim][min(i, 2)])
    sm.set_editor_property("material_interface", pick)
    print("slot", i, slot, "->", pick.get_name())
mesh.set_editor_property("materials", mats)
unreal.EditorAssetLibrary.save_loaded_asset(mesh)
b = mesh.get_bounds()
skin = unreal.load_asset("/Game/Character/Android/SKM_AndroidSkin")
print("outfit height", round(b.box_extent.z * 2, 1), "skin height", round(skin.get_bounds().box_extent.z * 2, 1),
      "morphs", [x.get_name() for x in mesh.get_editor_property("morph_targets")])
