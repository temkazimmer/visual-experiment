"""Walk the player forward in Play and shoot a strip of frames, to judge the gait.

  python3 Tools/ue.py -c "import unreal; unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector()); unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()"
  python3 Tools/ue.py Tools/char_walk_shots.py      # then wait ~4 s for Tools/.bridge/walk_*.png
  python3 Tools/ue.py -c "import unreal; unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()"

Only for a Play session you started yourself, in a level where the pawn spawns free
(Lvl_ThirdPerson). Don't save the level afterwards: the capture actor is scratch. Globals: SIDE = "front" | "back" | "side", SPRINT = True, FRAMES, EVERY.
"""
import os
import unreal

SIDE = globals().get("SIDE", "front")
FRAMES = globals().get("FRAMES", 6)
EVERY = globals().get("EVERY", 0.1)       # seconds between frames
WARMUP = 1.6                               # seconds of walking before the first frame
SPEED = 540.0 if globals().get("SPRINT") else None

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
if not world:
    raise RuntimeError("Play mode isn't running")
pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
if SPEED:
    pawn.get_editor_property("character_movement").set_editor_property("max_walk_speed", SPEED)
out_dir = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools", ".bridge")
rt = unreal.RenderingLibrary.create_render_target2d(world, 540, 900, unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
caps = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SceneCapture2D)
if not caps:
    raise RuntimeError("no SceneCapture2D in the level: spawn one before Play (Python can't spawn into the game world)")
cap = caps[0]
comp = cap.capture_component2d
comp.set_editor_property("texture_target", rt)
comp.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_TONE_CURVE_HDR)
comp.set_editor_property("capture_every_frame", False)
comp.set_editor_property("fov_angle", 30.0)

fwd = pawn.get_actor_forward_vector()
right = pawn.get_actor_right_vector()
offset = {"front": fwd * 420.0, "back": fwd * -420.0, "side": right * 420.0}[SIDE]
state = {"t": 0.0, "n": 0}


def tick(dt):
    try:
        pawn.add_movement_input(fwd, 1.0)
        state["t"] += dt
        if state["t"] >= WARMUP + state["n"] * EVERY:
            at = pawn.get_actor_location()
            eye = at + offset + unreal.Vector(0, 0, 10)
            cap.set_actor_location_and_rotation(eye, unreal.MathLibrary.find_look_at_rotation(eye, at), False, False)
            comp.capture_scene()
            unreal.RenderingLibrary.export_render_target(world, rt, out_dir, "walk_%s_%d.png" % (SIDE, state["n"]))
            state["n"] += 1
        if state["n"] >= FRAMES:
            raise StopIteration
    except Exception as e:
        unreal.unregister_slate_post_tick_callback(handle)
        if not isinstance(e, StopIteration):
            unreal.log_error("walk shots: %s" % e)


handle = unreal.register_slate_post_tick_callback(tick)
print("shooting", FRAMES, SIDE, "frames")
