"""Build /Game/VisEx/Intro/LS_Intro: the 7 s startup film (Tema, take 2).

One take. Out of black, close and low on her left: her bare feet climb the last treads below
the menu and step onto the plaza sand. Then the camera slows to a stop and zooms out on the
menu's own sensor while she walks on, away from the lens, into the menu framing: the last
frame IS the start of the live arrival (main's arrival-walk), same camera, lens and focus,
drift held, no panel, her mid-stride on the same line and at the same pace. The game takes
over from there: she walks to the column, turns and leans.

Run live: python3 Tools/ue.py Tools/intro_sequence.py   (rebuilds LS_Intro in place;
everything is a spawnable in the sequence; don't save Lvl_Dust.)

Timeline (24 fps): 0-4 s pre-roll (MRQ warms up on the camera cut's excess), film 4-11 s.
"""
import math
import unreal

PATH, NAME = "/Game/VisEx/Intro", "LS_Intro"
FPS, PRE, FILM = 24, 4.0, 7.0          # 4 s pre-roll: the scene loads and settles before frame 1
END = PRE + FILM
WALK = 0.6                             # x MaxWalkSpeed 190 -> ~114 cm/s: calm (Tema)
SPEED = 190.0 * WALK
WALK_T0 = PRE - 2.0                    # she is mid-stride when the film starts

# the hand-off: where she is (mid-stride, heading HEAD) on the film's last frame, and the
# camera there = the menu camera as the director stages it (read in Play). PROVISIONAL until
# main's arrival reports its own frame 0.
ARRIVE = (9560.0, -1010.0)             # in the menu frame, ~2.6 m out, a little right of centre
HEAD = 75.0
MENU_CAM = ((9303.64, -932.01, 69.31), (0.0, 18.445, -6.09))   # loc, (roll, pitch, yaw)
MENU_FOCAL, MENU_FSTOP, MENU_FOCUS = 45.0, 2.8, 226.1
SENSOR = (71.28, 71.28 / 1.6)          # the menu's scaled sensor width; 16:10 like a Mac screen
SETTLE = PRE + 4.5                     # the dolly starts to stop and the lens to open
D = (math.cos(math.radians(HEAD)), math.sin(math.radians(HEAD)))
L = (-D[1], D[0])                      # her left
DRUM = ((9575.8, -849.1, 108.0), -15.0, 1.011)   # the menu's lean column, as the director places it

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = {a.get_actor_label(): a for a in eas.get_all_level_actors()}
IGNORE = [a for a in actors.values() if a.get_actor_label().startswith("Dunes_Bound") or a.get_actor_label() == "StepRamp"]


def ground(x, y):
    r = unreal.SystemLibrary.line_trace_single(
        world, unreal.Vector(x, y, 3000), unreal.Vector(x, y, -3000),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, IGNORE, unreal.DrawDebugTrace.NONE, True)
    return r.to_tuple()[4].z if r and r.to_tuple()[0] else 0.0


# right after the editor starts, the dunes' collision isn't there yet and every trace falls
# through to the lower ground (-1000): cameras and walkers end up under the sand
_probe = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(8000, -3000, 3000), unreal.Vector(8000, -3000, -3000),
                                                unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, IGNORE, unreal.DrawDebugTrace.NONE, True)
if not (_probe and _probe.to_tuple()[0] and _probe.to_tuple()[9].get_actor_label() == "Dunes"):
    raise RuntimeError("dune collision not ready yet: run again in a few seconds")


def fr(t):
    return unreal.FrameNumber(int(round(t * FPS)))


def along(p, s):
    return (p[0] + D[0] * s, p[1] + D[1] * s)


def section_for(track, t0=0.0, t1=END):
    s = track.add_section()
    s.set_range(fr(t0).value, fr(t1).value)
    return s


def key_channel(ch, keys):
    for t, v in keys:
        ch.add_key(fr(t), v, interpolation=unreal.MovieSceneKeyInterpolation.AUTO)


