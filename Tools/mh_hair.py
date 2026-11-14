"""Import the card hairstyles refitted to the MetaHuman skull (Tools/mh_hair_fit.py --write) as new
meshes: /Game/Character/MH/Hair/SKM_Hair{Cards,Elf,Short}_MH, on the hair's own skeleton
(SK_HairCards, bones untouched) with the materials of the style they came from. The style's physics
AnimBP runs them as it is. Nothing under /Game/Character/Hair is saved.

  python3 Tools/ue.py Tools/mh_hair.py
"""
import os
import unreal

SRC = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "SourceArt", "Character", "MH", "Hair")
OLD, DIR = "/Game/Character/Hair", "/Game/Character/MH/Hair"
STYLES = (("hair_cards_mh.fbx", "SKM_HairCards"), ("hair_elf_mh.fbx", "SKM_HairElf"), ("hair_short_mh.fbx", "SKM_HairShort"))

assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "Play is running: not ours to stop"
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
skeleton = unreal.load_asset(OLD + "/SK_HairCards")
assert skeleton, "no hair skeleton"

for fbx, name in STYLES:
    old = unreal.load_asset(OLD + "/" + name)
    assert old and os.path.exists(os.path.join(SRC, fbx)), "missing " + name + " or " + fbx
    ui = unreal.FbxImportUI()            # as Tools/char_hair_cards.py
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("create_physics_asset", False)
    ui.set_editor_property("skeleton", skeleton)
    smd = ui.get_editor_property("skeletal_mesh_import_data")
    smd.set_editor_property("import_morph_targets", False)
    smd.set_editor_property("convert_scene", True)
    smd.set_editor_property("import_uniform_scale", 0.01)
    smd.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    smd.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(SRC, fbx))
    task.set_editor_property("destination_path", DIR)
    task.set_editor_property("destination_name", name + "_MH")
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    task.set_editor_property("options", ui)
    tools.import_asset_tasks([task])
    mesh = unreal.load_asset(DIR + "/" + name + "_MH")
    assert mesh, name + "_MH did not import"
    assert mesh.get_editor_property("skeleton") == skeleton, "landed on " + mesh.get_editor_property("skeleton").get_path_name()
    # the style's own materials, by slot name (the short cut's are instances with its colours)
    by_slot = {str(m.get_editor_property("material_slot_name")): m.get_editor_property("material_interface") for m in old.get_editor_property("materials")}
    mats = list(mesh.get_editor_property("materials"))
    for m in mats:
        slot = str(m.get_editor_property("material_slot_name"))
        assert slot in by_slot, "slot %s is not on %s" % (slot, name)
        m.set_editor_property("material_interface", by_slot[slot])
    mesh.set_editor_property("materials", mats)
    eal.save_loaded_asset(mesh)
    ob, nb = old.get_bounds(), mesh.get_bounds()
    print("HAIR %s_MH: slots %s, bounds origin (%.1f %.1f %.1f) extent (%.1f %.1f %.1f); %s was (%.1f %.1f %.1f) (%.1f %.1f %.1f)" % (
        name, [str(m.get_editor_property("material_slot_name")) for m in mats], nb.origin.x, nb.origin.y, nb.origin.z,
        nb.box_extent.x, nb.box_extent.y, nb.box_extent.z, name, ob.origin.x, ob.origin.y, ob.origin.z,
        ob.box_extent.x, ob.box_extent.y, ob.box_extent.z))
