"""The android: a smooth opaque skin over a mechanical body (Docs/CHARACTER_DIRECTION.md). Idempotent.

  python3 Tools/ue.py Tools/char_shell.py

- Scales the player (Quinn, 177.5 cm) to HEIGHT: mesh, capsule and mesh offset together.
- M_AerogelShell: a translucent skin. Milky where thick (the gap between skin and the
  body behind it, scene depth - pixel depth) and at grazing angles (Fresnel), clear where
  thin, so the machine inside shows faintly. Blue scatter, soft inner pulse. The shell
  is pushed out along its normals by INFLATE (World Position Offset).
- MI_AndroidCore: the inner machine, the mannequin material darkened to graphite.
- Adds Shell (UVisexShellComponent, C++) under Mesh: same skeleton, follows the body's
  pose via Leader Pose, so every animation, montage and pose drives both layers.

All look values are in SHELL; tweak and re-run. The material is rebuilt each run.
"""
import unreal

BP_PATH = "/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"
DIR = "/Game/Character/Android"
QUINN = "/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple"
QUINN_HEIGHT = 177.5     # measured: top of head in MM_Idle
HEIGHT = 159.0           # Tema's spec
SCALE = HEIGHT / QUINN_HEIGHT
CAPSULE = (90.0, 35.0)   # template half-height, radius at scale 1
MESH_Z = -89.0           # template mesh offset at scale 1

SHELL = dict(
    inflate=1.6,                     # cm the skin floats above the machine
    thick_scale=10.0,                 # cm of gap that reads fully milky
    fresnel_exponent=3.0,
    opacity_min=0.80,                # face-on: the machine is barely there
    opacity_max=0.98,                # edges and thick areas: solid white
    skin=(0.86, 0.86, 0.84),         # mannequin white, a hair warm
    scatter=(0.70, 0.80, 1.00),      # faint cool haze at the rim
    scatter_strength=0.35,
    glow=(0.35, 0.70, 1.00),         # inner pulse, seen through thin skin
    glow_strength=0.25,
    glow_speed=0.9,                  # rad/s: a slow cooling cycle (~7 s)
    roughness=0.75,                  # matte, like a display mannequin
    specular=0.25,
)
MODE = "real"                        # "real": human skin, subsurface profile, soft sheen (current)
                                     # "latex": glossy clear-coat skin (Tema's first reference)
                                     # "skin": matte mannequin white
                                     # "aerogel": translucent, the machine shows through
LATEX = dict(
    skin=(0.72, 0.36, 0.19),         # warm peach latex (reference)
    roughness=0.32,                  # the base under the coat: soft sheen
    specular=0.55,
    clearcoat=1.0,                   # the wet top layer
    clearcoat_roughness=0.05,        # sharp, long highlights
)
REAL = dict(
    skin=(0.58, 0.36, 0.25),         # warm tan albedo (linear); real skin is far less saturated than latex
    scatter=(0.85, 0.34, 0.20),      # blood-red bleed in shadows, ears, fingers, at the terminator
    roughness=0.52,                  # broad soft highlight, no mirror streaks
    roughness_var=0.035,             # pore-scale break-up, so the sheen isn't plastic (more reads as a cracked web)
    pore_scale=2.5,                  # noise cells per cm on the body at rest: pore-sized
    specular=0.35,                   # skin is ~2.8% reflective at normal incidence
    bump=0.012,                      # cm of pore relief: breaks the highlight into grain up close
    patch_rough=0.06,                # palm-sized areas a little drier or oilier
    patch_color=0.05,                # the same areas a little paler or ruddier
    patch_scale=0.09,                # noise cells per cm: about 11 cm patches
)
SKIN = dict(
    inflate=0.0,                     # the skin is its own mesh now (char_skin.py)
    skin=(0.74, 0.72, 0.69),         # mannequin white with a living warmth
    subsurface=(0.85, 0.66, 0.58),   # light bleeding through thin skin, like a human ear
    subsurface_amount=0.0,           # 0 = plain lit: the low sun blows subsurface out to flat white
    roughness=0.62,
    specular=0.30,
)
CORE_TINT = (0.20, 0.21, 0.23)       # mid-grey machine: low contrast under the skin

if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise RuntimeError("Play mode is running: stop PIE before saving assets")

at = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
MP = unreal.MaterialProperty


def asset(name, cls, factory):
    path = DIR + "/" + name
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    return at.create_asset(name, DIR, cls, factory)


# ---- shell material ----------------------------------------------------------------
m = asset("M_AerogelShell", unreal.Material, unreal.MaterialFactoryNew())
mel.delete_all_material_expressions(m)
X = [-1400]


def node(cls, x, y):
    return mel.create_material_expression(m, cls, x, y)


def scalar(name, v, x, y):
    n = node(unreal.MaterialExpressionScalarParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", v)
    return n


def vector(name, c, x, y):
    n = node(unreal.MaterialExpressionVectorParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", unreal.LinearColor(c[0], c[1], c[2], 1.0))
    return n


def op(cls, a, b, x, y, ia="A", ib="B"):
    n = node(cls, x, y)
    mel.connect_material_expressions(a, "", n, ia)
    if b is not None:
        mel.connect_material_expressions(b, "", n, ib)
    return n


def build_skin():
    """Opaque skin: hides the machine completely, soft light bleed like human skin."""
    K = SKIN
    m.set_editor_property("use_material_attributes", False)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SUBSURFACE if K["subsurface_amount"] > 0 else unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property("used_with_skeletal_mesh", True)
    mel.connect_material_property(vector("SkinColor", K["skin"], -450, -200), "", MP.MP_BASE_COLOR)
    mel.connect_material_property(scalar("Roughness", K["roughness"], -450, -80), "", MP.MP_ROUGHNESS)
    mel.connect_material_property(scalar("Specular", K["specular"], -450, -40), "", MP.MP_SPECULAR)
    mel.connect_material_property(vector("SubsurfaceColor", K["subsurface"], -450, 40), "", MP.MP_SUBSURFACE_COLOR)
    mel.connect_material_property(scalar("SubsurfaceAmount", K["subsurface_amount"], -450, 160), "", MP.MP_OPACITY)
    nrm = node(unreal.MaterialExpressionVertexNormalWS, -450, 300)
    wpo = op(unreal.MaterialExpressionMultiply, nrm, scalar("Inflate", K["inflate"], -450, 400), -250, 300)
    mel.connect_material_property(wpo, "", MP.MP_WORLD_POSITION_OFFSET)


def build_real():
    """Human skin: Subsurface Profile shading (UE's screen-space skin scatter) and a broad,
    slightly broken-up sheen. The roughness noise is fixed to the body at rest (pre-skinned
    position), so it doesn't swim when she moves."""
    K = REAL
    prof = asset("SSP_Skin", unreal.SubsurfaceProfile, unreal.SubsurfaceProfileFactory())
    st = prof.get_editor_property("settings")
    st.set_editor_property("surface_albedo", unreal.LinearColor(*K["skin"], 1.0))
    st.set_editor_property("mean_free_path_color", unreal.LinearColor(*K["scatter"], 1.0))
    st.set_editor_property("mean_free_path_distance", 0.9)
    prof.set_editor_property("settings", st)
    eal.save_loaded_asset(prof)
    m.set_editor_property("use_material_attributes", False)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SUBSURFACE_PROFILE)
    m.set_editor_property("subsurface_profile", prof)
    m.set_editor_property("used_with_skeletal_mesh", True)
    mel.connect_material_property(scalar("Specular", K["specular"], -450, -40), "", MP.MP_SPECULAR)
    mel.connect_material_property(scalar("Scatter", 1.0, -450, 160), "", MP.MP_OPACITY)
    m.set_editor_property("tangent_space_normal", False)
    skin = vector("SkinColor", K["skin"], -450, -200)
    vs = node(unreal.MaterialExpressionPreSkinnedPosition, -1300, 0)
    pos = node(unreal.MaterialExpressionVertexInterpolator, -1150, 0)
    mel.connect_material_expressions(vs, "", pos, "VS")

    def noise_at(scale, levels, y):
        n = node(unreal.MaterialExpressionNoise, -950, y)
        mel.connect_material_expressions(pos, "", n, "Position")
        n.set_editor_property("scale", scale)
        n.set_editor_property("levels", levels)
        n.set_editor_property("output_min", -1.0)
        n.set_editor_property("output_max", 1.0)
        return n

    def custom(code, out, inputs, x, y):
        c = node(unreal.MaterialExpressionCustom, x, y)
        c.set_editor_property("code", code)
        c.set_editor_property("output_type", out)
        ins = []
        for name, _ in inputs:
            i = unreal.CustomInput()
            i.set_editor_property("input_name", name)
            ins.append(i)
        c.set_editor_property("inputs", ins)
        for name, src in inputs:
            if not mel.connect_material_expressions(src, "", c, name):
                raise RuntimeError("custom pin " + name)
        return c

    pore = noise_at(K["pore_scale"], 3, 0)
    patch = noise_at(K["patch_scale"], 2, 260)
    # how much of the pore grain a pixel can still resolve: 1 up close, 0 once a pixel
    # covers more than about half a pore (then the grain would only shimmer)
    near = custom("float f = length(ddx(P)) + length(ddy(P)); return saturate((0.6 - f * 0.5 * S) / 0.35);",
                  unreal.CustomMaterialOutputType.CMOT_FLOAT1,
                  [("P", pos), ("S", scalar("PoreScale", K["pore_scale"], -950, 140))], -750, 140)
    # pore relief as a normal, from screen-space slopes of the noise (no UVs or textures needed)
    height = op(unreal.MaterialExpressionMultiply, pore, scalar("Bump", K["bump"], -750, 420), -600, 400)
    height = op(unreal.MaterialExpressionMultiply, height, near, -480, 400)
    bump = custom("float3 dpx = ddx(W), dpy = ddy(W); float dhx = ddx(H), dhy = ddy(H);\n"
                  "float3 r1 = cross(dpy, N), r2 = cross(N, dpx); float det = dot(dpx, r1);\n"
                  "float3 g = sign(det) * (dhx * r1 + dhy * r2);\n"
                  "return normalize(abs(det) * N - g);",
                  unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                  [("W", node(unreal.MaterialExpressionWorldPosition, -480, 520)),
                   ("N", node(unreal.MaterialExpressionVertexNormalWS, -480, 620)),
                   ("H", height)], -300, 480)
    mel.connect_material_property(bump, "", MP.MP_NORMAL)
    # roughness: pore break-up, drier and oilier patches, and the grain a far pixel can't
    # show any more folded back in as a slightly broader highlight
    var = op(unreal.MaterialExpressionMultiply, pore, scalar("RoughnessVar", K["roughness_var"], -950, 120), -750, 0)
    rough = op(unreal.MaterialExpressionAdd, scalar("Roughness", K["roughness"], -750, -100), var, -600, -80)
    rough = op(unreal.MaterialExpressionAdd, rough,
               op(unreal.MaterialExpressionMultiply, patch, scalar("PatchRoughness", K["patch_rough"], -950, 380), -750, 280),
               -450, -80)
    far = op(unreal.MaterialExpressionOneMinus, near, None, -600, 160, ia="")
    far = op(unreal.MaterialExpressionMultiply, far, None, -480, 160)
    far.set_editor_property("const_b", 0.05)
    rough = op(unreal.MaterialExpressionAdd, rough, far, -300, -80)
    mel.connect_material_property(rough, "", MP.MP_ROUGHNESS)
    # colour: the same patches, a touch paler or ruddier
    tone = op(unreal.MaterialExpressionMultiply, patch, scalar("PatchColor", K["patch_color"], -950, 460), -750, -260)
    tone = op(unreal.MaterialExpressionAdd, tone, None, -600, -260)
    tone.set_editor_property("const_b", 1.0)
    mel.connect_material_property(op(unreal.MaterialExpressionMultiply, skin, tone, -300, -240), "", MP.MP_BASE_COLOR)


def build_latex():
    """Glossy latex: opaque base with a mirror-like clear coat on top. Python has no
    clear-coat material pins, so everything goes through Make Material Attributes."""
    K = LATEX
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_CLEAR_COAT)
    m.set_editor_property("used_with_skeletal_mesh", True)
    m.set_editor_property("use_material_attributes", True)
    attrs = node(unreal.MaterialExpressionMakeMaterialAttributes, -150, 0)
    for pin, n in (("BaseColor", vector("SkinColor", K["skin"], -450, -200)),
                   ("Roughness", scalar("Roughness", K["roughness"], -450, -80)),
                   ("Specular", scalar("Specular", K["specular"], -450, -40)),
                   ("ClearCoat", scalar("ClearCoat", K["clearcoat"], -450, 40)),
                   ("ClearCoatRoughness", scalar("ClearCoatRoughness", K["clearcoat_roughness"], -450, 120))):
        if not mel.connect_material_expressions(n, "", attrs, pin):
            raise RuntimeError("pin " + pin)
    mel.connect_material_property(attrs, "", MP.MP_MATERIAL_ATTRIBUTES)


