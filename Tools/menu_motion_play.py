"""Start Play for the menu motion check: a SceneCapture2D for its frames (placed in the
editor level, never saved), the arrival off unless ARRIVAL=1, then Play.

  python3 Tools/ue.py -c "exec(open('Tools/menu_motion_play.py').read())"
"""
import unreal

LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if LES.is_in_play_in_editor():
    raise RuntimeError("already in Play (maybe Tema's): not touching it")
w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if not [a for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.SceneCapture2D) if a.get_actor_label() == "MotionCap"]:
    a = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(0, 0, 500))
    a.set_actor_label("MotionCap")
    a.capture_component2d.set_editor_property("capture_every_frame", False)
unreal.SystemLibrary.execute_console_command(None, "visex.Menu.Arrival %d" % int(globals().get("ARRIVAL", 0)))
LES.editor_request_begin_play()
print("play requested")
