"""Import the weathered stone meshes from Tools/.bridge/geo (stone_geo.py) and put
them on the level: Nanite on, walkable collision, swapped onto the existing
actors (plinths, columns, steps, rubble), plaza floor as separate slabs.

Run live: python3 Tools/ue.py Tools/apply_geo.py
Set ONLY = ["SM_Plinth_00", ...] in the job's globals to do a subset.
"""
import json
import os

import unreal

DEST = "/Game/VisEx/Geo"
GEO = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools", ".bridge", "geo")
WALKABLE = ("SM_Plinth_", "SM_Column_", "SM_Step_", "SM_PlazaSlabs_")

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
man = json.load(open(os.path.join(GEO, "manifest.json")))
only = globals().get("ONLY")
names = [n for n in man["meshes"] if (not only or n in only) and os.path.exists(os.path.join(GEO, n + ".obj"))]

tasks = []
for n in names:
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", os.path.join(GEO, n + ".obj"))
    t.set_editor_property("destination_path", DEST)
    t.set_editor_property("destination_name", n)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", False)
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

mat = unreal.load_asset("/Game/VisEx/M_SandCover")
for n in names:
    m = unreal.load_asset(DEST + "/" + n)
    ns = m.get_editor_property("nanite_settings")
    ns.set_editor_property("enabled", True)
    if n.startswith(WALKABLE):
        body = m.get_editor_property("body_setup")
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        # collision is cooked from the Nanite fallback: keep it full-res. AUTO ignores the
        # percentage, and a reduced fallback leaves the player walking under the surface.
        for prop, value in (("fallback_target", unreal.NaniteFallbackTarget.PERCENT_TRIANGLES),
                            ("fallback_percent_triangles", 1.0), ("fallback_relative_error", 0.0)):
            try:
                ns.set_editor_property(prop, value)
            except Exception:
                pass
    m.set_editor_property("nanite_settings", ns)
    m.set_material(0, mat)
    unreal.EditorAssetLibrary.save_asset(DEST + "/" + n)

by = {a.get_actor_label(): a for a in eas.get_all_level_actors()}
done = 0
for a in man["assign"]:
    if a["asset"] not in names or a["label"] not in by:
        continue
    actor = by[a["label"]]
    smc = actor.static_mesh_component
    smc.set_static_mesh(unreal.load_asset(DEST + "/" + a["asset"]))
    smc.set_material(0, mat)
    if "scale" in a:
        actor.set_actor_scale3d(unreal.Vector(*a["scale"]))
    if "rot" in a:
        actor.set_actor_rotation(unreal.Rotator(roll=a["rot"][0], pitch=a["rot"][1], yaw=a["rot"][2]), False)
    done += 1

# plaza floor: slab meshes are in world space; the old plaza block drops under them
slab_names = [n for n in names if n.startswith("SM_PlazaSlabs_")]
if slab_names:
    for label, actor in by.items():
        if label.startswith("PlazaSlabs_"):
            eas.destroy_actor(actor)
    for n in slab_names:
        actor = eas.spawn_actor_from_object(unreal.load_asset(DEST + "/" + n), unreal.Vector(0, 0, 0))
        actor.set_actor_label(n.replace("SM_", ""))
        actor.static_mesh_component.set_material(0, mat)
    plaza = by.get("Plaza")
    if plaza:
        loc = plaza.get_actor_location()
        half = plaza.get_actor_scale3d().z * 50
        plaza.set_actor_location(unreal.Vector(loc.x, loc.y, man["plaza_top"] - half), False, False)

print("imported %d meshes, swapped %d actors%s" % (len(names), done, ", plaza slabs placed" if slab_names else ""))
