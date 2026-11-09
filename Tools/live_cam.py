"""Move PhotoCam, pilot the viewport through it, and save a shot to Tools/.bridge/shot.png.
Edit CAM / TARGET / FOCAL and run: python3 Tools/ue.py Tools/live_cam.py"""
import os
import unreal

CAM = (-400, -1450, 520)
TARGET = (5000, -2300, 80)
FOCAL = 24.0

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
cam = next(a for a in eas.get_all_level_actors() if a.get_actor_label() == "PhotoCam")
cam.set_actor_location_and_rotation(
    unreal.Vector(*CAM),
    unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*CAM), unreal.Vector(*TARGET)), False, False)
cc = cam.get_cine_camera_component()
cc.set_editor_property("current_focal_length", FOCAL)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).pilot_level_actor(cam)

shot = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools", ".bridge", "shot.png")
unreal.AutomationLibrary.take_high_res_screenshot(1920, 1080, shot, camera=cam)
print("camera at", CAM, "->", TARGET, "@", FOCAL, "mm; shot:", shot)
