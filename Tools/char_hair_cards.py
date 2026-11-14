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
              elf=("hair_elf.fbx", "SKM_HairElf", "SK_HairCards"),
              short=("hair_short.fbx", "SKM_HairShort", "SK_HairCards"))   # adds its "top" chains to the skeleton
# A style with its own colours gets material instances of M_HairCards / M_HairCap (the masters
# stay the black hair). Vertex colour B on its cards is 1 - copper (blender_hair_cards.py).
TINTS = dict(short=dict(
    # auburn, kept a little less red than a neutral-light auburn: the plaza's light is orange already
    root=(0.050, 0.026, 0.014), tip=(0.215, 0.110, 0.048),
    streak=(0.38, 0.165, 0.050),                              # the copper fringe panel
    cap=(0.075, 0.038, 0.019),
    roughness=0.7, specular=0.08, anisotropy=-0.6,
    graze_fade=1.0,                 # no white glint on cards seen edge-on against the sun
    root_fade=0.16,
    both_sides=1.0,                 # the underside of a flicked lock is lit like its top
    atlas=("hair_atlas_fine.png", "T_HairAtlasFine"),   # finer strands, ragged roots (hair_textures.py)
    variation=(0.6, 0.7),           # a little calmer strand-to-strand contrast than the black hair
))
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
smd.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)   # baked occlusion
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
skel_path = skel.get_path_name().split(".")[0]
if skel_path != sk_path:
    # only ever name a skeleton this import just made: the importer can also pick an existing one
    # in the folder, and renaming that one re-points every other style (it did, 2026-11-14)
    assert skel_path.endswith("_Skeleton") and not eal.does_asset_exist(sk_path), \
        "the mesh landed on %s, expected %s" % (skel_path, sk_path)
    unreal.EditorAssetLibrary.rename_asset(skel_path, sk_path)

# ---- texture ----------------------------------------------------------------------------
KEEP_SHARED = STYLE in TINTS and eal.does_asset_exist(DIR + "/T_HairAtlas") and eal.does_asset_exist(DIR + "/M_HairCap")
t = unreal.AssetImportTask()
t.set_editor_property("filename", os.path.join(SRC, "hair_atlas.png"))
t.set_editor_property("destination_path", DIR)
t.set_editor_property("destination_name", "T_HairAtlas")
t.set_editor_property("replace_existing", True)
t.set_editor_property("automated", True)
if not KEEP_SHARED:
    tools.import_asset_tasks([t])
