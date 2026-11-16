"""Import an outfit cut on the MetaHuman body (Tools/blender_outfit.py --body=mh) into UE.

  python3 Tools/ue.py -c "OUTFIT='Silversuit'; exec(open('<project>/Tools/mh_outfit.py').read())"

Reads SourceArt/Character/MH/Outfits/outfit_<name>.fbx and cover_<name>.png, writes into
/Game/Character/MH/Outfits:
  SKM_<Name>_MH        the garment on the MetaHuman skeleton, wearing the outfit's existing
                       material instances (taken slot by slot from the old body's mesh)
  T_Cover_<Name>       the body's UV0, white where opaque fabric hides the skin
  MI_Body_<Name>       the body's material with that skin clipped away, so nothing pokes through,
                       and with the baked underwear painted out (T_Body_*_Bare, Tools/mh_bare_skin.py)
  M_Body_Under, MI_Body_Under   (shared) copies of the MetaHuman skin material and its instance
                       with an opacity mask added; the assembled originals are not edited
Nothing outside that folder is written. UVisexMHBodyComponent picks the three per-outfit assets
up by name.
"""
import os
import unreal

OUTFIT = globals().get("OUTFIT", "Silversuit")
DIR = "/Game/Character/MH/Outfits"
BODY = "/Game/Character/MH/Built/MHC_Her/Body/SKM_MHC_Her_BodyMesh"
OLD_MESH = {"Silversuit": "/Game/Character/Clothes/SKM_Outfit_Silversuit"}
SRC = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "SourceArt", "Character", "MH", "Outfits")
MASK_PARAM = "Cover Mask"
BARE = {"Basecolor Baked VT": "T_Body_BC_Bare", "Normal Baked VT": "T_Body_N_Bare", "SRMF Baked VT": "T_Body_SRMF_Bare"}
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
body = unreal.load_asset(BODY)
assert body, "no MetaHuman body at " + BODY


def import_cover():
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(SRC, "cover_%s.png" % OUTFIT.lower()))
    task.set_editor_property("destination_path", DIR)
    task.set_editor_property("destination_name", "T_Cover_" + OUTFIT)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    tex = unreal.load_asset(DIR + "/T_Cover_" + OUTFIT)
    assert tex, "cover import failed"
    tex.set_editor_property("srgb", False)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    tex.set_editor_property("virtual_texture_streaming", False)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_CHARACTER)
    eal.save_loaded_asset(tex)
    return tex


def under_material(cover):
    """The shared masked copy of the skin material, then this outfit's instance of it."""
    src_mi = body.get_editor_property("materials")[0].get_editor_property("material_interface")
    chain = [src_mi]
    while isinstance(chain[-1], unreal.MaterialInstance):
        chain.append(chain[-1].get_editor_property("parent"))
    assert len(chain) == 3, "expected body instance > shared instance > material, got %d levels" % len(chain)
    base_path, mid_path, mi_path = DIR + "/M_Body_Under", DIR + "/MI_Body_Under", DIR + "/MI_Body_" + OUTFIT
    base = unreal.load_asset(base_path) if eal.does_asset_exist(base_path) else None
    if not base:
        base = eal.duplicate_asset(chain[2].get_path_name().split(".")[0], base_path)
        base.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
        base.set_editor_property("opacity_mask_clip_value", 0.5)
        tex = mel.create_material_expression(base, unreal.MaterialExpressionTextureSampleParameter2D, -600, 1400)
        tex.set_editor_property("parameter_name", MASK_PARAM)
        tex.set_editor_property("texture", cover)          # any linear mask; every instance sets its own
        tex.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        inv = mel.create_material_expression(base, unreal.MaterialExpressionOneMinus, -350, 1400)
        mel.connect_material_expressions(tex, "R", inv, "")
        mel.connect_material_property(inv, "", unreal.MaterialProperty.MP_OPACITY_MASK)
        mel.recompile_material(base)
        eal.save_loaded_asset(base)
    mid = unreal.load_asset(mid_path) if eal.does_asset_exist(mid_path) else None
    if not mid:
        mid = eal.duplicate_asset(chain[1].get_path_name().split(".")[0], mid_path)
        mel.set_material_instance_parent(mid, base)
        mel.update_material_instance(mid)
        eal.save_loaded_asset(mid)
    mi = unreal.load_asset(mi_path) if eal.does_asset_exist(mi_path) else None
    if not mi:
        mi = eal.duplicate_asset(chain[0].get_path_name().split(".")[0], mi_path)
        mel.set_material_instance_parent(mi, mid)
    else:   # a re-assembly rewrites the body's own instance: carry its values over again
        for n in mel.get_texture_parameter_names(src_mi):
            mel.set_material_instance_texture_parameter_value(mi, n, mel.get_material_instance_texture_parameter_value(src_mi, n))
        for n in mel.get_scalar_parameter_names(src_mi):
            mel.set_material_instance_scalar_parameter_value(mi, n, mel.get_material_instance_scalar_parameter_value(src_mi, n))
        for n in mel.get_vector_parameter_names(src_mi):
            mel.set_material_instance_vector_parameter_value(mi, n, mel.get_material_instance_vector_parameter_value(src_mi, n))
    mel.set_material_instance_texture_parameter_value(mi, MASK_PARAM, cover)
    # under an outfit she wears no baked underwear: the maps with it painted out (Tools/mh_bare_skin.py)
    for param, name in BARE.items():
        if eal.does_asset_exist(DIR + "/" + name):
            mel.set_material_instance_texture_parameter_value(mi, param, unreal.load_asset(DIR + "/" + name))
        else:
            print("no", name, "- the baked underwear stays (run Tools/mh_bare_skin.py)")
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi)
    return mi


