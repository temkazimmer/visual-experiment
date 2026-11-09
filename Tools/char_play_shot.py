"""In Play: dress, pose and reshape the player pawn, and shoot it through PhotoCam.

  ( echo 'OUTFIT = "Sheer"; SHOT = "front"'; cat Tools/char_play_shot.py ) > Tools/.bridge/j.py
  python3 Tools/ue.py Tools/.bridge/j.py        # writes Tools/.bridge/play_<SHOT>.png

Globals: OUTFIT (wardrobe name), ANIM (asset path), BODY ({slider: value}),
SHOT (file label), CAMV (forward, right, up, look-at height, fov). Nothing is saved."""
import os, unreal
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
if not world:
    raise RuntimeError("not in Play")
pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
comps = pawn.get_components_by_class(unreal.SkeletalMeshComponent)
print([(c.get_name(), c.get_skeletal_mesh_asset().get_name()) for c in comps])
body = [c for c in comps if c.get_class().get_name() != "VisexShellComponent"][0]
anim = globals().get("ANIM")
if anim:
    body.override_animation_data(unreal.load_asset(anim), True, True, 0.0, 1.0)
bc = pawn.get_component_by_class(unreal.VisexBodyComponent)
if globals().get("OUTFIT"):
    bc.set_outfit(globals()["OUTFIT"])
for k, v in globals().get("BODY", {}).items():
    bc.set_value(k, v)
name = globals().get("SHOT")
if name:
    f = pawn.get_actor_forward_vector(); r = pawn.get_actor_right_vector()
    hh = pawn.get_component_by_class(unreal.CapsuleComponent).get_scaled_capsule_half_height()
    feet = pawn.get_actor_location() - unreal.Vector(0, 0, hh)
    a, b, up, tz, fov = globals().get("CAMV", (330, 170, 95, 80, 30.0))
    loc = feet + f * a + r * b + unreal.Vector(0, 0, up)
    rot = unreal.MathLibrary.find_look_at_rotation(loc, feet + unreal.Vector(0, 0, tz))
    cams = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
    cam = next(c for c in cams if "Photo" in c.get_actor_label() or "Photo" in c.get_name())
    cam.set_actor_location_and_rotation(loc, rot, False, False)
    cc = cam.get_component_by_class(unreal.CameraComponent)
    cc.set_field_of_view(fov)
    unreal.GameplayStatics.get_player_controller(world, 0).set_view_target_with_blend(cam, 0.0)
    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools", ".bridge", "play_%s.png" % name)
    unreal.AutomationLibrary.take_high_res_screenshot(1920, 1080, out)
    print("shot", name, [c.get_actor_label() for c in cams])
