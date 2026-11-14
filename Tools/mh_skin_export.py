"""Export the assembled MetaHuman body (LOD 0, skin weights, reference pose) to FBX: the skin
source for outfits on the MetaHuman body (Tools/blender_outfit.py --body=mh).

  python3 Tools/ue.py Tools/mh_skin_export.py

Writes SourceArt/Character/MH/mh_skin.fbx. Run again after every re-assembly or new shape: the
garments are cut from this mesh, so an old export fits the old body.
"""
import os
import unreal

MESH = globals().get("MESH", "/Game/Character/MH/Built/MHC_Her/Body/SKM_MHC_Her_BodyMesh")
OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()),
                   "SourceArt", "Character", "MH", "mh_skin.fbx")

mesh = unreal.load_asset(MESH)
assert mesh, "no mesh at " + MESH
os.makedirs(os.path.dirname(OUT), exist_ok=True)

opt = unreal.FbxExportOption()
opt.set_editor_property("level_of_detail", False)          # LOD 0 only
opt.set_editor_property("collision", False)
opt.set_editor_property("export_morph_targets", False)     # the body has none
opt.set_editor_property("vertex_color", False)
opt.set_editor_property("ascii", False)
opt.set_editor_property("force_front_x_axis", False)

task = unreal.AssetExportTask()
task.set_editor_property("object", mesh)
task.set_editor_property("filename", OUT)
task.set_editor_property("exporter", unreal.SkeletalMeshExporterFBX())
task.set_editor_property("options", opt)
task.set_editor_property("automated", True)
task.set_editor_property("prompt", False)
task.set_editor_property("replace_identical", True)
ok = unreal.Exporter.run_asset_export_task(task)
assert ok and os.path.exists(OUT), "export failed"
print("wrote", OUT, round(os.path.getsize(OUT) / 1e6, 1), "MB")
print("bones", len(mesh.skeleton.get_reference_pose().get_bone_names()) if hasattr(mesh.skeleton, "get_reference_pose") else "?",
      "materials", [str(m.get_editor_property("material_slot_name")) for m in mesh.get_editor_property("materials")],
      [m.get_editor_property("material_interface").get_path_name() for m in mesh.get_editor_property("materials")])
