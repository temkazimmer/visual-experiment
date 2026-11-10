"""Dust motes in the scene's colours: NS_Dust's sprites get M_DustMote instead of
Niagara's default white dot. Lit translucency, so motes glow in the sun shafts
and dim in shadow. Each mote is cream or sand-yellow at random, and more of them
are yellow the higher they float, so the air carries sand while the motes near
the floor stay pale. Depth fade softens them where they touch stone or sand,
so they never cut a hard line through the geometry. Idempotent.

Run live: python3 Tools/ue.py Tools/dust_colour.py
"""
import unreal

MAT = "/Game/VisEx/M_DustMote"
CREAM = (1.0, 0.86, 0.66)        # linear; pale motes, the old white warmed up
SAND = (0.92, 0.52, 0.18)        # linear; airborne sand, a touch more saturated than the dunes
YELLOW_LOW, YELLOW_HIGH = 0.2, 0.7   # share of yellow motes at the floor / 15 m up
FLOOR_Z, AIR_Z = -900.0, 600.0       # world z where the share starts and tops out
GLOW = 0.35                      # emissive share, so motes still read in shadow
SOFT = 40.0                      # cm depth fade against geometry

mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def build():
    if unreal.EditorAssetLibrary.does_asset_exist(MAT):
        m = unreal.load_asset(MAT)            # rebuild in place: NS_Dust keeps its reference
        mel.delete_all_material_expressions(m)
    else:
        m = tools.create_asset("M_DustMote", "/Game/VisEx", unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property("translucency_lighting_mode",
                          unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
    m.set_editor_property("used_with_niagara_sprites", True)

    def node(cls, x, y):
        return mel.create_material_expression(m, cls, x, y)

    def custom(code, inputs, x, y, out):
        c = node(unreal.MaterialExpressionCustom, x, y)
        c.set_editor_property("code", code)
        c.set_editor_property("output_type", out)
        ins = []
        for n in inputs:
            ci = unreal.CustomInput()
            ci.set_editor_property("input_name", n)
            ins.append(ci)
        c.set_editor_property("inputs", ins)
        return c

    pc = node(unreal.MaterialExpressionParticleColor, -900, 0)
    rnd = node(unreal.MaterialExpressionParticleRandom, -900, 150)
    wp = node(unreal.MaterialExpressionWorldPosition, -900, 250)
    uv = node(unreal.MaterialExpressionTextureCoordinate, -900, 400)

    colour = custom(
        "float h = saturate((WP.z - %f) / %f);\n"
        "float yellow = step(R, lerp(%f, %f, h));\n"
        "return lerp(float3(%f, %f, %f), float3(%f, %f, %f), yellow) * PC.rgb;"
        % (FLOOR_Z, AIR_Z - FLOOR_Z, YELLOW_LOW, YELLOW_HIGH, *CREAM, *SAND),
        ["PC", "R", "WP"], -550, 50, unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    mel.connect_material_expressions(pc, "", colour, "PC")
    mel.connect_material_expressions(rnd, "", colour, "R")
    mel.connect_material_expressions(wp, "", colour, "WP")

    dot = custom("float d = saturate(1 - length(UV - 0.5) * 2); return d * d * PA;",
                 ["UV", "PA"], -550, 350, unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    mel.connect_material_expressions(uv, "", dot, "UV")
    mel.connect_material_expressions(pc, "A", dot, "PA")
    fade = node(unreal.MaterialExpressionDepthFade, -300, 350)
    fade.set_editor_property("fade_distance_default", SOFT)
    mel.connect_material_expressions(dot, "", fade, "Opacity")

    glow = node(unreal.MaterialExpressionMultiply, -300, 150)
    glow.set_editor_property("const_b", GLOW)
    mel.connect_material_expressions(colour, "", glow, "A")

    mel.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(fade, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_asset(MAT)
    return m


mat = build()
ns = unreal.load_asset("/Game/VisEx/NS_Dust")
n = 0
for r in unreal.ObjectIterator(unreal.NiagaraSpriteRendererProperties):
    if r.get_path_name().startswith("/Game/VisEx/NS_Dust."):
        r.set_editor_property("material", mat)
        n += 1
unreal.EditorAssetLibrary.save_asset("/Game/VisEx/NS_Dust")

# the motes must never block or collide: no collision on the components either
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for a in eas.get_all_level_actors():
    if a.get_actor_label().startswith("Dust_"):
        a.niagara_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        a.niagara_component.reinitialize_system()
print("M_DustMote on %d sprite renderer(s)" % n)
