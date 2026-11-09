"""Fabric materials and the outfits, in UE. Idempotent; stop Play first.

  python3 Tools/ue.py Tools/char_outfits.py

- M_Fabric: the textile master. Cloth shading (a soft sheen at grazing angles, like satin
  and mesh), masked. Its edge comes from the outfit's vertex colour (R = coverage, cut at
  0.5; Tools/blender_outfits.py writes it). Patterns are fixed to the body (pre-skinned
  position), so they stay put when she moves:
    Sheer  < 1: see-through by dithering (0.55 = sheer mesh, 1 = opaque)
    Net    = 1: diamond fishnet (NetScale cm between strands, NetWidth = strand share)
    Lace   = 1: lace (noise motifs on a fine net)
- M_FabricLatex: the glossy one (clear coat), same coverage edge.
- Imports SourceArt/Character/Outfits/outfit_*.fbx as /Game/Character/Outfits/SKM_Outfit_*
  on Quinn's skeleton (with the skin's morph targets) and gives each slot its fabric.
The body component equips them (C++: FVisexBodyProfile::Outfits()).
"""
import os
import unreal

DIR = "/Game/Character/Outfits"
SKELETON = "/Game/Characters/Mannequins/Meshes/SK_Mannequin"
SRC = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "SourceArt", "Character", "Outfits")

# (outfit, slot) -> look. Colours are linear.
BLACK = (0.012, 0.012, 0.014)
LOOKS = {
    "Noir": {
        "Sheer": dict(Color=BLACK, Sheer=0.55, Roughness=0.45, Fuzz=(0.10, 0.10, 0.11)),
        "Solid": dict(Color=BLACK, Roughness=0.30, Fuzz=(0.20, 0.20, 0.22)),
        "Lace": dict(Color=BLACK, Lace=1.0, Sheer=0.85, Roughness=0.45),
    },
    "Garter": {
        "Lace": dict(Color=BLACK, Lace=1.0, Sheer=0.85, Roughness=0.45),
        "Solid": dict(Color=BLACK, Roughness=0.30, Fuzz=(0.20, 0.20, 0.22)),
        "Sheer": dict(Color=BLACK, Sheer=0.60, Roughness=0.45),
    },
    "Cocoa": {
        "Solid": dict(Color=(0.10, 0.045, 0.025), Roughness=0.28, Fuzz=(0.45, 0.30, 0.22)),
    },
    "Fishnet": {
        "Solid": dict(Color=BLACK, Roughness=0.30, Fuzz=(0.20, 0.20, 0.22)),
        "Net": dict(Color=BLACK, Net=1.0, NetScale=1.6, NetWidth=0.16, Roughness=0.5),
    },
    "Catsuit": {
        "Latex": dict(Color=(0.035, 0.012, 0.075)),
    },
}

if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise RuntimeError("Play mode is running: stop PIE before importing")

at = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def asset(name, cls, factory, folder=DIR):
    path = folder + "/" + name
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    return at.create_asset(name, folder, cls, factory)


class Graph:
    def __init__(self, m):
        self.m = m

    def node(self, cls, x=0, y=0):
        return mel.create_material_expression(self.m, cls, x, y)

    def scalar(self, name, v, x=-1600, y=0):
        n = self.node(unreal.MaterialExpressionScalarParameter, x, y)
        n.set_editor_property("parameter_name", name)
        n.set_editor_property("default_value", v)
        return n

    def vector(self, name, c, x=-1600, y=0):
        n = self.node(unreal.MaterialExpressionVectorParameter, x, y)
        n.set_editor_property("parameter_name", name)
        n.set_editor_property("default_value", unreal.LinearColor(c[0], c[1], c[2], 1.0))
        return n

    def op(self, cls, a, b=None, ia="A", ib="B", out_a="", **consts):
        n = self.node(cls)
        mel.connect_material_expressions(a, out_a, n, ia)
        if b is not None:
            mel.connect_material_expressions(b, "", n, ib)
        for k, v in consts.items():
            n.set_editor_property(k, v)
        return n

    def mul(self, a, b=None, **c):
        return self.op(unreal.MaterialExpressionMultiply, a, b, **c)

    def add(self, a, b=None, **c):
        return self.op(unreal.MaterialExpressionAdd, a, b, **c)

    def sub(self, a, b=None, **c):
        return self.op(unreal.MaterialExpressionSubtract, a, b, **c)

    def mx(self, a, b):
        return self.op(unreal.MaterialExpressionMax, a, b)

    def sat(self, a):
        return self.op(unreal.MaterialExpressionSaturate, a, ia="")

    def frac(self, a):
        return self.op(unreal.MaterialExpressionFrac, a, ia="")

    def absn(self, a):
        return self.op(unreal.MaterialExpressionAbs, a, ia="")

    def mask(self, a, r=False, g=False, b=False):
        n = self.node(unreal.MaterialExpressionComponentMask)
        mel.connect_material_expressions(a, "", n, "")
        n.set_editor_property("r", r)
        n.set_editor_property("g", g)
        n.set_editor_property("b", b)
        return n

    def lerp(self, a, b, alpha):
        n = self.node(unreal.MaterialExpressionLinearInterpolate)
        for src, pin in ((a, "A"), (b, "B"), (alpha, "Alpha")):
            mel.connect_material_expressions(src, "", n, pin)
        return n

    def const(self, v):
        n = self.node(unreal.MaterialExpressionConstant)
        n.set_editor_property("r", v)
        return n


