"""In Play: drive photo mode's motion sequence and shoot it through the photo camera.

  python3 Tools/ue.py -c "ACT='open'; exec(open('Tools/char_photoseq_play.py').read())"
  ACT='motion'; MOTION=1          # SetMotion(1)
  ACT='shot'; NAME='sit'          # Tools/.bridge/shots/photoseq_<NAME>.png, 2K, through the photo camera
  ACT='info'                      # motion index, her location and speed
Needs a SceneCapture2D placed in the level before Play (Python can't spawn into the game
world). Nothing is saved.
"""
import os
import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
if not world:
    raise RuntimeError("not in Play")
pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
photo = unreal.GameplayStatics.get_actor_of_class(world, unreal.VisexPhotoMode)
ACT = globals().get("ACT", "info")
if ACT == "open":
    d = unreal.GameplayStatics.get_actor_of_class(world, unreal.VisexMenuDirector)
    d.on_item_chosen(3)                                   # ItemPhoto
elif ACT == "motion":
    photo.set_motion(int(MOTION))
elif ACT == "info":
    v = pawn.get_velocity()
    print("motion", photo.get_motion() if photo else None, "loc", pawn.get_actor_location(), "speed %.0f" % (v.x * v.x + v.y * v.y) ** 0.5)
elif ACT == "shot":
    for c in pawn.get_components_by_class(unreal.SkeletalMeshComponent):
        c.set_editor_property("bounds_scale", 3.0)
    photo.set_hud_visible(False)
    cam = unreal.GameplayStatics.get_player_camera_manager(world, 0)
    cap = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SceneCapture2D)[0]
    comp = cap.capture_component2d
    rt = unreal.RenderingLibrary.create_render_target2d(world, 2048, 1152, unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
    comp.set_editor_property("texture_target", rt)
    comp.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_TONE_CURVE_HDR)
    comp.set_editor_property("capture_every_frame", False)
    comp.set_editor_property("fov_angle", cam.get_fov_angle())
    cap.set_actor_location_and_rotation(cam.get_camera_location(), cam.get_camera_rotation(), False, True)
    comp.capture_scene()
    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools", ".bridge", "shots")
    os.makedirs(out, exist_ok=True)
    unreal.RenderingLibrary.export_render_target(world, rt, out, "photoseq_%s.png" % NAME)
    photo.set_hud_visible(True)
    print("shot", os.path.join(out, "photoseq_%s.png" % NAME))
