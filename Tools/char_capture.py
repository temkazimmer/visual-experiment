"""Shoot the player character in Lvl_Dust without Play mode or saving the level.

  python3 Tools/ue.py Tools/char_capture.py      # writes Tools/.bridge/char_<shot>.png

Spawns a temporary BP_ThirdPersonCharacter where MenuModel stands (the spot the menu
puts the player), poses it with POSE, hides MenuModel, renders each SHOTS entry through
Tools/capture.py, then removes the character and unhides MenuModel. The level is left
dirty but unchanged: don't save it.

Shots (set SHOTS in the job globals to pick): menu (MenuCam), backlit (close, facing
the sun so the skin is lit from behind), fog (25 m out, deep in the dust).
"""
import math
import os
import shutil
import unreal

BP_PATH = "/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"
POSE = globals().get("POSE", "/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle")
SHOTS = globals().get("SHOTS", ["menu", "backlit", "fog"])

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
by = {a.get_actor_label(): a for a in eas.get_all_level_actors()}
stand = by["MenuModel"]
root = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools")
capture_src = open(os.path.join(root, "capture.py")).read()

bp = unreal.load_asset(BP_PATH)
hh = unreal.get_default_object(bp.generated_class()).get_editor_property("capsule_component").get_unscaled_capsule_half_height()
loc = stand.get_actor_location() + unreal.Vector(0, 0, hh)
yaw = stand.get_actor_rotation().yaw + 90.0
char = eas.spawn_actor_from_class(bp.generated_class(), loc, unreal.Rotator(roll=0, pitch=0, yaw=yaw))
body = char.get_component_by_class(unreal.SkeletalMeshComponent)
for c in char.get_components_by_class(unreal.SkeletalMeshComponent):
    if c.get_class().get_name() != "VisexShellComponent":
        body = c
body.override_animation_data(unreal.load_asset(POSE))
was_hidden = stand.get_editor_property("hidden")
stand.set_actor_hidden_in_game(True)
stand.set_is_temporarily_hidden_in_editor(True)

sun = next(a for a in by.values() if isinstance(a, unreal.DirectionalLight))
light_dir = sun.get_actor_forward_vector()          # direction the light travels
chest = loc + unreal.Vector(0, 0, 30)


def view_for(shot):
    if shot == "backlit":   # downstream of the light, looking back toward the sun
        d = unreal.Vector(light_dir.x, light_dir.y, 0).normal()
        cam = chest + d * 320 + unreal.Vector(0, 0, -20)
        return ((cam.x, cam.y, cam.z), (chest.x, chest.y, chest.z), 35.0)
    if shot == "fog":       # side-on, far back in the dust
        f = char.get_actor_right_vector() * -1.0   # away from the menu panel
        cam = chest + f * 3000 + unreal.Vector(0, 0, 80)
        return ((cam.x, cam.y, cam.z), (chest.x, chest.y, chest.z + 60), 14.0)
    return None


try:
    for shot in SHOTS:
        g = {"__name__": "__bridge__"}
        v = view_for(shot)
        if v:
            g["VIEW"] = v
        else:
            g["CAM"] = "MenuCam"
        exec(compile(capture_src, "capture.py", "exec"), g)
        dst = os.path.join(root, ".bridge", "char_%s.png" % shot)
        shutil.move(os.path.join(root, ".bridge", "capture.png"), dst)
        print("shot", dst)
finally:
    eas.destroy_actor(char)
    stand.set_actor_hidden_in_game(was_hidden)
    stand.set_is_temporarily_hidden_in_editor(False)
