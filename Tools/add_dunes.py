"""Dune field for Lvl_Dust: a heightfield mesh with wind-shaped ridges (gentle
windward slope, sharp crest, steep lee face) whose crests meander, vary in height
and break up — noise-driven, so nothing repeats — clear around the steps, burying
the distance, banked against the colonnade. Walkable: collision uses the mesh
itself. Replaces the old Drift_ mounds. Idempotent.

Run live: python3 Tools/ue.py Tools/add_dunes.py
"""
import math
import os

import unreal

# --- shape
X0, X1, Y0, Y1 = 1500, 42000, -18000, 3200   # cm, the area the sand can cover
CELL = 75                                    # cm between vertices
HEIGHT = 1100                                # cm, tallest crests
WAVELENGTH = 6500                            # cm between main ridges
CLEAN_CENTRE = (1000, -2000)                 # the steps: no sand here
CLEAN_RADIUS, FULL_RADIUS = 3500, 17000      # sand fades in between these
SINK = 8                                     # cm below the floor where there's no sand

ASSET_DIR, ASSET_NAME = "/Game/VisEx", "SM_Dunes"
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def smooth(a, b, x):
    t = min(1.0, max(0.0, (x - a) / (b - a)))
    return t * t * (3 - 2 * t)


def ground(y):
    """Height of the architecture under the sand: plaza, steps, lower ground."""
    if y >= -1500:
        return 0.0
    if y <= -4250:
        return -1000.0
    return -1000.0 * (-1500 - y) / 2750


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
    dunes = env * (HEIGHT * (main * amp * (0.35 + 0.65 * breaks) + 0.16 * small) + sheet)
    # sand banked against the colonnade wall, even close by
    bank = 260 * smooth(400, 2600, y) * smooth(1500, 7000, x)
    h = dunes + bank
    if h > 4:
        return ground(y) + h
    # no sand: tuck the surface under the architecture (well under, on the steps)
    return ground(y) - (250 if -4250 < y < -1500 else SINK)


def write_obj(path):
    nx, ny = int((X1 - X0) / CELL) + 1, int((Y1 - Y0) / CELL) + 1
    zs = [[sand(X0 + i * CELL, Y0 + j * CELL) for i in range(nx)] for j in range(ny)]
    lines = []
    for j in range(ny):
        for i in range(nx):
            # the importer keeps Z up but mirrors Y, so write -y
            lines.append("v %.1f %.1f %.1f" % (X0 + i * CELL, -(Y0 + j * CELL), zs[j][i]))
    # smooth per-vertex normals from the heightfield (no faceting on the crests)
    for j in range(ny):
        for i in range(nx):
            dzdx = (zs[j][min(i + 1, nx - 1)] - zs[j][max(i - 1, 0)]) / (2 * CELL)
            dzdy = (zs[min(j + 1, ny - 1)][i] - zs[max(j - 1, 0)][i]) / (2 * CELL)
            ln = math.sqrt(dzdx * dzdx + dzdy * dzdy + 1)
            lines.append("vn %.4f %.4f %.4f" % (-dzdx / ln, dzdy / ln, 1 / ln))   # y mirrored too
    for j in range(ny):
        for i in range(nx):
            lines.append("vt %.5f %.5f" % (i * CELL / 1000.0, j * CELL / 1000.0))
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
actor.static_mesh_component.set_material(0, dune_material())
o, e = actor.get_actor_bounds(False)
print("dunes: %d verts; bounds centre %s extent %s" % (verts, o, e))
