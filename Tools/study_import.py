"""Import a bought pack's FBX clips into Content/Study (gitignored) for measuring.

  python3 Tools/ue.py -c "PACK='GirlWalkRun'; exec(open('Tools/study_import.py').read())"

The pack's own mesh comes in first with a new skeleton of its own, and every clip goes
onto that skeleton, so our SK_Mannequin is never touched (no added bones or curves).
Licensed: Content/Study is gitignored; nothing here is ever committed.
"""
import glob
import os
import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
PACKS = {
    "GirlWalkRun": ("SourceArt/Fab/GirlWalkRun/fbx/SKM_Quinn_Simple.FBX", "SourceArt/Fab/GirlWalkRun/fbx/animations/*.fbx"),
    "LieDown": (None, "SourceArt/Fab/LieDown/*.FBX"),
}
PACK = globals().get("PACK", "GirlWalkRun")
ONLY = globals().get("ONLY")                 # e.g. ["AS_Walk_Strut_1"]
DEST = "/Game/Study/" + PACK
mesh_src, anim_glob = PACKS[PACK]

def task(src, dest, opts):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", src)
    t.set_editor_property("destination_path", dest)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    t.set_editor_property("options", opts)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    return [str(p) for p in t.get_editor_property("imported_object_paths")]

skeleton = None
skel_path = DEST + "/" + os.path.splitext(os.path.basename(mesh_src or "x"))[0] + "_Skeleton"
if mesh_src:
    if not unreal.EditorAssetLibrary.does_asset_exist(skel_path):
        ui = unreal.FbxImportUI()
        ui.set_editor_property("import_mesh", True)
        ui.set_editor_property("import_as_skeletal", True)
        ui.set_editor_property("import_animations", False)
        ui.set_editor_property("import_materials", False)
        ui.set_editor_property("import_textures", False)
        ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
        print("mesh:", task(os.path.join(ROOT, mesh_src), DEST, ui))
    skeleton = unreal.load_asset(skel_path)
    print("skeleton", skel_path, bool(skeleton))

for src in sorted(glob.glob(os.path.join(ROOT, anim_glob))):
    name = os.path.splitext(os.path.basename(src))[0]
    if ONLY and name not in ONLY:
        continue
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", False)
    ui.set_editor_property("import_animations", True)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
    if skeleton:
        ui.set_editor_property("skeleton", skeleton)
    print(name, task(src, DEST, ui))
