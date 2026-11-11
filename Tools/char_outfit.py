"""Import the wardrobe (made by Tools/blender_outfit.py) and build its materials.

  python3 Tools/ue.py Tools/char_outfit.py        # all six; ONLY = ["Noir"] for some

- Imports SourceArt/Character/Outfits/outfit_<name>.fbx as SKM_Outfit_<Name> on Quinn's
  skeleton with every body morph (bust, glutes, jiggle).
- Builds one base material per fabric and one instance per outfit and fabric
  (MI_Outfit_<Outfit>_<Fabric>): colours and weights live in WARDROBE below.

  Sheer  see-through mesh that closes up at grazing angles     Satin  opaque, soft sheen
  Lace   sheer ground with fan embroidery                      Latex  black clear coat
  Net    fishnet: diamond cords, cut out between them          Trim   piping and hardware
  Nylon  sheer knit: angle, stretch and denier set how much skin shows (the bodystocking)

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
    # refreshed 2026-11-11: the bodystocking's nylon and bindings, lace cups and welts, steel clasp
    "Noir": dict(Nylon=dict(Color=(0.006, 0.006, 0.008), CoverTorso=0.55, CoverLeg=0.8, StretchSheer=0.36, SeamStrength=1.0),
                 Lace=dict(Color=BLACK, Opacity=0.5, Cell=3.0),
                 Satin=dict(Color=BLACK, Roughness=0.3, RoughnessVariation=0.14),
                 # bindings as glossy as the satin: where a flat binding cuts across a curve the
                 # satin under it pokes through, and a matte binding showed it as bright flecks
                 Binding=dict(_base="Satin", Color=(0.006, 0.006, 0.008), Roughness=0.3, RoughnessVariation=0.14, NormalStrength=0.5),
                 Trim=STEEL),
    "Garter": dict(Sheer=dict(Color=BLACK, Opacity=0.5), Lace=dict(Color=BLACK, Opacity=0.3, Cell=4.0),
                   Satin=dict(Color=BLACK, Roughness=0.42, RoughnessVariation=0.2), Trim=GOLD),
    "Cocoa": dict(Satin=dict(Color=(0.14, 0.05, 0.025), Roughness=0.26, RoughnessVariation=0.18, NormalStrength=0.9), Trim=GOLD),
    "Fishnet": dict(Net=dict(Color=BLACK, Cell=3.0, Width=0.16), Satin=dict(Color=BLACK, Roughness=0.34, RoughnessVariation=0.16), Trim=STEEL),
    # deep navy latex, smooth: long soft highlights. The ripple normal read as hammered metal, and a mirror coat
    # (0.025) showed every hard edge of the scene as jagged black shapes on her (2026-11-11)
    "Catsuit": dict(Latex=dict(Color=(0.006, 0.009, 0.032), Roughness=0.3, RoughnessVariation=0.04, CoatRoughness=0.12, CoatVariation=0.02, NormalStrength=0.0),
                    Trim=STEEL),   # only the buckle and the zip pull: no white on it (Tema, 2026-11-11)
    # the hero outfit: one piece of sheer nylon, heavier on the legs, with gold jewellery
    "Bodystocking": dict(Nylon=dict(Color=(0.006, 0.006, 0.008), CoverTorso=0.6, CoverLeg=0.84, StretchSheer=0.36, SeamStrength=1.0),
                         Sheer=dict(Color=(0.006, 0.006, 0.008), Opacity=0.5, EdgeExponent=1.6, SeamStrength=1.0),
                         Binding=dict(_base="Satin", Color=(0.006, 0.006, 0.008), Roughness=0.74, RoughnessVariation=0.1, NormalStrength=0.5),
                         Trim=dict(Color=(1.0, 0.72, 0.36), Roughness=0.2, RoughnessVariation=0.08, Metallic=1.0)),
    # the bodystocking with a short black satin A-line skirt over it (Docs/Research/DESIGN_SKIRT.md)
    "BodystockingSkirt": dict(Nylon=dict(Color=(0.006, 0.006, 0.008), CoverTorso=0.6, CoverLeg=0.84, StretchSheer=0.36, SeamStrength=1.0),
                              Sheer=dict(Color=(0.006, 0.006, 0.008), Opacity=0.5, EdgeExponent=1.6, SeamStrength=1.0),
                              Binding=dict(_base="Satin", Color=(0.006, 0.006, 0.008), Roughness=0.74, RoughnessVariation=0.1, NormalStrength=0.5),
                              Satin=dict(Color=(0.008, 0.008, 0.01), Roughness=0.38, RoughnessVariation=0.1, NormalStrength=0.5, SeamStrength=1.0),
                              Trim=dict(Color=(1.0, 0.72, 0.36), Roughness=0.2, RoughnessVariation=0.08, Metallic=1.0)),
    # the skirt alone, the source mesh of the skirt cloth asset (Docs/Research/STUDY_SKIRT_SIM.md)
    "Skirt": dict(Satin=dict(Color=(0.008, 0.008, 0.01), Roughness=0.38, RoughnessVariation=0.1, NormalStrength=0.5, SeamStrength=1.0, _two_sided=True)),
    # bundle 03 (Tema's references, 2026-11-11): red tartan slingshot, black satin collar, black
    # bindings and straps, white sheer thigh-highs with a white lace top
    "Tartan": dict(Tartan=dict(Red=(0.30, 0.014, 0.012), Dark=(0.012, 0.008, 0.01), Line=(0.62, 0.6, 0.55),
                               Roughness=0.5, RoughnessVariation=0.12, NormalStrength=0.5, SeamStrength=1.0),
                   Satin=dict(Color=(0.008, 0.008, 0.01), Roughness=0.3, RoughnessVariation=0.12, NormalStrength=0.6, SeamStrength=1.0),
                   Binding=dict(_base="Satin", Color=(0.006, 0.006, 0.008), Roughness=0.5, RoughnessVariation=0.1, NormalStrength=0.5),
                   Sheer=dict(Color=(0.8, 0.8, 0.78), Opacity=0.3, EdgeOpacity=0.8, EdgeExponent=1.8, Roughness=0.55),
                   Lace=dict(Color=(0.85, 0.85, 0.83), Opacity=0.35, Cell=3.0)),
    "Sheer": dict(Sheer=dict(Color=BLACK, Opacity=0.66, EdgeExponent=1.5), Latex=dict(Color=(0.012, 0.013, 0.02), Roughness=0.34, CoatRoughness=0.06, CoatVariation=0.07, NormalStrength=0.6), Trim=STEEL),
    # bundle 02 (Tema's references, 2026-11-11): matte black elastic straps joined by steel rings,
    # opaque jersey chap stockings with a soft sheen, matte bound edges
    "Harness": dict(Satin=dict(Color=(0.008, 0.008, 0.01), Roughness=0.4, RoughnessVariation=0.12, NormalStrength=0.6, SeamStrength=1.0),
                    Jersey=dict(_base="Satin", Color=(0.01, 0.01, 0.012), Roughness=0.52, RoughnessVariation=0.1, NormalStrength=0.35, SeamStrength=1.0),
                    Binding=dict(_base="Satin", Color=(0.006, 0.006, 0.008), Roughness=0.7, RoughnessVariation=0.1, NormalStrength=0.5),
                    Trim=dict(Color=(0.8, 0.8, 0.82), Roughness=0.22, RoughnessVariation=0.06, Metallic=1.0)),
}
MESH_DIR = {"Sheer": "/Game/Character/Clothes", "Bodystocking": "/Game/Character/Clothes", "BodystockingSkirt": "/Game/Character/Clothes", "Skirt": "/Game/Character/Clothes", "Tartan": "/Game/Character/Clothes", "Harness": "/Game/Character/Clothes"}       # the rest: /Game/Character/Outfits
DIR = "/Game/Character/Clothes"
SKELETON = "/Game/Characters/Mannequins/Meshes/SK_Mannequin"
ONLY = globals().get("ONLY")
# KEEP_BASES = True: load the shared base materials and textures as they are and build only the
# tartan base, so a branch adding one outfit does not re-save binaries other branches also touch
KEEP_BASES = globals().get("KEEP_BASES", False)
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


# Sheer knit, physically: a yarn grid covers a fraction c of the area head-on. Looking along
# the cloth you look through more of it, so coverage rises as 1 - (1 - c)^(1 / cos). Where
# the knit is stretched (UV1.x) the loops open and c falls; the legs (UV1.y) are heavier.
NYLON_HLSL = """
float c = lerp(CoverTorso, CoverLeg, saturate(UV1.y)) * lerp(1.0 + StretchSheer * 0.45, 1.0 - StretchSheer, saturate(UV1.x));
float2 p = float2(UV.x, 1.0 - UV.y) / Cell;
// repeats per pixel along the finer (course, 1.7x) direction; the knit is gone by 3 px per
// repeat, since anything finer aliases into a hatched moire on the shoulders and back
float fw = max(fwidth(p.x), 1.7 * fwidth(p.y));
float wale = 0.5 + 0.5 * cos(6.2831853 * p.x);
float course = 0.5 + 0.5 * cos(6.2831853 * (p.y * 1.7 + 0.5 * floor(p.x + 0.5)));
float knit = lerp(wale * 0.65 + course * 0.35, 0.5, smoothstep(0.12, 0.33, fw));
c = saturate(c * (1.0 + Knit * (knit - 0.5) * 2.0));
float nv = max(abs(NoV), 0.14);
return 1.0 - pow(1.0 - min(c, 0.985), 1.0 / nv);
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

