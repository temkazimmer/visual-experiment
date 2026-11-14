"""Shoot the head on screen in Play from fixed angles in the HEAD BONE's own frame, hair hidden.

Stage 6 helper: the same camera, lens and light on her current head (visex.Body.MetaHuman 0) and
on the MetaHuman head (1), whatever the idle pose is doing, so the two can be put side by side.
Uses the capture actor and the Play session of Tools/mh_play.py:

  python3 Tools/ue.py -c "MODE='prepare'; exec(open('<project>/Tools/mh_play.py').read())"
  python3 Tools/ue.py -c "MODE='play'; exec(open('<project>/Tools/mh_play.py').read())"
  python3 Tools/ue.py -c "MODE='cmd'; CMD='visex.Body.MetaHuman 0'; exec(open('<project>/Tools/mh_play.py').read())"
  python3 Tools/ue.py -c "NAME='ref'; exec(open('<project>/Tools/mh_face_shots.py').read())"
  ... 'visex.Body.MetaHuman 1' (wait a few seconds), NAME='mh', then MODE='stop' and 'clean' of mh_play.py

Writes Saved/MH/<NAME>_<view>.png (SIZE=(w, h), default 1280x2160: three make a 4K sheet; FOV is horizontal). VIEWS picks from
SHOTS or gives new ones: (azimuth degrees from straight ahead towards her left, elevation, distance
cm, fov, (aim forward, aim left, aim up) in cm from the head joint). A scene capture has no post
grade: it shows shape and skin, not the look. HAIR=True leaves the hair on. EV and LIGHT trim the
fixed exposure and the three head-fixed lights; MODE='lights' places those before Play.

MODE='preview' needs no Play: it shoots the Creator's preview of CHAR_NAME (default MHC_Try) in the
editor world under the same lights, optionally with SKIN=(u, v) or FACE_TEX=n set on it first: a look at a fit or a
skin tone before the cloud steps. Brows and lashes do not draw there (grooms need a drawn view).
"""
import math
import os
import unreal

TAG = "MHPlay"
OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Saved", "MH")
NAME = globals().get("NAME", "face")
HAIR = globals().get("HAIR", False)
AIM = globals().get("AIM", (7.0, 0.0, 5.5))      # from the head joint to the middle of the face
FOV = globals().get("FOV", 9.0)                  # horizontal; 9 at 170 cm is 27 cm across (a 1280x2160 panel)
SHOTS = {
    "front": (0.0, 0.0, 170.0, FOV, AIM),
    "three": (-40.0, 0.0, 170.0, FOV, AIM),
    "side": (-90.0, 0.0, 170.0, FOV, (1.0, 0.0, 5.5)),
    "ear": (-100.0, 5.0, 80.0, FOV, (-1.0, -7.0, 6.0)),
    "earback": (-150.0, 10.0, 80.0, FOV, (-1.0, -7.0, 6.0)),
    "earfront": (-35.0, 5.0, 80.0, FOV, (-1.0, -7.0, 6.0)),
    "top": (0.0, 75.0, 170.0, FOV, (1.0, 0.0, 9.0)),
    "eyes": (0.0, 0.0, 60.0, FOV, (8.0, 0.0, 6.0)),
}
VIEWS = globals().get("VIEWS", ["front", "three", "side"])
LIGHTS = {       # in the head's frame, cm from the head joint: (forward, left, up), candelas, source radius
    "Key": ((95.0, 55.0, 60.0), 520.0, 14.0),
    "Fill": ((110.0, -80.0, 5.0), 170.0, 30.0),
    "Rim": ((-90.0, -60.0, 70.0), 420.0, 10.0),
}
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
MODE = globals().get("MODE", "play")


def tagged(world, cls):
    return [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, cls) if TAG in [str(x) for x in a.tags]]


def place_lights():
    for k in LIGHTS:
        a = eas.spawn_actor_from_class(unreal.PointLight, unreal.Vector(0.0, 0.0, 400.0))
        a.set_editor_property("tags", [TAG, "Light" + k])
        a.set_actor_label(TAG + "_" + k)
        a.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)


def unit(v):
    n = v.length()
    return unreal.Vector(v.x / n, v.y / n, v.z / n)


def head_frame(comp):
    """The head joint and the head bone's forward, left and up in the world. Both meshes face their
    component's +Y with her left hand at +X, so the component says which way each bone axis points."""
    t = comp.get_socket_transform("head", unreal.RelativeTransformSpace.RTS_WORLD)
    rot = t.rotation.rotator()
    ax = [unreal.MathLibrary.get_forward_vector(rot), unreal.MathLibrary.get_right_vector(rot), unreal.MathLibrary.get_up_vector(rot)]
    up = max(ax, key=lambda v: abs(v.z))
    up = up if up.z > 0 else up * -1.0
    f0 = comp.get_right_vector()
    fwd = max([a for a in ax if a is not up], key=lambda v: abs(v.dot(f0)))
    fwd = unit(fwd if fwd.dot(f0) > 0 else fwd * -1.0)
    left = unit(up.cross(fwd))
    left = left if left.dot(comp.get_forward_vector()) > 0 else left * -1.0
    return t.translation, fwd, left, up


