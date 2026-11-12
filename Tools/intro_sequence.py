"""Build /Game/VisEx/Intro/LS_Intro: the 20 s startup film (Tema's story).

Out of darkness, her bare feet walk the sand below the steps (shot 1, from behind-right);
a stone slides past the lens and we cut to the other side: her feet in profile climb the
half-buried treads in the colonnade's shadow, step into the sun at the top, cross the
plaza sand between stones and stop by the fallen drum she leans on in the menu (shot 2).
Light is the game's own sun, so the ending matches the live menu.

Run live: python3 Tools/ue.py Tools/intro_sequence.py   (rebuilds LS_Intro in place;
everything is a spawnable in the sequence; don't save Lvl_Dust.)

Timeline (24 fps): 0-4 s pre-roll (MRQ warms up on the camera cut's excess), film 4-24 s.
Both walkers spawn at 0, hidden and still; keyed Show/Walk say when each appears and walks
(spawn timing in MRQ isn't reliable enough to carry the timing).
"""
import math
import unreal

PATH, NAME = "/Game/VisEx/Intro", "LS_Intro"
FPS, PRE, FILM = 24, 4.0, 20.0     # 4 s pre-roll: the scene loads and settles before frame 1
END = PRE + FILM
CUT = PRE + 7.0                       # shot 1 -> shot 2
WALK = 0.75                           # x MaxWalkSpeed 190 -> ~142 cm/s
SPEED = 190.0 * WALK
HEAD = 60.0                           # her heading (UE yaw): up the steps toward the drum
D = (math.cos(math.radians(HEAD)), math.sin(math.radians(HEAD)))
L = (-D[1], D[0])                     # her left
DRUM = ((9575.8, -849.1, 108.0), -15.0, 1.011)   # the menu's lean column, as the director places it
STOP = (DRUM[0][0] - D[0] * 110, DRUM[0][1] - D[1] * 110)   # she stops just short of it
A_END = (8300.0, -3250.0)             # shot 1 walker reaches the sand edge at the cut
A_T0 = PRE - 2.0                      # shot 1 walker starts walking 2 s before the film
B_T0, B_STOP_T = CUT - 2.0, PRE + 18.5  # shot 2 walker: walks from 2 s before the cut, stops here
HAZE = (0.92, 0.62, 0.36)

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = {a.get_actor_label(): a for a in eas.get_all_level_actors()}
IGNORE = [a for a in actors.values() if a.get_actor_label().startswith("Dunes_Bound") or a.get_actor_label() == "StepRamp"]


def ground(x, y):
    r = unreal.SystemLibrary.line_trace_single(
        world, unreal.Vector(x, y, 3000), unreal.Vector(x, y, -3000),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, IGNORE, unreal.DrawDebugTrace.NONE, True)
    return r.to_tuple()[4].z if r and r.to_tuple()[0] else 0.0


def fr(t):
    return unreal.FrameNumber(int(round(t * FPS)))


def along(p, s):
    return (p[0] + D[0] * s, p[1] + D[1] * s)


# where each walker is at time t (a straight line at SPEED; ~0.15 s lost to acceleration)
A0 = along(A_END, -SPEED * (CUT - A_T0 - 0.15))
B_LEN = SPEED * (B_STOP_T - B_T0 - 0.15)
B0 = along(STOP, -B_LEN)


def her(t):
    if t < CUT:
        return along(A0, SPEED * max(0.0, t - A_T0 - 0.15))
    return along(B0, SPEED * min(B_LEN / SPEED, max(0.0, t - B_T0 - 0.15)))


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


def rig(name, start, walk_keys, show_keys, player):
    b = spawnable(unreal.load_class(None, "/Script/visex.VisexIntroRig"), name)
    b.get_object_template().set_editor_property("player", player)
    place(b, (start[0], start[1], ground(*start)), HEAD, 1.0)
    for prop, keys in (("Walk", walk_keys), ("Show", show_keys)):
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

