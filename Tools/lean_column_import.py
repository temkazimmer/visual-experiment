"""Import SM_LeanColumn_HD (Tools/lean_column_geo.py) into /Game/VisEx/Geo.

  python3 Tools/lean_column_geo.py && python3 Tools/ue.py Tools/lean_column_import.py

Nanite off, so LOD 0 is the drawn mesh and the collision is cooked from those same
triangles (complex as simple): a trace lands on the stone she is drawn against. The material
is SM_Drum_0's. Saves only this asset; the level is untouched (the menu spawns it at Play).
"""
import os

import unreal

DEST, NAME = "/Game/VisEx/Geo", "SM_LeanColumn_HD"
OBJ = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()),
                   "Tools", ".bridge", "geo", NAME + ".obj")

t = unreal.AssetImportTask()
for k, v in (("filename", OBJ), ("destination_path", DEST), ("destination_name", NAME),
             ("automated", True), ("replace_existing", True), ("save", False)):
    t.set_editor_property(k, v)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])

m = unreal.load_asset(DEST + "/" + NAME)
old = unreal.load_asset(DEST + "/SM_Drum_0")
mat = old.get_material(0)
ns = m.get_editor_property("nanite_settings")
ns.set_editor_property("enabled", False)
m.set_editor_property("nanite_settings", ns)
body = m.get_editor_property("body_setup")
body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
m.set_material(0, mat)
unreal.EditorAssetLibrary.save_asset(DEST + "/" + NAME)

b = m.get_bounding_box()
print("%s: %d tris, material %s, bounds %s .. %s, nanite %s, collision %s" % (
    NAME, m.get_num_triangles(0), mat.get_path_name(), b.min, b.max,
    m.get_editor_property("nanite_settings").enabled,
    m.get_editor_property("body_setup").get_editor_property("collision_trace_flag")))