def key_transform(section, keys):
    """keys: (t, (x, y, z), (roll, pitch, yaw)[, scale])"""
    ch = section.get_all_channels()
    for k in keys:
        t, loc, rot = k[:3]
        vals = list(loc) + list(rot) + ([k[3]] * 3 if len(k) > 3 else [])
        for i, v in enumerate(vals):
            ch[i].add_key(fr(t), float(v), interpolation=unreal.MovieSceneKeyInterpolation.AUTO)


def spawnable(cls, name, t0=0.0, t1=END):
    b = seq.add_spawnable_from_class(cls)
    b.set_display_name(name)
    # explicit keys: a lone True key reads as spawned from the very start
    ch = section_for(b.add_track(unreal.MovieSceneSpawnTrack), 0.0, END).get_all_channels()[0]
    if t0 > 0:
        ch.add_key(fr(0.0), False)
    ch.add_key(fr(t0), True)
    if t1 < END:
        ch.add_key(fr(t1), False)
    return b


def place(b, loc, yaw=0.0, scale=1.0, t0=0.0, t1=END):
    key_transform(section_for(b.add_track(unreal.MovieScene3DTransformTrack), t0, t1),
                  [(t0, loc, (0, 0, yaw), scale)])


def rig(name, start, walk_keys, show_keys, player, extra=()):
    b = spawnable(unreal.load_class(None, "/Script/visex.VisexIntroRig"), name)
    b.get_object_template().set_editor_property("auto_player", player)
    place(b, (start[0], start[1], ground(*start)), HEAD, 1.0)
    for prop, keys in (("Walk", walk_keys), ("Show", show_keys)) + tuple(extra):
        tr = b.add_track(unreal.MovieSceneFloatTrack)
        tr.set_property_name_and_path(prop, prop)
        ch = section_for(tr).get_all_channels()[0]
        for t, v in keys:   # stepped: on/off, no ramps between keys
            ch.add_key(fr(t), float(v), interpolation=unreal.MovieSceneKeyInterpolation.CONSTANT)
    return b


def mesh(name, path, loc, yaw, scale, t0=0.0, t1=END):
    b = spawnable(unreal.StaticMeshActor, name, t0, t1)
    smc = b.get_object_template().static_mesh_component
    smc.set_static_mesh(unreal.load_asset(path))
    smc.set_collision_profile_name("NoCollision")
    place(b, loc, yaw, scale, t0, t1)
    return b


# ---- fresh sequence (cleared in place: deleting an open sequence hangs the editor)
unreal.LevelSequenceEditorBlueprintLibrary.close_level_sequence()
for a in eas.get_all_level_actors():
    if a.get_actor_label().startswith("CineCameraActor"):
        eas.destroy_actor(a)
seq = unreal.load_asset(PATH + "/" + NAME)
for b in seq.get_bindings():
    b.remove()
for tr in seq.get_tracks():
    seq.remove_track(tr)
seq.set_display_rate(unreal.FrameRate(FPS, 1))
seq.set_playback_start(fr(PRE).value)
seq.set_playback_end(fr(END).value)
unreal.LevelSequenceEditorBlueprintLibrary.open_level_sequence(seq)
les = unreal.get_editor_subsystem(unreal.LevelSequenceEditorSubsystem)


def her(t):
    """Her (x, y) at time t: a straight line at SPEED, through ARRIVE at END."""
    return along(ARRIVE, -SPEED * (END - max(t, WALK_T0 + 0.15)))


START = her(0.0)

# ---- her: one walker, on camera from the start (player 0: the footprints follow her)
rig("Her", START, [(0.0, 0.0), (WALK_T0, WALK)], [(0.0, 0.0), (WALK_T0, 1.0)], True)

# ---- light: the game sun as is (the menu matches), but SlopedMass, 175 m off, no longer
# shades the treads: her feet stay readable (Tema)
mass = seq.add_possessable(actors["SlopedMass"].static_mesh_component)
st = mass.add_track(unreal.MovieSceneBoolTrack)
st.set_property_name_and_path("CastShadow", "CastShadow")
section_for(st).get_all_channels()[0].add_key(fr(0.0), False)

