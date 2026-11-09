"""Import the wardrobe (made by Tools/blender_outfit.py) and build its materials.

  python3 Tools/ue.py Tools/char_outfit.py        # all six; ONLY = ["Noir"] for some

- Imports SourceArt/Character/Outfits/outfit_<name>.fbx as SKM_Outfit_<Name> on Quinn's
  skeleton with every body morph (bust, glutes, jiggle).
- Builds one base material per fabric and one instance per outfit and fabric
  (MI_Outfit_<Outfit>_<Fabric>): colours and weights live in WARDROBE below.

  Sheer  see-through mesh that closes up at grazing angles     Satin  opaque, soft sheen
  Lace   sheer ground with fan embroidery                      Latex  black clear coat
  Net    fishnet: diamond cords, cut out between them          Trim   piping and hardware

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
                 Satin=dict(Color=BLACK), Trim=STEEL),
    "Garter": dict(Sheer=dict(Color=BLACK, Opacity=0.5), Lace=dict(Color=BLACK, Opacity=0.3, Cell=4.0),
                   Satin=dict(Color=BLACK), Trim=GOLD),
    "Cocoa": dict(Satin=dict(Color=(0.14, 0.05, 0.025), Roughness=0.3), Trim=GOLD),
    "Fishnet": dict(Net=dict(Color=BLACK, Cell=3.0, Width=0.16), Satin=dict(Color=BLACK), Trim=STEEL),
    "Catsuit": dict(Latex=dict(Color=(0.01, 0.014, 0.05)), Trim=dict(Color=(0.85, 0.85, 0.9), Roughness=0.25, Metallic=0.0)),
    "Sheer": dict(Sheer=dict(Color=BLACK, Opacity=0.66, EdgeExponent=1.5), Latex=dict(Color=(0.012, 0.013, 0.02)), Trim=STEEL),
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
    return m


def scalar(m, name, v, y):
    n = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -700, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", v)
    return n


def color(m, name, c, y):
    n = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -700, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", unreal.LinearColor(c[0], c[1], c[2], 1.0))
    return n


def custom(m, code, desc, inputs, y):
    node = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -450, y)
    node.set_editor_property("code", code)
    node.set_editor_property("description", desc)
    node.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    ins = []
    for n, _ in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    node.set_editor_property("inputs", ins)
    for n, src in inputs:
        if not mel.connect_material_expressions(src, "", node, n):
            raise RuntimeError(desc + " input " + n)
    return node


def sheer_opacity(m, base):
    """See-through head-on, closing up where you look along the surface."""
    fr = mel.create_material_expression(m, unreal.MaterialExpressionFresnel, -700, 300)
    mel.connect_material_expressions(scalar(m, "EdgeExponent", 2.0, 420), "", fr, "ExponentIn")
    lerp = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -450, 200)
    mel.connect_material_expressions(scalar(m, "Opacity", base, 120), "", lerp, "A")
    mel.connect_material_expressions(scalar(m, "EdgeOpacity", 0.97, 200), "", lerp, "B")
    mel.connect_material_expressions(fr, "", lerp, "Alpha")
    return lerp


def done(m):
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    return m


def build_materials():
    T = unreal.BlendMode.BLEND_TRANSLUCENT
    base = {}
    m = material("M_OutfitSheer", T)
    mel.connect_material_property(color(m, "Color", BLACK, -200), "", MP.MP_BASE_COLOR)
    mel.connect_material_property(scalar(m, "Roughness", 0.65, -80), "", MP.MP_ROUGHNESS)
    mel.connect_material_property(sheer_opacity(m, 0.5), "", MP.MP_OPACITY)
    base["Sheer"] = done(m)

    m = material("M_OutfitLace", T)
    mel.connect_material_property(color(m, "Color", BLACK, -200), "", MP.MP_BASE_COLOR)
    mel.connect_material_property(scalar(m, "Roughness", 0.75, -80), "", MP.MP_ROUGHNESS)
    uv = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -700, 560)
    lace = custom(m, LACE_HLSL, "Lace", [("UV", uv), ("Cell", scalar(m, "Cell", 3.5, 640))], 600)
    mx = mel.create_material_expression(m, unreal.MaterialExpressionMax, -200, 300)
    mel.connect_material_expressions(sheer_opacity(m, 0.35), "", mx, "A")
    mel.connect_material_expressions(lace, "", mx, "B")
    mel.connect_material_property(mx, "", MP.MP_OPACITY)
    base["Lace"] = done(m)

    m = material("M_OutfitNet", unreal.BlendMode.BLEND_MASKED)
    mel.connect_material_property(color(m, "Color", BLACK, -200), "", MP.MP_BASE_COLOR)
    mel.connect_material_property(scalar(m, "Roughness", 0.55, -80), "", MP.MP_ROUGHNESS)
    uv = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -700, 560)
    net = custom(m, NET_HLSL, "Net", [("UV", uv), ("Cell", scalar(m, "Cell", 3.0, 640)), ("Width", scalar(m, "Width", 0.16, 720))], 600)
    mel.connect_material_property(net, "", MP.MP_OPACITY_MASK)
    base["Net"] = done(m)

    m = material("M_OutfitSatin", unreal.BlendMode.BLEND_OPAQUE)
    mel.connect_material_property(color(m, "Color", BLACK, -200), "", MP.MP_BASE_COLOR)
    mel.connect_material_property(scalar(m, "Roughness", 0.36, -80), "", MP.MP_ROUGHNESS)
    base["Satin"] = done(m)

    m = material("M_OutfitLatex", unreal.BlendMode.BLEND_OPAQUE, unreal.MaterialShadingModel.MSM_CLEAR_COAT)
    m.set_editor_property("use_material_attributes", True)   # Python has no clear-coat pins
    attrs = mel.create_material_expression(m, unreal.MaterialExpressionMakeMaterialAttributes, -150, 0)
    for pin, n in (("BaseColor", color(m, "Color", (0.012, 0.013, 0.02), -200)), ("Roughness", scalar(m, "Roughness", 0.32, -80)),
                   ("ClearCoat", scalar(m, "Coat", 1.0, 40)), ("ClearCoatRoughness", scalar(m, "CoatRoughness", 0.06, 160))):
        if not mel.connect_material_expressions(n, "", attrs, pin):
            raise RuntimeError("pin " + pin)
    mel.connect_material_property(attrs, "", MP.MP_MATERIAL_ATTRIBUTES)
    base["Latex"] = done(m)

    m = material("M_OutfitTrim", unreal.BlendMode.BLEND_OPAQUE)
    mel.connect_material_property(color(m, "Color", STEEL["Color"], -200), "", MP.MP_BASE_COLOR)
    mel.connect_material_property(scalar(m, "Roughness", 0.28, -80), "", MP.MP_ROUGHNESS)
    mel.connect_material_property(scalar(m, "Metallic", 1.0, 40), "", MP.MP_METALLIC)
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
