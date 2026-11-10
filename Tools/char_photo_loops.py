"""Photo mode's walk and run: in-place copies of the template's forward walk and jog.

  python3 Tools/ue.py Tools/char_photo_loops.py

Writes /Game/Characters/Menu/A_Photo_Walk and A_Photo_Run: root motion off and the root
locked, so on a slot they loop on the spot. Her catwalk is a layer in VisexAnimInstance
that follows her speed; standing still it is off, so photo mode sets the anim instance's
PhotoGait (1 for the walk, 0.35 for the run) while it plays these, and -1 after.
"""
import unreal

if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise RuntimeError("Play mode is running: stop PIE first")
SRC = "/Game/Characters/Mannequins/Anims/Unarmed"
for src, name in ((SRC + "/Walk/MF_Unarmed_Walk_Fwd", "A_Photo_Walk"), (SRC + "/Jog/MF_Unarmed_Jog_Fwd", "A_Photo_Run")):
    dst = "/Game/Characters/Menu/" + name
    if not unreal.EditorAssetLibrary.does_asset_exist(dst):
        unreal.EditorAssetLibrary.duplicate_asset(src, dst)
    seq = unreal.load_asset(dst)
    seq.set_editor_property("enable_root_motion", False)
    seq.set_editor_property("force_root_lock", True)
    unreal.EditorAssetLibrary.save_loaded_asset(seq, only_if_is_dirty=False)
    print("saved", dst, "%.2f s" % seq.get_play_length())
