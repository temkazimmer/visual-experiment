"""Import the android's skin mesh (made by Tools/blender_skin.py) and put it on the player.

  python3 Tools/ue.py Tools/char_skin.py        # FBX path: SKIN_FBX global or default below

- Imports the FBX as /Game/Character/Android/SKM_AndroidSkin on Quinn's skeleton
  (SK_Mannequin), with its morph targets (BustUp/BustDown, GlutesUp/GlutesDown).
- The Shell component shows the skin; the body (Quinn) stays as the animation leader but
  is hidden, so no mechanical panel can poke through. The skin casts the shadow.
Run Tools/char_shell.py first (material, scale, shell component).
"""
import os
import unreal

FBX = globals().get("SKIN_FBX") or os.path.join(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "SourceArt", "Character", "android_skin.fbx")
DIR = "/Game/Character/Android"
NAME = "SKM_AndroidSkin"
BP_PATH = "/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"
SKELETON = "/Game/Characters/Mannequins/Meshes/SK_Mannequin"

if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise RuntimeError("Play mode is running: stop PIE before importing")

ui = unreal.FbxImportUI()
ui.set_editor_property("import_mesh", True)
ui.set_editor_property("import_as_skeletal", True)
ui.set_editor_property("import_materials", False)
ui.set_editor_property("import_textures", False)
ui.set_editor_property("import_animations", False)
ui.set_editor_property("create_physics_asset", False)
ui.set_editor_property("skeleton", unreal.load_asset(SKELETON))
ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
sk = ui.get_editor_property("skeletal_mesh_import_data")
sk.set_editor_property("import_morph_targets", True)
sk.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_COMPUTE_NORMALS)
sk.set_editor_property("update_skeleton_reference_pose", False)
sk.set_editor_property("use_t0_as_ref_pose", False)
sk.set_editor_property("import_uniform_scale", float(globals().get("SKIN_SCALE", 0.01)))   # Blender writes metres

task = unreal.AssetImportTask()
task.set_editor_property("filename", FBX)
task.set_editor_property("destination_path", DIR)
task.set_editor_property("destination_name", NAME)
task.set_editor_property("automated", True)
task.set_editor_property("replace_existing", True)
task.set_editor_property("save", True)
task.set_editor_property("options", ui)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

skin = unreal.load_asset(DIR + "/" + NAME)
if not skin:
    raise RuntimeError("import failed: " + FBX)
b = skin.get_bounds()
print("skin height", round(b.box_extent.z * 2, 1), "morphs", [m.get_name() for m in skin.get_editor_property("morph_targets")])

bp = unreal.load_asset(BP_PATH)
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
by = {str(lib.get_variable_name(lib.get_data(h))): h for h in sds.k2_gather_subobject_data_for_blueprint(bp)}
shell = lib.get_object(lib.get_data(by["Shell"]))
shell.set_skeletal_mesh_asset(skin)
shell.set_editor_property("override_materials", [unreal.load_asset(DIR + "/M_AerogelShell")])
shell.set_editor_property("cast_shadow", True)

body = unreal.get_default_object(bp.generated_class()).get_editor_property("mesh")
body.set_editor_property("visible", False)
body.set_editor_property("cast_shadow", False)
# hidden, but it still leads the pose
body.set_editor_property("visibility_based_anim_tick_option",
                         unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)

unreal.BlueprintEditorLibrary.compile_blueprint(bp)
unreal.EditorAssetLibrary.save_loaded_asset(bp)
print("shell ->", shell.get_editor_property("skeletal_mesh_asset").get_name())
