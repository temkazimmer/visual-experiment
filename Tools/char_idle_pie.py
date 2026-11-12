"""Start Play straight into gameplay (menu skipped) with a scratch capture, for the idle checks.

  python3 Tools/ue.py Tools/char_idle_pie.py        # then Tools/char_idle_check.py
  Stop: python3 Tools/ue.py -c "import unreal; unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()"

Sets bShowMenuOnPlay off on the level's menu director (in memory only) and spawns TmpIdleCap.
Never save Lvl_Dust after this.
"""
import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if les.is_in_play_in_editor():
    raise RuntimeError("already in Play: not mine, aborting")
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for a in eas.get_all_level_actors():
    if a.get_actor_label().startswith("TmpIdle"):
        eas.destroy_actor(a)
for a in eas.get_all_level_actors():
    if isinstance(a, unreal.VisexMenuDirector):
        a.set_editor_property("show_menu_on_play", False)
        print("menu off on", a.get_actor_label())
cap = eas.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(0, 0, 300))
cap.set_actor_label("TmpIdleCap")
cap.capture_component2d.set_editor_property("capture_every_frame", False)
les.editor_request_begin_play()
print("play requested")
