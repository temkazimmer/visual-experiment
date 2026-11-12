"""Build /Game/VisEx/Intro/LS_Intro: the 7 s startup film (Tema, take 3).

Just her bare feet walking across open, flat sand: rippled, wind-blown, raked by a low sun
behind her. A low camera beside her line dollies slowly and pans with her feet. Black to
black: the game then comes up from black with her walking into the menu.

Run live: python3 Tools/ue.py Tools/intro_sequence.py   (rebuilds LS_Intro in place;
everything is a spawnable in the sequence, plus a key on the Sun; don't save Lvl_Dust.)

Timeline (24 fps): 0-4 s pre-roll (MRQ warms up on the camera cut's excess), film 4-11 s.
"""
import math
import os
import sys
import unreal

sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools"))
import importlib
import intro_params as P            # the stage: where she walks, and the hero sand
importlib.reload(P)

PATH, NAME = "/Game/VisEx/Intro", "LS_Intro"
FPS, PRE, FILM = 24, 4.0, 7.0          # 4 s pre-roll: the scene loads and settles before frame 1
END = PRE + FILM
WALK = 0.6                             # x MaxWalkSpeed 190 -> ~114 cm/s: calm (Tema)
SPEED = 190.0 * WALK
WALK_T0 = PRE - 2.0                    # she is mid-stride when the film starts
HEAD = P.HEADING
SUN = (-7.0, -60.0)                    # (pitch, yaw): low in the west-north-west, 60 deg off the lens: rakes the
                                       # ripples and rims her feet instead of silhouetting them
D = (math.cos(math.radians(HEAD)), math.sin(math.radians(HEAD)))
L = (-D[1], D[0])                      # her left
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
        mode = unreal.MovieSceneKeyInterpolation.AUTO if prop == "SinkCm" else unreal.MovieSceneKeyInterpolation.CONSTANT
        for t, v in keys:   # switches are stepped (no ramps); the sink eases
            ch.add_key(fr(t), float(v), interpolation=mode)
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
    """Her (x, y) at time t: a straight line at SPEED from the stage's start."""
    return along(START, SPEED * max(0.0, t - WALK_T0 - 0.15))


START = P.world(P.U_START + 150.0, 0.0)

# ---- her (player 0: the footprints and mounds follow her), soles pressed into the sand
rig("Her", START, [(0.0, 0.0), (WALK_T0, WALK)], [(0.0, 0.0), (WALK_T0, 1.0)], True)

# ---- the stage: 1 cm rippled sand along her line (Tools/intro_sand_mesh.py), over the flat dune
sand = mesh("Hero sand", PATH + "/SM_IntroSand", (P.CENTER[0], P.CENTER[1], 0.0), 0.0, 1.0)
sand.get_object_template().static_mesh_component.set_material(0, unreal.load_asset(PATH + "/MI_IntroSand"))

# ---- light: the sun low in the west, behind her line as the camera sees it
sun = seq.add_possessable(actors["Sun"])
sl = actors["Sun"].get_actor_location()
key_transform(section_for(sun.add_track(unreal.MovieScene3DTransformTrack)), [(0.0, (sl.x, sl.y, sl.z), (0.0, SUN[0], SUN[1]))])

# ---- camera: low on her right (east, looking into the sun), dollying at 0.6x her pace and
# panning with her feet across the frame
OFF = 360.0                            # cm to her right
LEAD = -170.0                          # the dolly starts this far behind her along her line


def cam(t):
    hx, hy = her(t)
    s_ = LEAD + 0.6 * SPEED * max(0.0, t - WALK_T0)
    bx, by = along(START, s_)
    cx, cy = bx - L[0] * OFF, by - L[1] * OFF
    cz = ground(cx, cy) + 40           # a little above the sand, looking down at her feet: the frame is
    tx, ty, tz = hx, hy, ground(hx, hy) + 8   # mostly lit ripples, the glow only a sliver at the top
    dx, dy, dz = tx - cx, ty - cy, tz - cz
    rot = (0.0, math.degrees(math.atan2(dz, math.hypot(dx, dy))), math.degrees(math.atan2(dy, dx)))
    return (cx, cy, cz), rot, math.sqrt(dx * dx + dy * dy + dz * dz)


