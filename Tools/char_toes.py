"""Give Quinn (the hidden body that leads her pose) the skin's toe bones.

  python3 Tools/ue.py Tools/char_toes.py      # after Tools/char_skin.py imported the toed skin

The skin follows Quinn through a leader pose; a skin bone Quinn lacks gets no transform at
all, so the toes would fly off. This copies each toe bone from SKM_AndroidSkin onto
SKM_Quinn_Simple with the same parent and the same local reference transform. No weights:
Quinn is never drawn. Safe to rerun: bones already there are skipped.
"""
import unreal

SKIN = "/Game/Character/Android/SKM_AndroidSkin"
QUINN = "/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple"

if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise RuntimeError("Play mode is running: stop PIE first")
src = unreal.SkeletonModifier()
src.set_skeletal_mesh(unreal.load_asset(SKIN))
quinn = unreal.load_asset(QUINN)
dst = unreal.SkeletonModifier()
dst.set_skeletal_mesh(quinn)
have = {str(n) for n in dst.get_all_bone_names()}
added = []
for n in [str(b) for b in src.get_all_bone_names()]:      # parents come before children
    if "toe_0" not in n or n in have:
        continue
    parent = str(src.get_parent_name(n))
    assert parent in have, "parent %s of %s missing on Quinn" % (parent, n)
    assert dst.add_bone(n, parent, src.get_bone_transform(n, False)), n
    have.add(n)
    added.append(n)
if added:
    assert dst.commit_skeleton_to_skeletal_mesh(), "commit failed"
    unreal.EditorAssetLibrary.save_loaded_asset(quinn)
print("Quinn: added", len(added), "toe bones")
