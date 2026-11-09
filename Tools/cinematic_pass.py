"""Cinematic look for Lvl_Dust: sun, sky light, layered dust fog, grade, lens.
One place for every look value, so the mood can be tuned and re-applied.

Run live: python3 Tools/ue.py Tools/cinematic_pass.py
Doesn't save; save the level afterwards (not while in Play mode).

Idea: a low sun cutting long shafts through the colonnade. Warm, bright light against
cooler, deeper shadows, so the orange has a value range instead of being one flat wash.
Near fog thin so the foreground reads; dense dust wall at depth.
"""
import unreal

SUN = dict(pitch=-24.0, yaw=125.0, intensity=12.0, color=(255, 184, 120),
           source_angle=1.2, volumetric_scattering_intensity=5.0,
           shaft_bloom=True, bloom_scale=0.25, bloom_threshold=0.6, bloom_tint=(255, 190, 120))
# a cool, weak sky light gives the shadow side its blue-grey against the warm sun
SKY = dict(intensity=0.8, color=(190, 200, 225))
FOG = dict(density=0.03, falloff=0.02, start_distance=400.0,
           inscatter=(1.5, 0.55, 0.12),            # ambient dust: the orange identity
           directional=(1.8, 0.75, 0.22),           # glow when looking toward the sun
           directional_exponent=6.0,
           second=dict(fog_density=0.0, fog_height_falloff=0.2, fog_height_offset=0.0),  # a low layer washes all shading out: keep off
           vol_distance=9000.0, vol_scatter=0.75, vol_albedo=(255, 200, 150), vol_extinction=0.9)
# dust ceiling: a big local fog volume that gets denser with height, so the tops of the
# colonnade and monoliths dissolve into the storm (exponential height fog thins upward)
HIGH = dict(center=(12000, -1500, 6000), radius=40000.0, extinction=0.0, height_extinction=2.5,
            height_falloff=-1.5, height_offset=-0.12, albedo=(1.0, 0.62, 0.35), phase_g=0.4,
            emissive=(0.0, 0.0, 0.0))
POST = dict(
    # tone + exposure (min = max: fixed exposure, no pumping while you walk into shade)
    auto_exposure_min_brightness=2.3, auto_exposure_max_brightness=2.3,
    film_slope=0.9, film_toe=0.68, film_shoulder=0.3, film_black_clip=0.0,
    # grade: teal-ish shadows, amber highlights, a little less saturation overall
    color_saturation=(1.05, 1.05, 1.05, 1.0),
    color_contrast=(1.22, 1.22, 1.22, 1.0),
    color_gain_shadows=(0.88, 0.97, 1.12, 1.0),
    color_saturation_shadows=(0.85, 0.85, 0.85, 1.0),
    color_gamma_shadows=(1.0, 1.0, 1.0, 0.88),
    color_gain_highlights=(1.06, 1.0, 0.9, 1.0),
    color_offset_shadows=(0.0, 0.004, 0.012, 0.0),
    white_temp=6100.0,
    # lens
    bloom_intensity=0.9, bloom_threshold=-1.0,
    vignette_intensity=0.45,
    scene_fringe_intensity=0.22,
    film_grain_intensity=0.3, film_grain_intensity_shadows=0.6,
    lens_flare_intensity=0.35, lens_flare_bokeh_size=4.0,
)

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
by = {a.get_actor_label(): a for a in eas.get_all_level_actors()}
C = lambda c: unreal.Color(r=c[0], g=c[1], b=c[2], a=255)
L = lambda c: unreal.LinearColor(c[0], c[1], c[2], 1.0)

sun = by["Sun"]
sun.set_actor_rotation(unreal.Rotator(roll=0, pitch=SUN["pitch"], yaw=SUN["yaw"]), False)
lc = sun.light_component
lc.set_intensity(SUN["intensity"])
lc.set_light_color(L([c / 255 for c in SUN["color"]]))
for p, v in (("light_source_angle", SUN["source_angle"]),
             ("volumetric_scattering_intensity", SUN["volumetric_scattering_intensity"]),
             ("enable_light_shaft_bloom", SUN["shaft_bloom"]),
             ("bloom_scale", SUN["bloom_scale"]), ("bloom_threshold", SUN["bloom_threshold"]),
             ("bloom_tint", C(SUN["bloom_tint"]))):
    lc.set_editor_property(p, v)

sl = by["SkyLight"].light_component
sl.set_intensity(SKY["intensity"])
sl.set_light_color(L([c / 255 for c in SKY["color"]]))

f = by["DustFog"].component
for p, v in (("fog_density", FOG["density"]), ("fog_height_falloff", FOG["falloff"]),
             ("start_distance", FOG["start_distance"]),
             ("fog_inscattering_luminance", L(FOG["inscatter"])),
             ("directional_inscattering_luminance", L(FOG["directional"])),
             ("directional_inscattering_exponent", FOG["directional_exponent"]),
             ("volumetric_fog_distance", FOG["vol_distance"]),
             ("volumetric_fog_scattering_distribution", FOG["vol_scatter"]),
             ("volumetric_fog_albedo", C(FOG["vol_albedo"])),
             ("volumetric_fog_extinction_scale", FOG["vol_extinction"])):
    f.set_editor_property(p, v)
sd = f.get_editor_property("second_fog_data")
for k, v in FOG["second"].items():
    sd.set_editor_property(k, v)
f.set_editor_property("second_fog_data", sd)

hv = by.get("HighDust") or eas.spawn_actor_from_class(unreal.LocalFogVolume, unreal.Vector(*HIGH["center"]))
hv.set_actor_label("HighDust")
hv.set_actor_location(unreal.Vector(*HIGH["center"]), False, False)
hv.set_actor_scale3d(unreal.Vector(1, 1, 1) * (HIGH["radius"] / 100.0))   # unit sphere is 100 cm radius
lfv = hv.get_component_by_class(unreal.LocalFogVolumeComponent)
lfv.set_radial_fog_extinction(HIGH["extinction"])
lfv.set_height_fog_extinction(HIGH["height_extinction"])
lfv.set_height_fog_falloff(HIGH["height_falloff"])
lfv.set_height_fog_offset(HIGH["height_offset"])
lfv.set_fog_albedo(L(HIGH["albedo"]))
lfv.set_fog_phase_g(HIGH["phase_g"])
lfv.set_fog_emissive(L(HIGH["emissive"]))

pp = by["PostProcess"]
s = pp.get_editor_property("settings")
for k, v in POST.items():
    if isinstance(v, tuple):
        v = unreal.Vector4(*v)
    s.set_editor_property(k, v)
    try:
        s.set_editor_property("override_" + k, True)
    except Exception:
        pass
pp.set_editor_property("settings", s)

print("cinematic look applied: sun pitch %s, fog %s, sky %s" % (SUN["pitch"], FOG["density"], SKY["intensity"]))
