"""Bake our own tileable stone texture sets: a generator shader is drawn into 4K
render targets, exported to SourceArt/VisEx/Stone, and imported as textures.

  Wall  1720 x 1520 cm tile: 4 x 8 masonry blocks (430 x 190), the column/plinth scale
  Slab  3200 x 2000 cm tile: giant 16 x 10 m floor slabs, once polished, now cracked,
        chipped, spalled, with missing corners and etched patches that lost their polish

Maps per set: _B base colour (sRGB), _N normal (our own convention: x along +u, y along
+v, read with no green flip), _ORM (R ambient occlusion, G roughness, B height).
Block/slab grids line up with the world-space grids in add_sand.py.

Run live: python3 Tools/ue.py Tools/bake_stone.py   (then add_sand.py)
"""
import os

import unreal

RES = 4096
GEN = "/Game/VisEx/Stone/M_StoneGen"
DEST = "/Game/VisEx/Stone"
OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "SourceArt", "VisEx", "Stone")
KINDS = {"Wall": 0, "Slab": 1}
MAPS = {"B": 0, "N": 1, "ORM": 2}

HLSL = """
struct Gen {
    float2 T;     // tile size, cm
    float hash2(float2 p) { float3 q = frac(float3(p.xyx) * 0.1031); q += dot(q, q.yzx + 33.33); return frac((q.x + q.y) * q.z); }
    // value noise, periodic with 'per' cells
    float vnoise(float2 x, float2 per) {
        float2 i = floor(x), f = frac(x); f = f * f * (3 - 2 * f);
        float2 a = fmod(i, per), b = fmod(i + 1, per);
        return lerp(lerp(hash2(a), hash2(float2(b.x, a.y)), f.x), lerp(hash2(float2(a.x, b.y)), hash2(b), f.x), f.y);
    }
    // fbm with n cells across the tile width; tileable
    float fbm(float2 p, float n, int oct) {
        float2 per = float2(n, max(1, round(n * T.y / T.x)));
        float2 x = p / T * per;
        float s = 0, a = 0.5, w = 0;
        for (int k = 0; k < oct; k++) { s += a * vnoise(x, per); w += a; x *= 2; per *= 2; a *= 0.5; }
        return s / w;
    }
    // cellular edge distance (F2 - F1), n cells across, tileable
    float cracks(float2 p, float n) {
        float2 per = float2(n, max(1, round(n * T.y / T.x)));
        float2 x = p / T * per, i = floor(x), f = frac(x);
        float d1 = 9, d2 = 9;
        for (int j = -1; j <= 1; j++) for (int k = -1; k <= 1; k++) {
            float2 g = float2(j, k), c = fmod(i + g + per, per);
            float2 o = float2(hash2(c), hash2(c + 17.3));
            float d = length(g + o - f);
            if (d < d1) { d2 = d1; d1 = d; } else if (d < d2) d2 = d;
        }
        return (d2 - d1) * T.x / n;    // back to cm
    }

    // ---- wall: masonry blocks. h in cm; id per block; j = joint/edge wear
    float wall(float2 p, out float id, out float j, out float rough, out float spall) {
        const float H = 190, L = 430;
        float row = floor(p.y / H);
        float u = p.x / L + 0.5 * fmod(row, 2);
        float col = floor(u);
        float fu = frac(u) * L, fv = frac(p.y / H) * H;
        float e = min(min(fu, L - fu), min(fv, H - fv));
        e += 6 * (fbm(p, 70, 3) - 0.5);                                   // joints wander, not ruler-straight
        id = hash2(float2(fmod(col, 4), fmod(row, 8)) + 3.1);
        float chip = fbm(p, 40, 4);
        float r = 4 + 26 * smoothstep(0.4, 0.85, chip);                  // worn, rounded, chipped arrises
        float2 cd = float2(min(fu, L - fu), min(fv, H - fv));
        float corner = 1 - smoothstep(10, 45 * (0.6 + chip), length(cd));  // knocked-off corners
        j = 1 - smoothstep(0.5, r, e);
        j = max(j, corner * step(0.35, hash2(float2(col, row) + 2.2)));
        float h = -6 * pow(j, 0.7);
        h += 0.9 * sin(3.1416 * fu / L) * sin(3.1416 * fv / H);           // slightly pillowed face
        h += 0.35 * (fbm(p, 120, 4) - 0.5);                                // grain
        float pits = smoothstep(0.62, 0.82, fbm(p, 260, 3));
        h -= 0.8 * pits;
        spall = smoothstep(0.64, 0.665, fbm(p + id * 97, 14, 5)) * (1 - j);
        h -= 1.6 * spall;
        rough = saturate(0.82 + 0.1 * j + 0.06 * spall + 0.06 * pits);
        return h;
    }

    // ---- slab: giant polished floor slabs, ruined
    float slab(float2 p, out float id, out float j, out float rough, out float spall) {
        const float SW = 1600, SH = 1000;
        float row = floor(p.y / SH);
        float u = p.x / SW + 0.5 * fmod(row, 2);
        float col = floor(u);
        float fu = frac(u) * SW, fv = frac(p.y / SH) * SH;
        float e = min(min(fu, SW - fu), min(fv, SH - fv));
        e += 9 * (fbm(p, 30, 3) - 0.5);                                   // worn joints wander
        id = hash2(float2(fmod(col, 2), fmod(row, 2)) + 7.7);
        // edges: a tight joint, broken away in places
        float chip = fbm(p, 50, 4);
        float broken = 2.5 + 28 * smoothstep(0.52, 0.8, chip);
        j = 1 - smoothstep(1.2, broken, e);
        float h = -3.5 * j;
        // one or two corners snapped off
        float2 cd = float2(min(fu, SW - fu), min(fv, SH - fv));
        float cr = lerp(30, 170, hash2(float2(col, row) + 1.3)) * (0.75 + 0.5 * fbm(p, 60, 3));
        float corner = (1 - smoothstep(cr - 2, cr + 2, length(cd))) * step(0.45, hash2(float2(col + 5, row)));
        h = min(h, -5 * corner);
        // fractures: warped cell edges, only some of them
        float2 w = p + 220 * (float2(fbm(p, 6, 3), fbm(p + 50, 6, 3)) - 0.5);
        float ce = cracks(w, 8);
        float keep = smoothstep(0.5, 0.62, fbm(p + 300, 5, 3));
        float crack = (1 - smoothstep(0.25, 1.4 + 2.5 * chip, ce)) * keep;
        h = min(h, -2.8 * crack);
        // slow undulation of a once-flat surface
        h += 0.5 * (fbm(p, 4, 3) - 0.5);
        // etched: polish lost, fine pitting
        float etch = smoothstep(0.42, 0.72, fbm(p + 900, 9, 5));
        h -= etch * 0.5 * smoothstep(0.5, 0.85, fbm(p, 700, 2));
        // spalls: top layer flaked away, sharp rims
        spall = smoothstep(0.70, 0.712, fbm(p + id * 131, 16, 6)) * (1 - j);
        h -= 1.4 * spall;
        j = max(j, max(crack, corner));
        rough = lerp(0.45, 0.8, etch);
        rough = lerp(rough, 0.86, max(spall, j));
        return h;
    }

    float height(float2 p, float kind, out float id, out float j, out float rough, out float spall) {
        p = fmod(p + T * 4, T);
        // not ?: -- HLSL evaluates both sides, and the slab would overwrite the out params
        if (kind < 0.5) return wall(p, id, j, rough, spall);
        return slab(p, id, j, rough, spall);
    }
};

Gen G;
G.T = Kind < 0.5 ? float2(1720, 1520) : float2(3200, 2000);
float2 p = UV * G.T;
float id, j, rough, spall;
float h = G.height(p, Kind, id, j, rough, spall);

if (Map < 0.5) {
    // base colour
    float3 c = Kind < 0.5 ? float3(0.46, 0.30, 0.19) : float3(0.50, 0.34, 0.22);
    c *= lerp(0.84, 1.14, id) * float3(1, 0.97 + 0.06 * G.hash2(id + 2), 0.94 + 0.1 * G.hash2(id + 5));
    float grain = G.fbm(p, 400, 3);
    c *= lerp(0.92, 1.08, grain);
    c *= 1 - 0.25 * smoothstep(0.72, 0.9, G.fbm(p + 40, 900, 2));      // dark specks
    if (Kind > 0.5) {                                                    // faint veining in the polished stone
        float v = 1 - smoothstep(0.0, 0.012, abs(G.fbm(p + 77, 3, 6) - 0.5));
        c = lerp(c, c * 0.8, v * 0.45);
        c *= lerp(0.92, 1.0, rough);                                     // polish reads deeper
    }
    c = lerp(c, c * 1.08 + 0.01, spall);                                 // fresher stone under spalls
    c = lerp(c, c * 0.55, saturate(-h / 5) * j);                         // grime in joints and cracks
    return pow(saturate(c), 1 / 2.2);
}
if (Map < 1.5) {
    // normal from the height field (true slope; one texel step)
    float e = G.T.x / 4096;
    float a, b, cc, d;
    float hu = G.height(p + float2(e, 0), Kind, a, b, cc, d);
    float hv = G.height(p + float2(0, e), Kind, a, b, cc, d);
    float3 n = normalize(float3(-(hu - h) / e, -(hv - h) / e, 1));
    return n * 0.5 + 0.5;
}
// ORM: AO, roughness, height
float ao = saturate(1 - 0.55 * saturate(-h / 4) - 0.25 * spall);
return float3(ao, rough, saturate((h + 6) / 8));
"""