b, actor = les.create_camera(True)
b.set_display_name("IntroCam")
for tr in b.get_tracks():
    if isinstance(tr, unreal.MovieScene3DTransformTrack):
        b.remove_track(tr)
tkeys, focus = [], []
t = 0.0
while t <= END + 1e-6:
    loc, rot, fd = cam(t)
    tkeys.append((t, loc, rot))
    focus.append((t, fd))
    t += 0.125
key_transform(section_for(b.add_track(unreal.MovieScene3DTransformTrack)), tkeys)
comp = b.get_child_possessables()[0]
for tr in comp.get_tracks():
    comp.remove_track(tr)
cc = actor.get_cine_camera_component()
cc.set_editor_property("filmback", unreal.CameraFilmbackSettings(sensor_width=36.0, sensor_height=22.5))   # 16:10
fs = cc.get_editor_property("focus_settings")
fs.set_editor_property("focus_method", unreal.CameraFocusMethod.MANUAL)
cc.set_editor_property("focus_settings", fs)
pp = cc.get_editor_property("post_process_settings")
pp.set_editor_property("override_motion_blur_amount", True)
pp.set_editor_property("motion_blur_amount", 0.3)
cc.set_editor_property("post_process_settings", pp)
les.save_default_spawnable_state(b)
for prop, path, keys in (("CurrentFocalLength", "CurrentFocalLength", [(0.0, 100.0)]),
                         ("CurrentAperture", "CurrentAperture", [(0.0, 2.0)]),
                         ("ManualFocusDistance", "FocusSettings.ManualFocusDistance", focus)):
    tr = comp.add_track(unreal.MovieSceneFloatTrack)
    tr.set_property_name_and_path(prop, path)
    key_channel(section_for(tr).get_all_channels()[0], keys)

# ---- bounce: a warm board by the lens, so her feet and the sand they press read against the glow
fill = spawnable(unreal.RectLight, "Bounce")
lc = fill.get_object_template().get_editor_property("rect_light_component")
for prop, v in (("intensity", 300.0), ("source_width", 100.0), ("source_height", 60.0), ("attenuation_radius", 700.0),
                ("cast_shadows", False), ("light_color", unreal.Color(255, 190, 130, 255))):
    lc.set_editor_property(prop, v)
fkeys = []
for t_, (x, y, z), _ in tkeys:
    hx, hy = her(t_)
    x, y, z = x + L[0] * 40, y + L[1] * 40, z + 45
    dx, dy, dz = hx - x, hy - y, ground(hx, hy) + 10 - z
    fkeys.append((t_, (x, y, z), (0.0, math.degrees(math.atan2(dz, math.hypot(dx, dy))), math.degrees(math.atan2(dy, dx)))))
key_transform(section_for(fill.add_track(unreal.MovieScene3DTransformTrack)), fkeys)

cut_track = next(t for t in seq.get_tracks() if isinstance(t, unreal.MovieSceneCameraCutTrack))
for s in list(cut_track.get_sections()):
    cut_track.remove_section(s)
s = cut_track.add_section()
s.set_range(0, fr(END).value)
s.set_camera_binding_id(unreal.MovieSceneSequenceExtensions.get_portable_binding_id(seq, seq, b))

# ---- black to black
fade = seq.add_track(unreal.MovieSceneFadeTrack)
s = section_for(fade, 0.0, END)
s.set_editor_property("fade_color", unreal.LinearColor(0, 0, 0, 1.0))
key_channel(s.get_all_channels()[0], [(0.0, 1.0), (PRE + 0.2, 1.0), (PRE + 2.0, 0.0), (END - 1.5, 0.0), (END - 0.5, 1.0), (END, 1.0)])

unreal.EditorAssetLibrary.save_loaded_asset(seq)
unreal.LevelSequenceEditorBlueprintLibrary.close_level_sequence()   # an open rebuilt sequence crashed the editor once
print("LS_Intro take 3: she walks north from %s; camera %d cm to her right" % ([round(v) for v in START], OFF))
