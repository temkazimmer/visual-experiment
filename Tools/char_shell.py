"""The android under aerogel skin (Docs/CHARACTER_DIRECTION.md, first task). Idempotent.

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
    opacity_min=0.08,                # face-on over thin skin: the machine shows
    opacity_max=0.85,                # edges and thick areas: milky
    skin=(0.60, 0.68, 0.80),         # milky white with a cool cast
    scatter=(0.30, 0.55, 1.00),      # aerogel blue, strongest at the rim
    scatter_strength=2.6,
    glow=(0.35, 0.70, 1.00),         # inner pulse, seen through thin skin
    glow_strength=0.8,
    glow_speed=0.9,                  # rad/s: a slow cooling cycle (~7 s)
    roughness=0.28,
)
CORE_TINT = (0.035, 0.038, 0.045)    # graphite machine

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
mel.recompile_material(m)
eal.save_loaded_asset(m)

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
shell.set_skeletal_mesh_asset(quinn)
shell.set_editor_property("override_materials", [m] * len(quinn.materials))
shell.set_editor_property("cast_shadow", False)

unreal.BlueprintEditorLibrary.compile_blueprint(bp)
eal.save_loaded_asset(bp)

cdo = unreal.get_default_object(bp.generated_class())
cap = cdo.get_editor_property("capsule_component")
print("scale %.3f -> %.1f cm; capsule %.1f/%.1f" % (SCALE, QUINN_HEIGHT * SCALE,
      cap.get_unscaled_capsule_half_height(), cap.get_unscaled_capsule_radius()))
print("shell:", shell.get_class().get_name(), shell.get_editor_property("skeletal_mesh_asset").get_name())
