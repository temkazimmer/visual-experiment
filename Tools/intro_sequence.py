"""Build /Game/VisEx/Intro/LS_Intro: the startup film, her bare feet climbing the dune ridge.

Run live: python3 Tools/ue.py Tools/intro_sequence.py   (rebuilds the sequence from scratch;
Lvl_Dust is only read: everything lives in the sequence as spawnables, plus a possessable
binding on the Sun for the low raking light. Don't save the level.)

Timeline (24 fps): 0-2 s is pre-roll so she is already mid-stride; the film is 2 s to END.
She walks +x up the slope at y = -4000 (crest ~x 13150); the low sun is ahead-right, so the
camera sits behind-left of her near the sand and looks into it: feet rim-lit, shadows long
toward the lens. The camera tracks her feet, then lets her go, tilting up as she crests.
"""
import math
import unreal

PATH, NAME = "/Game/VisEx/Intro", "LS_Intro"
FPS = 24
PRE = 2.0                 # s of pre-roll before the film starts
FILM = 11.5               # s of film
END = PRE + FILM
X0, Y = 11500.0, -4000.0  # where she starts (t = 0)
WALK = 0.75               # x MaxWalkSpeed 190 -> ~142 cm/s
SPEED = 190.0 * WALK
SUN_PITCH, SUN_YAW = -7.0, 125.0
HAZE = (0.92, 0.62, 0.36)  # fade target; matched to the fog in-scattering later

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = {a.get_actor_label(): a for a in eas.get_all_level_actors()}