# ---- her: shot 1 on the sand (player 0: footprints), shot 2 up the steps to the drum
rig("Her shot 1", A0, [(0.0, 0.0), (A_T0, WALK), (CUT + 0.5, 0.0)], [(0.0, 0.0), (A_T0, 1.0), (CUT + 0.1, 0.0)], True)
rig("Her shot 2", B0, [(0.0, 0.0), (B_T0, WALK), (B_STOP_T, 0.0)], [(0.0, 0.0), (B_T0, 1.0)], False)

# ---- the menu's stand-in mannequin (the director hides it in game; hide it here too)
stand = seq.add_possessable(actors["MenuModel"])
vt = stand.add_track(unreal.MovieSceneVisibilityTrack)
vt.set_property_name_and_path("bHiddenInGame", "bHiddenInGame")
section_for(vt).get_all_channels()[0].add_key(fr(0.0), True)

# ---- the drum she leans on in the menu (the director only spawns it with the menu)
mesh("Lean column", "/Game/VisEx/Geo/SM_LeanColumn_HD", DRUM[0], DRUM[1], DRUM[2])

# ---- rubble on her way (film only; Tema wants more here in the level too: main decides)
# (s along shot 2's line from B0, v to her left, mesh, scale, yaw); stones stay >= 60 cm off her line
R = "/Game/VisEx/Geo/SM_Rubble_%d"
STONES = [(250, 95, 2, 0.45, 20), (520, -85, 4, 0.35, 70), (700, 120, 1, 0.6, -30),
          (900, -110, 3, 0.4, 10), (1150, 90, 5, 0.3, 45), (1350, -140, 0, 0.75, -60),
          (1550, 105, 2, 0.35, 15), (300, -260, 1, 1.4, 30), (800, -330, 4, 1.8, -20),
          (1250, -300, 3, 1.2, 80), (1700, -240, 0, 0.9, 5), (1000, 260, 5, 1.1, -45)]
for i, (s, v, m, sc, yaw) in enumerate(STONES):
    p = along(B0, s)
    x, y = p[0] + L[0] * v, p[1] + L[1] * v
    mesh("Stone %02d" % i, R % m, (x, y, ground(x, y) - 8 * sc), yaw, sc)

# ---- cameras: shot 1 behind-right of her on the sand, shot 2 her left side, in profile
cams = []
for name, t0, t1 in (("Cam 1 behind", 0.0, CUT), ("Cam 2 profile", CUT, END)):
    b, actor = les.create_camera(True)
    b.set_display_name(name)
    for tr in b.get_tracks():
        if isinstance(tr, unreal.MovieScene3DTransformTrack):
            b.remove_track(tr)
    cams.append((b, actor, t0, t1))


def cam1(t):
    hx, hy = her(t)
    cx, cy = hx - D[0] * 110 - L[0] * 80, hy - D[1] * 110 - L[1] * 80
    cz = ground(cx, cy) + 14
    return (cx, cy, cz), (hx + D[0] * 25, hy + D[1] * 25, ground(hx, hy) + 10), 65.0


CAM2_STOP = PRE + 15.5                 # the dolly settles; she walks on to the drum


def cam2(t):
    tc = min(t, CAM2_STOP + 1.0)
    if t > CAM2_STOP:                    # ease to a stop over 2 s
        u = min(1.0, (t - CAM2_STOP) / 2.0)
        tc = CAM2_STOP + 2.0 * (u - 0.5 * u * u)
    hx, hy = her(tc)
    cx, cy = hx + L[0] * 170 + D[0] * 20, hy + L[1] * 170 + D[1] * 20
    fx, fy = her(t)
    cz = max(ground(hx, hy) + 16, ground(cx, cy) + 8)
    rise = max(0.0, min(1.0, (t - CAM2_STOP) / 3.0))
    tz = ground(fx, fy) + 10 + 25 * rise   # end: lift to take in the drum's flank
    return (cx, cy, cz), (fx + D[0] * 15, fy + D[1] * 15, tz), 85.0 - 20.0 * rise

