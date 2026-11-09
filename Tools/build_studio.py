import unreal

LEVEL = "/Game/VisEx/Lvl_Studio"
log = lambda m: unreal.log("VISEX " + str(m))

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if unreal.EditorAssetLibrary.does_asset_exist(LEVEL):
    unreal.EditorAssetLibrary.delete_asset(LEVEL)
log("new_level %s" % les.new_level(LEVEL))

def spawn(cls, loc, rot=(0, 0, 0), label=None):  # rot = (roll, pitch, yaw)
    a = eas.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(roll=rot[0], pitch=rot[1], yaw=rot[2]))
    if label:
        a.set_actor_label(label)
    return a

def look_at(src, dst):
    return unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*src), unreal.Vector(*dst))

# --- backdrop material: dark warm grey instance of the basic shape material
mi_path = "/Game/VisEx/MI_Backdrop"
if not unreal.EditorAssetLibrary.does_asset_exist(mi_path):
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mi = tools.create_asset("MI_Backdrop", "/Game/VisEx", unreal.MaterialInstanceConstant,
                            unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial"))
    log("params %s" % [str(p) for p in unreal.MaterialEditingLibrary.get_vector_parameter_names(mi)])
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
        mi, "Color", unreal.LinearColor(0.05, 0.045, 0.045, 1))
    unreal.EditorAssetLibrary.save_asset(mi_path)
mi = unreal.load_asset(mi_path)

# --- cyclorama: floor + back wall
plane = unreal.load_asset("/Engine/BasicShapes/Plane")
cube = unreal.load_asset("/Engine/BasicShapes/Cube")
floor = spawn(unreal.StaticMeshActor, (0, 0, 0), label="Floor")
floor.static_mesh_component.set_static_mesh(plane)
floor.set_actor_scale3d(unreal.Vector(60, 60, 1))
wall = spawn(unreal.StaticMeshActor, (-700, 0, 1000), label="Backdrop")
wall.static_mesh_component.set_static_mesh(cube)
wall.set_actor_scale3d(unreal.Vector(0.2, 60, 20))
for a in (floor, wall):
    a.static_mesh_component.set_material(0, mi)

# --- the model: Quinn, frozen mid-move
model = spawn(unreal.SkeletalMeshActor, (0, 0, 0), (0, 0, -50), label="Model")
smc = model.skeletal_mesh_component
smc.set_skinned_asset_and_update(unreal.load_asset("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple"))
smc.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_SINGLE_NODE)
anim = unreal.load_asset("/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd")
smc.override_animation_data(anim, False, False, anim.get_play_length() * 0.25, 0.0)
unreal.log_warning("VISEX pose %s @ %.2fs" % (anim.get_name(), anim.get_play_length() * 0.25))

# --- lights: warm key front-left, cool rim behind-right, soft fill
target = (0, 0, 110)
key_pos = (300, 260, 260)
key = spawn(unreal.RectLight, key_pos, label="Key")
key.set_actor_rotation(look_at(key_pos, target), False)
kc = key.get_component_by_class(unreal.RectLightComponent)
kc.set_intensity(120.0)
kc.set_light_color(unreal.LinearColor(1.0, 0.85, 0.7, 1))
kc.set_editor_property("source_width", 150.0)
kc.set_editor_property("source_height", 150.0)

rim_pos = (-350, -250, 300)
rim = spawn(unreal.SpotLight, rim_pos, label="Rim")
rim.set_actor_rotation(look_at(rim_pos, (0, 0, 150)), False)
rc = rim.get_component_by_class(unreal.SpotLightComponent)
rc.set_intensity(80.0)
rc.set_light_color(unreal.LinearColor(0.5, 0.7, 1.0, 1))
rc.set_editor_property("outer_cone_angle", 18.0)

fill_pos = (350, -300, 150)
fill = spawn(unreal.RectLight, fill_pos, label="Fill")
fill.set_actor_rotation(look_at(fill_pos, target), False)
fill.get_component_by_class(unreal.RectLightComponent).set_intensity(15.0)

# --- backdrop glow: a soft pool of light on the wall behind the model
glow_pos = (-250, 0, 40)
glow = spawn(unreal.SpotLight, glow_pos, label="BackdropGlow")
glow.set_actor_rotation(look_at(glow_pos, (-700, 0, 220)), False)
gc = glow.get_component_by_class(unreal.SpotLightComponent)
gc.set_intensity(60.0)
gc.set_light_color(unreal.LinearColor(1.0, 0.45, 0.35, 1))
gc.set_editor_property("outer_cone_angle", 45.0)
gc.set_editor_property("cast_shadows", False)

# --- camera: 35mm, waist-high, auto-activates as the player's view
cam_pos = (640, 140, 100)
cam = spawn(unreal.CineCameraActor, cam_pos, label="PhotoCam")
cam.set_actor_rotation(look_at(cam_pos, (0, 0, 88)), False)
cc = cam.get_cine_camera_component()
cc.set_editor_property("current_focal_length", 35.0)
cc.set_editor_property("current_aperture", 2.8)
fs = cc.get_editor_property("focus_settings")
fs.set_editor_property("focus_method", unreal.CameraFocusMethod.MANUAL)
fs.set_editor_property("manual_focus_distance", 655.0)
cc.set_editor_property("focus_settings", fs)
cam.set_editor_property("auto_activate_for_player", unreal.AutoReceiveInput.PLAYER0)

# --- player spawns behind the camera, out of shot
spawn(unreal.PlayerStart, (1200, 0, 100), label="PlayerStart")

# --- exposure: lock it so the shot is repeatable
ppv = spawn(unreal.PostProcessVolume, (0, 0, 0), label="PostProcess")
ppv.set_editor_property("unbound", True)
s = ppv.get_editor_property("settings")
s.set_editor_property("override_auto_exposure_min_brightness", True)
s.set_editor_property("auto_exposure_min_brightness", 1.0)
s.set_editor_property("override_auto_exposure_max_brightness", True)
s.set_editor_property("auto_exposure_max_brightness", 1.0)
s.set_editor_property("override_vignette_intensity", True)
s.set_editor_property("vignette_intensity", 0.5)
ppv.set_editor_property("settings", s)

log("save %s" % les.save_current_level())
