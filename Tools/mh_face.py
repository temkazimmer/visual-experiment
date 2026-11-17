"""Give the MetaHuman her face: fit the Creator's face to the meshes Tools/mh_face_fit.py wrote.

Used by Tools/mh_create.py as its 'face' step (between 'shape' and 'look' of the one clean pass).
On its own, through the bridge, for trying a fit on a scratch character before the cloud steps:

  python3 Tools/ue.py -c "CHAR_NAME='MHC_Try'; STEPS=['create','shape','face']; exec(open('<project>/Tools/mh_create.py').read())"
  python3 Tools/ue.py -c "MODE='export'; CHAR_NAME='MHC_Try'; exec(open('<project>/Tools/mh_face.py').read())"

'export' writes the Creator's preview face mesh to Saved/MH/face/<CHAR_NAME>_head.fbx (to measure and
look at in Blender: what the Creator made of the target, ears included).

The four static meshes carry the MetaHuman's own UVs, so `import_from_template` finds each DNA vertex
by UV; alignment NONE keeps them where they are on the body (cm, the assembled mesh's frame). The fit
is the face model plus per-vertex deltas at full strength, so the target comes back as given.
"""
import os
import unreal

eal = unreal.EditorAssetLibrary
mh = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
FIT_SRC = os.path.join(PROJECT, "SourceArt", "Character", "MH", "Face")
FIT_DIR = "/Game/Character/MH/FaceFit"
FIT_PARTS = ("head", "eye_l", "eye_r", "teeth")


def import_parts():
    """The four FBX files as static meshes (legacy importer: Interchange scales Blender files by 100)."""
    tasks = []
    for key in FIT_PARTS:
        ui = unreal.FbxImportUI()
        for k, v in (("import_mesh", True), ("import_as_skeletal", False), ("import_materials", False),
                     ("import_textures", False), ("import_animations", False)):
            ui.set_editor_property(k, v)
        ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
        sm = ui.get_editor_property("static_mesh_import_data")
        for k, v in (("combine_meshes", True), ("remove_degenerates", False), ("generate_lightmap_u_vs", False),
                     ("auto_generate_collision", False), ("build_nanite", False), ("import_uniform_scale", 1.0),
                     ("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_COMPUTE_NORMALS)):
            sm.set_editor_property(k, v)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", os.path.join(FIT_SRC, "face_%s.fbx" % key))
        task.set_editor_property("destination_path", FIT_DIR)
        task.set_editor_property("destination_name", "SM_Face_" + key)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", True)
        task.set_editor_property("options", ui)
        tasks.append(task)
    world = unreal.EditorLevelLibrary.get_editor_world()
    cvar = "Interchange.FeatureFlags.Import.FBX"
    was = unreal.SystemLibrary.get_console_variable_bool_value(cvar)
    unreal.SystemLibrary.execute_console_command(world, cvar + " 0")
    try:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    finally:
        unreal.SystemLibrary.execute_console_command(world, "%s %d" % (cvar, 1 if was else 0))
    out = {}
    for key in FIT_PARTS:
        out[key] = unreal.load_asset(FIT_DIR + "/SM_Face_" + key)
        assert out[key], "import failed: face_%s.fbx" % key
        b = out[key].get_bounding_box()
        print("MHFACE %-6s min %s max %s" % (key, [round(x, 2) for x in (b.min.x, b.min.y, b.min.z)], [round(x, 2) for x in (b.max.x, b.max.y, b.max.z)]))
    return out


def fit_face(character):
    """Fit the character's face to the imported meshes. The character must be added for editing."""
    parts = import_parts()
    params = unreal.ImportFromTemplateParams()
    params.set_editor_property("match_vertices_by_u_vs", True)
    params.set_editor_property("use_eye_meshes", True)
    params.set_editor_property("use_teeth_mesh", True)
    params.set_editor_property("alignment_options", unreal.MetaHumanAlignmentOptions.NONE)
    result = mh.import_from_template(character, parts["head"], parts["eye_l"], parts["eye_r"], parts["teeth"], params)
    print("MHFACE import_from_template:", result)
    if result != unreal.ImportErrorCode.SUCCESS:
        raise RuntimeError("the face fit failed: %s (the reason is in the editor log)" % result)
    try:
        fes = character.get_editor_property("face_evaluation_settings")
        print("MHFACE face evaluation: global delta %.2f, high frequency delta %.2f" % (
            fes.get_editor_property("global_delta"), fes.get_editor_property("high_frequency_delta")))
    except Exception as e:
        print("MHFACE face evaluation settings not readable:", e)


def export_preview(name):
    """The Creator's preview face and body meshes of a character, as FBX, to look at what a fit gave."""
    character = unreal.load_asset("/Game/Character/MH/" + name)
    was = mh.is_object_added_for_editing(character)
    assert was or mh.try_add_object_to_edit(character), "cannot edit " + name
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    out = os.path.join(PROJECT, "Saved", "MH", "face")
    os.makedirs(out, exist_ok=True)
    actor = mh.spawn_meta_human_actor(character)
    try:
        for c in actor.get_components_by_class(unreal.SkeletalMeshComponent):
            mesh = c.get_skinned_asset()
            if mesh is None or c.get_name() not in ("Face", "Body"):
                continue
            opt = unreal.FbxExportOption()
            for k, v in (("level_of_detail", False), ("collision", False), ("export_morph_targets", False),
                         ("vertex_color", False), ("ascii", False), ("force_front_x_axis", False)):
                opt.set_editor_property(k, v)
            task = unreal.AssetExportTask()
            path = os.path.join(out, "%s_%s.fbx" % (name, c.get_name().lower()))
            for k, v in (("object", mesh), ("filename", path), ("exporter", unreal.SkeletalMeshExporterFBX()),
                         ("options", opt), ("automated", True), ("prompt", False), ("replace_identical", True)):
                task.set_editor_property(k, v)
            print("MHFACE export", c.get_name(), mesh.get_name(), unreal.Exporter.run_asset_export_task(task), path)
    finally:
        eas.destroy_actor(actor)
        if not was:
            mh.remove_object_to_edit(character)


if globals().get("MODE") == "export":
    export_preview(globals().get("CHAR_NAME", "MHC_Try"))
elif globals().get("MODE") == "import":
    import_parts()