tex = unreal.load_asset(DIR + "/T_HairAtlas")
if not KEEP_SHARED:
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
# occlusion baked into the vertex colour by blender_hair_cards.py: inner layers and roots darker
vcol = mel.create_material_expression(m, unreal.MaterialExpressionVertexColor, -650, -420)
# a second colour for chosen locks (vertex B = 1 - amount; 1 on the black styles, so no change there)
sk_ = mel.create_material_expression(m, unreal.MaterialExpressionOneMinus, -450, -420)
mel.connect_material_expressions(vcol, "B", sk_, "")
sgrad = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -650, -560)
c055 = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -850, -600)
c055.set_editor_property("r", 0.55)
c1 = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -850, -540)
c1.set_editor_property("r", 1.0)
mel.connect_material_expressions(c055, "", sgrad, "A")
mel.connect_material_expressions(c1, "", sgrad, "B")
mel.connect_material_expressions(ts, "B", sgrad, "Alpha")
scol = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -450, -560)
mel.connect_material_expressions(vec("StreakColor", HAIR["tip"], -650, -700), "", scol, "A")
mel.connect_material_expressions(sgrad, "", scol, "B")
scol2 = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -300, -500)
mel.connect_material_expressions(scol, "", scol2, "A")
mel.connect_material_expressions(vb, "", scol2, "B")
smix = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -150, -300)
mel.connect_material_expressions(base, "", smix, "A")
mel.connect_material_expressions(scol2, "", smix, "B")
mel.connect_material_expressions(sk_, "", smix, "Alpha")
base = smix
occ = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -250, -100)
mel.connect_material_expressions(base, "", occ, "A")
mel.connect_material_expressions(vcol, "R", occ, "B")
mel.connect_material_property(occ, "", MP.MP_BASE_COLOR)
# soft strand edges: dithered coverage that TSR resolves, not a hard alpha cut
gain = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -800, 440)
mel.connect_material_expressions(ts, "A", gain, "A")
mel.connect_material_expressions(num("Coverage", HAIR["coverage"], -1000, 480), "", gain, "B")
cov0 = mel.create_material_expression(m, unreal.MaterialExpressionSaturate, -650, 440)
mel.connect_material_expressions(gain, "", cov0, "")
# RootFade: coverage ramps in over this share of a card's length, on cards whose vertex alpha is 1
# (a layered cut shows its upper layers' roots). 0 on the black styles: no change.
rf = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -1000, 620)
mel.connect_material_expressions(num("RootFade", 0.0, -1200, 600), "", rf, "A")
mel.connect_material_expressions(vcol, "A", rf, "B")
rmax = mel.create_material_expression(m, unreal.MaterialExpressionMax, -850, 620)
ceps = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -1000, 700)
ceps.set_editor_property("r", 0.0001)
mel.connect_material_expressions(rf, "", rmax, "A")
mel.connect_material_expressions(ceps, "", rmax, "B")
rdiv = mel.create_material_expression(m, unreal.MaterialExpressionDivide, -700, 620)
mel.connect_material_expressions(ts, "B", rdiv, "A")
mel.connect_material_expressions(rmax, "", rdiv, "B")
rsat = mel.create_material_expression(m, unreal.MaterialExpressionSaturate, -560, 620)
mel.connect_material_expressions(rdiv, "", rsat, "")
cov = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -520, 500)
mel.connect_material_expressions(cov0, "", cov, "A")
mel.connect_material_expressions(rsat, "", cov, "B")
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
spec = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -250, 280)
mel.connect_material_expressions(num("Specular", HAIR["specular"], -450, 280), "", spec, "A")
mel.connect_material_expressions(vcol, "G", spec, "B")       # a little less highlight deep inside the mass
# GrazeFade 1: no highlight where a card is seen edge-on. With the sun ahead and the camera behind,
# cards on the crown went white (light hair shows it; lowering Specular alone did not). 0: as before.
fr = mel.create_material_expression(m, unreal.MaterialExpressionFresnel, -450, 760)
fr.set_editor_property("exponent", 1.5)
fr.set_editor_property("base_reflect_fraction", 0.0)
fri = mel.create_material_expression(m, unreal.MaterialExpressionOneMinus, -300, 760)
mel.connect_material_expressions(fr, "", fri, "")
gz = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -150, 720)
gone = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -300, 700)
gone.set_editor_property("r", 1.0)
mel.connect_material_expressions(gone, "", gz, "A")
mel.connect_material_expressions(fri, "", gz, "B")
mel.connect_material_expressions(num("GrazeFade", 0.0, -300, 820), "", gz, "Alpha")
spec2 = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -50, 300)
mel.connect_material_expressions(spec, "", spec2, "A")
mel.connect_material_expressions(gz, "", spec2, "B")
spec = spec2
mel.connect_material_property(spec, "", MP.MP_SPECULAR)
if LIT:
    mel.connect_material_property(num("Anisotropy", HAIR["anisotropy"], -450, 360), "", MP.MP_ANISOTROPY)
else:
    mel.connect_material_property(num("Scatter", HAIR["scatter"], -450, 360), "", MP.MP_METALLIC)  # Hair: Metallic pin = Scatter
# Backlit (CustomData0) has no MaterialProperty in 5.7 Python: the Hair model's default is used
# BothSides 1: a card's back face keeps the front's normal (a two-sided material flips it, and
# the underside of a lock that flicks up went dark). 0 on the black styles: as before.
tss = mel.create_material_expression(m, unreal.MaterialExpressionTwoSidedSign, -900, 640)
one_ = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -900, 700)
one_.set_editor_property("r", 1.0)
nz = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -700, 660)
mel.connect_material_expressions(one_, "", nz, "A")
mel.connect_material_expressions(tss, "", nz, "B")
mel.connect_material_expressions(num("BothSides", 0.0, -900, 760), "", nz, "Alpha")
nxy = mel.create_material_expression(m, unreal.MaterialExpressionConstant2Vector, -700, 600)
nxy.set_editor_property("r", 0.0)
nxy.set_editor_property("g", 0.0)
nvec = mel.create_material_expression(m, unreal.MaterialExpressionAppendVector, -500, 640)
mel.connect_material_expressions(nxy, "", nvec, "A")
mel.connect_material_expressions(nz, "", nvec, "B")
mel.connect_material_property(nvec, "", MP.MP_NORMAL)
tan = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -450, 520)
tan.set_editor_property("constant", unreal.LinearColor(0, 1, 0, 1))        # strands run along V
mel.connect_material_property(tan, "", MP.MP_TANGENT)
mel.recompile_material(m)

