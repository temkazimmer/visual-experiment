"""Lvl_Dust — orange dust-fog plaza: steps on the left, colossal columns on the
right, monoliths fading into the haze. Rebuilds the level from scratch.

Run: UnrealEditor visex.uproject -run=pythonscript -script=Tools/build_dust.py -unattended
Then, live in the editor (via Tools/ue.py): import_sand_tex.py, bake_stone.py (twice: the
first run compiles the generator), add_sand.py, add_dunes.py, add_ruins.py; then dump
actors.json, run stone_geo.py outside the editor, and apply_geo.py.
"""
import unreal

LEVEL = "/Game/VisEx/Lvl_Dust"
EXPOSURE_EV = 2.0          # fixed EV100; raise to darken
FOG_COLOR = (2.0, 0.85, 0.12)
SAND = (0.42, 0.24, 0.12)

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
log = lambda m: unreal.log_warning("VISEX " + str(m))

if unreal.EditorAssetLibrary.does_asset_exist(LEVEL):
    unreal.EditorAssetLibrary.delete_asset(LEVEL)
les.new_level(LEVEL)

def spawn(cls, loc, yaw=0.0, pitch=0.0, roll=0.0, label=None):
    a = eas.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw))
    if label:
        a.set_actor_label(label)
    return a

def look_at(src, dst):
    return unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*src), unreal.Vector(*dst))

def setp(obj, names, value):
    """Set the first property name that exists (names shift between UE versions)."""
    for n in names if isinstance(names, (list, tuple)) else [names]:
        try:
            obj.set_editor_property(n, value)
            return
        except Exception:
            pass
    log("could not set %s on %s" % (names, obj.get_name()))

# --- material
mi_path = "/Game/VisEx/MI_Sandstone"
if not unreal.EditorAssetLibrary.does_asset_exist(mi_path):
    mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "MI_Sandstone", "/Game/VisEx", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial"))
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(*SAND, 1))
    unreal.EditorAssetLibrary.save_asset(mi_path)
SANDSTONE = unreal.load_asset(mi_path)
CUBE = unreal.load_asset("/Engine/BasicShapes/Cube")
CYL = unreal.load_asset("/Engine/BasicShapes/Cylinder")

def block(center, size, mesh=CUBE, label=None, yaw=0.0, pitch=0.0, roll=0.0):
    """Mesh actor sized in cm (basic shapes are 100 cm, pivot at centre)."""
    a = spawn(unreal.StaticMeshActor, center, yaw, pitch, roll, label)
    a.static_mesh_component.set_static_mesh(mesh)
    a.static_mesh_component.set_material(0, SANDSTONE)
    a.set_actor_scale3d(unreal.Vector(size[0] / 100, size[1] / 100, size[2] / 100))
    return a

# --- plaza: raised platform (top at z=0), lower ground to the left
block((15000, 15000, -500), (40000, 33000, 1000), label="Plaza")
block((15000, -12000, -1100), (40000, 21000, 200), label="LowerGround")

# --- steps descending from the plaza's left edge (y=-1500), comfortable for the
# mannequin: 20 cm risers, 220 cm treads. The flight drops 4 m and ends at y=-5900;
# add_dunes.py's sand ramp carries the last 6 m down to the lower ground.
STEPS, RISE, RUN = 20, 20, 220
for i in range(STEPS):
    top = -RISE * (i + 1)
    y = -1500 - RUN * i - RUN / 2
    block((15000, y, (top - 1000) / 2), (40000, RUN, 1000 + top), label="Step_%02d" % i)

# --- colossal columns on the right, each on a plinth
for i, x in enumerate(range(800, 30000, 1800)):
    block((x, 1900, 350), (1400, 1400, 700), label="Plinth_%02d" % i)
    block((x, 1900, 700 + 5000), (1000, 1000, 10000), mesh=CYL, label="Column_%02d" % i)
block((15000, 3600, 6000), (40000, 1200, 12000), label="ColonnadeWall")

# --- megastructures: a vast sloped mass on the left, monoliths deep in the haze
block((14000, -14000, 1500), (16000, 9000, 9000), roll=40, label="SlopedMass")
for i, (x, y, w, d, h) in enumerate([
        (22000, -3000, 5000, 5000, 18000), (30000, 2000, 7000, 6000, 26000),
        (26000, -12000, 6000, 9000, 14000), (38000, -6000, 9000, 9000, 32000),
        (20000, 9000, 6000, 4000, 22000)]):
    block((x, y, h / 2 - 1000), (w, d, h), label="Monolith_%d" % i)

