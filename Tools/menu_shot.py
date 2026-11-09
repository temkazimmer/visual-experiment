"""Menu screen shot: the game boots into this frame. A low camera on the dune, the figure
right of centre against a plinth, fallen drums and the columns climbing out of frame
behind. The logo and menu sit on the left (see the menu panel), angled toward the figure.

Places MenuCam (CineCamera) and MenuModel (idle stand-in for the player in this spot).
Run live: python3 Tools/ue.py Tools/menu_shot.py
"""
import unreal

MODEL = globals().get("MODEL_O") or dict(loc=(9500, -900), yaw=165.0)   # z found on the sand
CAM = globals().get("CAM_O") or dict(offset=(-250, -700, 35), look_at_height=135, focal=28.0, fstop=2.8, yaw_off=-9.0)

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
by = {a.get_actor_label(): a for a in eas.get_all_level_actors()}


# the stand-in and camera must not be found as "ground" on re-runs
IGNORE = [a for l, a in by.items() if l in ("MenuModel", "MenuCam")]


def ground(x, y):
    hit = unreal.SystemLibrary.line_trace_single(w, unreal.Vector(x, y, 8000), unreal.Vector(x, y, -3000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, IGNORE,
                                                 unreal.DrawDebugTrace.NONE, True)
    if hit is None:                      # collision not cooked yet (e.g. right after a restart)
        return None
    t = hit.to_tuple()
    return t[4].z if t[0] else None


mx, my = MODEL["loc"]
mz = ground(mx, my)
old = by.get("MenuModel")
if mz is None:
    mz = old.get_actor_location().z if old else 0.0
model = by.get("MenuModel")
if not model:
    model = eas.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(mx, my, mz))
    model.set_actor_label("MenuModel")
    smc = model.skeletal_mesh_component
    smc.set_skinned_asset_and_update(unreal.load_asset("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple"))
    smc.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    smc.set_editor_property("animation_data", unreal.SingleAnimationPlayData(
        anim_to_play=unreal.load_asset("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle"), saved_looping=True, saved_playing=True))
# it marks the spot the player stands on: no collision, or it blocks the player's camera boom
model.set_actor_enable_collision(False)
model.skeletal_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
model.set_actor_location_and_rotation(unreal.Vector(mx, my, mz), unreal.Rotator(roll=0, pitch=0, yaw=MODEL["yaw"]), False, False)

ox, oy, oz = CAM["offset"]
gz = ground(mx + ox, my + oy)
if gz is None:
    gz = by["MenuCam"].get_actor_location().z - oz if "MenuCam" in by else mz
cl = unreal.Vector(mx + ox, my + oy, gz + oz)
target = unreal.Vector(mx, my, mz + CAM["look_at_height"])
cam = by.get("MenuCam") or eas.spawn_actor_from_class(unreal.CineCameraActor, cl)
cam.set_actor_label("MenuCam")
rot = unreal.MathLibrary.find_look_at_rotation(cl, target)
rot.yaw += CAM.get("yaw_off", 0.0)          # aim left of the figure: it sits right, the menu gets the left
cam.set_actor_location_and_rotation(cl, rot, False, False)
cc = cam.get_cine_camera_component()
cc.set_editor_property("current_focal_length", CAM["focal"])
cc.set_editor_property("current_aperture", CAM["fstop"])
fs = cc.get_editor_property("focus_settings")
fs.set_editor_property("focus_method", unreal.CameraFocusMethod.MANUAL)
fs.set_editor_property("manual_focus_distance", (target - cl).length())
cc.set_editor_property("focus_settings", fs)
print("MenuModel at", [round(v) for v in (mx, my, mz)], "MenuCam at", [round(v) for v in (cl.x, cl.y, cl.z)],
      "fov", round(cc.get_editor_property("field_of_view"), 1))
