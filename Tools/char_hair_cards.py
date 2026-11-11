"""Import the card hair (Tools/blender_hair_cards.py ... rig) and build its material.

  python3 Tools/ue.py Tools/char_hair_cards.py

- /Game/Character/Hair/SKM_HairCards: skeletal mesh on its own skeleton SK_HairCards
  (pelvis..head copied from Quinn, plus 11 hair chains for Kawaii Physics).
- /Game/Character/Hair/T_HairAtlas: strand atlas, linear (A = mask, R = per-strand
  brightness, G = strand id, B = root 0 .. tip 1).
- /Game/Character/Hair/M_HairCards: Hair shading model, masked, two-sided. Values in HAIR.

The AnimBP (ABP_HairCards: Copy Pose From Mesh (use attached parent) -> Kawaii Physics ->
Output) is made by hand: Python in 5.7 can't add AnimGraph nodes. See Docs/Research/HAIR.md.
"""
import os

import unreal

HAIR = dict(
    root=(0.012, 0.010, 0.0085),   # linear colour at the roots: soft black, a touch warm
    tip=(0.040, 0.032, 0.026),      # ...and at the tips. No blue: the plaza's sky light adds it
    variation=(0.55, 0.8),          # brightness = R * b + a, per strand
    roughness=0.58,
    specular=0.12,                  # soft: on the lit model the cards' own normals show
    scatter=0.0,                    # Hair model only
    # "lit": Default Lit with an anisotropic highlight along the strands. The Hair model always adds
    # light transmitted through the fibre for card hair (HairBsdf.ush: the TT lobe; the Backlit pin
    # only works for strand grooms) and it barely falls with darker colour, so with the sun behind
    # her, black hair went white at the edges (her first Body-page shot, QA 0269 to 0286).
    model="lit",
    anisotropy=-0.8,                # negative: the highlight runs across the strands (a band round
                                    # the head); positive drew a pale streak down each lock
    coverage=1.7,                   # strand alpha gain: solid strands, not see-through against haze
    clip=0.5,                       # opacity mask clip, after the temporal dither
)
DIR = "/Game/Character/Hair"
# hairstyles, each its own mesh and AnimBP (the Body page's Hair tab lists them); one skeleton,
# atlas and material for all. The skeleton holds every style's chains (UE's importer puts a new
# style's mesh on the existing hair skeleton by itself); each mesh only has its own.  python3 Tools/ue.py -c "STYLE='elf'\nexec(open('<abs path>/Tools/char_hair_cards.py').read())"
STYLES = dict(long=("hair_cards.fbx", "SKM_HairCards", "SK_HairCards"),
              elf=("hair_elf.fbx", "SKM_HairElf", "SK_HairCards"))
STYLE = globals().get("STYLE", "long")
FBX, MESH, SKEL = STYLES[STYLE]
SRC = os.path.join(unreal.Paths.project_dir(), "SourceArt/Character/Hair")

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if les.is_in_play_in_editor():
    raise RuntimeError("Play mode is running: stop PIE first")
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary

# ---- mesh -------------------------------------------------------------------------------
ui = unreal.FbxImportUI()
ui.set_editor_property("import_mesh", True)
ui.set_editor_property("import_as_skeletal", True)
ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
ui.set_editor_property("import_materials", False)
ui.set_editor_property("import_textures", False)
ui.set_editor_property("import_animations", False)
ui.set_editor_property("create_physics_asset", False)
sk_path = DIR + "/" + SKEL
ui.set_editor_property("skeleton", unreal.load_asset(sk_path) if eal.does_asset_exist(sk_path) else None)
smd = ui.get_editor_property("skeletal_mesh_import_data")
smd.set_editor_property("import_morph_targets", False)
smd.set_editor_property("convert_scene", True)
smd.set_editor_property("import_uniform_scale", 0.01)     # as Tools/char_skin.py: Blender writes metres
smd.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
task = unreal.AssetImportTask()
task.set_editor_property("filename", os.path.join(SRC, FBX))
task.set_editor_property("destination_path", DIR)
task.set_editor_property("destination_name", MESH)
task.set_editor_property("replace_existing", True)
task.set_editor_property("automated", True)
task.set_editor_property("save", False)
task.set_editor_property("options", ui)
tools.import_asset_tasks([task])
mesh = unreal.load_asset(DIR + "/" + MESH)
assert mesh, "hair mesh did not import"
skel = mesh.get_editor_property("skeleton")
if skel.get_path_name().split(".")[0] != sk_path:
    unreal.EditorAssetLibrary.rename_asset(skel.get_path_name().split(".")[0], sk_path)

