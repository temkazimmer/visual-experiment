"""Grade by distance (the BR2049 Las Vegas approach): the foreground stays near
neutral and contrasty, the orange lives in the middle distance, and the far
distance and sky go lighter and less saturated, so the haze reads as air rather
than a flat orange filter. A post-process material (before tonemapping) on the
PostProcess volume, tuned through MI_DistanceGrade's parameters. Idempotent.

Run live: python3 Tools/ue.py Tools/distance_grade.py   (GRADE_O = {...} overrides)
"""
import unreal

MAT, MI = "/Game/VisEx/M_DistanceGrade", "/Game/VisEx/MI_DistanceGrade"
GRADE = dict(
    NearEnd=1500.0,         # cm: fully "near" up to here...
    MidStart=5000.0,        # ...blending to the orange middle by here
    FarStart=20000.0,       # the far band starts...
    FarEnd=70000.0,         # ...and is full here (and on the sky)
    NearNeutral=0.65,        # how much of the orange cast the foreground loses (0..1)
    NearContrast=1.15,      # power around Pivot
    Pivot=0.25,
    MidSat=1.0,
    FarSat=0.55,
    FarLift=1.2,
)
VEC = dict(
    Cast=(1.0, 0.62, 0.36),     # the scene's orange cast, removed in the foreground
    MidWarm=(1.04, 0.96, 0.91), # amber-pink push in the middle distance (the BR2049 filter pack)
    FarTint=(1.0, 0.82, 0.65),   # what the far distance desaturates toward: pale sand, not grey
)

mel = unreal.MaterialEditingLibrary
CODE = """
float3 w3 = float3(0.2126, 0.7152, 0.0722);
float l = dot(C, w3);
float wn = 1 - smoothstep(NearEnd, MidStart, D);
float wf = smoothstep(FarStart, FarEnd, D);
float wm = saturate(1 - wn - wf);
float3 cast = Cast / dot(Cast, w3);
float3 n = lerp(C, C / cast, NearNeutral);
float ln = max(dot(n, w3), 1e-4);
n *= pow(ln / Pivot, NearContrast - 1);                // contrast on luminance only: hue stays
float3 m = (l + (C - l) * MidSat) * MidWarm;
float3 f = lerp(l * FarTint / dot(FarTint, w3), C, FarSat) * FarLift;   // toward a pale cream
return n * wn + m * wm + f * wf;
"""


def build():
    if unreal.EditorAssetLibrary.does_asset_exist(MAT):
        m = unreal.load_asset(MAT)
        mel.delete_all_material_expressions(m)
    else:
        m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_DistanceGrade", "/Game/VisEx", unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    m.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_DOF)

    st = mel.create_material_expression(m, unreal.MaterialExpressionSceneTexture, -900, 0)
    st.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    depth = mel.create_material_expression(m, unreal.MaterialExpressionSceneDepth, -900, 150)
    mask = mel.create_material_expression(m, unreal.MaterialExpressionComponentMask, -700, 0)
    for c, on in zip("rgba", (True, True, True, False)):
        mask.set_editor_property(c, on)
    mel.connect_material_expressions(st, "Color", mask, "")

    cu = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -400, 0)
    cu.set_editor_property("code", CODE)
    cu.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    names = ["C", "D"] + list(GRADE) + list(VEC)
    ins = []
    for n in names:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    cu.set_editor_property("inputs", ins)
    mel.connect_material_expressions(mask, "", cu, "C")
    mel.connect_material_expressions(depth, "", cu, "D")
    y = 300
    for n, v in GRADE.items():
        p = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -700, y)
        p.set_editor_property("parameter_name", n)
        p.set_editor_property("default_value", v)
        mel.connect_material_expressions(p, "", cu, n)
        y += 80
    for n, v in VEC.items():
        p = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -700, y)
        p.set_editor_property("parameter_name", n)
        p.set_editor_property("default_value", unreal.LinearColor(*v, 1))
        mask3 = mel.create_material_expression(m, unreal.MaterialExpressionComponentMask, -550, y)
        for c, on in zip("rgba", (True, True, True, False)):
            mask3.set_editor_property(c, on)
        mel.connect_material_expressions(p, "", mask3, "")
        mel.connect_material_expressions(mask3, "", cu, n)
        y += 120
    mel.connect_material_property(cu, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_asset(MAT)
    return m


def instance(m):
    if not unreal.EditorAssetLibrary.does_asset_exist(MI):
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "MI_DistanceGrade", "/Game/VisEx", unreal.MaterialInstanceConstant,
            unreal.MaterialInstanceConstantFactoryNew())
        mi.set_editor_property("parent", m)
    mi = unreal.load_asset(MI)
    g = dict(GRADE, **(globals().get("GRADE_O") or {}))
    for n in GRADE:
        mel.set_material_instance_scalar_parameter_value(mi, n, g[n])
    for n, v in VEC.items():
        mel.set_material_instance_vector_parameter_value(mi, n, unreal.LinearColor(*g.get(n, v), 1))
    unreal.EditorAssetLibrary.save_asset(MI)
    return mi


m = unreal.load_asset(MAT) if globals().get("GRADE_O") and unreal.EditorAssetLibrary.does_asset_exist(MAT) else build()
mi = instance(m)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
pp = next(a for a in eas.get_all_level_actors() if a.get_actor_label() == "PostProcess")
s = pp.get_editor_property("settings")
wb = s.get_editor_property("weighted_blendables")
arr = [b for b in wb.get_editor_property("array") if b.get_editor_property("object") not in (mi, m)]
arr.append(unreal.WeightedBlendable(weight=1.0, object=mi))
wb.set_editor_property("array", arr)
s.set_editor_property("weighted_blendables", wb)
pp.set_editor_property("settings", s)
print("distance grade on PostProcess (%d blendables)" % len(arr))