def build_aerogel():
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    m.set_editor_property("used_with_skeletal_mesh", True)
    for prop, val in (("apply_fogging", True), ("compute_fog_per_pixel", True),
                      ("output_translucent_velocity", False)):
        try:
            m.set_editor_property(prop, val)
        except Exception as e:
            print("skip", prop, e)

    S = SHELL
    # thickness: how much skin-to-body gap is behind this pixel
    sd = node(unreal.MaterialExpressionSceneDepth, -1400, 0)
    pd = node(unreal.MaterialExpressionPixelDepth, -1400, 120)
    gap = op(unreal.MaterialExpressionSubtract, sd, pd, -1200, 40)
    thick = op(unreal.MaterialExpressionDivide, gap, scalar("ThickScale", S["thick_scale"], -1400, 220), -1000, 60)
    thick = op(unreal.MaterialExpressionSaturate, thick, None, -850, 60, ia="")
    # rim
    fr = node(unreal.MaterialExpressionFresnel, -1200, 320)
    mel.connect_material_expressions(scalar("FresnelExponent", S["fresnel_exponent"], -1400, 340), "", fr, "ExponentIn")
    milk = op(unreal.MaterialExpressionMax, thick, fr, -700, 160)
    # opacity
    lo, hi = scalar("OpacityMin", S["opacity_min"], -700, 300), scalar("OpacityMax", S["opacity_max"], -700, 380)
    opac = node(unreal.MaterialExpressionLinearInterpolate, -450, 260)
    mel.connect_material_expressions(lo, "", opac, "A")
    mel.connect_material_expressions(hi, "", opac, "B")
    mel.connect_material_expressions(milk, "", opac, "Alpha")
    mel.connect_material_property(opac, "", MP.MP_OPACITY)
    # color
    mel.connect_material_property(vector("SkinColor", S["skin"], -450, -200), "", MP.MP_BASE_COLOR)
    mel.connect_material_property(scalar("Roughness", S["roughness"], -450, -80), "", MP.MP_ROUGHNESS)
    mel.connect_material_property(scalar("Specular", S["specular"], -450, -40), "", MP.MP_SPECULAR)
    # emissive: blue scatter at the rim + inner pulse where the skin is thin
    sc = op(unreal.MaterialExpressionMultiply, vector("ScatterColor", S["scatter"], -700, 480),
            scalar("ScatterStrength", S["scatter_strength"], -700, 600), -500, 500)
    sc = op(unreal.MaterialExpressionMultiply, sc, fr, -350, 500)
    t = node(unreal.MaterialExpressionTime, -1200, 700)
    wave = op(unreal.MaterialExpressionMultiply, t, scalar("GlowSpeed", S["glow_speed"], -1200, 780), -1000, 720)
    wave = op(unreal.MaterialExpressionSine, wave, None, -850, 720, ia="")
    wave = op(unreal.MaterialExpressionMultiply, wave, None, -700, 720)
    wave.set_editor_property("const_b", 0.35)
    wave = op(unreal.MaterialExpressionAdd, wave, None, -550, 720)
    wave.set_editor_property("const_b", 0.65)
    thin = op(unreal.MaterialExpressionOneMinus, milk, None, -550, 820, ia="")
    glow = op(unreal.MaterialExpressionMultiply, vector("GlowColor", S["glow"], -550, 900),
              scalar("GlowStrength", S["glow_strength"], -550, 1020), -350, 900)
    glow = op(unreal.MaterialExpressionMultiply, glow, wave, -200, 800)
    glow = op(unreal.MaterialExpressionMultiply, glow, thin, -50, 800)
    emis = op(unreal.MaterialExpressionAdd, sc, glow, 100, 600)
    mel.connect_material_property(emis, "", MP.MP_EMISSIVE_COLOR)
    # float the skin above the body
    nrm = node(unreal.MaterialExpressionVertexNormalWS, -450, 1150)
    wpo = op(unreal.MaterialExpressionMultiply, nrm, scalar("Inflate", S["inflate"], -450, 1250), -250, 1150)
    mel.connect_material_property(wpo, "", MP.MP_WORLD_POSITION_OFFSET)


