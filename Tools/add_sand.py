"""Sand cover for Lvl_Dust: a material that buries upward-facing surfaces in sand,
lightly near the steps and heavily in the distance, with wind ripples.
Dunes come from add_dunes.py. Idempotent — safe to re-run.

Run live: python3 Tools/ue.py Tools/add_sand.py
"""
import unreal

MAT = "/Game/VisEx/M_SandCover"
STONE = (0.46, 0.30, 0.19)  # sandstone, a touch less red so the blocks read
SAND = (0.92, 0.62, 0.36)
CLEAN_CENTRE = (1500, -2800, 0)   # the steps: least sand here
NEAR, FAR = 3000.0, 22000.0       # cm: sand ramps from MIN_COVER to full over this range
MIN_COVER = 0.15

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
mel = unreal.MaterialEditingLibrary


# HLSL shared by the stone nodes. A struct is the usual way to get helper
# functions into a Custom node.
STONE_LIB = """
struct StoneLib {
    float hash(float3 p) { p = frac(p * 0.3183099 + 0.1); p *= 17.0; return frac(p.x * p.y * p.z * (p.x + p.y + p.z)); }
    float noise(float3 x) {
        float3 i = floor(x); float3 f = frac(x); f = f * f * (3 - 2 * f);
        return lerp(lerp(lerp(hash(i), hash(i + float3(1,0,0)), f.x), lerp(hash(i + float3(0,1,0)), hash(i + float3(1,1,0)), f.x), f.y),
                    lerp(lerp(hash(i + float3(0,0,1)), hash(i + float3(1,0,1)), f.x), lerp(hash(i + float3(0,1,1)), hash(i + float3(1,1,1)), f.x), f.y), f.z);
    }
    float fbm(float3 p) { float s = 0, a = 0.5; for (int k = 0; k < 4; k++) { s += a * noise(p); p *= 2.03; a *= 0.5; } return s; }
    // project onto the plane the surface mostly faces; v is vertical on walls
    float2 plane(float3 wp, float3 n) {
        float3 an = abs(n);
        if (an.z > an.x && an.z > an.y) return wp.xy;
        return an.x > an.y ? wp.yz : wp.xz;
    }
    // masonry: x = joint (1 in the mortar), y = block id 0..1
    float2 blocks(float2 uv, float wear) {
        const float H = 190, L = 430;   // monumental blocks
        float row = floor(uv.y / H);
        float u = uv.x / L + 0.5 * fmod(abs(row), 2);
        float col = floor(u);
        float fu = frac(u) * L, fv = frac(uv.y / H) * H;
        float e = min(min(fu, L - fu), min(fv, H - fv));
        float chip = noise(float3(uv * 0.05, row)) * 16 * wear;
        return float2(1 - smoothstep(1.5, 3.5 + chip, e), hash(float3(col, row, 3.7)));
    }
    float height(float2 uv, float3 wp, float wear) {
        return -blocks(uv, wear).x - 0.35 * wear * noise(wp * 0.12) - 0.4 * wear * smoothstep(0.55, 0.8, fbm(wp * 0.01));
    }
};
StoneLib S;
float2 uv = S.plane(WP, N);
"""

# Sand surface: macro colour variation at several scales (darker troughs, paler
# windward faces, faint streaks), so the dunes don't read as one flat orange.
SAND_COLOR = """
float big = S.fbm(WP * 0.00012);
float mid = S.fbm(WP * 0.0009 + 7);
float fine = S.noise(WP * 0.02);
float streak = S.fbm(float3(WP.x * 0.0004, WP.y * 0.004, 3));
float3 c = Sand * lerp(0.82, 1.12, big) * lerp(0.93, 1.06, mid) * lerp(0.97, 1.03, fine);
c = lerp(c, c * float3(1.06, 1.0, 0.9), smoothstep(0.4, 0.7, streak) * 0.6);     // warmer streaks
c *= lerp(1.0, 0.86, saturate(N.x * 2.5));                                        // lee faces a touch darker
c = lerp(c, c * float3(1.08, 1.05, 1.0) + 0.02, saturate(-N.x * 3) * 0.5);        // windward paler
return c;
"""