# ---- texture ----------------------------------------------------------------------------
t = unreal.AssetImportTask()
t.set_editor_property("filename", os.path.join(SRC, "hair_atlas.png"))
t.set_editor_property("destination_path", DIR)
t.set_editor_property("destination_name", "T_HairAtlas")
t.set_editor_property("replace_existing", True)
t.set_editor_property("automated", True)
tools.import_asset_tasks([t])
tex = unreal.load_asset(DIR + "/T_HairAtlas")
tex.set_editor_property("srgb", False)
tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_SHARPEN4)
tex.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)   # tips never wrap onto roots
# keep the strands' coverage in the small mips, or the hair thins out and gaps open at a distance
tex.set_editor_property("do_scale_mips_for_alpha_coverage", True)
tex.set_editor_property("alpha_coverage_thresholds", unreal.Vector4(0, 0, 0, 0.5))

# ---- material ---------------------------------------------------------------------------
mel = unreal.MaterialEditingLibrary
MP = unreal.MaterialProperty
mpath = DIR + "/M_HairCards"
m = unreal.load_asset(mpath) if eal.does_asset_exist(mpath) else \
    tools.create_asset("M_HairCards", DIR, unreal.Material, unreal.MaterialFactoryNew())
mel.delete_all_material_expressions(m)
m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
LIT = HAIR["model"] == "lit"
m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT if LIT else unreal.MaterialShadingModel.MSM_HAIR)
m.set_editor_property("two_sided", True)
m.set_editor_property("opacity_mask_clip_value", HAIR["clip"])
m.set_editor_property("dithered_lod_transition", True)
m.set_editor_property("tangent_space_normal", True)
m.set_editor_property("used_with_skeletal_mesh", True)


def vec(name, v, x, y):
    n = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", unreal.LinearColor(v[0], v[1], v[2], 1))
    return n


def num(name, v, x, y):
    n = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", v)
    return n


ts = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -1100, 0)
ts.set_editor_property("parameter_name", "Atlas")
ts.set_editor_property("texture", tex)
ts.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
lerp = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -650, -150)
mel.connect_material_expressions(vec("RootColor", HAIR["root"], -900, -300), "", lerp, "A")
mel.connect_material_expressions(vec("TipColor", HAIR["tip"], -900, -200), "", lerp, "B")
mel.connect_material_expressions(ts, "B", lerp, "Alpha")
va = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -800, 100)
mel.connect_material_expressions(ts, "R", va, "A")
mel.connect_material_expressions(num("VariationScale", HAIR["variation"][1], -1000, 250), "", va, "B")
vb = mel.create_material_expression(m, unreal.MaterialExpressionAdd, -650, 100)
mel.connect_material_expressions(va, "", vb, "A")
mel.connect_material_expressions(num("VariationBase", HAIR["variation"][0], -1000, 330), "", vb, "B")
base = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -450, -100)
mel.connect_material_expressions(lerp, "", base, "A")
mel.connect_material_expressions(vb, "", base, "B")
mel.connect_material_property(base, "", MP.MP_BASE_COLOR)
# soft strand edges: dithered coverage that TSR resolves, not a hard alpha cut
gain = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -800, 440)
mel.connect_material_expressions(ts, "A", gain, "A")
mel.connect_material_expressions(num("Coverage", HAIR["coverage"], -1000, 480), "", gain, "B")
cov = mel.create_material_expression(m, unreal.MaterialExpressionSaturate, -650, 440)
mel.connect_material_expressions(gain, "", cov, "")
dith = mel.create_material_expression(m, unreal.MaterialExpressionMaterialFunctionCall, -450, 440)
dith.set_editor_property("material_function", unreal.load_asset("/Engine/Functions/Engine_MaterialFunctions02/Utility/DitherTemporalAA"))
assert mel.connect_material_expressions(cov, "", dith, "Alpha Threshold"), "DitherTemporalAA input"
# shadows use the hard mask: dithered shadow maps came out as grainy smudges on her arms (QA 0269)
pas = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -250, 440)
ins = []
for nm in ("Dithered", "Hard"):
    ci = unreal.CustomInput()
    ci.set_editor_property("input_name", nm)
    ins.append(ci)
