"""Import the wardrobe (made by Tools/blender_outfit.py) and build its materials.

  python3 Tools/ue.py Tools/char_outfit.py        # all six; ONLY = ["Noir"] for some

- Imports SourceArt/Character/Outfits/outfit_<name>.fbx as SKM_Outfit_<Name> on Quinn's
  skeleton with every body morph (bust, glutes, jiggle).
- Builds one base material per fabric and one instance per outfit and fabric
  (MI_Outfit_<Outfit>_<Fabric>): colours and weights live in WARDROBE below.

  Sheer  see-through mesh that closes up at grazing angles     Satin  opaque, soft sheen
  Lace   sheer ground with fan embroidery                      Latex  black clear coat
  Net    fishnet: diamond cords, cut out between them          Trim   piping and hardware

Detail comes from three 2K textures (Tools/blender_outfit_textures.py): a satin weave normal,
a latex ripple normal and a mask for roughness variation, so nothing reads as flat plastic.

Lace and Net read UV0, the garment coordinate in cm written by blender_outfit.py.
She wears an outfit through its row in FVisexBodyProfile::Outfits() (the DRESS tab).
"""
import os
import unreal

BLACK = (0.012, 0.012, 0.016)
STEEL = dict(Color=(0.78, 0.8, 0.84), Roughness=0.28, Metallic=1.0)
GOLD = dict(Color=(0.9, 0.62, 0.3), Roughness=0.3, Metallic=1.0)
WARDROBE = {
    "Noir": dict(Sheer=dict(Color=BLACK, Opacity=0.5), Lace=dict(Color=BLACK, Opacity=0.5, Cell=3.0),
                 Satin=dict(Color=BLACK, Roughness=0.3, RoughnessVariation=0.14), Trim=STEEL),
    "Garter": dict(Sheer=dict(Color=BLACK, Opacity=0.5), Lace=dict(Color=BLACK, Opacity=0.3, Cell=4.0),
                   Satin=dict(Color=BLACK, Roughness=0.42, RoughnessVariation=0.2), Trim=GOLD),
    "Cocoa": dict(Satin=dict(Color=(0.14, 0.05, 0.025), Roughness=0.26, RoughnessVariation=0.18, NormalStrength=0.9), Trim=GOLD),
    "Fishnet": dict(Net=dict(Color=BLACK, Cell=3.0, Width=0.16), Satin=dict(Color=BLACK, Roughness=0.34, RoughnessVariation=0.16), Trim=STEEL),
    "Catsuit": dict(Latex=dict(Color=(0.01, 0.014, 0.05), Roughness=0.24, CoatRoughness=0.04, CoatVariation=0.04), Trim=dict(Color=(0.85, 0.85, 0.9), Roughness=0.25, Metallic=0.0)),
    "Sheer": dict(Sheer=dict(Color=BLACK, Opacity=0.66, EdgeExponent=1.5), Latex=dict(Color=(0.012, 0.013, 0.02), Roughness=0.34, CoatRoughness=0.06, CoatVariation=0.07, NormalStrength=0.6), Trim=STEEL),
}
MESH_DIR = {"Sheer": "/Game/Character/Clothes"}       # the rest: /Game/Character/Outfits
DIR = "/Game/Character/Clothes"
SKELETON = "/Game/Characters/Mannequins/Meshes/SK_Mannequin"
ONLY = globals().get("ONLY")
SRC = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "SourceArt", "Character", "Outfits")

if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise RuntimeError("Play mode is running: stop PIE before importing")

mel = unreal.MaterialEditingLibrary
MP = unreal.MaterialProperty
tools = unreal.AssetToolsHelpers.get_asset_tools()

NET_HLSL = """
float2 p = float2(UV.x, 1.0 - UV.y) / Cell;
float a = abs(frac(p.x + p.y) - 0.5);
float b = abs(frac(p.x - p.y) - 0.5);
float d = 0.5 - max(a, b);
float aa = max(fwidth(d), 1e-4);
return 1.0 - smoothstep(Width * 0.5 - aa, Width * 0.5 + aa, d);
"""

LACE_HLSL = """
float2 p = float2(UV.x, 1.0 - UV.y) / Cell;
float m = 0.0;
float jr = floor(p.y * 2.0);
for (int r = 0; r < 3; r++)
{
    float j = jr - r;
    float off = 0.5 * fmod(abs(j), 2.0);
    float bi = floor(p.x - off + 0.5);
    for (int k = -1; k <= 1; k++)
    {
        float2 d = p - float2(bi + k + off, j * 0.5);
        float rad = length(d) / 0.62;
        if (d.y >= 0.0 && rad < 1.0)
        {
            float ang = atan2(d.x, d.y);
            float rim = smoothstep(0.10, 0.04, abs(rad - 0.93));
            float arc = smoothstep(0.07, 0.02, abs(rad - 0.58));
            float ribd = abs(frac(ang * 9.0 / 3.14159265 + 0.5) - 0.5) * (3.14159265 / 9.0) * rad;
            float ribs = smoothstep(0.045, 0.015, ribd) * step(0.22, rad);
            float core = smoothstep(0.26, 0.2, rad);
            m = max(m, max(max(rim, arc), max(ribs, core)));
        }
    }
}
return m;
"""


