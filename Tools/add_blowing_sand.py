"""Blowing sand: a thin translucent sheet draped 15 cm over the dunes, with gusty
streaks of sand racing downwind (+X) across it. The sheet uses the dune mesh's own
vertex grid and settled heights, so it never dips into the sand. (Crest plumes
were tried and removed: hovering sheet edges read as torn geometry.)

Lit translucency, depth fade where it touches anything, gone by ~50 m from the
camera. Labelled Dunes_Blow so photo mode keeps it. No collision. Idempotent;
re-run after add_dunes.py.

Run live: python3 Tools/ue.py Tools/add_blowing_sand.py
"""
import os

import unreal

HERE = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools")
LIFT = 15.0                  # cm over the sand
SPEED = 650.0                # cm/s, gusts downwind
OPACITY = 0.45
FADE_NEAR, FADE_FAR = 2000.0, 5000.0    # cm from the camera: full until, gone by (grazing sheets smear further out)
SAND = (1.0, 0.74, 0.5)       # paler than the dunes: airborne sand catches light
GLOW = 0.25                   # emissive share, so sheets read against same-coloured sand
ASSET_DIR, ASSET_NAME, MAT = "/Game/VisEx", "SM_BlowingSand", "/Game/VisEx/M_BlowingSand"

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
mel = unreal.MaterialEditingLibrary

# the dune field's shape, grid and height function, without running it
src = open(os.path.join(HERE, "add_dunes.py")).read()
D = {"__name__": "dunes_lib"}
exec(src[:src.index("\nfor a in eas.get_all_level_actors():")], D)

NOISE = """
float3 p = float3((WP.x - T * Speed) / 900.0, WP.y / 140.0, T * 0.12);
float n = 0, a = 0.5;
for (int k = 0; k < 4; k++) {
    float3 i = floor(p), f = frac(p); f = f * f * (3 - 2 * f);
    float4 h = frac(sin(float4(dot(i, float3(127.1, 311.7, 74.7)), dot(i + float3(1,0,0), float3(127.1, 311.7, 74.7)),
                               dot(i + float3(0,1,0), float3(127.1, 311.7, 74.7)), dot(i + float3(1,1,0), float3(127.1, 311.7, 74.7)))) * 43758.5453);
    float4 h2 = frac(sin(float4(dot(i + float3(0,0,1), float3(127.1, 311.7, 74.7)), dot(i + float3(1,0,1), float3(127.1, 311.7, 74.7)),
                                dot(i + float3(0,1,1), float3(127.1, 311.7, 74.7)), dot(i + float3(1,1,1), float3(127.1, 311.7, 74.7)))) * 43758.5453);
    float v0 = lerp(lerp(h.x, h.y, f.x), lerp(h.z, h.w, f.x), f.y);
    float v1 = lerp(lerp(h2.x, h2.y, f.x), lerp(h2.z, h2.w, f.x), f.y);
    n += a * lerp(v0, v1, f.z);
    p = p * float3(2.1, 2.3, 1.7) + float3(T * Speed / 900.0 * 0.6, 0, 0);
    a *= 0.5;
}
n /= 0.9375;
float gust = smoothstep(0.42, 0.78, n);                         // sheets come and go
// fine streaks racing along the wind
float2 q = float2((WP.x - T * Speed * 1.4) / 70.0, WP.y / 9.0);
float2 qi = floor(q), qf = frac(q); qf = qf * qf * (3 - 2 * qf);
float s00 = frac(sin(dot(qi, float2(12.9898, 78.233))) * 43758.5453);
float s10 = frac(sin(dot(qi + float2(1, 0), float2(12.9898, 78.233))) * 43758.5453);
float s01 = frac(sin(dot(qi + float2(0, 1), float2(12.9898, 78.233))) * 43758.5453);
float s11 = frac(sin(dot(qi + float2(1, 1), float2(12.9898, 78.233))) * 43758.5453);
float streak = lerp(lerp(s00, s10, qf.x), lerp(s01, s11, qf.x), qf.y);
float d = distance(WP, Cam);
float near = 1 - smoothstep(FadeNear, FadeFar, d);
float plume = exp(-Gap / 1.2) * (1 + 1.5 * saturate(Gap * 4) * (1 - saturate(Gap / 2)));   // thick just off the crest
return saturate(gust * (0.35 + 0.65 * streak) * plume * near * Opacity);
"""