# Wind ripples, wind along +X. Wavelength and direction drift with noise, the
# pattern breaks into patches, and the steep lee faces (normal facing downwind)
# stay smooth like real slip faces. Two scales fade with camera distance; close
# up the scanned texture takes over.
RIPPLES = """
float d = distance(WP, Cam);
float ang = (S.fbm(WP * 0.00035) - 0.5) * 0.9;                    // direction drift, radians
float2 dir = float2(cos(ang), sin(ang));
float along = dot(WP.xy, dir);
float lam1 = lerp(16, 30, S.noise(WP * 0.0007 + 3));                // varying wavelength
float u1 = (along + 30 * S.fbm(WP * 0.006) * 4) / lam1;
float u2 = (WP.x * 0.97 + 300 * S.fbm(WP * 0.0011 + 9)) / 95.0;
float near = 1 - saturate((d - 800) / 2700);
float far = 1 - saturate((d - 3000) / 12000);
float lee = 1 - smoothstep(0.06, 0.28, N.x);                         // no ripples on slip faces
float patchy = lerp(0.25, 1.15, smoothstep(0.3, 0.7, S.fbm(WP * 0.0005 + 21)));
patchy *= 1 - 0.75 * smoothstep(0.45, 0.62, S.fbm(WP * 0.0004 + 40));     // desert patches are flatter
float r1 = frac(u1); r1 = r1 < 0.7 ? r1 / 0.7 : (1 - r1) / 0.3;    // asymmetric ripple profile
float scan = 0.3 + 0.7 * saturate((d - 1500) / 1500);               // the scan takes over close up
float s = -0.45 * (r1 - 0.5) * near * scan - 0.25 * cos(6.2832 * u2) * far;
return normalize(N + float3(dir * s * Mask * Strength * lee * patchy, 0));
"""

# Glints: a few grains catch the light, sparse random cells that twinkle with the
# view angle and vanish beyond ~15 m before they can alias.
# emissive the hazed stone fades to; calibrated so fully hazed stone matches the fog
# behind it (Tools/.bridge calibration: HAZE_O Start=0 End=1, compare stone vs sky)
HAZE_COLOR = (1.5, 0.55, 0.12)       # = DustFog inscattering in cinematic_pass.py: keep them equal

HAZE = """
// 0..1: how far this point has dissolved into the high dust. Rises with height, wobbles
// so the haze line billows, and stays off right next to the camera.
float wob = sin(WP.x * 0.00031 + WP.y * 0.00017) * 0.5 + sin(WP.y * 0.00043 - WP.x * 0.00011) * 0.5;
float h = smoothstep(Start, End, WP.z + wob * 1500.0);
float nearFade = saturate(distance(WP, Cam) / Near);
return Amount * h * h * (3.0 - 2.0 * h) * nearFade;
"""

GLINTS = """
float d = distance(WP, Cam);
float fade = 1 - saturate((d - 400) / 1100);
if (fade <= 0) return 0;
float3 v = normalize(WP - Cam);
float3 cell = floor(WP / 0.8);
float h = S.hash(cell + floor(v * 40) * 0.37);
float g = smoothstep(0.9975, 1.0, h) * (0.5 + S.hash(cell * 1.7));
return Sand * g * 6 * fade * Mask * Strength * saturate(N.z);
"""

# Megascans Rippled Sand (2 m tile), in distance tiers so far sand costs nothing:
#   < ~30 m   8K maps, plus a rotated 2K layer at 6.7 m to break the tiling
#   30-100 m  2K layer only
#   > 100 m   no texture reads at all; procedural colour and big ripples carry it
# Desert Sand 1K is mixed in as wind-flattened patches up to ~100 m.
# [branch] skips the samples entirely; Grad sampling keeps the right mips inside it.
TEX = "/Game/VisEx/Megascans/RippledSand/"
TEX_COMMON = """
float d = distance(WP, Cam);
float w8 = 1 - saturate((d - 1500) / 1500);
float w2 = 1 - saturate((d - 6000) / 4000);
float2 uv8 = WP.xy / 200;
const float ca = 0.799, sa = 0.602;                     // 37 degrees
float2 uv2 = float2(ca * WP.x - sa * WP.y, sa * WP.x + ca * WP.y) / 670;
float2 dx8 = ddx(uv8), dy8 = ddy(uv8), dx2 = ddx(uv2), dy2 = ddy(uv2);
// Desert Sand patches: wind-flattened ground between the rippled fields
float flat = smoothstep(0.45, 0.62, S.fbm(WP * 0.0004 + 40));
float2 uvd = float2(-sa * WP.x + ca * WP.y, ca * WP.x + sa * WP.y) / 230;
float2 dxd = ddx(uvd), dyd = ddy(uvd);
"""

