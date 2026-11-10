"""Dune field for Lvl_Dust: a heightfield mesh with wind-shaped ridges (gentle
windward slope, sharp crest, steep lee face) whose crests meander, vary in height
and break up — noise-driven, so nothing repeats — clear around the steps, burying
the distance, banked against the colonnade. Walkable: collision uses the mesh
itself. Replaces the old Drift_ mounds. Idempotent.

Endless: past the set the grid cells grow (75 cm -> 20 m) and the same dunes run
on for a kilometre in every direction, into fog that is opaque long before the
mesh ends. Invisible walls (Dunes_Bound_*) 600 m out keep the player inside.

Run live: python3 Tools/ue.py Tools/add_dunes.py
"""
import math
import os

import unreal

# --- shape
X0, X1, Y0, Y1 = -5000, 42000, -18000, 3200  # cm, the detailed area (75 cm cells)
CORE = (-5000, 42000, -22500, 31500)         # the set: plaza (behind the wall too), lower ground
BOUND = 60000                                # invisible walls this far outside the set
FAR = 100000                                 # sand runs this far outside the set
GROW, MAX_CELL = 1.08, 2000                  # cell growth per step outside, and its cap
LOW = -1000                                  # the open desert sits at the lower ground's level
DUNES_X0 = 1500                              # dunes fade out west of here (the ramp runs on)
CELL = 75                                    # cm between vertices
HEIGHT = 1100                                # cm, tallest crests
WAVELENGTH = 6500                            # cm between main ridges
CLEAN_CENTRE = (1000, -2800)                 # the steps: no sand here
CLEAN_RADIUS, FULL_RADIUS = 3500, 17000      # sand fades in between these
SINK = 8                                     # cm below the floor where there's no sand

ASSET_DIR, ASSET_NAME = "/Game/VisEx", "SM_Dunes"
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def smooth(a, b, x):
    t = min(1.0, max(0.0, (x - a) / (b - a)))
    return t * t * (3 - 2 * t)


# the step flight, from build_dust.py: 20 steps x 20 cm rise x 220 cm tread
STEPS_TOP, STEPS_END, STEPS_DROP = -1500, -1500 - 20 * 220, 20 * 20
RAMP_END = STEPS_END - 2100                  # sand ramp: last tread (-400) down to -1000


def in_steps(y):
    return STEPS_END < y < STEPS_TOP


def ground(y):
    """Height of the surface under the sand: plaza, steps, sand ramp, lower ground."""
    if y >= STEPS_TOP:
        return 0.0
    if in_steps(y):
        return -STEPS_DROP * (STEPS_TOP - y) / (STEPS_TOP - STEPS_END)
    if y > RAMP_END:
        return -STEPS_DROP - (1000 - STEPS_DROP) * smooth(STEPS_END, RAMP_END, y)
    return -1000.0


def outside(x, y):
    """cm from the set's footprint (0 inside)."""
    return math.hypot(max(CORE[0] - x, 0, x - CORE[1]), max(CORE[2] - y, 0, y - CORE[3]))


def base(x, y):
    """Ground under the sand anywhere: the set's profile, easing down to the open
    desert (LOW) within 60 m of its edges, e.g. a long drift off the plaza's west end."""
    g = ground(min(y, CORE[3]))
    return g + (LOW - g) * smooth(0, 6000, outside(x, y))


def _hash(ix, iy):
    h = (ix * 374761393 + iy * 668265263) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((h ^ (h >> 16)) & 0xFFFF) / 65535.0


def noise(x, y):
    """Smooth value noise, 0..1."""
    ix, iy = math.floor(x), math.floor(y)
    fx, fy = x - ix, y - iy
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    a, b = _hash(ix, iy), _hash(ix + 1, iy)
    c, d = _hash(ix, iy + 1), _hash(ix + 1, iy + 1)
    return (a + (b - a) * fx) * (1 - fy) + (c + (d - c) * fx) * fy


def fbm(x, y, octaves=4):
    s, amp, norm = 0.0, 0.5, 0.0
    for _ in range(octaves):
        s += amp * noise(x, y)
        norm += amp
        x, y, amp = x * 2.03 + 17.1, y * 2.03 - 9.3, amp * 0.5
    return s / norm


def ridge(u):
    """One dune profile, 0..1: long windward rise, crest at 0.78, steep lee."""
    p = u - math.floor(u)
    return (p / 0.78) ** 1.7 if p < 0.78 else ((1 - p) / 0.22) ** 2.2


