"""Footprints in the sand: builds M_Footprint (a normal-only deferred decal: a
procedural bare footprint, heel, sole edge, ball and toes, pressed ~2.5 cm in, with a pushed-up rim) and places the
Footprints actor (AVisexFootprints, C++), which stamps one per step on surfaces
tagged "Sand" and fades the trail out over ~8 steps. Tags the dunes as sand.
Idempotent.

Run live: python3 Tools/ue.py Tools/add_footprints.py   (after add_dunes.py)
"""
import unreal

MAT = "/Game/VisEx/M_Footprint"
DEPTH, RIM = 2.6, 0.6          # cm: how deep a fresh print is, and the sand pushed up round it
HALF = (8.0, 16.0)              # cm: decal half width, half length (matches the actor's HalfSize)
mel = unreal.MaterialEditingLibrary

# h(u, v) in cm. Sole as a soft union of heel, ball and toe ellipses (cm, foot length
# along v, the big toe on +x), deepest under heel and ball, rim just outside.
SHAPE = """
// a bare right foot, cm: x across (+ = big-toe side), y along (+ = toes). Heel, the outer
// edge of the sole (the arch leaves no mark on the inner side), the ball, five toes.
float2 p = float2((UV.y - 0.5) * 2 * HalfW, (UV.x - 0.5) * 2 * HalfL);   // decal UV: x runs along the foot
float2 q;
q = (p - float2(0.0, -8.6)) / float2(3.1, 3.8);   float heel = (length(q) - 1) * 3.1;
q = (p - float2(0.6, 4.4)) / float2(4.9, 3.3);    float ball = (length(q) - 1) * 3.3;
float2 e0 = float2(-1.9, -6.5), e1 = float2(-2.4, 3.5), ee = e1 - e0;
float te = saturate(dot(p - e0, ee) / dot(ee, ee));
float edge = length(p - (e0 + ee * te)) - lerp(2.1, 1.7, te);
float k = 1.2, d = heel, hh;
hh = saturate(0.5 + 0.5 * (edge - d) / k); d = lerp(edge, d, hh) - k * hh * (1 - hh);
hh = saturate(0.5 + 0.5 * (ball - d) / k); d = lerp(ball, d, hh) - k * hh * (1 - hh);
float toes = length(p - float2(2.6, 9.4)) - 1.6;                // big toe
toes = min(toes, length(p - float2(0.1, 9.9)) - 1.05);
toes = min(toes, length(p - float2(-1.6, 9.3)) - 0.95);
toes = min(toes, length(p - float2(-3.0, 8.3)) - 0.85);
toes = min(toes, length(p - float2(-4.0, 7.0)) - 0.75);
d = min(d, toes);                                                // cm outside the print (<0 inside)
float press = 1 - smoothstep(-0.7, 0.25, d);
float deep = 0.75 + 0.35 * saturate(exp(-dot(p - float2(0, -8.6), p - float2(0, -8.6)) / 10) + exp(-dot(p - float2(0.5, 4.3), p - float2(0.5, 4.3)) / 12));
float grain = frac(sin(dot(floor(p * 2.5), float2(12.9898, 78.233))) * 43758.5453) - 0.5;
float H = (-Depth * press * deep + Rim * exp(-pow((d - 0.9) / 0.9, 2)) + 0.1 * grain * press) * Fade;
"""
NORMAL = SHAPE + """
float3 dpx = ddx(WP), dpy = ddy(WP);
float3 Ns = normalize(cross(dpx, dpy)); if (Ns.z < 0) Ns = -Ns;
float3 r1 = cross(dpy, Ns), r2 = cross(Ns, dpx);
float det = dot(dpx, r1);
float3 g = sign(det) * (ddx(H) * r1 + ddy(H) * r2);
return normalize(abs(det) * Ns - g);
"""
OPACITY = SHAPE + """
return (1 - smoothstep(2.2, 3.6, d)) * saturate(Fade * 3);
"""


def build():
    if unreal.EditorAssetLibrary.does_asset_exist(MAT):
        m = unreal.load_asset(MAT)
        mel.delete_all_material_expressions(m)
    else:
        m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_Footprint", "/Game/VisEx", unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("tangent_space_normal", False)     # the normal is built in world space

    uv = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    wp = mel.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -900, 100)
    fade = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -900, 200)
    fade.set_editor_property("parameter_name", "Fade")
    fade.set_editor_property("default_value", 1.0)
    consts = {}
    for i, (n, v) in enumerate((("Depth", DEPTH), ("Rim", RIM), ("HalfW", HALF[0]), ("HalfL", HALF[1]))):
        c = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -900, 300 + 80 * i)
        c.set_editor_property("parameter_name", n)
        c.set_editor_property("default_value", v)
        consts[n] = c

    def custom(code, out, y):
        cu = mel.create_material_expression(m, unreal.MaterialExpressionCustom, -450, y)
        cu.set_editor_property("code", code)
        cu.set_editor_property("output_type", out)
        names = ["UV", "WP", "Fade"] + list(consts)
        ins = []
        for n in names:
            ci = unreal.CustomInput()
            ci.set_editor_property("input_name", n)
            ins.append(ci)
        cu.set_editor_property("inputs", ins)
        for n, e in (("UV", uv), ("WP", wp), ("Fade", fade), *consts.items()):
            mel.connect_material_expressions(e, "", cu, n)
        return cu

    n = custom(NORMAL, unreal.CustomMaterialOutputType.CMOT_FLOAT3, 0)
    o = custom(OPACITY, unreal.CustomMaterialOutputType.CMOT_FLOAT1, 250)
    mel.connect_material_property(n, "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(o, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_asset(MAT)
    return m


mat = build()
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for a in eas.get_all_level_actors():
    lab = a.get_actor_label()
    if lab == "Dunes":
        tags = list(a.get_editor_property("tags"))
        if "Sand" not in [str(t) for t in tags]:
            a.set_editor_property("tags", tags + [unreal.Name("Sand")])
    if lab == "Footprints":
        eas.destroy_actor(a)
fp = eas.spawn_actor_from_class(unreal.VisexFootprints, unreal.Vector(0, 0, 0))
fp.set_actor_label("Footprints")
fp.set_editor_property("material", mat)
fp.set_editor_property("half_size", unreal.Vector2D(HALF[1], HALF[0]))
fp.set_editor_property("plant_height", 12.5)     # ankle bone: ~7-11 cm planted, ~17-21 cm mid-swing
fp.set_editor_property("lift_margin", 3.5)
print("footprints ready")