def ground(x, y):
    hits = unreal.SystemLibrary.line_trace_multi(
        world, unreal.Vector(x, y, 5000), unreal.Vector(x, y, -5000),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
    for h in hits:
        t = h.to_tuple()
        if t[9] and t[9].get_actor_label() == "Dunes":
            return t[4].z
    return 0.0


def her_x(t):
    return X0 + SPEED * max(0.0, t - 0.15)   # ~0.15 s lost to acceleration


def fr(t):
    return unreal.FrameNumber(int(round(t * FPS)))


def section_for(track, t0=0.0, t1=END):
    s = track.add_section()
    s.set_range(fr(t0).value, fr(t1).value)
    return s


def key_float(track, keys):
    ch = section_for(track).get_all_channels()[0]
    for t, v in keys:
        ch.add_key(fr(t), float(v), interpolation=unreal.MovieSceneKeyInterpolation.AUTO)


def key_transform(section, keys):
    """keys: (t, (x, y, z), (roll, pitch, yaw))"""
    ch = section.get_all_channels()
    for t, loc, rot in keys:
        for i, v in enumerate(list(loc) + list(rot)):
            ch[i].add_key(fr(t), float(v), interpolation=unreal.MovieSceneKeyInterpolation.AUTO)


# fresh sequence (cleared in place: deleting an open sequence hangs the editor)
unreal.LevelSequenceEditorBlueprintLibrary.close_level_sequence()
for a in eas.get_all_level_actors():
    if a.get_actor_label().startswith("CineCameraActor"):
        eas.destroy_actor(a)        # leftovers of earlier camera probes (unsaved)
seq = unreal.load_asset(PATH + "/" + NAME)
if seq:
    for b in seq.get_bindings():
        b.remove()
    for tr in seq.get_tracks():
        seq.remove_track(tr)
else:
    seq = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        NAME, PATH, unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
seq.set_display_rate(unreal.FrameRate(FPS, 1))
seq.set_playback_start(fr(PRE).value)   # the film; MRQ warms up on the camera cut's excess before it
seq.set_playback_end(fr(END).value)
unreal.LevelSequenceEditorBlueprintLibrary.open_level_sequence(seq)
les = unreal.get_editor_subsystem(unreal.LevelSequenceEditorSubsystem)

# her: the rig spawns and walks the player character in the render world
rig = seq.add_spawnable_from_class(unreal.load_class(None, "/Script/visex.VisexIntroRig"))
rig.set_display_name("Her (rig)")
key_transform(section_for(rig.add_track(unreal.MovieScene3DTransformTrack)),
              [(0.0, (X0, Y, ground(X0, Y)), (0, 0, 0))])
wt = rig.add_track(unreal.MovieSceneFloatTrack)
wt.set_property_name_and_path("Walk", "Walk")
key_float(wt, [(0.0, WALK), (END, WALK)])
section_for(rig.add_track(unreal.MovieSceneSpawnTrack)).get_all_channels()[0].add_key(fr(0), True)

# hero sand: 1 cm Nanite relief over the dune along her path
sand = seq.add_spawnable_from_class(unreal.StaticMeshActor)
sand.set_display_name("Hero sand")
tmpl = sand.get_object_template()
tmpl.static_mesh_component.set_static_mesh(unreal.load_asset(PATH + "/SM_IntroSand"))
tmpl.static_mesh_component.set_collision_profile_name("NoCollision")
key_transform(section_for(sand.add_track(unreal.MovieScene3DTransformTrack)),
              [(0.0, (12500, -4000, 0), (0, 0, 0))])
section_for(sand.add_track(unreal.MovieSceneSpawnTrack)).get_all_channels()[0].add_key(fr(0), True)

# sun: low and raking, from ahead-right of her walk
sun = seq.add_possessable(actors["Sun"])
sl = actors["Sun"].get_actor_location()
key_transform(section_for(sun.add_track(unreal.MovieScene3DTransformTrack)),
              [(0.0, (sl.x, sl.y, sl.z), (0, SUN_PITCH, SUN_YAW))])

# camera: behind-left of her, a hand's height over the sand, looking into the sun
OFF = (-75.0, 95.0)        # camera offset from her ankles (x behind, +y her left)
STOP = PRE + 6.5           # camera stops tracking here; she walks on to the crest
cam_binding, cam_actor = les.create_camera(True)
cam_binding.set_display_name("IntroCam")
cam_keys, lens_keys, focus_keys = [], [], []
cam_stop_x = her_x(STOP) + OFF[0] + 60.0
t = 0.0
while t <= END + 1e-6:
    hx = her_x(t)
    if t <= STOP:
        cx = hx + OFF[0]
    else:   # ease the dolly to a stop over 1.5 s
        u = min(1.0, (t - STOP) / 1.5)
        cx = her_x(STOP) + OFF[0] + 60.0 * (1 - (1 - u) ** 2)
    cy = Y + OFF[1]
    lift = 10.0 + 22.0 * max(0.0, min(1.0, (t - STOP) / 4.0)) ** 2
    cz = ground(cx, cy) + lift
    # aim: her ankles, rising to her hips / head as she crests and the camera tilts up
    rise = max(0.0, min(1.0, (t - STOP - 0.5) / 4.0))
    rise = rise * rise * (3 - 2 * rise)
    tx, tz = hx + 12.0, ground(hx, Y) + 8.0 + 120.0 * rise
    dx, dy, dz = tx - cx, Y - cy, tz - cz
    yaw = math.degrees(math.atan2(dy, dx))
    pitch = math.degrees(math.atan2(dz, math.hypot(dx, dy)))
    cam_keys.append((t, (cx, cy, cz), (0.0, pitch, yaw)))
    lens_keys.append((t, 85.0 - 35.0 * rise))
    focus_keys.append((t, math.sqrt(dx * dx + dy * dy + dz * dz)))
    t += 0.25
for tr in cam_binding.get_tracks():
    if isinstance(tr, unreal.MovieScene3DTransformTrack):
        cam_binding.remove_track(tr)
key_transform(section_for(cam_binding.add_track(unreal.MovieScene3DTransformTrack)), cam_keys)

comp = next(b for b in seq.get_bindings() if b.get_display_name() == "CameraComponent")
for tr in comp.get_tracks():
    comp.remove_track(tr)
cc = cam_actor.get_cine_camera_component()
cc.set_editor_property("filmback", unreal.CameraFilmbackSettings(sensor_width=36.0, sensor_height=20.25))
fs = cc.get_editor_property("focus_settings")
fs.set_editor_property("focus_method", unreal.CameraFocusMethod.MANUAL)
cc.set_editor_property("focus_settings", fs)
for prop, path, keys in (("CurrentFocalLength", "CurrentFocalLength", lens_keys),
                         ("CurrentAperture", "CurrentAperture", [(0.0, 1.8), (END, 2.8)]),
                         ("ManualFocusDistance", "FocusSettings.ManualFocusDistance", focus_keys)):
    tr = comp.add_track(unreal.MovieSceneFloatTrack)
    tr.set_property_name_and_path(prop, path)
    key_float(tr, keys)
les.save_default_spawnable_state(cam_binding)   # the template keeps the 16:9 filmback + manual focus

# camera cut over the whole film, and a fade to the haze colour at the end
cut_track = next(t for t in seq.get_tracks() if isinstance(t, unreal.MovieSceneCameraCutTrack))
for s in cut_track.get_sections():
    s.set_range(0, fr(END).value)   # starts before playback: MRQ runs that pre-roll as warm-up
fade = seq.add_track(unreal.MovieSceneFadeTrack)
fs_ = section_for(fade)
fs_.set_editor_property("fade_color", unreal.LinearColor(*HAZE, 1.0))
fch = fs_.get_all_channels()[0]
for t_, v_ in [(0.0, 1.0), (PRE + 0.05, 1.0), (PRE + 1.2, 0.0), (END - 1.6, 0.0), (END, 1.0)]:
    fch.add_key(fr(t_), v_, interpolation=unreal.MovieSceneKeyInterpolation.AUTO)

unreal.EditorAssetLibrary.save_loaded_asset(seq)
print("LS_Intro", len(cam_keys), "camera keys; she crests ~", round((13150 - X0) / SPEED + 0.15 - PRE, 1),
      "s into the film; camera stops at x", round(cam_stop_x))