def material():
    if unreal.EditorAssetLibrary.does_asset_exist(MAT):
        m = unreal.load_asset(MAT)
        mel.delete_all_material_expressions(m)
    else:
        m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_BlowingSand", ASSET_DIR, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property("translucency_lighting_mode",
                          unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
    m.set_editor_property("two_sided", True)

    def node(cls, x, y):
        return mel.create_material_expression(m, cls, x, y)

    wp = node(unreal.MaterialExpressionWorldPosition, -900, 0)
    cam = node(unreal.MaterialExpressionCameraPositionWS, -900, 100)
    t = node(unreal.MaterialExpressionTime, -900, 200)
    uv = node(unreal.MaterialExpressionTextureCoordinate, -900, 300)
    gap = node(unreal.MaterialExpressionComponentMask, -750, 300)
    gap.set_editor_property("r", True)
    mel.connect_material_expressions(uv, "", gap, "")
    cu = node(unreal.MaterialExpressionCustom, -500, 100)
    cu.set_editor_property("code", NOISE)
    cu.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    consts = dict(Speed=SPEED, FadeNear=FADE_NEAR, FadeFar=FADE_FAR, Opacity=OPACITY)
    ins = []
    for n in ["WP", "Cam", "T", "Gap"] + list(consts):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    cu.set_editor_property("inputs", ins)
    for n, e in (("WP", wp), ("Cam", cam), ("T", t), ("Gap", gap)):
        mel.connect_material_expressions(e, "", cu, n)
    y = 450
    for n, v in consts.items():
        p = node(unreal.MaterialExpressionScalarParameter, -750, y)
        p.set_editor_property("parameter_name", n)
        p.set_editor_property("default_value", v)
        mel.connect_material_expressions(p, "", cu, n)
        y += 80
    fade = node(unreal.MaterialExpressionDepthFade, -250, 150)
    fade.set_editor_property("fade_distance_default", 40.0)
    mel.connect_material_expressions(cu, "", fade, "Opacity")
    col = node(unreal.MaterialExpressionVectorParameter, -500, -150)
    col.set_editor_property("parameter_name", "SandColor")
    col.set_editor_property("default_value", unreal.LinearColor(*SAND, 1))
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    glow = node(unreal.MaterialExpressionMultiply, -250, -150)
    glow.set_editor_property("const_b", GLOW)
    mel.connect_material_expressions(col, "", glow, "A")
    mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(fade, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_asset(MAT)
    return m


def write_obj(path):
    x0, x1 = D["CORE"][0] - D["BOUND"] - 5000, D["CORE"][1] + D["BOUND"] + 5000
    y0, y1 = D["CORE"][2] - D["BOUND"] - 5000, D["CORE"][3] + D["BOUND"] + 5000
    xs = [x for x in D["axis"](D["X0"], D["X1"], D["CORE"][0] - D["FAR"], D["CORE"][1] + D["FAR"]) if x0 <= x <= x1]
    ys = [y for y in D["axis"](D["Y0"], D["Y1"], D["CORE"][2] - D["FAR"], D["CORE"][3] + D["FAR"]) if y0 <= y <= y1]
    nx, ny = len(xs), len(ys)
    v, vt = [], []
    zs = D["heights"](xs, ys)                                  # the dunes exactly, settled slopes included
    for j, y in enumerate(ys):
        for i, x in enumerate(xs):
            v.append("v %.1f %.1f %.1f" % (x, -y, zs[j][i] + LIFT))   # importer mirrors Y
            vt.append("vt 0 0")
    f = []
    for j in range(ny - 1):
        for i in range(nx - 1):
            a = j * nx + i + 1
            b, c, d = a + 1, a + nx, a + nx + 1
            f.append("f %d/%d %d/%d %d/%d" % (a, a, c, c, b, b))
            f.append("f %d/%d %d/%d %d/%d" % (b, b, c, c, d, d))
    with open(path, "w") as fh:
        fh.write("\n".join(v + vt + f) + "\n")
    return nx * ny


def import_mesh(path):
    full = ASSET_DIR + "/" + ASSET_NAME
    for a in eas.get_all_level_actors():
        if a.get_actor_label() == "Dunes_Blow":
            eas.destroy_actor(a)
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        unreal.EditorAssetLibrary.delete_asset(full)
    task = unreal.AssetImportTask()
    for k, val in (("filename", path), ("destination_path", ASSET_DIR), ("destination_name", ASSET_NAME),
                   ("automated", True), ("save", False), ("replace_existing", True)):
        task.set_editor_property(k, val)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.load_asset(full)
    ns = mesh.get_editor_property("nanite_settings")
    ns.set_editor_property("enabled", False)          # translucent: no Nanite
    mesh.set_editor_property("nanite_settings", ns)
    unreal.EditorAssetLibrary.save_asset(full)
    return mesh


mat = material()
obj = os.path.join(HERE, ".bridge", "blowing_sand.obj")
verts = write_obj(obj)
mesh = import_mesh(obj)
actor = eas.spawn_actor_from_object(mesh, unreal.Vector(0, 0, 0))
actor.set_actor_label("Dunes_Blow")
smc = actor.static_mesh_component
smc.set_material(0, mat)
# it spans the whole map: any collision blocks the pawn spawn, cameras and floor traces.
# The profile must be set after the mesh (spawning from the mesh gives BlockAll).
smc.set_collision_profile_name("NoCollision")
smc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
actor.set_actor_enable_collision(False)
smc.set_editor_property("cast_shadow", False)
print("blowing sand: %d verts" % verts)