# Construction in the normal (Docs/Research/STUDY_SKIRT_CORSET.md): a rolled edge along every cut
# and the fabric pulled into each seam, with a topstitch groove beside both. UVSeam (UV2) holds
# the distance to the nearest cut edge (x) and the signed distance to the nearest seam line (y)
# in cm, from blender_outfit.py; the profile is evaluated here per pixel, so a 2.5-5.5 mm bevel
# stays sharp without a body-sized texture (the study's 70 px/cm would need far more than 16K).
SEAM_HLSL = """
float3 N = normalize(Parameters.TangentToWorld[2]);
float3 P = Parameters.WorldPosition_CamRelative;
float3 dpx = ddx(P), dpy = ddy(P);
float3 sx = dpx - dot(dpx, N) * N, sy = dpy - dot(dpy, N) * N;
float3 r1 = cross(sy, N), r2 = cross(N, sx);
float det = dot(sx, r1);
float fp = max(length(dpx), length(dpy));
float sg = 0.035;
float fe = saturate(1.5 - fp / (0.5 * Width)), fs = saturate(1.5 - fp / (2.0 * sg));
float ax = abs(Seam.y), ws = 0.75 * Width;
float te = saturate(Seam.x / Width), ts = saturate(ax / ws);
float ue = (Seam.x - Width - 0.12) / sg, us = (ax - 0.25) / sg;
float he = fe * 2.0 * Depth * (1.0 - te) / Width + fs * 2.0 * Stitch * ue / sg * exp(-ue * ue);
float hs = fe * 2.0 * Depth * (1.0 - ts) / ws + fs * 2.0 * Stitch * us / sg * exp(-us * us);
hs *= Seam.y < 0.0 ? -1.0 : 1.0;
float dhx = he * ddx(Seam.x) + hs * ddx(Seam.y);
float dhy = he * ddy(Seam.x) + hs * ddy(Seam.y);
float3 g = abs(det) > 1e-12 ? sign(det) * (dhx * r1 + dhy * r2) / abs(det) : float3(0, 0, 0);
float3 nt = mul(Parameters.TangentToWorld, normalize(N - Strength * g));
return normalize(float3(nt.xy + Detail.xy, nt.z * Detail.z));
"""

