"""Footprints in the sand: builds M_Footprint (a normal-only deferred decal: a
procedural bare footprint, heel, sole edge, ball and toes, a soft dish ~2 cm deep, with a pushed-up rim) and places the
Footprints actor (AVisexFootprints, C++), which stamps one per step on surfaces
tagged "Sand" and fades the trail out over ~8 steps; standing, each foot sinks into a small sand mound
(SM_SandMound, built here) that hides any gap under the sole. Tags the dunes as sand.
Idempotent.

Run live: python3 Tools/ue.py Tools/add_footprints.py   (after add_dunes.py)
"""
import unreal

MAT = "/Game/VisEx/M_Footprint"
DEPTH, RIM = 2.2, 0.4          # cm: how deep a fresh print is, and the sand pushed up round it
HALF = (10.0, 18.0)              # cm: decal half width, half length (matches the actor's HalfSize)
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
float k = 2.2, d = heel, hh;
hh = saturate(0.5 + 0.5 * (edge - d) / k); d = lerp(edge, d, hh) - k * hh * (1 - hh);
hh = saturate(0.5 + 0.5 * (ball - d) / k); d = lerp(ball, d, hh) - k * hh * (1 - hh);
// toes: soft dents that run into the ball, not punched dots
float toes = length(p - float2(2.6, 9.2)) - 1.7;                // big toe
float t2 = length(p - float2(0.1, 9.6)) - 1.15;
hh = saturate(0.5 + 0.5 * (t2 - toes) / 0.8); toes = lerp(t2, toes, hh) - 0.8 * hh * (1 - hh);
t2 = length(p - float2(-1.6, 9.0)) - 1.0;
hh = saturate(0.5 + 0.5 * (t2 - toes) / 0.8); toes = lerp(t2, toes, hh) - 0.8 * hh * (1 - hh);
t2 = length(p - float2(-3.0, 8.0)) - 0.9;
hh = saturate(0.5 + 0.5 * (t2 - toes) / 0.8); toes = lerp(t2, toes, hh) - 0.8 * hh * (1 - hh);
k = 1.6;
hh = saturate(0.5 + 0.5 * (toes - d) / k); d = lerp(toes, d, hh) - k * hh * (1 - hh);   // cm outside the print (<0 inside)
// soft walls: loose sand slumps, so the print is a smooth dish, not a stamped outline
float press = 1 - smoothstep(-2.2, 1.4, d);
press = press * press * (3 - 2 * press);
float deep = 0.75 + 0.35 * saturate(exp(-dot(p - float2(0, -8.6), p - float2(0, -8.6)) / 10) + exp(-dot(p - float2(0.5, 4.3), p - float2(0.5, 4.3)) / 12));
float H = (-Depth * press * deep + Rim * exp(-pow((d - 2.4) / 2.0, 2))) * Fade;
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
return (1 - smoothstep(3.5, 5.5, d)) * saturate(Fade * 3);
"""


def update(m):
    """The material exists: the C++ class holds it (rooted), so its nodes can't be
    deleted. Update the two Custom nodes' code and the parameter defaults in place."""
    for e in unreal.ObjectIterator(unreal.MaterialExpressionCustom):
        if e.get_outer() == m:
            out = e.get_editor_property("output_type")
            e.set_editor_property("code", NORMAL if out == unreal.CustomMaterialOutputType.CMOT_FLOAT3 else OPACITY)
    vals = {"Depth": DEPTH, "Rim": RIM, "HalfW": HALF[0], "HalfL": HALF[1]}
    for e in unreal.ObjectIterator(unreal.MaterialExpressionScalarParameter):
        if e.get_outer() == m and str(e.get_editor_property("parameter_name")) in vals:
            e.set_editor_property("default_value", vals[str(e.get_editor_property("parameter_name"))])
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_asset(MAT)
    return m


def build():
    existing = unreal.load_asset(MAT)          # (the registry can lag right after startup)
    if existing:
        return update(existing)
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


MOUND = "/Game/VisEx/SM_SandMound"


def mound_mesh():
    """A unit mound: 100 cm across and 10 cm high (close to its real proportions, so
    the actor's scale stays near uniform and the normals hold up). Profile
    h = (1 - r^2)^3: a broad soft top and a long feathered skirt with no slope at the
    rim, so it melts into the sand. Analytic normals, UVs from x/y. Scaled per foot."""
    import math, os
    path = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools", ".bridge", "sand_mound.obj")
    R, H, rings, segs = 50.0, 10.0, 24, 48
    v, t, n, f = ["v 0 0 %.3f" % H], ["vt 0.5 0.5"], ["vn 0 0 1"], []
    for i in range(1, rings + 1):
        r = i / rings
        h = H * (1 - r * r) ** 3
        dh = H * 3 * (1 - r * r) ** 2 * (-2 * r) / R          # dh/dR, R in cm
        for j in range(segs):
            a = 2 * math.pi * j / segs
            x, y = R * r * math.cos(a), -R * r * math.sin(a)
            v.append("v %.3f %.3f %.3f" % (x, y, h))
            t.append("vt %.4f %.4f" % (0.5 + x / (2 * R), 0.5 + y / (2 * R)))
            ln = math.sqrt(dh * dh + 1)
            n.append("vn %.5f %.5f %.5f" % (-dh * math.cos(a) / ln, dh * math.sin(a) / ln, 1 / ln))
    for j in range(segs):
        a, b = 2 + j, 2 + (j + 1) % segs
        f.append("f 1/1/1 %d/%d/%d %d/%d/%d" % (b, b, b, a, a, a))
    for i in range(rings - 1):
        for j in range(segs):
            a = 2 + i * segs + j
            b = 2 + i * segs + (j + 1) % segs
            c, d = a + segs, b + segs
            f.append("f %d/%d/%d %d/%d/%d %d/%d/%d" % (a, a, a, d, d, d, c, c, c))
            f.append("f %d/%d/%d %d/%d/%d %d/%d/%d" % (a, a, a, b, b, b, d, d, d))
    with open(path, "w") as fh:
        fh.write("\n".join(v + t + n + f) + "\n")
    # reimport over the old one: the footprints CDO holds it, so it can't be deleted
    task = unreal.AssetImportTask()
    for k, val in (("filename", path), ("destination_path", "/Game/VisEx"), ("destination_name", "SM_SandMound"),
                   ("automated", True), ("save", False), ("replace_existing", True)):
        task.set_editor_property(k, val)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.load_asset(MOUND)
    mesh.set_material(0, unreal.load_asset("/Game/VisEx/MI_Dunes"))
    unreal.EditorAssetLibrary.save_asset(MOUND)
    return mesh


mat = build()
mound = mound_mesh()
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
fp.set_editor_property("mound_mesh", mound)
fp.set_editor_property("mound_material", unreal.load_asset("/Game/VisEx/MI_Dunes"))
print("footprints ready")