# --- the model, small in the plaza for scale
model = spawn(unreal.SkeletalMeshActor, (2600, -1050, 0), yaw=150, label="Model")
smc = model.skeletal_mesh_component
smc.set_skinned_asset_and_update(unreal.load_asset("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple"))
smc.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_SINGLE_NODE)
anim = unreal.load_asset("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle")
smc.override_animation_data(anim, False, False, 0.0, 0.0)

# --- dust: NS_Dust is Epic's HangingParticulates emitter (made by hand — Python can't
# add emitters to a system); tiled as a strip of volumes along the walkable area
if unreal.EditorAssetLibrary.does_asset_exist("/Game/VisEx/NS_Dust"):
    ns = unreal.load_asset("/Game/VisEx/NS_Dust")
    i = 0
    for x in range(1200, 16000, 3300):
        for y in (-1300, 3000):
            d = eas.spawn_actor_from_object(ns, unreal.Vector(x, y, 250))
            d.set_actor_label("Dust_%02d" % i)
            d.set_actor_scale3d(unreal.Vector(8, 12, 4))
            i += 1

# --- sun: low, ahead of the camera, so the haze glows
sun = spawn(unreal.DirectionalLight, (0, 0, 2000), yaw=125, pitch=-22, label="Sun")
sc = sun.get_component_by_class(unreal.DirectionalLightComponent)
sc.set_intensity(10.0)
sc.set_light_color(unreal.LinearColor(1.0, 0.62, 0.32, 1))
setp(sc, ["atmosphere_sun_light"], True)
setp(sc, ["volumetric_scattering_intensity"], 2.0)

spawn(unreal.SkyAtmosphere, (0, 0, 0), label="SkyAtmosphere")
sky = spawn(unreal.SkyLight, (0, 0, 500), label="SkyLight")
skc = sky.get_component_by_class(unreal.SkyLightComponent)
setp(skc, ["real_time_capture"], True)
skc.set_light_color(unreal.LinearColor(1.0, 0.55, 0.3, 1))

# --- the fog does most of the work
fog = spawn(unreal.ExponentialHeightFog, (0, 0, -1100), label="DustFog")
fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
setp(fc, ["fog_density"], 0.035)
setp(fc, ["fog_height_falloff"], 0.02)
setp(fc, ["fog_inscattering_luminance", "fog_inscattering_color"], unreal.LinearColor(*FOG_COLOR, 1))
setp(fc, ["directional_inscattering_luminance", "directional_inscattering_color"], unreal.LinearColor(1.0, 0.55, 0.2, 1))
setp(fc, ["directional_inscattering_exponent"], 6.0)
setp(fc, ["enable_volumetric_fog", "volumetric_fog"], True)
setp(fc, ["volumetric_fog_albedo"], unreal.Color(r=255, g=150, b=70, a=255))
setp(fc, ["volumetric_fog_extinction_scale"], 1.5)
setp(fc, ["volumetric_fog_scattering_distribution"], 0.6)

# --- camera: low, wide, looking across the plaza into the haze
cam_pos = (-400, -1450, 520)
cam = spawn(unreal.CineCameraActor, cam_pos, label="PhotoCam")
cam.set_actor_rotation(look_at(cam_pos, (5000, -2300, 80)), False)
cc = cam.get_cine_camera_component()
cc.set_editor_property("current_focal_length", 24.0)
cc.set_editor_property("current_aperture", 5.6)
fs = cc.get_editor_property("focus_settings")
fs.set_editor_property("focus_method", unreal.CameraFocusMethod.MANUAL)
fs.set_editor_property("manual_focus_distance", 3900.0)
cc.set_editor_property("focus_settings", fs)
# play from the character; PhotoCam is for stills
spawn(unreal.PlayerStart, (-300, -1000, 100), yaw=-10, label="PlayerStart")

# --- post: fixed exposure, soft bloom, grain for the dust
ppv = spawn(unreal.PostProcessVolume, (0, 0, 0), label="PostProcess")
ppv.set_editor_property("unbound", True)
s = ppv.get_editor_property("settings")
for k, v in [("auto_exposure_min_brightness", EXPOSURE_EV), ("auto_exposure_max_brightness", EXPOSURE_EV),
             ("bloom_intensity", 1.2), ("vignette_intensity", 0.6),
             ("film_grain_intensity", 0.35), ("scene_fringe_intensity", 0.4)]:
    setp(s, "override_" + k, True)
    setp(s, k, v)
ppv.set_editor_property("settings", s)

log("saved %s" % les.save_current_level())