# Tartan, woven: the same sett of stripes runs in warp (along UV.x) and weft (along UV.y), each
# mirrored about its half-period, and a 2/2 twill decides per thread which one shows. Each
# stripe is box-filtered over the pixel's footprint and the twill fades to its average, so
# a 0.6 mm white line neither aliases nor shimmers at full-body distance.
TARTAN_HLSL = """
float2 p = float2(UV.x, 1.0 - UV.y);
float P = Period;
// half sett (cm, of P / 2 = 3): red, broad dark, red, thin dark, red, white line, red
float e[8] = {0.0, 1.10, 1.70, 1.85, 1.95, 2.70, 2.76, 3.0};
float k[7] = {0, 1, 0, 1, 0, 2, 0};
float3 cw = 0, cf = 0;
float2 fw = max(fwidth(p), 1e-4);
for (int axis = 0; axis < 2; axis++)
{
    float t = axis == 0 ? p.x : p.y;
    float w = axis == 0 ? fw.x : fw.y;
    float tm = P * 0.5 - abs(frac(t / P) * P - P * 0.5);
    float3 c = 0;
    for (int i = 0; i < 7; i++)
    {
        float cover = saturate((min(tm + w * 0.5, e[i + 1]) - max(tm - w * 0.5, e[i])) / w);
        float3 col = k[i] == 0 ? Red : (k[i] == 1 ? Dark : Line);
        c += col * cover;
    }
    if (axis == 0) cw = c; else cf = c;
}
float2 th = p / Thread;
float tw = frac((floor(th.x) + floor(th.y)) * 0.25) < 0.5 ? 1.0 : 0.0;
tw = lerp(tw, 0.5, saturate(max(fwidth(th.x), fwidth(th.y)) * 0.7));
return lerp(cf, cw, tw);
"""