def import_mesh():
    name = "SKM_%s_MH" % OUTFIT
    ui = unreal.FbxImportUI()
    for k, v in (("import_mesh", True), ("import_as_skeletal", True), ("import_materials", False), ("import_textures", False),
                 ("import_animations", False), ("create_physics_asset", False)):
        ui.set_editor_property(k, v)
    ui.set_editor_property("skeleton", body.get_editor_property("skeleton"))
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    sk = ui.get_editor_property("skeletal_mesh_import_data")
    # morph targets: the body's shape and jiggle morphs, carried by the fabric (Tools/mh_morphs.py)
    settings = dict(import_morph_targets=True, normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_COMPUTE_NORMALS,
                    update_skeleton_reference_pose=False, use_t0_as_ref_pose=False, import_uniform_scale=0.01)
    for k, v in settings.items():
        sk.set_editor_property(k, v)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(SRC, "outfit_%s.fbx" % OUTFIT.lower()))
    task.set_editor_property("destination_path", DIR)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", ui)
    # the legacy importer, and the settings stamped on a mesh that is being replaced: see
    # import_mesh() in Tools/char_outfit.py (Interchange brings these in 100x too large)
    old = unreal.load_asset(DIR + "/" + name) if eal.does_asset_exist(DIR + "/" + name) else None
    data = old.get_editor_property("asset_import_data") if old else None
    if isinstance(data, unreal.FbxSkeletalMeshImportData):
        for k, v in settings.items():
            data.set_editor_property(k, v)
    world = unreal.EditorLevelLibrary.get_editor_world()
    cvar = "Interchange.FeatureFlags.Import.FBX"
    was = unreal.SystemLibrary.get_console_variable_bool_value(cvar)
    unreal.SystemLibrary.execute_console_command(world, cvar + " 0")
    try:
        tools.import_asset_tasks([task])
    finally:
        unreal.SystemLibrary.execute_console_command(world, "%s %d" % (cvar, 1 if was else 0))
    mesh = unreal.load_asset(DIR + "/" + name)
    assert mesh, "import failed: " + name
    # the outfit's own material instances, slot by slot from the mesh the old body wears
    worn = {str(m.get_editor_property("material_slot_name")): m.get_editor_property("material_interface")
            for m in unreal.load_asset(OLD_MESH[OUTFIT]).get_editor_property("materials")}
    mats = list(mesh.get_editor_property("materials"))
    for sm in mats:
        pick = worn.get(str(sm.get_editor_property("material_slot_name")))
        if pick:
            sm.set_editor_property("material_interface", pick)
    mesh.set_editor_property("materials", mats)
    eal.save_loaded_asset(mesh)
    print(name, "height", round(mesh.get_bounds().box_extent.z * 2, 1), "skeleton", mesh.get_editor_property("skeleton").get_name(),
          [(str(m.get_editor_property("material_slot_name")), m.get_editor_property("material_interface").get_name()
            if m.get_editor_property("material_interface") else None) for m in mats])
    return mesh


cover = import_cover()
print("body material", under_material(cover).get_path_name())
import_mesh()