# ---- the menu's stand-in mannequin (the director hides it in game; the channel is "visible")
stand = seq.add_possessable(actors["MenuModel"])
vt = stand.add_track(unreal.MovieSceneVisibilityTrack)
vt.set_property_name_and_path("bHiddenInGame", "bHiddenInGame")
section_for(vt).get_all_channels()[0].add_key(fr(0.0), False)

# ---- the drum she leans on in the menu, where the director places it
mesh("Lean column", "/Game/VisEx/Geo/SM_LeanColumn_HD", DRUM[0], DRUM[1], DRUM[2])

# ---- a few broken stones on the treads beside her line (s along her path from START, v to her left)
R = "/Game/VisEx/Geo/SM_Rubble_%d"
STONES = [(60, 70, 2, 0.32, 20), (190, -80, 4, 0.3, 70), (300, 60, 5, 0.25, 45), (420, -120, 1, 0.5, -30)]
for i, (s_, v, m, sc, yaw) in enumerate(STONES):
    p = along(START, s_)
    x, y = p[0] + L[0] * v, p[1] + L[1] * v
    mesh("Stone %02d" % i, R % m, (x, y, ground(x, y) - 8 * sc), yaw, sc)


def smooth(u):
    u = max(0.0, min(1.0, u))
    return u * u * (3 - 2 * u)


def lerp(a, b, w):
    return a + (b - a) * w


def angle_lerp(a, b, w):
    d = (b - a + 180.0) % 360.0 - 180.0
    return a + d * w


CLOSE_FOCAL = SENSOR[0] / 2 / math.tan(math.radians(20.0))   # ~40 deg across: her feet and shins, not just a foot


def cam(t):
    """(loc, (roll, pitch, yaw), focal, focus): tracking her feet, then settling on the menu camera."""
    hx, hy = her(t)
    gz = ground(hx, hy)
    cx, cy = hx + L[0] * 150 - D[0] * 15, hy + L[1] * 150 - D[1] * 15
    cz = max(gz + 20, ground(cx, cy) + 10)
    tx, ty, tz = hx + D[0] * 12, hy + D[1] * 12, gz + 18
    dx, dy, dz = tx - cx, ty - cy, tz - cz
    pitch, yaw = math.degrees(math.atan2(dz, math.hypot(dx, dy))), math.degrees(math.atan2(dy, dx))
    w = smooth((t - SETTLE) / (END - SETTLE))   # 0 while tracking; eases to the menu camera, at rest at END
    (mx, my, mz), (_, mp, myaw) = MENU_CAM
    loc = (lerp(cx, mx, w), lerp(cy, my, w), lerp(cz, mz, w))
    rot = (0.0, angle_lerp(pitch, mp, w), angle_lerp(yaw, myaw, w))
    focal = math.exp(lerp(math.log(CLOSE_FOCAL), math.log(MENU_FOCAL), w))
    focus = lerp(math.sqrt(dx * dx + dy * dy + dz * dz), MENU_FOCUS, w)
    return loc, rot, focal, focus


b, actor = les.create_camera(True)
b.set_display_name("IntroCam")
for tr in b.get_tracks():
    if isinstance(tr, unreal.MovieScene3DTransformTrack):
        b.remove_track(tr)
tkeys, lens, focus = [], [], []
t = 0.0
while t <= END + 1e-6:
    loc, rot, fl, fd = cam(t)
    tkeys.append((t, loc, rot))
    lens.append((t, fl))
    focus.append((t, fd))
    t += 0.125
key_transform(section_for(b.add_track(unreal.MovieScene3DTransformTrack)), tkeys)
comp = b.get_child_possessables()[0]
for tr in comp.get_tracks():
    comp.remove_track(tr)
