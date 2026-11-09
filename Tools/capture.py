"""Render PhotoCam's view to Tools/.bridge/capture.png without using the editor viewport.
Run live: python3 Tools/ue.py Tools/capture.py
Set VIEW = ((x, y, z), (target x, y, z), fov) in the job's globals to shoot from elsewhere,
or CAM = "MenuCam" to shoot through another camera actor."""
import os
import unreal

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
cam = next(a for a in eas.get_all_level_actors() if a.get_actor_label() == globals().get("CAM", "PhotoCam"))
out_dir = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools", ".bridge")

rt = unreal.RenderingLibrary.create_render_target2d(world, 1920, 1080, unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
view = globals().get("VIEW")
if view:
    loc = unreal.Vector(*view[0])
    rot = unreal.MathLibrary.find_look_at_rotation(loc, unreal.Vector(*view[1]))
else:
    loc, rot = cam.get_actor_location(), cam.get_actor_rotation()
cap = eas.spawn_actor_from_class(unreal.SceneCapture2D, loc, rot)
c = cap.capture_component2d
c.set_editor_property("texture_target", rt)
c.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_TONE_CURVE_HDR)
c.set_editor_property("fov_angle", view[2] if view else cam.get_cine_camera_component().get_editor_property("field_of_view"))
c.capture_scene()
c.capture_scene()   # second pass lets Lumen settle
unreal.RenderingLibrary.export_render_target(world, rt, out_dir, "capture.png")
eas.destroy_actor(cap)
print("captured", os.path.join(out_dir, "capture.png"))