TULLE_HLSL = """
float2 p = float2(UV.x, 1.0 - UV.y) / Cell;
float a = abs(frac(p.x + p.y) - 0.5);
float b = abs(frac(p.x - p.y) - 0.5);
float d = 0.5 - max(a, b);
float fw = fwidth(p.x) + fwidth(p.y);
float thread = 1.0 - smoothstep(Width * 0.5 - fw, Width * 0.5 + fw, d);
float mean = 2.0 * Width - Width * Width;
return saturate(Base * lerp(thread, mean, saturate(fw * 1.5)) / mean);
"""

NET_NORMAL_HLSL = """
float2 p = float2(UV.x, 1.0 - UV.y) / Cell;
float sa = frac(p.x + p.y) - 0.5;
float sb = frac(p.x - p.y) - 0.5;
float2 n2;
if (abs(sa) > abs(sb)) { n2 = float2(1.0, 1.0) * (-(0.5 - abs(sa)) * sign(sa)); }
else { n2 = float2(1.0, -1.0) * (-(0.5 - abs(sb)) * sign(sb)); }
n2 = clamp(n2 / (Width * 0.5), -1.0, 1.0) * Bulge;
return normalize(float3(n2.x, -n2.y, 1.0));
"""

TEX_DIR = DIR + "/Textures"


def texture(name, normal):
    """Import a detail texture: linear, normal or masks compression, never streamed."""
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(SRC, "Textures", name + ".png"))
    task.set_editor_property("destination_path", TEX_DIR)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    tools.import_asset_tasks([task])
    t = unreal.load_asset(TEX_DIR + "/" + name)
    t.set_editor_property("srgb", False)
    t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP if normal
                          else unreal.TextureCompressionSettings.TC_MASKS)
    t.set_editor_property("never_stream", True)
    t.set_editor_property("lod_bias", 0)
    unreal.EditorAssetLibrary.save_loaded_asset(t)
    return t


