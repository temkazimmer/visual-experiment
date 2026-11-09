"""Import the Megascans Rippled Sand set (vd3lecfs, 2 m tile) from SourceArt into
/Game/VisEx/Megascans/RippledSand. 8K UE-packed maps for close range, the 2K maps
for the far layer, and Desert Sand 1K for patches. Idempotent (replaces).

Run live: python3 Tools/ue.py Tools/import_sand_tex.py
"""
import os

import unreal

ROOT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "SourceArt", "Megascans")
DEST = "/Game/VisEx/Megascans/RippledSand"
# file, asset name, kind
FILES = [
    ("RippledSand_vd3lecfs/Textures/T_vd3lecfs_8K_B.png", "T_Sand_8K_B", "color"),
    ("RippledSand_vd3lecfs/Textures/T_vd3lecfs_8K_N.png", "T_Sand_8K_N", "normal"),       # DirectX already
    ("RippledSand_vd3lecfs/Textures/T_vd3lecfs_8K_ORM.png", "T_Sand_8K_ORM", "masks"),
    ("RippledSand_vd3lecfs_2k/Rippled_Sand_vd3lecfs_2K_BaseColor.jpg", "T_Sand_2K_B", "color"),
    ("RippledSand_vd3lecfs_2k/Rippled_Sand_vd3lecfs_2K_Normal.jpg", "T_Sand_2K_N", "normal_gl"),  # OpenGL: flip green
    ("RippledSand_vd3lecfs_2k/Rippled_Sand_vd3lecfs_2K_Roughness.jpg", "T_Sand_2K_R", "masks"),
    # Desert Sand (wkjgche, 2 m tile): smooth wind-flattened sand, mixed in as patches
    ("DesertSand_wkjgche_1k/Desert_Sand_wkjgche_1K_BaseColor.jpg", "T_Desert_1K_B", "color"),
    ("DesertSand_wkjgche_1k/Desert_Sand_wkjgche_1K_Normal.jpg", "T_Desert_1K_N", "normal_gl"),
    ("DesertSand_wkjgche_1k/Desert_Sand_wkjgche_1K_Roughness.jpg", "T_Desert_1K_R", "masks"),
]

tasks = []
for rel, name, _ in FILES:
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", os.path.join(ROOT, rel))
    t.set_editor_property("destination_path", DEST)
    t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", False)
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

for _, name, kind in FILES:
    path = DEST + "/" + name
    tex = unreal.load_asset(path)
    if kind.startswith("normal"):
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("flip_green_channel", kind == "normal_gl")
    elif kind == "masks":
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        tex.set_editor_property("srgb", False)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    unreal.EditorAssetLibrary.save_asset(path)
    print(name, tex.blueprint_get_size_x(), "px", kind)
