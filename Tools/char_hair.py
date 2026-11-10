"""The hair's material, on the skin mesh's second slot ("Hair", from Tools/blender_hair.py).

  python3 Tools/ue.py Tools/char_hair.py      # after Tools/char_skin.py

M_Hair: blue-black solid locks. Colour runs root to tip along each lock (UV.v from
blender_hair.py: 0 at the root, 1 at the tip): dark and occluded at the root, a faint blue at
the tip. A thresholded Kajiya-Kay band (tangent along the lock, half vector from the sun)
catches the light as a hard-edged cool stripe, and a warm rim lifts the silhouette against
the dust. Every look value is in HAIR; rebuilt each run.
"""
import unreal

HAIR = dict(
    root=(0.006, 0.008, 0.016),    # base colour at the root
    mid=(0.022, 0.030, 0.058),     # base colour along the lock (lighter so form reads)
    tip=(0.030, 0.075, 0.190),     # faint blue at the tips
    grad_len=0.55,                 # v where root -> mid is complete
    tip_start=0.60,                # v where the blue starts
    ao_min=0.35,                   # root occlusion (1 = none)
    ao_len=0.45,                   # v where the occlusion has faded out
    sun_dir=(0.524, -0.748, 0.407),  # direction TOWARD the sun (world): minus Lvl_Dust's key light travel dir
    spec_color=(0.45, 0.62, 1.0),  # cool band tint
    spec_power=160.0,               # sin(T,H)^power: tighter = thinner band
    spec_lo=0.55,                  # smoothstep edge: below = none
    spec_hi=0.60,                  # above = full (small gap = hard edge)
    spec_max=0.7,                  # clamped intensity
    spec_shift=0.12,               # slides the band along the lock (tangent nudged by normal)
    rim=(0.55, 0.26, 0.09),        # warm, picks up the dust
    rim_strength=0.10,
    rim_power=3.5,
    roughness=0.55,
    specular=0.15,
)
DIR = "/Game/Character/Android"
if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise RuntimeError("Play mode is running: stop PIE first")

mel = unreal.MaterialEditingLibrary
MP = unreal.MaterialProperty
path = DIR + "/M_Hair"
m = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else \
    unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_Hair", DIR, unreal.Material, unreal.MaterialFactoryNew())
mel.delete_all_material_expressions(m)


def vec(name, v, y):
    n = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -700, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", unreal.LinearColor(v[0], v[1], v[2], 1))
    return n


def num(name, v, y):
    n = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -700, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", v)
    return n


def custom(code, ins, y):
    n = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -200, y)
    n.set_editor_property("code", code)
    n.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    lst = []
    for nm, _ in ins:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", nm)
        lst.append(ci)
    n.set_editor_property("inputs", lst)
    for nm, src in ins:
        mel.connect_material_expressions(src, "", n, nm)
    return n


def const(v, y):
    n = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -700, y)
    n.set_editor_property("constant", unreal.LinearColor(v[0], v[1], v[2], 1))
    return n


uv = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -900, 0)
vv = mel.create_material_expression(m, unreal.MaterialExpressionComponentMask, -700, 0)
vv.set_editor_property("g", True)
mel.connect_material_expressions(uv, "", vv, "")
vv_f = vv
tan = mel.create_material_expression(m, unreal.MaterialExpressionTransform, -900, 600)
tan.set_editor_property("transform_source_type", unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_TANGENT)
tan.set_editor_property("transform_type", unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
mel.connect_material_expressions(const((0, 1, 0), 600), "", tan, "")   # tangent-space Y = along the lock
nrm = mel.create_material_expression(m, unreal.MaterialExpressionVertexNormalWS, -900, 700)
cam = mel.create_material_expression(m, unreal.MaterialExpressionCameraVectorWS, -900, 800)
sun = const(HAIR["sun_dir"], 900)

P = lambda k, y: num(k, HAIR[k], y)
base = custom("""
float v = saturate(V);
float3 c = lerp(Root, Mid, smoothstep(0.0, GradLen, v));
c = lerp(c, Tip, smoothstep(TipStart, 1.0, v));
float ao = lerp(AoMin, 1.0, smoothstep(0.0, AoLen, v));
return c * ao;
""", [("V", vv_f), ("Root", vec("Root", HAIR["root"], -300)), ("Mid", vec("Mid", HAIR["mid"], -240)),
      ("Tip", vec("Tip", HAIR["tip"], -180)), ("GradLen", P("grad_len", -120)),
      ("TipStart", P("tip_start", -60)), ("AoMin", P("ao_min", 0)), ("AoLen", P("ao_len", 60))], -200)
mel.connect_material_property(base, "", MP.MP_BASE_COLOR)

glow = custom("""
float3 T = normalize(Tan);
float3 N = normalize(Nrm);
float3 L = normalize(SunDir);
float3 H = normalize(L + normalize(Cam));
T = normalize(T + N * Shift);
float sinTH = sqrt(saturate(1.0 - dot(T, H) * dot(T, H)));
float band = smoothstep(Lo, Hi, pow(sinTH, Pow));
band *= saturate(dot(N, L) * 2.0 + 0.3);                 // only where the sun reaches
float3 spec = SpecCol * min(band, 1.0) * SpecMax;
float rim = pow(1.0 - saturate(dot(N, normalize(Cam))), RimPow) * RimStr;
return spec + RimCol * rim;
""", [("Tan", tan), ("Nrm", nrm), ("Cam", cam), ("SunDir", sun), ("Shift", P("spec_shift", 1000)),
      ("Lo", P("spec_lo", 1060)), ("Hi", P("spec_hi", 1120)), ("Pow", P("spec_power", 1180)),
      ("SpecMax", P("spec_max", 1240)), ("SpecCol", vec("SpecCol", HAIR["spec_color"], 1300)),
      ("RimCol", vec("RimCol", HAIR["rim"], 1360)), ("RimStr", P("rim_strength", 1420)),
      ("RimPow", P("rim_power", 1480))], 600)
mel.connect_material_property(glow, "", MP.MP_EMISSIVE_COLOR)
mel.connect_material_property(num("Roughness", HAIR["roughness"], 1540), "", MP.MP_ROUGHNESS)
mel.connect_material_property(num("Specular", HAIR["specular"], 1600), "", MP.MP_SPECULAR)
m.set_editor_property("two_sided", True)
mel.recompile_material(m)
unreal.EditorAssetLibrary.save_loaded_asset(m)

skin = unreal.load_asset(DIR + "/SKM_AndroidSkin")
mats = list(skin.get_editor_property("materials"))
names = [str(x.get_editor_property("material_slot_name")) for x in mats]
assert "Hair" in names, "no Hair slot on the skin: %s (run blender_hair.py, then char_skin.py)" % names
for x in mats:
    slot = str(x.get_editor_property("material_slot_name"))
    x.set_editor_property("material_interface", m if slot == "Hair" else unreal.load_asset(DIR + "/M_AerogelShell"))
skin.set_editor_property("materials", mats)
unreal.EditorAssetLibrary.save_loaded_asset(skin)
print("slots", names)
