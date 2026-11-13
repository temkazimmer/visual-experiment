"""Menu motion check: in Play, run one menu transition and watch it.

  python3 Tools/ue.py -c "PAGE='body'; DIR='in'; exec(open('Tools/menu_motion.py').read())"
  PAGE: body | wardrobe | photo | settings      DIR: in | out
  SHOTS=5      also capture that many frames over SPAN seconds through the player camera
               (2K, Tools/.bridge/shots/motion_<page>_<dir>_<i>.png; strip them with
               Tools/menu_motion_strip.sh). 0 = timing only (captures hitch the frame).
The time and the planted-foot skate are logged by the director ("Menu motion: ..."); this
prints her top speed and the frames it shot. Needs a SceneCapture2D placed before Play
(SHOTS > 0). Nothing is saved.
"""
import os
import unreal

PAGE = globals().get("PAGE", "body")
DIR = globals().get("DIR", "in")
SHOTS = int(globals().get("SHOTS", 0))
SPAN = float(globals().get("SPAN", 3.0))
END = float(globals().get("END", 5.5))   # game seconds watched (run under slomo: the editor draws ~6 fps in the background)
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
if not world or not LES.is_in_play_in_editor():
    raise RuntimeError("not in Play")
d = unreal.GameplayStatics.get_actor_of_class(world, unreal.VisexMenuDirector)
pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools", ".bridge", "shots")
os.makedirs(OUT, exist_ok=True)


def go():
    if DIR == "in":
        if PAGE == "body":
            d.open_body()
        elif PAGE == "wardrobe":
            d.open_wardrobe()
        elif PAGE == "photo":
            d.on_item_chosen(3)
        else:
            d.open_settings()
        return
    if PAGE in ("body", "wardrobe"):
        unreal.GameplayStatics.get_actor_of_class(world, unreal.VisexBodyEditor).close()
    elif PAGE == "photo":
        unreal.GameplayStatics.get_actor_of_class(world, unreal.VisexPhotoMode).close()
    else:
        d.close_settings()


cap = rt = None
if SHOTS > 0:
    for c in pawn.get_components_by_class(unreal.SkeletalMeshComponent):
        c.set_editor_property("bounds_scale", 3.0)
    cap = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SceneCapture2D)[0]
    comp = cap.capture_component2d
    rt = unreal.RenderingLibrary.create_render_target2d(world, 2048, 1152, unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
    comp.set_editor_property("texture_target", rt)
    comp.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_TONE_CURVE_HDR)
    comp.set_editor_property("capture_every_frame", False)
    comp.set_editor_property("capture_on_movement", False)

# frame times to shoot: spread over SPAN, the first just after the click, the last at its end
times = [SPAN * (0.1 + 0.9 * i / max(1, SHOTS - 1)) for i in range(SHOTS)]
state = {"t": 0.0, "top": 0.0, "shot": 0, "h": None, "rows": []}


def shoot(i):
    cm = unreal.GameplayStatics.get_player_camera_manager(world, 0)
    comp = cap.capture_component2d
    comp.set_editor_property("fov_angle", cm.get_fov_angle())
    cap.set_actor_location_and_rotation(cm.get_camera_location(), cm.get_camera_rotation(), False, True)
    comp.capture_scene()
    name = "motion_%s_%s_%d.png" % (PAGE, DIR, i)
    unreal.RenderingLibrary.export_render_target(world, rt, OUT, name)


def tick(dt):
    if not LES.is_in_play_in_editor():
        unreal.unregister_slate_post_tick_callback(state["h"])
        return
    state["t"] += unreal.GameplayStatics.get_world_delta_seconds(world)
    v = pawn.get_velocity()
    state["top"] = max(state["top"], (v.x * v.x + v.y * v.y) ** 0.5)
    p, r = pawn.get_actor_location(), pawn.get_actor_rotation()
    state["rows"].append("%.3f,%.1f,%.1f,%.1f,%.0f" % (state["t"], p.x, p.y, r.yaw, (v.x * v.x + v.y * v.y) ** 0.5))
    while state["shot"] < SHOTS and state["t"] >= times[state["shot"]]:
        shoot(state["shot"])
        state["shot"] += 1
    if state["t"] >= max(SPAN, END):
        unreal.unregister_slate_post_tick_callback(state["h"])
        with open(os.path.join(OUT, "motion_%s_%s.csv" % (PAGE, DIR)), "w") as f:
            f.write("t,x,y,yaw,speed\n" + "\n".join(state["rows"]))
        unreal.log("Menu motion check: %s %s top speed %.0f cm/s, %d frames" % (PAGE, DIR, state["top"], state["shot"]))


go()
state["h"] = unreal.register_slate_post_tick_callback(tick)
print("started", PAGE, DIR, "shots", SHOTS)