def sand(x, y):
    d = math.hypot(x - CLEAN_CENTRE[0], y - CLEAN_CENTRE[1])
    env = smooth(CLEAN_RADIUS, FULL_RADIUS, d)
    # wind blows along +X. Noise-warped phase keeps crests from repeating: they
    # meander, merge, and break into separate dunes along their length.
    warp = 5200 * (fbm(x / 16000, y / 13000) - 0.5) + 1800 * (fbm(x / 5000 + 3.1, y / 4200 - 7.7, 3) - 0.5)
    main = ridge((x + warp) / WAVELENGTH + 1.2 * fbm(x / 30000 + 9, y / 30000))
    small = ridge((x * 0.93 + warp * 0.5 + 2000) / (WAVELENGTH * 0.37))
    amp = 0.25 + 1.0 * fbm(x / 11000 - 4, y / 9000 + 2, 3)            # crest height varies
    breaks = smooth(0.28, 0.55, fbm(x / 7000 + 5, y / 3800 - 1, 3))   # crests fade in and out
    sheet = 220 * fbm(x / 2600 + 11, y / 2600, 3)                     # low sand sheets between
    out = outside(x, y)
    # off the west end of the steps and plaza; the dunes come back beyond the set
    env *= 1 - (1 - smooth(DUNES_X0 - 3000, DUNES_X0, x)) * (1 - smooth(0, 8000, out))
    env = max(env, smooth(0, 20000, out))
    dunes = env * (HEIGHT * (main * amp * (0.35 + 0.65 * breaks) + 0.16 * small) + sheet)
    # sand banked against the colonnade wall, even close by
    bank = 260 * smooth(400, 2600, y) * smooth(DUNES_X0, 7000, x)
    h = dunes + bank
    if out > 0:
        return base(x, y) + max(h, 0) - (SINK if h <= 4 else 0)
    if h > 4:
        return ground(y) + h
    # no sand: tuck the surface under the architecture (well under, on the steps)
    # (the tuck stops short of the last tread, so the ramp meets it without a groove)
    return ground(y) - (250 if STEPS_END + 100 < y < STEPS_TOP else SINK)


def axis(a0, a1, lo, hi):
    """Vertex coordinates: CELL apart over [a0, a1], cells growing out to [lo, hi]."""
    inner = [a0 + i * CELL for i in range(int((a1 - a0) / CELL) + 1)]
    def out(start, end, sign):
        pts, v, step = [], start, CELL
        while sign * (end - v) > 0:
            step = min(step * GROW, MAX_CELL)
            v += sign * step
            pts.append(v)
        return pts
    return out(a0, lo, -1)[::-1] + inner + out(inner[-1], hi, 1)


def write_obj(path):
    xs = axis(X0, X1, CORE[0] - FAR, CORE[1] + FAR)
    ys = axis(Y0, Y1, CORE[2] - FAR, CORE[3] + FAR)
    nx, ny = len(xs), len(ys)
    zs = [[sand(x, y) for x in xs] for y in ys]
    lines = []
    for j in range(ny):
        for i in range(nx):
            # the importer keeps Z up but mirrors Y, so write -y
            lines.append("v %.1f %.1f %.1f" % (xs[i], -ys[j], zs[j][i]))
    # smooth per-vertex normals from the heightfield (no faceting on the crests)
    for j in range(ny):
        for i in range(nx):
            i0, i1, j0, j1 = max(i - 1, 0), min(i + 1, nx - 1), max(j - 1, 0), min(j + 1, ny - 1)
            dzdx = (zs[j][i1] - zs[j][i0]) / (xs[i1] - xs[i0])
            dzdy = (zs[j1][i] - zs[j0][i]) / (ys[j1] - ys[j0])
            ln = math.sqrt(dzdx * dzdx + dzdy * dzdy + 1)
            lines.append("vn %.4f %.4f %.4f" % (-dzdx / ln, dzdy / ln, 1 / ln))   # y mirrored too
    for j in range(ny):
        for i in range(nx):
            lines.append("vt %.5f %.5f" % ((xs[i] - X0) / 1000.0, (ys[j] - Y0) / 1000.0))
    for j in range(ny - 1):
        for i in range(nx - 1):
            a = j * nx + i + 1
            b, c, d = a + 1, a + nx, a + nx + 1
            lines.append("f %d/%d/%d %d/%d/%d %d/%d/%d" % (a, a, a, c, c, c, b, b, b))
            lines.append("f %d/%d/%d %d/%d/%d %d/%d/%d" % (b, b, b, c, c, c, d, d, d))
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")
    return nx * ny