# multiplies the sand colour by each scan's detail, normalised by its own average
# (the 1x1 mip), so neither scan's hue leaks in and both match SandColor
TEX_COLOR = """
float3 det = 1;
[branch] if (w8 > 0) {
    float3 a = Texture2DSampleGrad(B8, B8Sampler, uv8, dx8, dy8).rgb;
    float3 m = Texture2DSampleLevel(B8, B8Sampler, float2(0.5, 0.5), 13).rgb;
    det *= lerp(1, a / max(m, 0.02), w8 * TexAmt);
}
[branch] if (w2 > 0) {
    float3 a = Texture2DSampleGrad(B2, B2Sampler, uv2, dx2, dy2).rgb;
    float3 m = Texture2DSampleLevel(B2, B2Sampler, float2(0.5, 0.5), 11).rgb;
    det *= lerp(1, a / max(m, 0.02), w2 * TexAmt * 0.6);
}
float3 des = 1;
[branch] if (w2 > 0 && flat > 0) {
    float3 a = Texture2DSampleGrad(D, DSampler, uvd, dxd, dyd).rgb;
    float3 m = Texture2DSampleLevel(D, DSampler, float2(0.5, 0.5), 10).rgb;
    des = lerp(1, a / max(m, 0.02), w2 * TexAmt);
}
det = lerp(det, des, flat);
float l = dot(det, float3(0.3, 0.59, 0.11));
return c * lerp(l.xxx, det, 0.3);
"""

TEX_NORMAL = """
float2 t = 0;
[branch] if (w8 > 0) {
    t += (Texture2DSampleGrad(N8, N8Sampler, uv8, dx8, dy8).xy * 2 - 1) * w8;
}
[branch] if (w2 > 0) {
    float2 n = Texture2DSampleGrad(N2, N2Sampler, uv2, dx2, dy2).xy * 2 - 1;
    t += float2(ca * n.x + sa * n.y, -sa * n.x + ca * n.y) * w2 * 0.6;   // back to world axes
}
float2 td = 0;
[branch] if (w2 > 0 && flat > 0) {
    float2 n = Texture2DSampleGrad(DN, DNSampler, uvd, dxd, dyd).xy * 2 - 1;
    td = float2(-sa * n.x + ca * n.y, ca * n.x + sa * n.y) * w2;
}
t = lerp(t, td, flat);
t.y *= FlipY;
return normalize(RN + float3(t * Strength * Mask, 0));
"""

# Stone from our baked maps (bake_stone.py): masonry on walls (4 x 8 blocks of
# 430 x 190 per tile), giant once-polished 16 x 10 m slabs on horizontal faces.
# The tiles' grids line up with the world grids below, so per-block/slab tint,
# grime, streaks and erosion stay world-space and the tiling never shows.
STONE_TEX = """
float3 an = abs(N);
bool top = an.z > an.x && an.z > an.y;
float2 tile = top ? float2(3200, 2000) : float2(1720, 1520);
float2 tuv = uv / tile;
float2 tdx = ddx(tuv), tdy = ddy(tuv);
float3 tb, tm, tn, orm;
if (top) {
    tb = Texture2DSampleGrad(SB, SBSampler, tuv, tdx, tdy).rgb;
    tm = Texture2DSampleLevel(SB, SBSampler, float2(0.5, 0.5), 12).rgb;
    tn = Texture2DSampleGrad(SN, SNSampler, tuv, tdx, tdy).rgb;
    orm = Texture2DSampleGrad(SO, SOSampler, tuv, tdx, tdy).rgb;
} else {
    tb = Texture2DSampleGrad(WB, WBSampler, tuv, tdx, tdy).rgb;
    tm = Texture2DSampleLevel(WB, WBSampler, float2(0.5, 0.5), 12).rgb;
    tn = Texture2DSampleGrad(WN, WNSampler, tuv, tdx, tdy).rgb;
    orm = Texture2DSampleGrad(WO, WOSampler, tuv, tdx, tdy).rgb;
}
"""

