"""Fabric materials and the outfits, in UE. Idempotent; stop Play first.

  python3 Tools/ue.py Tools/char_outfits.py

- M_Fabric: the textile master. Cloth shading (a soft sheen at grazing angles, like satin
  and mesh), masked. Its edge comes from the outfit's vertex colour (R = coverage, cut at
  0.5; Tools/blender_outfits.py writes it). Patterns are fixed to the body (pre-skinned
  position), so they stay put when she moves:
    Pattern 1: diamond fishnet (Cell cm, Width = cord share, Aspect = how tall)
- M_FabricSheer: translucent mesh you see skin through, thicker at grazing angles
  (Sheer facing, SheerEdge at the silhouette). Pattern 2: embroidered fans on it.
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
NUDE = (0.42, 0.27, 0.19)
LACE = dict(Pattern=2, Cell=10.0, Width=0.12, Color=BLACK, Roughness=0.55)
LOOKS = {
    "Noir": {   # black sheer mesh, solid trims, black-on-black embroidery
        "Sheer": dict(Color=BLACK, Ground=BLACK, Sheer=0.45, SheerEdge=0.97, Roughness=0.45),
        "Solid": dict(Color=BLACK, Roughness=0.30, Fuzz=(0.20, 0.20, 0.22)),
        "Lace": dict(LACE, Ground=BLACK, Sheer=0.40, SheerEdge=0.95),
    },
    "Garter": {   # black embroidery on nude mesh, sheer black stockings
        "Lace": dict(LACE, Ground=NUDE, Sheer=0.16, SheerEdge=0.55),
        "Solid": dict(Color=BLACK, Roughness=0.30, Fuzz=(0.20, 0.20, 0.22)),
        "Sheer": dict(Color=BLACK, Ground=BLACK, Sheer=0.38, SheerEdge=0.96, Roughness=0.4),
    },
    "Cocoa": {
        "Solid": dict(Color=(0.10, 0.045, 0.025), Roughness=0.28, Fuzz=(0.45, 0.30, 0.22)),
    },
    "Fishnet": {   # open diamond net of round cords, ~3 cm cells
        "Solid": dict(Color=BLACK, Roughness=0.30, Fuzz=(0.20, 0.20, 0.22)),
        "Net": dict(Color=BLACK, Pattern=1, Cell=3.0, Width=0.13, Aspect=1.3, Roughness=0.5, Fuzz=(0.25, 0.25, 0.27)),
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


# The weave, in cm on the body at rest (pre-skinned), so it stays put when she moves.
# Projected flat along whichever axis the surface faces most, like a fabric panel cut
# for the front, the sides and the top. Kind 1: fishnet, tall diamonds of round cords.
# Kind 2: embroidery, ginkgo fans with radial ribs and a scalloped outline, staggered.
PATTERN_HLSL = r"""
float3 a = abs(N);
float2 uv = P.xz;
if (a.z > 0.8) uv = P.xy;
else if (a.x > a.y) uv = P.yz;
if (Kind < 0.5) return Empty;
if (Kind < 1.5)
{
    float2 q = float2(uv.x * Aspect + uv.y, uv.x * Aspect - uv.y) / Cell;
    float2 d = abs(frac(q) - 0.5);
    float2 fw = fwidth(q) + 1e-4;
    float2 s = smoothstep(0.5 - Width - fw, 0.5 - Width + fw, d);
    return max(s.x, s.y);
}
float2 c = uv / Cell;
c.x += 0.5 * fmod(abs(floor(c.y)), 2.0);
float2 id = floor(c);
float2 f = frac(c) - float2(0.5, 0.1);
float h = frac(sin(dot(id, float2(12.9898, 78.233))) * 43758.5453);
float ang = (h - 0.5) * 1.4;
float sn = sin(ang), cs = cos(ang);
f = float2(cs * f.x - sn * f.y, sn * f.x + cs * f.y);
float r = length(f);
float th = atan2(f.x, f.y);
float spread = 0.92;
float R = 0.6 + 0.025 * sin(th * 11.0);
float aa = fwidth(r) * 1.5 + 1e-4;
float wedge = 1.0 - smoothstep(spread - 0.03, spread + 0.03, abs(th));
float inside = (1.0 - smoothstep(R - aa, R + aa, r)) * wedge;
float t = th / spread * 10.0;
float rib = smoothstep(0.5 - Width - fwidth(t), 0.5 - Width, abs(frac(t) - 0.5)) * smoothstep(0.08, 0.14, r);
float w = 0.022;
float rim = (1.0 - smoothstep(w - aa, w + aa, abs(r - R))) * wedge;
float side = (1.0 - smoothstep(w - aa, w + aa, abs(abs(th) - spread) * r)) * step(r, R + w);
float stem = (1.0 - smoothstep(w - aa, w + aa, abs(f.x))) * step(-0.1, f.y) * step(f.y, 0.0);
return saturate(max(max(rim, side), max(inside * rib, stem)));
"""


def pattern(g, empty):
    """The weave as 0..1 (Pattern param picks it; 0 gives `empty`)."""
    out = []
    for cls, x in ((unreal.MaterialExpressionPreSkinnedPosition, 400), (unreal.MaterialExpressionPreSkinnedNormal, 520)):
        vs = g.node(cls, -1900, x)
        vi = g.node(unreal.MaterialExpressionVertexInterpolator, -1700, x)
        mel.connect_material_expressions(vs, "", vi, "VS")
        out.append(vi)
    node = g.node(unreal.MaterialExpressionCustom, -1300, 500)
    node.set_editor_property("code", PATTERN_HLSL)
    node.set_editor_property("description", "Weave")
    node.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    names = ["P", "N", "Kind", "Cell", "Width", "Aspect", "Empty"]
    ins = []
    for n in names:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    node.set_editor_property("inputs", ins)
    srcs = [out[0], out[1], g.scalar("Pattern", 0.0, -1600, 700), g.scalar("Cell", 3.0, -1600, 760),
            g.scalar("Width", 0.18, -1600, 820), g.scalar("Aspect", 1.3, -1600, 880), g.const(empty)]
    for n, src in zip(names, srcs):
        if not mel.connect_material_expressions(src, "", node, n):
            raise RuntimeError("Weave input " + n)
    return node


def build_fabric():
    """Opaque cloth, masked: solid pieces and fishnet (Pattern 1)."""
    m = asset("M_Fabric", unreal.Material, unreal.MaterialFactoryNew(), "/Game/Character/Android")
    mel.delete_all_material_expressions(m)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_CLOTH)
    m.set_editor_property("used_with_skeletal_mesh", True)
    m.set_editor_property("use_material_attributes", True)
    m.set_editor_property("opacity_mask_clip_value", 0.5)
    m.set_editor_property("two_sided", True)
    g = Graph(m)
    opacity = g.mul(coverage(g), pattern(g, 1.0))
    attrs = g.node(unreal.MaterialExpressionMakeMaterialAttributes, 200, 0)
    for pin, src in (("BaseColor", g.vector("Color", BLACK, -1600, -300)),
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


def build_sheer():
    """See-through mesh, translucent: a tinted film you see skin through where it faces
    you and that thickens to nearly opaque where it turns away (stocking denier). With
    Pattern 2, embroidery (Color) lies opaque on the mesh ground (Ground)."""
    m = asset("M_FabricSheer", unreal.Material, unreal.MaterialFactoryNew(), "/Game/Character/Android")
    mel.delete_all_material_expressions(m)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    m.set_editor_property("used_with_skeletal_mesh", True)
    m.set_editor_property("use_material_attributes", True)
    g = Graph(m)
    emb = pattern(g, 0.0)
    fres = g.node(unreal.MaterialExpressionFresnel, -1300, 200)
    fres.set_editor_property("exponent", 2.2)
    fres.set_editor_property("base_reflect_fraction", 0.0)
    ground = g.lerp(g.scalar("Sheer", 0.4, -1600, 100), g.scalar("SheerEdge", 0.95, -1600, 160), fres)
    opacity = g.mul(coverage(g), g.mx(ground, emb))
    color = g.lerp(g.vector("Ground", BLACK, -1600, -380), g.vector("Color", BLACK, -1600, -300), emb)
    attrs = g.node(unreal.MaterialExpressionMakeMaterialAttributes, 200, 0)
    for pin, src in (("BaseColor", color),
                     ("Roughness", g.scalar("Roughness", 0.5, -1600, -200)),
                     ("Specular", g.scalar("Specular", 0.4, -1600, -150)),
                     ("Opacity", opacity)):
        if not mel.connect_material_expressions(src, "", attrs, pin):
            raise RuntimeError("M_FabricSheer pin " + pin)
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


fabric, sheer, latex = build_fabric(), build_sheer(), build_latex()


def instance(outfit, slot, look):
    parent = latex if slot == "Latex" else (sheer if slot in ("Sheer", "Lace") else fabric)
    mi = asset("MI_%s_%s" % (outfit, slot), unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    mel.clear_all_material_instance_parameters(mi)
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