# Tartan relief: the 2/2 twill raises diagonal ribs (Rib cm apart) and each thread is a small
# round ridge along its own direction, warp or weft by the same twill as the colour. The height
# is analytic, so its slope is exact per pixel; ribs and threads fade out as they shrink below a
# pixel, so the cloth goes from visible weave up close to soft sheen at full-body distance.
# Laid over the satin weave detail texture (UDN blend). Returns (normal.xyz) and roughness in w.
TARTAN_RELIEF_HLSL = """
float2 p = float2(UV.x, 1.0 - UV.y);
const float TAU = 6.2831853;
// twill ribs along (u + v)
float ph = TAU * (p.x + p.y) / Rib;
float fr = 1.0 - saturate(fwidth(ph) / 3.14159);
float rs = -sin(ph) * (Height * TAU / Rib) * fr;
float2 g = float2(rs, rs);
// threads: warp ridges where warp floats, weft ridges elsewhere
float2 th = p / Thread;
float warp = frac((floor(th.x) + floor(th.y)) * 0.25) < 0.5 ? 1.0 : 0.0;
float ft = 1.0 - saturate(max(fwidth(th.x), fwidth(th.y)) * 1.2);
float tx = -sin(TAU * th.x) * (Height * 0.6 * TAU / Thread) * ft;
float ty = -sin(TAU * th.y) * (Height * 0.6 * TAU / Thread) * ft;
g += warp > 0.5 ? float2(tx, 0.0) : float2(0.0, ty);
float3 n = normalize(float3(-g.x * Strength, g.y * Strength, 1.0));
float3 d = Detail;
float3 o = normalize(float3(n.xy + d.xy, n.z * d.z));
// the valleys between ribs hold less sheen
float valley = 0.5 - 0.5 * cos(ph);
return float4(o, valley * fr);
"""

TEX_DIR = DIR + "/Textures"


def texture(name, normal):
    """Import a detail texture: linear, normal or masks compression, never streamed."""
    if KEEP_BASES and unreal.EditorAssetLibrary.does_asset_exist(TEX_DIR + "/" + name):
        return unreal.load_asset(TEX_DIR + "/" + name)
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


def seam_normal(m, y, detail=None):
    """Rolled edges and seams from UVSeam, laid over a tangent-space detail normal. Off
    (SeamStrength 0) unless an outfit's instance turns it on: older meshes have no UV2."""
    detail = detail or node(m, unreal.MaterialExpressionConstant3Vector, -900, y, constant=unreal.LinearColor(0, 0, 1, 1))
    return custom(m, SEAM_HLSL, "SeamNormal", [
        ("Seam", node(m, unreal.MaterialExpressionTextureCoordinate, -900, y + 60, coordinate_index=2)),
        ("Width", scalar(m, "SeamWidth", 0.4, y + 120)), ("Depth", scalar(m, "SeamDepth", 0.12, y + 180)),
        ("Stitch", scalar(m, "SeamStitch", 0.012, y + 240)), ("Strength", scalar(m, "SeamStrength", 0.0, y + 300)),
        ("Detail", detail)], y, unreal.CustomMaterialOutputType.CMOT_FLOAT3)


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
    # depth bias: every vertex pulled DepthBias cm toward the camera, along its view ray, so
    # nothing moves on screen; it only wins the depth test. Cloth sits 2-3 mm off the skin,
    # and where a deep bend folds the skin (Mermaid's hip) the skin overtook it and showed
    # through in flecks, broke bindings and dashed the seam tapes
    # (CameraVector is pixel-shader only: the view ray is camera position minus world position)
    ray = op(m, unreal.MaterialExpressionSubtract, node(m, unreal.MaterialExpressionCameraPositionWS, -900, 1400),
             node(m, unreal.MaterialExpressionWorldPosition, -900, 1440), 1400)
    to_cam = node(m, unreal.MaterialExpressionNormalize, -300, 1400)
    link(ray, to_cam, "VectorInput")
    bias = op(m, unreal.MaterialExpressionMultiply, to_cam, scalar(m, "DepthBias", 0.5, 1480), 1460)
    mel.connect_material_property(bias, "", MP.MP_WORLD_POSITION_OFFSET)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    return m