STONE_COLOR = """
float vert = 1 - an.z;
float srow = floor(uv.y / 1000);
float id = top ? S.hash(float3(floor(uv.x / 1600 + 0.5 * fmod(abs(srow), 2)), srow, 9.1)) : S.blocks(uv, Wear).y;
float3 c = Stone * (tb / max(tm, 0.02)) * lerp(0.88, 1.12, id);
float grime = S.fbm(WP * 0.004);
float streaks = smoothstep(0.45, 0.8, S.fbm(float3(uv.x * 0.012, uv.y * 0.0012, id * 9))) * vert;
float eroded = smoothstep(0.5, 0.75, S.fbm(WP * 0.0025));           // sand-blasted patches
c *= lerp(1, 0.65 + 0.6 * grime, saturate(Wear * 0.8));
c = lerp(c, c * 1.18 + 0.02, eroded * saturate(Wear) * 0.6);
c *= 1 - saturate(0.4 * Wear) * streaks;
// sand settles in joints, cracks and spalls
float recess = smoothstep(0.62, 0.4, orm.b);
return lerp(c, Sand * 0.8, recess * (top ? 0.4 : 0.3));
"""

STONE_NORMAL = """
float3 tu, tv;
if (top)              { tu = float3(1,0,0); tv = float3(0,1,0); }
else if (an.x > an.y) { tu = float3(0,1,0); tv = float3(0,0,1); }
else                  { tu = float3(1,0,0); tv = float3(0,0,1); }
float2 n = tn.xy * 2 - 1;
return normalize(RN + (n.x * tu + n.y * tv) * (1 - Mask) * Strength);
"""

STONE_ORM = """
return float3(lerp(1, orm.r, 0.35), orm.g, 0);   // baked AO is strong; keep a third of it
"""