# ---- the cut: a stone slides past each lens (shot 1 out, shot 2 in)
p1 = cam1(CUT - 0.05)[0]
x, y = p1[0] + D[0] * 20 + L[0] * 30, p1[1] + D[1] * 20 + L[1] * 30
mesh("Wipe stone 1", R % 4, (x, y, ground(x, y) - 5), 35, 0.42)
hx, hy = her(CUT + 0.35)
x, y = hx + L[0] * 128 + D[0] * 20, hy + L[1] * 128 + D[1] * 20
mesh("Wipe stone 2", R % 1, (x, y, ground(x, y) - 6), -25, 0.5)

# ---- key the cameras (every 1/4 s), lens, focus
for (b, actor, t0, t1), fn in zip(cams, (cam1, cam2)):
    tkeys, lens, focus = [], [], []
    t = t0 - (0.5 if t0 > 0 else 0.0)
    while t <= t1 + 0.26:
        c, tg, fl = fn(t)
        dx, dy, dz = tg[0] - c[0], tg[1] - c[1], tg[2] - c[2]
        tkeys.append((t, c, (0.0, math.degrees(math.atan2(dz, math.hypot(dx, dy))), math.degrees(math.atan2(dy, dx)))))
        lens.append((t, fl))
        focus.append((t, math.sqrt(dx * dx + dy * dy + dz * dz)))
        t += 0.25
    key_transform(section_for(b.add_track(unreal.MovieScene3DTransformTrack)), tkeys)
    comp = b.get_child_possessables()[0]
    for tr in comp.get_tracks():
        comp.remove_track(tr)
    cc = actor.get_cine_camera_component()
    cc.set_editor_property("filmback", unreal.CameraFilmbackSettings(sensor_width=36.0, sensor_height=20.25))
    fs = cc.get_editor_property("focus_settings")
    fs.set_editor_property("focus_method", unreal.CameraFocusMethod.MANUAL)
    cc.set_editor_property("focus_settings", fs)
    les.save_default_spawnable_state(b)
    for prop, path, keys in (("CurrentFocalLength", "CurrentFocalLength", lens),
                             ("CurrentAperture", "CurrentAperture", [(t0, 2.0)]),
                             ("ManualFocusDistance", "FocusSettings.ManualFocusDistance", focus)):
        tr = comp.add_track(unreal.MovieSceneFloatTrack)
        tr.set_property_name_and_path(prop, path)
        key_channel(section_for(tr).get_all_channels()[0], keys)

# ---- camera cuts: shot 1 runs from 0 (the pre-roll is MRQ's warm-up), shot 2 from CUT
cut_track = next(t for t in seq.get_tracks() if isinstance(t, unreal.MovieSceneCameraCutTrack))
for s in list(cut_track.get_sections()):
    cut_track.remove_section(s)
for b, actor, t0, t1 in cams:
    s = cut_track.add_section()
    s.set_range(fr(t0).value, fr(t1).value)
    s.set_camera_binding_id(seq.get_binding_id(b) if hasattr(seq, "get_binding_id") else
                            unreal.MovieSceneSequenceExtensions.get_portable_binding_id(seq, seq, b))

# ---- out of darkness, and into the haze at the end
fade = seq.add_track(unreal.MovieSceneFadeTrack)
for t0, t1, col, keys in ((0.0, PRE + 3.5, (0, 0, 0), [(0.0, 1.0), (PRE + 0.3, 1.0), (PRE + 3.5, 0.0)]),
                          (END - 2.2, END, HAZE, [(END - 2.2, 0.0), (END, 1.0)])):
    s = section_for(fade, t0, t1)
    s.set_editor_property("fade_color", unreal.LinearColor(*col, 1.0))
    key_channel(s.get_all_channels()[0], keys)

unreal.EditorAssetLibrary.save_loaded_asset(seq)
print("LS_Intro: shot 1 from %s, shot 2 from %s to %s; cut at %.1f s" % (
    [round(v) for v in A0], [round(v) for v in B0], [round(v) for v in STOP], CUT - PRE))