def build_tartan(weave, detail):
    m = material("M_OutfitTartan", unreal.BlendMode.BLEND_OPAQUE)
    uv = node(m, unreal.MaterialExpressionTextureCoordinate, -900, -400)
    mel.connect_material_property(custom(m, TARTAN_HLSL, "Tartan", [
        ("UV", uv), ("Period", scalar(m, "Period", 6.0, -340)), ("Thread", scalar(m, "Thread", 0.05, -280)),
        ("Red", color(m, "Red", (0.30, 0.014, 0.012), -220)), ("Dark", color(m, "Dark", (0.012, 0.008, 0.01), -160)),
        ("Line", color(m, "Line", (0.62, 0.6, 0.55), -100))], -300, unreal.CustomMaterialOutputType.CMOT_FLOAT3), "", MP.MP_BASE_COLOR)
    relief = custom(m, TARTAN_RELIEF_HLSL, "TartanRelief", [
        ("UV", node(m, unreal.MaterialExpressionTextureCoordinate, -900, 700)),
        ("Rib", scalar(m, "Rib", 0.16, 760)), ("Thread", scalar(m, "Thread", 0.05, 820)),
        ("Height", scalar(m, "ReliefHeight", 0.006, 880)), ("Strength", scalar(m, "ReliefStrength", 1.0, 940)),
        ("Detail", detail_normal(m, weave, 1.5, 0.5, 600))], 700, unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    mask = sample(m, "DetailMask", detail, False, 24.0, 300)
    rough = varied(m, scalar(m, "Roughness", 0.5, -40), mask, "R", "RoughnessVariation", 0.12, 0)
    mask_a = node(m, unreal.MaterialExpressionComponentMask, -500, 160, r=False, g=False, b=False, a=True)
    link(relief, mask_a, "")
    valley = op(m, unreal.MaterialExpressionMultiply, mask_a, scalar(m, "ValleyRoughness", 0.12, 160), 120)
    mel.connect_material_property(op(m, unreal.MaterialExpressionAdd, rough, valley, 80), "", MP.MP_ROUGHNESS)
    mask3 = node(m, unreal.MaterialExpressionComponentMask, -500, 700, r=True, g=True, b=True, a=False)
    link(relief, mask3, "")
    mel.connect_material_property(seam_normal(m, 1000, mask3), "", MP.MP_NORMAL)
    return done(m)


def build_materials():
    T = unreal.BlendMode.BLEND_TRANSLUCENT
    weave, latex_n, detail = texture("T_Outfit_Weave_N", True), texture("T_Outfit_Latex_N", True), texture("T_Outfit_Detail_M", False)
    base = {}
    if KEEP_BASES:
        for n in ("Sheer", "Nylon", "Lace", "Net", "Satin", "Latex", "Trim"):
            base[n] = unreal.load_asset(DIR + "/M_Outfit" + n)
        # Tartan too, once it exists: a run for another outfit must not re-save it (bundle-02)
        have = unreal.EditorAssetLibrary.does_asset_exist(DIR + "/M_OutfitTartan")
        # REBUILD_TARTAN = True: the tartan's own owner changed the base (done() keeps the depth bias)
        rebuild = globals().get("REBUILD_TARTAN", False)
        base["Tartan"] = unreal.load_asset(DIR + "/M_OutfitTartan") if have and not rebuild else build_tartan(weave, detail)
        return base

    m = material("M_OutfitSheer", T)
    mel.connect_material_property(color(m, "Color", BLACK, -200), "", MP.MP_BASE_COLOR)
    mel.connect_material_property(scalar(m, "Roughness", 0.65, -80), "", MP.MP_ROUGHNESS)
    mel.connect_material_property(sheer_opacity(m, 0.5), "", MP.MP_OPACITY)
    mel.connect_material_property(seam_normal(m, 1000), "", MP.MP_NORMAL)
    base["Sheer"] = done(m)

    m = material("M_OutfitNylon", T)
    mel.connect_material_property(color(m, "Color", (0.006, 0.006, 0.008), -200), "", MP.MP_BASE_COLOR)
    mel.connect_material_property(scalar(m, "Roughness", 0.46, -80), "", MP.MP_ROUGHNESS)
    mel.connect_material_property(scalar(m, "Specular", 0.4, 0), "", MP.MP_SPECULAR)
    nov = op(m, unreal.MaterialExpressionDotProduct, node(m, unreal.MaterialExpressionPixelNormalWS, -900, 520),
             node(m, unreal.MaterialExpressionCameraVectorWS, -900, 580), 540)
    nylon = custom(m, NYLON_HLSL, "Nylon", [
        ("UV", node(m, unreal.MaterialExpressionTextureCoordinate, -900, 300)),
        ("UV1", node(m, unreal.MaterialExpressionTextureCoordinate, -900, 360, coordinate_index=1)),
        ("NoV", nov), ("CoverTorso", scalar(m, "CoverTorso", 0.44, 640)), ("CoverLeg", scalar(m, "CoverLeg", 0.7, 700)),
        ("StretchSheer", scalar(m, "StretchSheer", 0.42, 760)), ("Cell", scalar(m, "KnitCell", 0.09, 820)),
        ("Knit", scalar(m, "Knit", 0.5, 880))], 400)
    mel.connect_material_property(nylon, "", MP.MP_OPACITY)
    mel.connect_material_property(seam_normal(m, 1000), "", MP.MP_NORMAL)
    base["Nylon"] = done(m)

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
    mel.connect_material_property(seam_normal(m, 1000, detail_normal(m, weave, 3.0, 0.7, 600)), "", MP.MP_NORMAL)
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
    base["Tartan"] = build_tartan(weave, detail)
    return base


def instance(outfit, slot, parent, values):
    name = "MI_Outfit_%s_%s" % (outfit, slot)
    path = DIR + "/" + name
    mi = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else \
        tools.create_asset(name, DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    two = values.pop("_two_sided", False) if isinstance(values, dict) else False
    if two:                                    # cloth folds show their inside
        o = mi.get_editor_property("base_property_overrides")
        o.set_editor_property("override_two_sided", True)
        o.set_editor_property("two_sided", True)
        mi.set_editor_property("base_property_overrides", o)
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
    # Legacy FBX importer, not Interchange: Interchange places these outfit meshes 100x too
    # large (bones right, vertices not), so they render around her, invisible.
    # Replacing an existing mesh reuses the import settings stored on it, not `ui`, so a mesh
    # once imported at scale 1 or without morphs stays broken: stamp the settings on it first.
    old = unreal.load_asset(MESH_DIR.get(outfit, "/Game/Character/Outfits") + "/SKM_Outfit_" + outfit)
    data = old.get_editor_property("asset_import_data") if old else None
    if isinstance(data, unreal.FbxSkeletalMeshImportData):
        for k in ("import_morph_targets", "normal_import_method", "update_skeleton_reference_pose",
                  "use_t0_as_ref_pose", "import_uniform_scale"):
            data.set_editor_property(k, sk.get_editor_property(k))
    world = unreal.EditorLevelLibrary.get_editor_world()
    cvar = "Interchange.FeatureFlags.Import.FBX"
    was = unreal.SystemLibrary.get_console_variable_bool_value(cvar)
    unreal.SystemLibrary.execute_console_command(world, cvar + " 0")
    try:
        tools.import_asset_tasks([task])
    finally:
        unreal.SystemLibrary.execute_console_command(world, "%s %d" % (cvar, 1 if was else 0))
    mesh = unreal.load_asset(MESH_DIR.get(outfit, "/Game/Character/Outfits") + "/SKM_Outfit_" + outfit)
    if not mesh:
        raise RuntimeError("import failed: " + outfit)
    return mesh


base = build_materials()
for outfit, fabrics in WARDROBE.items():
    if ONLY and outfit not in ONLY:
        continue
    mis = {slot: instance(outfit, slot, base[values.get("_base", slot)], {k: v for k, v in values.items() if k != "_base"})
           for slot, values in fabrics.items()}
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