def material(name, blend, model=unreal.MaterialShadingModel.MSM_DEFAULT_LIT):
    path = DIR + "/" + name
    m = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else \
        tools.create_asset(name, DIR, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(m)
    m.set_editor_property("used_with_skeletal_mesh", True)
    m.set_editor_property("used_with_morph_targets", True)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("use_material_attributes", False)
    m.set_editor_property("blend_mode", blend)
    m.set_editor_property("shading_model", model)
    if blend == unreal.BlendMode.BLEND_TRANSLUCENT:
        m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
        try:      # motion vectors for the upscaler: without them thin cloth smears as she moves
            m.set_editor_property("output_translucent_velocity", True)
        except Exception as e:
            print("no translucent velocity:", e)
    return m


def node(m, cls, x, y, **props):
    n = mel.create_material_expression(m, cls, x, y)
    for k, v in props.items():
        n.set_editor_property(k, v)
    return n


def link(src, dst, pin, out=""):
    if not mel.connect_material_expressions(src, out, dst, pin):
        raise RuntimeError("link %s -> %s" % (out, pin))
    return dst


def scalar(m, name, v, y):
    return node(m, unreal.MaterialExpressionScalarParameter, -900, y, parameter_name=name, default_value=v)


def color(m, name, c, y):
    return node(m, unreal.MaterialExpressionVectorParameter, -900, y, parameter_name=name,
                default_value=unreal.LinearColor(c[0], c[1], c[2], 1.0))


def const(m, v, y):
    return node(m, unreal.MaterialExpressionConstant, -900, y, r=v)


def op(m, cls, a, b, y, out_a="", out_b=""):
    n = node(m, cls, -400, y)
    link(a, n, "A", out_a)
    link(b, n, "B", out_b)
    return n


def sample(m, name, tex, normal, span, y):
    """A detail texture repeated every `span` cm of garment UV."""
    uv = node(m, unreal.MaterialExpressionTextureCoordinate, -1300, y, u_tiling=1.0 / span, v_tiling=1.0 / span)
    t = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -1100, y, parameter_name=name, texture=tex,
             sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    return link(uv, t, "UVs")


def detail_normal(m, tex, span, strength, y):
    flat = node(m, unreal.MaterialExpressionConstant3Vector, -900, y + 60, constant=unreal.LinearColor(0, 0, 1, 1))
    lerp = node(m, unreal.MaterialExpressionLinearInterpolate, -400, y)
    link(flat, lerp, "A")
    link(sample(m, "DetailNormal", tex, True, span, y), lerp, "B", "RGB")
    link(scalar(m, "NormalStrength", strength, y + 120), lerp, "Alpha")
    return lerp


def varied(m, base, mask, channel, amount_name, amount, y):
    """base + (mask - 0.5) * amount: a surface that is not the same everywhere."""
    d = op(m, unreal.MaterialExpressionSubtract, mask, const(m, 0.5, y + 40), y, out_a=channel)
    return op(m, unreal.MaterialExpressionAdd, base, op(m, unreal.MaterialExpressionMultiply, d, scalar(m, amount_name, amount, y + 80), y + 40), y)


def custom(m, code, desc, inputs, y, out=unreal.CustomMaterialOutputType.CMOT_FLOAT1):
    n = node(m, unreal.MaterialExpressionCustom, -650, y, code=code, description=desc, output_type=out)
    ins = []
    for name, _ in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        ins.append(ci)
    n.set_editor_property("inputs", ins)
    for name, src in inputs:
        link(src, n, name)
    return n


def sheer_opacity(m, base, y=200):
    """See-through head-on, closing up where you look along the surface; up close the
    cloth resolves into a fine tulle mesh instead of flat tint."""
    fr = node(m, unreal.MaterialExpressionFresnel, -900, y + 100)
    link(scalar(m, "EdgeExponent", 2.0, y + 220), fr, "ExponentIn")
    lerp = node(m, unreal.MaterialExpressionLinearInterpolate, -700, y)
    link(scalar(m, "Opacity", base, y - 80), lerp, "A")
    link(scalar(m, "EdgeOpacity", 0.97, y), lerp, "B")
    link(fr, lerp, "Alpha")
    uv = node(m, unreal.MaterialExpressionTextureCoordinate, -900, y + 300)
    return custom(m, TULLE_HLSL, "Tulle", [("UV", uv), ("Cell", scalar(m, "TulleCell", 0.14, y + 360)),
                                           ("Width", scalar(m, "TulleThread", 0.45, y + 420)), ("Base", lerp)], y + 300)


def done(m):
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    return m


def build_materials():
    T = unreal.BlendMode.BLEND_TRANSLUCENT
    weave, latex_n, detail = texture("T_Outfit_Weave_N", True), texture("T_Outfit_Latex_N", True), texture("T_Outfit_Detail_M", False)
    base = {}

    m = material("M_OutfitSheer", T)
    mel.connect_material_property(color(m, "Color", BLACK, -200), "", MP.MP_BASE_COLOR)
    mel.connect_material_property(scalar(m, "Roughness", 0.65, -80), "", MP.MP_ROUGHNESS)
    mel.connect_material_property(sheer_opacity(m, 0.5), "", MP.MP_OPACITY)
    base["Sheer"] = done(m)

    m = material("M_OutfitLace", T)
    mel.connect_material_property(color(m, "Color", BLACK, -200), "", MP.MP_BASE_COLOR)
    mel.connect_material_property(scalar(m, "Roughness", 0.75, -80), "", MP.MP_ROUGHNESS)
    uv = node(m, unreal.MaterialExpressionTextureCoordinate, -900, 900)
    lace = custom(m, LACE_HLSL, "Lace", [("UV", uv), ("Cell", scalar(m, "Cell", 3.5, 960))], 900)
    mel.connect_material_property(op(m, unreal.MaterialExpressionMax, sheer_opacity(m, 0.35), lace, 500), "", MP.MP_OPACITY)
    base["Lace"] = done(m)

    m = material("M_OutfitNet", unreal.BlendMode.BLEND_MASKED)
    mel.connect_material_property(color(m, "Color", BLACK, -200), "", MP.MP_BASE_COLOR)
    mel.connect_material_property(scalar(m, "Roughness", 0.5, -80), "", MP.MP_ROUGHNESS)
    uv = node(m, unreal.MaterialExpressionTextureCoordinate, -900, 560)
    cell, width = scalar(m, "Cell", 3.0, 640), scalar(m, "Width", 0.16, 720)
    mel.connect_material_property(custom(m, NET_HLSL, "Net", [("UV", uv), ("Cell", cell), ("Width", width)], 600), "", MP.MP_OPACITY_MASK)
    mel.connect_material_property(custom(m, NET_NORMAL_HLSL, "NetNormal", [("UV", uv), ("Cell", cell), ("Width", width),
                                                                          ("Bulge", scalar(m, "Bulge", 1.2, 800))], 800,
                                         unreal.CustomMaterialOutputType.CMOT_FLOAT3), "", MP.MP_NORMAL)
    base["Net"] = done(m)

    m = material("M_OutfitSatin", unreal.BlendMode.BLEND_OPAQUE)
    mel.connect_material_property(color(m, "Color", BLACK, -200), "", MP.MP_BASE_COLOR)
    mask = sample(m, "DetailMask", detail, False, 24.0, 300)
    mel.connect_material_property(varied(m, scalar(m, "Roughness", 0.36, -80), mask, "R", "RoughnessVariation", 0.16, 0), "", MP.MP_ROUGHNESS)
    mel.connect_material_property(detail_normal(m, weave, 3.0, 0.7, 600), "", MP.MP_NORMAL)
    base["Satin"] = done(m)

    m = material("M_OutfitLatex", unreal.BlendMode.BLEND_OPAQUE, unreal.MaterialShadingModel.MSM_CLEAR_COAT)
    m.set_editor_property("use_material_attributes", True)   # Python has no clear-coat pins
    attrs = node(m, unreal.MaterialExpressionMakeMaterialAttributes, -150, 0)
    mask = sample(m, "DetailMask", detail, False, 24.0, 500)
    for pin, n in (("BaseColor", color(m, "Color", (0.012, 0.013, 0.02), -300)),
                   ("Roughness", varied(m, scalar(m, "Roughness", 0.3, -200), mask, "B", "RoughnessVariation", 0.12, -120)),
                   ("ClearCoat", scalar(m, "Coat", 1.0, 100)),
                   # the coat stays near mirror-sharp; smudges only dull it a little in places
                   ("ClearCoatRoughness", varied(m, scalar(m, "CoatRoughness", 0.05, 200), mask, "R", "CoatVariation", 0.05, 280)),
                   ("Normal", detail_normal(m, latex_n, 12.0, 0.45, 800))):
        link(n, attrs, pin)
    mel.connect_material_property(attrs, "", MP.MP_MATERIAL_ATTRIBUTES)
    base["Latex"] = done(m)

    m = material("M_OutfitTrim", unreal.BlendMode.BLEND_OPAQUE)
    mel.connect_material_property(color(m, "Color", STEEL["Color"], -200), "", MP.MP_BASE_COLOR)
    mask = sample(m, "DetailMask", detail, False, 3.0, 300)
    mel.connect_material_property(varied(m, scalar(m, "Roughness", 0.28, -80), mask, "G", "RoughnessVariation", 0.12, 0), "", MP.MP_ROUGHNESS)
    mel.connect_material_property(scalar(m, "Metallic", 1.0, 160), "", MP.MP_METALLIC)
    base["Trim"] = done(m)
    return base


def instance(outfit, slot, parent, values):
    name = "MI_Outfit_%s_%s" % (outfit, slot)
    path = DIR + "/" + name
    mi = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else \
        tools.create_asset(name, DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    for k, v in values.items():
        if isinstance(v, tuple):
            mel.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(v[0], v[1], v[2], 1.0))
        else:
            mel.set_material_instance_scalar_parameter_value(mi, k, float(v))
    mel.update_material_instance(mi)
    unreal.EditorAssetLibrary.save_loaded_asset(mi)
    return mi


def import_mesh(outfit):
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
    sk.set_editor_property("import_uniform_scale", 0.01)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(SRC, "outfit_%s.fbx" % outfit.lower()))
    task.set_editor_property("destination_path", MESH_DIR.get(outfit, "/Game/Character/Outfits"))
    task.set_editor_property("destination_name", "SKM_Outfit_" + outfit)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", ui)
    tools.import_asset_tasks([task])
    mesh = unreal.load_asset(MESH_DIR.get(outfit, "/Game/Character/Outfits") + "/SKM_Outfit_" + outfit)
    if not mesh:
        raise RuntimeError("import failed: " + outfit)
    return mesh


base = build_materials()
for outfit, fabrics in WARDROBE.items():
    if ONLY and outfit not in ONLY:
        continue
    mis = {slot: instance(outfit, slot, base[slot], values) for slot, values in fabrics.items()}
    if globals().get("MATERIALS_ONLY"):
        mesh = unreal.load_asset(MESH_DIR.get(outfit, "/Game/Character/Outfits") + "/SKM_Outfit_" + outfit)
    else:
        mesh = import_mesh(outfit)
    mats = list(mesh.get_editor_property("materials"))
    used = []
    for sm in mats:
        slot = str(sm.get_editor_property("material_slot_name"))
        pick = mis.get(slot) or base.get(slot)
        if pick:
            sm.set_editor_property("material_interface", pick)
        used.append("%s=%s" % (slot, pick.get_name() if pick else "?"))
    mesh.set_editor_property("materials", mats)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    print(outfit, "height", round(mesh.get_bounds().box_extent.z * 2, 1), "morphs", len(mesh.get_editor_property("morph_targets")), used)