cc = actor.get_cine_camera_component()
cc.set_editor_property("filmback", unreal.CameraFilmbackSettings(sensor_width=SENSOR[0], sensor_height=SENSOR[1]))
cc.set_editor_property("constrain_aspect_ratio", False)      # as the menu: fills the window
pp = cc.get_editor_property("post_process_settings")
pp.set_editor_property("override_motion_blur_amount", True)
pp.set_editor_property("motion_blur_amount", 0.25)            # half the default: her feet stay legible
cc.set_editor_property("post_process_settings", pp)
fs = cc.get_editor_property("focus_settings")
fs.set_editor_property("focus_method", unreal.CameraFocusMethod.MANUAL)
cc.set_editor_property("focus_settings", fs)
ls_ = cc.get_editor_property("lens_settings")
ls_.set_editor_property("min_f_stop", 1.2)
ls_.set_editor_property("min_focal_length", 10.0)
ls_.set_editor_property("max_focal_length", 400.0)
cc.set_editor_property("lens_settings", ls_)
les.save_default_spawnable_state(b)
for prop, path, keys in (("CurrentFocalLength", "CurrentFocalLength", lens),
                         ("CurrentAperture", "CurrentAperture", [(0.0, 2.0), (SETTLE, 2.0), (END, MENU_FSTOP)]),
                         ("ManualFocusDistance", "FocusSettings.ManualFocusDistance", focus)):
    tr = comp.add_track(unreal.MovieSceneFloatTrack)
    tr.set_property_name_and_path(prop, path)
    key_channel(section_for(tr).get_all_channels()[0], keys)

# ---- bounce: a warm board by the lens while it's close (it sees the shadow side of her feet);
# gone before the end, since the live menu has none
fill = spawnable(unreal.RectLight, "Bounce")
lc = fill.get_object_template().get_editor_property("rect_light_component")
for prop, v in (("intensity", 18.0), ("source_width", 80.0), ("source_height", 50.0), ("attenuation_radius", 450.0),
                ("cast_shadows", False), ("light_color", unreal.Color(255, 196, 140, 255))):
    lc.set_editor_property(prop, v)
fkeys = []
for k in tkeys:
    t, (x, y, z), _ = k
    hx, hy = her(t)
    tz = ground(hx, hy) + 18
    x, y, z = x - L[0] * 20, y - L[1] * 20, z + 35
    dx, dy, dz = hx - x, hy - y, tz - z
    fkeys.append((t, (x, y, z), (0.0, math.degrees(math.atan2(dz, math.hypot(dx, dy))), math.degrees(math.atan2(dy, dx)))))
key_transform(section_for(fill.add_track(unreal.MovieScene3DTransformTrack)), fkeys)
it = fill.add_track(unreal.MovieSceneFloatTrack)
it.set_property_name_and_path("Intensity", "RectLightComponent.Intensity")
key_channel(section_for(it).get_all_channels()[0], [(0.0, 18.0), (SETTLE, 18.0), (END - 0.8, 0.0)])

# ---- camera cut: from 0 (the pre-roll is MRQ's warm-up)
cut_track = next(t for t in seq.get_tracks() if isinstance(t, unreal.MovieSceneCameraCutTrack))
for s in list(cut_track.get_sections()):
    cut_track.remove_section(s)
s = cut_track.add_section()
s.set_range(0, fr(END).value)
s.set_camera_binding_id(unreal.MovieSceneSequenceExtensions.get_portable_binding_id(seq, seq, b))

# ---- out of darkness
fade = seq.add_track(unreal.MovieSceneFadeTrack)
s = section_for(fade, 0.0, PRE + 2.0)
s.set_editor_property("fade_color", unreal.LinearColor(0, 0, 0, 1.0))
key_channel(s.get_all_channels()[0], [(0.0, 1.0), (PRE + 0.2, 1.0), (PRE + 2.0, 0.0)])

unreal.EditorAssetLibrary.save_loaded_asset(seq)
# close it: Sequencer's curve editor crashed the editor (SIGSEGV in a transaction) when a rebuilt
# sequence stayed open
unreal.LevelSequenceEditorBlueprintLibrary.close_level_sequence()
print("LS_Intro 7 s: she walks from %s to %s (heading %d); camera settles on the menu camera from %.1f s" % (
    [round(v) for v in START], [round(v) for v in ARRIVE], HEAD, SETTLE - PRE))