def build_generator():
    """Returns (material, rebuilt). Shaders compile asynchronously, so after a rebuild
    the bake has to wait for a second run."""
    mel = unreal.MaterialEditingLibrary
    if unreal.EditorAssetLibrary.does_asset_exist(GEN):
        mat = unreal.load_asset(GEN)
        cur = mel.get_material_property_input_node(mat, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        if cur and isinstance(cur, unreal.MaterialExpressionCustom) and cur.get_editor_property("code") == HLSL:
            return mat, False
        mel.delete_all_material_expressions(mat)
    else:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_StoneGen", DEST, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    node = lambda cls, x, y: mel.create_material_expression(mat, cls, x, y)
    cn = node(unreal.MaterialExpressionCustom, -300, 0)
    cn.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    ins = []
    for name in ("UV", "Kind", "Map"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        ins.append(ci)
    cn.set_editor_property("inputs", ins)
    cn.set_editor_property("code", HLSL)
    mel.connect_material_expressions(node(unreal.MaterialExpressionTextureCoordinate, -600, 0), "", cn, "UV")
    for i, name in enumerate(("Kind", "Map")):
        sp = node(unreal.MaterialExpressionScalarParameter, -600, 100 + i * 100)
        sp.set_editor_property("parameter_name", name)
        mel.connect_material_expressions(sp, "", cn, name)
    mel.connect_material_property(cn, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(GEN)
    return mat, True


os.makedirs(OUT, exist_ok=True)
gen, rebuilt = build_generator()
if rebuilt:
    raise SystemExit("generator rebuilt; shaders compiling, run again in a moment to bake")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
# one instance and one render target per map: parameter changes on a shared
# instance reach the GPU late, so draws would pick up the previous map's settings
files = []
for kind, k in KINDS.items():
    for m, mi in MAPS.items():
        mid = unreal.MaterialLibrary.create_dynamic_material_instance(world, gen)
        mid.set_scalar_parameter_value("Kind", k)
        mid.set_scalar_parameter_value("Map", mi)
        rt = unreal.RenderingLibrary.create_render_target2d(world, RES, RES, unreal.TextureRenderTargetFormat.RTF_RGBA8)
        unreal.RenderingLibrary.draw_material_to_render_target(world, rt, mid)
        files.append(("T_Stone_%s_%s" % (kind, m), m, rt))
for name, m, rt in files:
    unreal.RenderingLibrary.export_render_target(world, rt, OUT, name + ".png")
files = [(n, m) for n, m, _ in files]

tasks = []
for name, _ in files:
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", os.path.join(OUT, name + ".png"))
    t.set_editor_property("destination_path", DEST)
    t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", False)
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for name, m in files:
    tex = unreal.load_asset(DEST + "/" + name)
    if m == "N":
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
    elif m == "ORM":
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        tex.set_editor_property("srgb", False)
    tex.set_editor_property("compression_no_alpha", True)     # the render target leaves alpha at 0
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    unreal.EditorAssetLibrary.save_asset(DEST + "/" + name)
print("baked:", ", ".join(n for n, _ in files), "->", OUT)