# ---- the cap's material: hair-coloured, fading out at the hairline (UV.x is the fade) ----------
cpath = DIR + "/M_HairCap"
cm = unreal.load_asset(cpath) if eal.does_asset_exist(cpath) else \
    tools.create_asset("M_HairCap", DIR, unreal.Material, unreal.MaterialFactoryNew())
if not KEEP_SHARED:                  # a tinted style leaves the shared cap master as it is
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

# ---- a tinted style: instances of the two masters, set on its mesh ------------------------------
card_mat, cap_mat, extra = m, cm, []
if STYLE in TINTS:
    T_ = TINTS[STYLE]
    suffix = MESH.replace("SKM_Hair", "")           # SKM_HairShort -> Short


    def instance(name, parent):
        ipath = DIR + "/" + name
        mi = unreal.load_asset(ipath) if eal.does_asset_exist(ipath) else \
            tools.create_asset(name, DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, parent)
        return mi


    def col(mi, name, v):
        mel.set_material_instance_vector_parameter_value(mi, name, unreal.LinearColor(v[0], v[1], v[2], 1))


    card_mat = instance("MI_Hair" + suffix, m)
    col(card_mat, "RootColor", T_["root"])
    col(card_mat, "TipColor", T_["tip"])
    col(card_mat, "StreakColor", T_["streak"])
    for nm, key in (("Roughness", "roughness"), ("Specular", "specular"), ("Anisotropy", "anisotropy"),
                    ("BothSides", "both_sides"), ("RootFade", "root_fade"),
                    ("GrazeFade", "graze_fade")):
        mel.set_material_instance_scalar_parameter_value(card_mat, nm, T_[key])
    if "variation" in T_:
        mel.set_material_instance_scalar_parameter_value(card_mat, "VariationBase", T_["variation"][0])
        mel.set_material_instance_scalar_parameter_value(card_mat, "VariationScale", T_["variation"][1])
    if "atlas" in T_:
        at = unreal.AssetImportTask()
        at.set_editor_property("filename", os.path.join(SRC, T_["atlas"][0]))
        at.set_editor_property("destination_path", DIR)
        at.set_editor_property("destination_name", T_["atlas"][1])
        at.set_editor_property("replace_existing", True)
        at.set_editor_property("automated", True)
        # import once (REIMPORT_ATLAS = True after hair_textures.py changes): a re-import streams
        # in late, and shots taken straight after show low mips, solid feather-like cards
        if not eal.does_asset_exist(DIR + "/" + T_["atlas"][1]) or globals().get("REIMPORT_ATLAS"):
            tools.import_asset_tasks([at])
        ftex = unreal.load_asset(DIR + "/" + T_["atlas"][1])
        ftex.set_editor_property("never_stream", True)      # 2048x1024: always sharp, also on a camera cut
        ftex.set_editor_property("srgb", False)
        ftex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        ftex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_SHARPEN4)
        ftex.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
        ftex.set_editor_property("do_scale_mips_for_alpha_coverage", True)
        ftex.set_editor_property("alpha_coverage_thresholds", unreal.Vector4(0, 0, 0, 0.5))
        mel.set_material_instance_texture_parameter_value(card_mat, "Atlas", ftex)
        extra_tex = [ftex]
    else:
        extra_tex = []
    cap_mat = instance("MI_HairCap" + suffix, cm)
    col(cap_mat, "RootColor", T_["cap"])
    mel.update_material_instance(card_mat)
    mel.update_material_instance(cap_mat)
    extra = extra_tex + [card_mat, cap_mat]

mesh_mats = list(mesh.get_editor_property("materials"))
for x in mesh_mats:
    is_cap = "cap" in str(x.get_editor_property("material_slot_name")).lower()
    x.set_editor_property("material_interface", cap_mat if is_cap else card_mat)
mesh.set_editor_property("materials", mesh_mats)
print("slots", [(str(x.get_editor_property("material_slot_name")), x.get_editor_property("material_interface").get_name()) for x in mesh_mats])
for a in [mesh, m, unreal.load_asset(sk_path)] + ([] if KEEP_SHARED else [tex, cm]) + extra:
    eal.save_loaded_asset(a)
print("hair imported:", mesh.get_path_name())