def coverage(g):
    """1 inside the garment, 0 outside: the vertex colour cut sharply at 0.5."""
    vc = g.node(unreal.MaterialExpressionVertexColor, -1600, -600)
    c = g.node(unreal.MaterialExpressionSubtract)
    mel.connect_material_expressions(vc, "R", c, "A")
    c.set_editor_property("const_b", 0.5)
    return g.sat(g.add(g.mul(c, const_b=40.0), const_b=0.5))


def lines(g, coord, scale, width):
    """1 on the strands of a family of parallel lines: |frac(coord/scale) - 0.5| > 0.5 - width."""
    t = g.frac(g.op(unreal.MaterialExpressionDivide, coord, scale))
    d = g.absn(g.sub(t, const_b=0.5))
    edge = g.sub(g.const(0.5), width)
    return g.sat(g.mul(g.sub(d, edge), const_b=60.0))


def build_fabric():
    m = asset("M_Fabric", unreal.Material, unreal.MaterialFactoryNew(), "/Game/Character/Android")
    mel.delete_all_material_expressions(m)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_CLOTH)
    m.set_editor_property("used_with_skeletal_mesh", True)
    m.set_editor_property("use_material_attributes", True)
    m.set_editor_property("opacity_mask_clip_value", 0.5)
    m.set_editor_property("two_sided", True)
    g = Graph(m)
    cov = coverage(g)

    # pre-skinned position exists only per vertex: carry it to the pixels
    vs = g.node(unreal.MaterialExpressionPreSkinnedPosition, -1800, 400)
    pos = g.node(unreal.MaterialExpressionVertexInterpolator, -1600, 400)
    mel.connect_material_expressions(vs, "", pos, "VS")
    x, y, z = g.mask(pos, r=True), g.mask(pos, g=True), g.mask(pos, b=True)
    # fishnet: four families of slanted lines make diamonds on any side of a limb
    scale = g.scalar("NetScale", 1.6, -1600, 600)
    width = g.scalar("NetWidth", 0.16, -1600, 680)
    net = None
    for a, b, sign in ((x, z, 1), (x, z, -1), (y, z, 1), (y, z, -1)):
        coord = g.add(a, b) if sign > 0 else g.sub(a, b)
        l = lines(g, coord, scale, width)
        net = l if net is None else g.mx(net, l)
    # lace: noise motifs over a fine net
    noise = g.node(unreal.MaterialExpressionNoise)
    mel.connect_material_expressions(pos, "", noise, "Position")
    noise.set_editor_property("scale", 1.4)
    noise.set_editor_property("levels", 3)
    noise.set_editor_property("output_min", 0.0)
    noise.set_editor_property("output_max", 1.0)
    motif = g.sat(g.mul(g.sub(noise, const_b=0.52), const_b=12.0))
    fine = None
    for a, b, sign in ((x, z, 1), (x, z, -1), (y, z, 1), (y, z, -1)):
        coord = g.add(a, b) if sign > 0 else g.sub(a, b)
        l = lines(g, coord, g.const(0.45), g.const(0.12))
        fine = l if fine is None else g.mx(fine, l)
    lace = g.mx(motif, fine)

    use_net, use_lace = g.scalar("Net", 0.0, -1600, 800), g.scalar("Lace", 0.0, -1600, 880)
    one = g.const(1.0)
    pattern = g.mul(g.lerp(one, net, use_net), g.lerp(one, lace, use_lace))
    dither = g.node(unreal.MaterialExpressionMaterialFunctionCall)
    dither.set_material_function(unreal.load_asset("/Engine/Functions/Engine_MaterialFunctions02/Utility/DitherTemporalAA"))
    if not mel.connect_material_expressions(g.scalar("Sheer", 1.0, -1600, 960), "", dither, "Alpha Threshold"):
        raise RuntimeError("DitherTemporalAA input")
    opacity = g.mul(g.mul(cov, pattern), dither)

    attrs = g.node(unreal.MaterialExpressionMakeMaterialAttributes, 200, 0)
    color = g.vector("Color", BLACK, -1600, -300)
    for pin, src in (("BaseColor", color),
                     ("Roughness", g.scalar("Roughness", 0.35, -1600, -200)),
                     ("Specular", g.scalar("Specular", 0.5, -1600, -150)),
                     ("SubsurfaceColor", g.vector("Fuzz", (0.15, 0.15, 0.16), -1600, -100)),   # cloth: fuzz colour
                     ("ClearCoat", g.scalar("Cloth", 0.8, -1600, -20)),                         # cloth: sheen amount
                     ("OpacityMask", opacity)):
        if not mel.connect_material_expressions(src, "", attrs, pin):
            raise RuntimeError("M_Fabric pin " + pin)
    mel.connect_material_property(attrs, "", unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def build_latex():
    m = asset("M_FabricLatex", unreal.Material, unreal.MaterialFactoryNew(), "/Game/Character/Android")
    mel.delete_all_material_expressions(m)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_CLEAR_COAT)
    m.set_editor_property("used_with_skeletal_mesh", True)
    m.set_editor_property("use_material_attributes", True)
    m.set_editor_property("opacity_mask_clip_value", 0.5)
    g = Graph(m)
    attrs = g.node(unreal.MaterialExpressionMakeMaterialAttributes, 200, 0)
    for pin, src in (("BaseColor", g.vector("Color", (0.035, 0.012, 0.075), -800, -300)),
                     ("Roughness", g.scalar("Roughness", 0.25, -800, -200)),
                     ("Specular", g.scalar("Specular", 0.6, -800, -150)),
                     ("ClearCoat", g.scalar("ClearCoat", 1.0, -800, -100)),
                     ("ClearCoatRoughness", g.scalar("ClearCoatRoughness", 0.04, -800, -50)),
                     ("OpacityMask", coverage(g))):
        if not mel.connect_material_expressions(src, "", attrs, pin):
            raise RuntimeError("M_FabricLatex pin " + pin)
    mel.connect_material_property(attrs, "", unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


fabric, latex = build_fabric(), build_latex()


def instance(outfit, slot, look):
    parent = latex if slot == "Latex" else fabric
    mi = asset("MI_%s_%s" % (outfit, slot), unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    for k, v in look.items():
        if isinstance(v, tuple):
            mel.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(v[0], v[1], v[2], 1.0))
        else:
            mel.set_material_instance_scalar_parameter_value(mi, k, float(v))
    eal.save_loaded_asset(mi)
    return mi


def import_outfit(name):
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
    sk.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
    sk.set_editor_property("update_skeleton_reference_pose", False)
    sk.set_editor_property("use_t0_as_ref_pose", False)
    sk.set_editor_property("import_uniform_scale", float(globals().get("OUTFIT_SCALE", 0.01)))
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(SRC, "outfit_%s.fbx" % name.lower()))
    task.set_editor_property("destination_path", DIR)
    task.set_editor_property("destination_name", "SKM_Outfit_" + name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", ui)
    at.import_asset_tasks([task])
    return unreal.load_asset(DIR + "/SKM_Outfit_" + name)


for name, slots in LOOKS.items():
    mesh = import_outfit(name)
    if not mesh:
        raise RuntimeError("import failed: " + name)
    mats = list(mesh.get_editor_property("materials"))
    for i, sm in enumerate(mats):
        slot = str(sm.get_editor_property("material_slot_name"))
        key = next((k for k in slots if slot.startswith(k)), None)
        if key:
            sm.set_editor_property("material_interface", instance(name, key, slots[key]))
            mats[i] = sm
    mesh.set_editor_property("materials", mats)
    eal.save_loaded_asset(mesh)
    b = mesh.get_bounds()
    print(name, "height", round(b.box_extent.z * 2, 1), "slots", [str(s.get_editor_property("material_slot_name")) for s in mats],
          "morphs", len(mesh.get_editor_property("morph_targets")))