pas.set_editor_property("inputs", ins)
pas.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
pas.set_editor_property("code", "#if SHADOW_DEPTH_SHADER\nreturn Hard;\n#else\nreturn Dithered;\n#endif")
assert mel.connect_material_expressions(dith, "", pas, "Dithered")
assert mel.connect_material_expressions(cov, "", pas, "Hard")
mel.connect_material_property(pas, "", MP.MP_OPACITY_MASK)
mel.connect_material_property(num("Roughness", HAIR["roughness"], -450, 200), "", MP.MP_ROUGHNESS)
mel.connect_material_property(num("Specular", HAIR["specular"], -450, 280), "", MP.MP_SPECULAR)
if LIT:
    mel.connect_material_property(num("Anisotropy", HAIR["anisotropy"], -450, 360), "", MP.MP_ANISOTROPY)
else:
    mel.connect_material_property(num("Scatter", HAIR["scatter"], -450, 360), "", MP.MP_METALLIC)  # Hair: Metallic pin = Scatter
# Backlit (CustomData0) has no MaterialProperty in 5.7 Python: the Hair model's default is used
tan = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -450, 520)
tan.set_editor_property("constant", unreal.LinearColor(0, 1, 0, 1))        # strands run along V
mel.connect_material_property(tan, "", MP.MP_TANGENT)
mel.recompile_material(m)

# ---- the cap's material: hair-coloured, fading out at the hairline (UV.x is the fade) ----------
cpath = DIR + "/M_HairCap"
cm = unreal.load_asset(cpath) if eal.does_asset_exist(cpath) else \
    tools.create_asset("M_HairCap", DIR, unreal.Material, unreal.MaterialFactoryNew())
mel.delete_all_material_expressions(cm)
cm.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
cm.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
cm.set_editor_property("opacity_mask_clip_value", 0.5)
cm.set_editor_property("used_with_skeletal_mesh", True)
cc_ = mel.create_material_expression(cm, unreal.MaterialExpressionVectorParameter, -500, -100)
cc_.set_editor_property("parameter_name", "RootColor")
cc_.set_editor_property("default_value", unreal.LinearColor(HAIR["root"][0], HAIR["root"][1], HAIR["root"][2], 1))
mel.connect_material_property(cc_, "", MP.MP_BASE_COLOR)
for nm, val, prop, y in (("Roughness", 0.85, MP.MP_ROUGHNESS, 50), ("Specular", 0.1, MP.MP_SPECULAR, 130)):
    k = mel.create_material_expression(cm, unreal.MaterialExpressionConstant, -500, y)
    k.set_editor_property("r", val)
    mel.connect_material_property(k, "", prop)
tc = mel.create_material_expression(cm, unreal.MaterialExpressionTextureCoordinate, -900, 300)
mk = mel.create_material_expression(cm, unreal.MaterialExpressionComponentMask, -700, 300)
for ch, on in (("r", True), ("g", False), ("b", False), ("a", False)):
    mk.set_editor_property(ch, on)
mel.connect_material_expressions(tc, "", mk, "")
cd = mel.create_material_expression(cm, unreal.MaterialExpressionMaterialFunctionCall, -450, 300)
cd.set_editor_property("material_function", unreal.load_asset("/Engine/Functions/Engine_MaterialFunctions02/Utility/DitherTemporalAA"))
assert mel.connect_material_expressions(mk, "", cd, "Alpha Threshold")
mel.connect_material_property(cd, "", MP.MP_OPACITY_MASK)
mel.recompile_material(cm)

mesh_mats = list(mesh.get_editor_property("materials"))
for x in mesh_mats:
    is_cap = "cap" in str(x.get_editor_property("material_slot_name")).lower()
    x.set_editor_property("material_interface", cm if is_cap else m)
mesh.set_editor_property("materials", mesh_mats)
print("slots", [(str(x.get_editor_property("material_slot_name")), x.get_editor_property("material_interface").get_name()) for x in mesh_mats])
for a in (mesh, tex, m, cm, unreal.load_asset(sk_path)):
    eal.save_loaded_asset(a)
print("hair imported:", mesh.get_path_name(), "bones", unreal.load_asset(sk_path).get_editor_property("skeleton_name") if False else "")