def build_material():
    # rebuild the graph in place so instances (MI_Dunes) keep their parent
    if unreal.EditorAssetLibrary.does_asset_exist(MAT):
        mat = unreal.load_asset(MAT)
        mel.delete_all_material_expressions(mat)
    else:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_SandCover", "/Game/VisEx", unreal.Material, unreal.MaterialFactoryNew())
    node = lambda cls, x, y: mel.create_material_expression(mat, cls, x, y)
    def link(src, out, dst, inp):
        if not mel.connect_material_expressions(src, out, dst, inp):
            print("LINK FAILED: %s -> %s.%s" % (src.get_class().get_name(), dst.get_class().get_name(), inp))

    def vparam(name, rgb, x, y):
        n = node(unreal.MaterialExpressionVectorParameter, x, y)
        n.set_editor_property("parameter_name", name)
        n.set_editor_property("default_value", unreal.LinearColor(*rgb, 1))
        return n

    def sparam(name, v, x, y):
        n = node(unreal.MaterialExpressionScalarParameter, x, y)
        n.set_editor_property("parameter_name", name)
        n.set_editor_property("default_value", v)
        return n

    stone, sand = vparam("StoneColor", STONE, -400, -300), vparam("SandColor", SAND, -400, -150)

    # how much a surface faces up: saturate(normal.z) ^ sharpness
    normal = node(unreal.MaterialExpressionVertexNormalWS, -1600, 0)
    nz = node(unreal.MaterialExpressionComponentMask, -1400, 0)
    for c, on in zip("rgba", (False, False, True, False)):
        nz.set_editor_property(c, on)
    link(normal, "", nz, "")
    nz_sat = node(unreal.MaterialExpressionSaturate, -1250, 0)
    link(nz, "", nz_sat, "")
    up = node(unreal.MaterialExpressionPower, -1100, 0)
    link(nz_sat, "", up, "Base")
    link(sparam("TopSharpness", 3.0, -1300, 100), "", up, "Exp")

    # distance from the steps → 0..1
    wp = node(unreal.MaterialExpressionWorldPosition, -1600, 300)
    centre = node(unreal.MaterialExpressionConstant3Vector, -1600, 420)
    centre.set_editor_property("constant", unreal.LinearColor(*CLEAN_CENTRE, 0))
    dist = node(unreal.MaterialExpressionDistance, -1400, 350)
    link(wp, "", dist, "A")
    link(centre, "", dist, "B")
    minus = node(unreal.MaterialExpressionSubtract, -1250, 350)
    link(dist, "", minus, "A")
    link(sparam("SandNear", NEAR, -1400, 480), "", minus, "B")
    div = node(unreal.MaterialExpressionDivide, -1100, 350)
    link(minus, "", div, "A")
    link(sparam("SandRange", FAR - NEAR, -1250, 480), "", div, "B")
    far01 = node(unreal.MaterialExpressionSaturate, -950, 350)
    link(div, "", far01, "")
    amount = node(unreal.MaterialExpressionLinearInterpolate, -800, 300)
    link(sparam("MinCover", MIN_COVER, -950, 220), "", amount, "A")
    one = node(unreal.MaterialExpressionConstant, -950, 280)
    one.set_editor_property("r", 1.0)
    link(one, "", amount, "B")
    link(far01, "", amount, "Alpha")

    # patchiness: world-space noise scales the cover between 0.4x and 1.6x
    noise = node(unreal.MaterialExpressionNoise, -1100, 600)
    noise.set_editor_property("scale", 0.0012)
    noise.set_editor_property("output_min", 0.4)
    noise.set_editor_property("output_max", 1.6)
    link(wp, "", noise, "Position")

    cover = node(unreal.MaterialExpressionMultiply, -650, 100)
    link(up, "", cover, "A")
    link(amount, "", cover, "B")
    patchy = node(unreal.MaterialExpressionMultiply, -500, 200)
    link(cover, "", patchy, "A")
    link(noise, "", patchy, "B")
    mask = node(unreal.MaterialExpressionSaturate, -350, 200)
    link(patchy, "", mask, "")

    def custom(code, names, x, y, out=unreal.CustomMaterialOutputType.CMOT_FLOAT3):
        n = node(unreal.MaterialExpressionCustom, x, y)
        n.set_editor_property("output_type", out)
        inputs = []
        for name in names:
            ci = unreal.CustomInput()
            ci.set_editor_property("input_name", name)
            inputs.append(ci)
        n.set_editor_property("inputs", inputs)
        n.set_editor_property("code", code)
        return n

    # weathered masonry: world-aligned blocks, chipped joints, per-block tint, grime
    # streaks running down vertical faces, pitting, sand settled in the joints
    stone_dir = "/Game/VisEx/Stone/"
    stone_tex = {}
    for k, (asset, nm, y) in {"WB": ("T_Stone_Wall_B", False, -900), "WN": ("T_Stone_Wall_N", True, -850),
                              "WO": ("T_Stone_Wall_ORM", False, -800), "SB": ("T_Stone_Slab_B", False, -750),
                              "SN": ("T_Stone_Slab_N", True, -700), "SO": ("T_Stone_Slab_ORM", False, -650)}.items():
        t = node(unreal.MaterialExpressionTextureObjectParameter, -1100, y)
        t.set_editor_property("parameter_name", "Stone" + k)
        t.set_editor_property("texture", unreal.load_asset(stone_dir + asset))
        t.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if nm
                              else (unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if k.endswith("B")
                                    else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS))
        stone_tex[k] = t

    def stone_node(code, names, x, y, out=unreal.CustomMaterialOutputType.CMOT_FLOAT3):
        n = custom(STONE_LIB + STONE_TEX + code, ("WP", "N") + names + tuple(stone_tex), x, y, out)
        link(wp, "", n, "WP")
        link(normal, "", n, "N")
        for k, t in stone_tex.items():
            link(t, "", n, k)
        return n

    wear = sparam("Weathering", 1.6, -700, -450)
    stone_col = stone_node(STONE_COLOR, ("Stone", "Sand", "Wear"), -500, -450)
    link(stone, "", stone_col, "Stone")
    link(sand, "", stone_col, "Sand")
    link(wear, "", stone_col, "Wear")

    def tparam(name, asset, normal_map, x, y):
        n = node(unreal.MaterialExpressionTextureObjectParameter, x, y)
        n.set_editor_property("parameter_name", name)
        n.set_editor_property("texture", unreal.load_asset(TEX + asset))
        n.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal_map
                              else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        return n

    cam_pos = node(unreal.MaterialExpressionCameraPositionWS, -600, 750)
    sand_col = custom(STONE_LIB + TEX_COMMON + SAND_COLOR.replace("return c;", TEX_COLOR),
                      ("WP", "N", "Sand", "Cam", "B8", "B2", "D", "TexAmt"), -500, -150)
    for name, src in (("WP", wp), ("N", normal), ("Sand", sand), ("Cam", cam_pos),
                      ("B8", tparam("SandAlbedo8K", "T_Sand_8K_B", False, -900, -250)),
                      ("B2", tparam("SandAlbedo2K", "T_Sand_2K_B", False, -900, -100)),
                      ("D", tparam("DesertAlbedo1K", "T_Desert_1K_B", False, -900, -50)),
                      ("TexAmt", sparam("SandTexDetail", 0.8, -900, 0))):
        link(src, "", sand_col, name)

    base = node(unreal.MaterialExpressionLinearInterpolate, -200, -200)
    link(stone_col, "", base, "A")
    link(sand_col, "", base, "B")
    link(mask, "", base, "Alpha")
    # height haze: high stone dissolves into the dust (height fog only thickens downward,
    # and local fog volumes don't render here). Lit colour fades out, dust colour fades in.
    haze_t = custom(HAZE, ("WP", "Cam", "Start", "End", "Amount", "Near"), -200, -350,
                    unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    link(wp, "", haze_t, "WP")
    link(node(unreal.MaterialExpressionCameraPositionWS, -400, -380), "", haze_t, "Cam")
    # HAZE_O (job global) overrides these for calibration runs
    hz = dict(Start=3000.0, End=30000.0, Amount=1.0, Near=4000.0, Color=HAZE_COLOR)
    hz.update(globals().get("HAZE_O") or {})
    for name, y in (("Start", -560), ("End", -520), ("Amount", -480), ("Near", -440)):
        link(sparam("Haze" + name, hz[name], -400, y), "", haze_t, name)
    hazed = node(unreal.MaterialExpressionLinearInterpolate, -50, -250)
    link(base, "", hazed, "A")
    zero = node(unreal.MaterialExpressionConstant3Vector, -200, -280)
    link(zero, "", hazed, "B")
    link(haze_t, "", hazed, "Alpha")
    mel.connect_material_property(hazed, "", unreal.MaterialProperty.MP_BASE_COLOR)
    # no sun glints on hazed stone, or the tops catch light the fog doesn't
    spec = node(unreal.MaterialExpressionLinearInterpolate, -50, -150)
    half = node(unreal.MaterialExpressionConstant, -200, -150)
    half.set_editor_property("r", 0.5)
    link(half, "", spec, "A")
    link(zero, "", spec, "B")
    link(haze_t, "", spec, "Alpha")
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    # wind ripples bend the world-space normal where there is sand (see RIPPLES)
    ripples = custom(STONE_LIB + RIPPLES, ("WP", "Cam", "N", "Mask", "Strength"), -200, 600)
    link(wp, "", ripples, "WP")
    link(cam_pos, "", ripples, "Cam")
    link(normal, "", ripples, "N")
    link(mask, "", ripples, "Mask")
    link(sparam("RippleStrength", 1.0, -400, 850), "", ripples, "Strength")

    sand_n = custom(STONE_LIB + TEX_COMMON + TEX_NORMAL,
                    ("WP", "N", "Cam", "RN", "Mask", "N8", "N2", "DN", "Strength", "FlipY"), -100, 750)
    for name, src in (("WP", wp), ("N", normal), ("Cam", cam_pos), ("RN", ripples), ("Mask", mask),
                      ("N8", tparam("SandNormal8K", "T_Sand_8K_N", True, -400, 1100)),
                      ("N2", tparam("SandNormal2K", "T_Sand_2K_N", True, -400, 1250)),
                      ("DN", tparam("DesertNormal1K", "T_Desert_1K_N", True, -400, 1320)),
                      ("Strength", sparam("SandTexNormal", 0.6, -400, 1400)),
                      ("FlipY", sparam("SandTexFlipY", -1.0, -400, 1500))):
        link(src, "", sand_n, name)

    # baked stone relief on top of the rippled normal, where there's no sand
    stone_n = stone_node(STONE_NORMAL, ("RN", "Mask", "Strength"), 0, 600)
    link(sand_n, "", stone_n, "RN")
    link(mask, "", stone_n, "Mask")
    link(sparam("StoneNormal", 1.0, -200, 700), "", stone_n, "Strength")
    mat.set_editor_property("tangent_space_normal", False)
    mel.connect_material_property(stone_n, "", unreal.MaterialProperty.MP_NORMAL)

    glints = custom(STONE_LIB + GLINTS, ("WP", "Cam", "N", "Sand", "Mask", "Strength"), -200, 900)
    for name, src in (("WP", wp), ("Cam", cam_pos), ("N", normal), ("Sand", sand), ("Mask", mask)):
        link(src, "", glints, name)
    link(sparam("GlintStrength", 1.0, -400, 1000), "", glints, "Strength")
    glow = node(unreal.MaterialExpressionLinearInterpolate, 0, 900)
    link(glints, "", glow, "A")
    link(vparam("HazeColor", hz["Color"], -200, 1000), "", glow, "B")
    link(haze_t, "", glow, "Alpha")
    mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    # roughness and AO from the baked ORM where there's stone; dry sand is matte
    orm = stone_node(STONE_ORM, (), -500, 50)
    orm_r = node(unreal.MaterialExpressionComponentMask, -300, 0)
    orm_r.set_editor_property("r", True); orm_r.set_editor_property("g", False)
    orm_g = node(unreal.MaterialExpressionComponentMask, -300, 80)
    orm_g.set_editor_property("r", False); orm_g.set_editor_property("g", True)
    link(orm, "", orm_r, ""); link(orm, "", orm_g, "")
    # weathered stone is never really shiny; this floor keeps the old polish subtle
    rough_min = node(unreal.MaterialExpressionMax, -220, 40)
    link(orm_g, "", rough_min, "A")
    link(sparam("StoneMinRoughness", 0.68, -380, 120), "", rough_min, "B")
    rough = node(unreal.MaterialExpressionLinearInterpolate, -150, 60)
    link(rough_min, "", rough, "A")
    link(sparam("SandRoughness", 0.95, -300, 160), "", rough, "B")
    link(mask, "", rough, "Alpha")
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    ao = node(unreal.MaterialExpressionLinearInterpolate, -150, 160)
    link(orm_r, "", ao, "A")
    one_ao = node(unreal.MaterialExpressionConstant, -300, 240)
    one_ao.set_editor_property("r", 1.0)
    link(one_ao, "", ao, "B")
    link(mask, "", ao, "Alpha")
    ao_h = node(unreal.MaterialExpressionLinearInterpolate, 0, 160)     # hazed stone: no AO pattern
    link(ao, "", ao_h, "A")
    link(one_ao, "", ao_h, "B")
    link(haze_t, "", ao_h, "Alpha")
    mel.connect_material_property(ao_h, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)

    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(MAT)
    return mat


mat = build_material()
n = 0
for a in eas.get_all_level_actors():
    # the dune field has its own all-sand instance (add_dunes.py), which replaced the drifts
    # StepRamp is the steps' invisible collision ramp: leave its material alone
    if isinstance(a, unreal.StaticMeshActor) and a.get_actor_label() not in ("Dunes", "StepRamp") \
            and not a.get_actor_label().startswith("Drift_"):
        a.static_mesh_component.set_material(0, mat)
        n += 1
print("sand material on %d meshes" % n)
print("saved", unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level())