{"real": build_real, "skin": build_skin, "latex": build_latex, "aerogel": build_aerogel}[MODE]()
mel.recompile_material(m)
eal.save_loaded_asset(m)

# MATERIAL_ONLY = True (a global set by the caller) rebuilds the skin material and stops:
# nothing below is touched, so BP_ThirdPersonCharacter is not saved.
if not globals().get("MATERIAL_ONLY"):
    # ---- inner machine -------------------------------------------------------------------
    quinn = unreal.load_asset(QUINN)
    base = quinn.materials[0].material_interface
    parent = base.get_base_material() if base else unreal.load_asset("/Game/Characters/Mannequins/Materials/M_Mannequin")
    core = asset("MI_AndroidCore", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(core, parent)
    names = [str(n) for n in mel.get_vector_parameter_names(parent)]
    tinted = [n for n in names if "tint" in n.lower() or "color" in n.lower()]
    for n in tinted:
        mel.set_material_instance_vector_parameter_value(core, n, unreal.LinearColor(*CORE_TINT, 1.0))
    eal.save_loaded_asset(core)
    print("core parent", parent.get_name(), "vector params", names, "tinted", tinted)

    # ---- Blueprint: scale, core material, shell component --------------------------------
    bp = unreal.load_asset(BP_PATH)
    cdo = unreal.get_default_object(bp.generated_class())
    cap = cdo.get_editor_property("capsule_component")
    cap.set_capsule_size(CAPSULE[1] * SCALE, CAPSULE[0] * SCALE)
    body = cdo.get_editor_property("mesh")
    body.set_editor_property("relative_scale3d", unreal.Vector(SCALE, SCALE, SCALE))
    body.set_editor_property("relative_location", unreal.Vector(0, 0, MESH_Z * SCALE))
    body.set_editor_property("override_materials", [core] * len(quinn.materials))

    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    handles = sds.k2_gather_subobject_data_for_blueprint(bp)
    by = {str(lib.get_variable_name(lib.get_data(h))): h for h in handles}
    if "Shell" not in by:
        h, fail = sds.add_new_subobject(unreal.AddNewSubobjectParams(
            parent_handle=by["Mesh"], new_class=unreal.VisexShellComponent, blueprint_context=bp))
        if not fail.is_empty():
            raise RuntimeError("add Shell failed: %s" % fail)
        sds.rename_subobject(h, unreal.Text("Shell"))
        by["Shell"] = h
    shell = lib.get_object(lib.get_data(by["Shell"]))
    # the shell wears the smooth skin mesh once Tools/char_skin.py has imported it
    skin_mesh = unreal.load_asset(DIR + "/SKM_AndroidSkin") if eal.does_asset_exist(DIR + "/SKM_AndroidSkin") else quinn
    shell.set_skeletal_mesh_asset(skin_mesh)
    shell.set_editor_property("override_materials", [m] * len(skin_mesh.materials))
    shell.set_editor_property("cast_shadow", skin_mesh != quinn)
    if skin_mesh != quinn:   # opaque skin over its own mesh: the body only leads the pose
        body.set_editor_property("visible", False)
        body.set_editor_property("cast_shadow", False)
        body.set_editor_property("visibility_based_anim_tick_option",
                                 unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    eal.save_loaded_asset(bp)

    cdo = unreal.get_default_object(bp.generated_class())
    cap = cdo.get_editor_property("capsule_component")
    print("scale %.3f -> %.1f cm; capsule %.1f/%.1f" % (SCALE, QUINN_HEIGHT * SCALE,
          cap.get_unscaled_capsule_half_height(), cap.get_unscaled_capsule_radius()))
    print("shell:", shell.get_class().get_name(), shell.get_editor_property("skeletal_mesh_asset").get_name())