def import_mesh(path):
    full = ASSET_DIR + "/" + ASSET_NAME
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        for a in eas.get_all_level_actors():
            if a.get_actor_label() == "Dunes":
                eas.destroy_actor(a)
        unreal.EditorAssetLibrary.delete_asset(full)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", ASSET_DIR)
    task.set_editor_property("destination_name", ASSET_NAME)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    task.set_editor_property("replace_existing", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.load_asset(full)
    # walkable: collide against the actual surface
    body = mesh.get_editor_property("body_setup")
    body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    # Nanite: cheaper to draw and shadow (the VSM overflow warning). Keep the full-res
    # fallback, since collision is built from it.
    ns = mesh.get_editor_property("nanite_settings")
    ns.set_editor_property("enabled", True)
    # collision is cooked from the Nanite fallback: keep it full-res. AUTO ignores the
    # percentage, and a reduced fallback leaves the player walking under the surface.
    for prop, value in (("fallback_target", unreal.NaniteFallbackTarget.PERCENT_TRIANGLES),
                        ("fallback_percent_triangles", 1.0), ("fallback_relative_error", 0.0)):
        try:
            ns.set_editor_property(prop, value)
        except Exception:
            pass
    mesh.set_editor_property("nanite_settings", ns)
    unreal.EditorAssetLibrary.save_asset(full)
    return mesh


def dune_material():
    """Instance of M_SandCover that is sand everywhere."""
    path = ASSET_DIR + "/MI_Dunes"
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "MI_Dunes", ASSET_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mi.set_editor_property("parent", unreal.load_asset(ASSET_DIR + "/M_SandCover"))
    mi = unreal.load_asset(path)
    mel = unreal.MaterialEditingLibrary
    sand = mel.get_material_instance_vector_parameter_value(mi, "SandColor")
    mel.set_material_instance_vector_parameter_value(mi, "StoneColor", sand)  # all sand, no patches
    mel.set_material_instance_scalar_parameter_value(mi, "MinCover", 1.0)
    unreal.EditorAssetLibrary.save_asset(path)
    return mi


for a in eas.get_all_level_actors():
    if a.get_actor_label().startswith("Drift_"):
        eas.destroy_actor(a)

obj = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools", ".bridge", "dunes.obj")
verts = write_obj(obj)
mesh = import_mesh(obj)
actor = eas.spawn_actor_from_object(mesh, unreal.Vector(0, 0, 0))
actor.set_actor_label("Dunes")

# invisible walls BOUND outside the set: block the player only, deep in opaque fog
CR = unreal.CollisionResponseType
for a in eas.get_all_level_actors():
    if a.get_actor_label().startswith("Dunes_Bound_"):
        eas.destroy_actor(a)
cube = unreal.load_asset("/Engine/BasicShapes/Cube")
x0, x1, y0, y1 = CORE[0] - BOUND, CORE[1] + BOUND, CORE[2] - BOUND, CORE[3] + BOUND
for name, centre, size in (("W", (x0, (y0 + y1) / 2), (100, y1 - y0)), ("E", (x1, (y0 + y1) / 2), (100, y1 - y0)),
                           ("S", ((x0 + x1) / 2, y0), (x1 - x0, 100)), ("N", ((x0 + x1) / 2, y1), (x1 - x0, 100))):
    w = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(centre[0], centre[1], 5000))
    w.set_actor_label("Dunes_Bound_" + name)
    w.static_mesh_component.set_static_mesh(cube)
    w.set_actor_scale3d(unreal.Vector(size[0] / 100, size[1] / 100, 200))   # 200 m tall
    w.static_mesh_component.set_visibility(False)
    w.set_actor_hidden_in_game(True)
    w.static_mesh_component.set_collision_response_to_all_channels(CR.ECR_IGNORE)
    w.static_mesh_component.set_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN, CR.ECR_BLOCK)
actor.static_mesh_component.set_material(0, dune_material())
o, e = actor.get_actor_bounds(False)
print("dunes: %d verts; bounds centre %s extent %s" % (verts, o, e))
