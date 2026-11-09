"""In Play: dress, pose, reshape and walk the player pawn, and shoot her through PhotoCam.

  ( echo 'OUTFIT = "Noir"; SHOT = "front"'; cat Tools/char_play_shot.py ) > Tools/.bridge/j.py
  python3 Tools/ue.py Tools/.bridge/j.py        # writes Tools/.bridge/play_<SHOT>.png

Globals (all optional):
  OUTFIT  wardrobe name                     BODY   {slider: value}
  ANIM    asset path to loop in place       WALK   seconds of real walking (the AnimBP's walk)
  SHOT    file label; takes a HighResShot   RES    (2560, 1440)
  START   True: choose Start on the menu, so the menu is gone and she is playable
  CAMV    (forward, right, up, look-at height, fov) relative to her feet
The shot focuses on her (f/8), so a close frame shows the materials, not depth of field.
Nothing is saved.
"""
import os
import time
import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
if not world:
    raise RuntimeError("not in Play")
pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
comps = pawn.get_components_by_class(unreal.SkeletalMeshComponent)
body = [c for c in comps if c.get_class().get_name() != "VisexShellComponent"][0]
anim = globals().get("ANIM")
if anim:
    body.override_animation_data(unreal.load_asset(anim), True, True, 0.0, 1.0)
bc = pawn.get_component_by_class(unreal.VisexBodyComponent)
if globals().get("OUTFIT"):
    bc.set_outfit(globals()["OUTFIT"])
for k, v in globals().get("BODY", {}).items():
    bc.set_value(k, v)

if globals().get("START"):
    d = unreal.GameplayStatics.get_actor_of_class(world, unreal.VisexMenuDirector)
    if d and d.is_menu_open():
        d.start_game()

if globals().get("WALK"):
    _end = time.time() + float(WALK)
    _h = []

    def _walk(dt):
        w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not w or time.time() > _end:
            unreal.unregister_slate_post_tick_callback(_h[0])
            return
        p = unreal.GameplayStatics.get_player_pawn(w, 0)
        p.add_movement_input(p.get_actor_forward_vector(), 1.0, True)

    _h.append(unreal.register_slate_post_tick_callback(_walk))

name = globals().get("SHOT")
if name:
    f, r = pawn.get_actor_forward_vector(), pawn.get_actor_right_vector()
    hh = pawn.get_component_by_class(unreal.CapsuleComponent).get_scaled_capsule_half_height()
    feet = pawn.get_actor_location() - unreal.Vector(0, 0, hh)
    a, b, up, tz, fov = globals().get("CAMV", (330, 170, 95, 80, 30.0))
    loc = feet + f * a + r * b + unreal.Vector(0, 0, up)
    target = feet + unreal.Vector(0, 0, tz)
    rot = unreal.MathLibrary.find_look_at_rotation(loc, target)
    cams = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
    cam = next(c for c in cams if "Photo" in c.get_actor_label() or "Photo" in c.get_name())
    cam.set_actor_location_and_rotation(loc, rot, False, False)
    cam.attach_to_actor(pawn, "", unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD,
                        unreal.AttachmentRule.KEEP_WORLD, False)      # stays framed while she walks
    cc = cam.get_component_by_class(unreal.CameraComponent)
    cc.set_field_of_view(fov)
    if isinstance(cc, unreal.CineCameraComponent):
        fs = cc.get_editor_property("focus_settings")
        fs.set_editor_property("focus_method", unreal.CameraFocusMethod.MANUAL)
        fs.set_editor_property("manual_focus_distance", (target - loc).length())
        cc.set_editor_property("focus_settings", fs)
        cc.set_editor_property("current_aperture", 8.0)
    unreal.GameplayStatics.get_player_controller(world, 0).set_view_target_with_blend(cam, 0.0)
    w, h = globals().get("RES", (2560, 1440))
    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools", ".bridge", "play_%s.png" % name)
    unreal.AutomationLibrary.take_high_res_screenshot(w, h, out)
    print("shot", name, w, h, "speed", round(pawn.get_velocity().length()))