def shoot(world, comp, show):
    joint, fwd, left, up = head_frame(comp)
    for a in tagged(world, unreal.PointLight):
        key = [str(x)[5:] for x in a.tags if str(x).startswith("Light")]
        (lf, ll, lu), candelas, radius = LIGHTS[key[0]]
        a.set_actor_location(joint + fwd * lf + left * ll + up * lu, False, True)
        lc = a.point_light_component
        lc.set_intensity_units(unreal.LightUnits.CANDELAS)
        lc.set_intensity(candelas * globals().get("LIGHT", 1.0))
        lc.set_source_radius(radius)
        # as an edit, not a setter: that re-registers the light, so it is at its new place for this
        # capture (a moved light otherwise waits for the end of a frame the background editor never draws)
        lc.set_editor_property("attenuation_radius", 800.0)
    cap = tagged(world, unreal.SceneCapture2D)[0]
    c = cap.capture_component2d
    size = globals().get("SIZE", (1280, 2160))
    c.set_editor_property("texture_target", unreal.RenderingLibrary.create_render_target2d(
        world, size[0], size[1], unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB))
    # the figure only, on black, at a fixed exposure: the level's own set and auto exposure stay out
    c.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_TONE_CURVE_HDR)
    c.set_editor_property("primitive_render_mode", unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
    c.clear_show_only_components()
    for actor in show:
        c.show_only_actor_components(actor, True)
    pps = c.get_editor_property("post_process_settings")
    for k, v in (("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL),
                 ("auto_exposure_bias", -float(globals().get("EV", 7.0))),      # manual, no camera: scale = 2^bias
                 ("auto_exposure_apply_physical_camera_exposure", False), ("bloom_intensity", 0.0), ("vignette_intensity", 0.0)):
        pps.set_editor_property("override_" + k, True)
        pps.set_editor_property(k, v)
    c.set_editor_property("post_process_settings", pps)
    c.set_editor_property("post_process_blend_weight", 1.0)
    os.makedirs(OUT, exist_ok=True)
    print("FACE", comp.get_owner().get_class().get_name(), "head joint", [round(x, 1) for x in (joint.x, joint.y, joint.z)])
    for view in VIEWS:
        az, el, dist, fov, aim = SHOTS[view] if isinstance(view, str) else view[1]
        label = view if isinstance(view, str) else view[0]
        a, e = math.radians(az), math.radians(el)
        target = joint + fwd * aim[0] + left * aim[1] + up * aim[2]
        d = (fwd * math.cos(a) + left * math.sin(a)) * math.cos(e) + up * math.sin(e)
        cap.set_actor_location_and_rotation(target + d * dist, unreal.MathLibrary.make_rot_from_xz(d * -1.0, up), False, True)
        c.set_editor_property("fov_angle", float(fov))
        c.capture_scene()
        c.capture_scene()
        unreal.RenderingLibrary.export_render_target(world, c.get_editor_property("texture_target"), OUT, "%s_%s.png" % (NAME, label))
        print("FACE shot", os.path.join(OUT, "%s_%s.png" % (NAME, label)))


def play_shot():
    world = ues.get_game_world()
    assert world, "not in Play"
    her = unreal.GameplayStatics.get_player_character(world, 0)
    comp, show = her.mesh, [her]
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        if a.get_class().get_name().startswith("BP_MHC_"):
            for c in a.get_components_by_class(unreal.SkeletalMeshComponent):
                if c.get_name() == "Body" and c.is_visible():
                    comp, show = c, [her, a]
    if not HAIR:
        for c in her.get_components_by_class(unreal.SkeletalMeshComponent):
            mesh = c.get_editor_property("skeletal_mesh_asset")
            if mesh and "Hair" in mesh.get_name():
                c.set_visibility(False, True)
    shoot(world, comp, show)


def preview_shot():
    world = ues.get_editor_world()
    assert not ues.get_game_world(), "stop Play first"
    mh = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
    character = unreal.load_asset("/Game/Character/MH/" + globals().get("CHAR_NAME", "MHC_Try"))
    was = mh.is_object_added_for_editing(character)
    assert was or mh.try_add_object_to_edit(character), "cannot edit the character"
    if not tagged(world, unreal.SceneCapture2D):
        cap = eas.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(0.0, 0.0, 300.0))
        cap.set_editor_property("tags", [TAG])
        cap.capture_component2d.set_editor_property("capture_every_frame", False)
        cap.capture_component2d.set_editor_property("capture_on_movement", False)
        cap.capture_component2d.set_editor_property("always_persist_rendering_state", True)
    if not tagged(world, unreal.PointLight):
        place_lights()
    actor = None
    try:
        if "SKIN" in globals() or "FACE_TEX" in globals():
            settings = character.skin_settings
            skin = settings.skin
            if "SKIN" in globals():
                skin.u, skin.v = SKIN
            if "FACE_TEX" in globals():
                skin.face_texture_index = int(FACE_TEX)      # which of the Creator's face skins (pores, marks, age)
            settings.skin = skin
            mh.commit_skin_settings(character, settings)
        actor = mh.spawn_meta_human_actor(character)
        actor.set_actor_location(unreal.Vector(0.0, 0.0, 3000.0), False, True)     # clear of the set: it shadows her at the origin
        body = [c for c in actor.get_components_by_class(unreal.SkeletalMeshComponent) if c.get_name() == "Body"][0]
        shoot(world, body, [actor])
    finally:
        if actor:
            eas.destroy_actor(actor)
        if not was:
            mh.remove_object_to_edit(character)


if MODE == "lights":
    assert not ues.get_game_world(), "stop Play first"      # Python cannot spawn into a running game world
    place_lights()
    print("FACE lights placed (level left unsaved)")
elif MODE == "preview":
    preview_shot()
else:
    play_shot()
