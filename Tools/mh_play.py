"""Shoot the player in Play, with the MetaHuman body on or off, through a 4K scene capture.

Stage 2 helper (visex.Body.MetaHuman). Run through the bridge, one MODE per call:

  python3 Tools/ue.py -c "MODE='prepare'; exec(open('<project>/Tools/mh_play.py').read())"   # capture actor, before Play
  python3 Tools/ue.py -c "MODE='play'; exec(open('Tools/mh_play.py').read())"
  python3 Tools/ue.py -c "MODE='cmd'; CMD='visex.Body.MetaHuman 1'; exec(open('Tools/mh_play.py').read())"
  python3 Tools/ue.py -c "MODE='shot'; NAME='knee'; VIEW=((120,-60,45),(0,0,45),30); exec(open('Tools/mh_play.py').read())"
  python3 Tools/ue.py -c "MODE='stop'; exec(open('Tools/mh_play.py').read())"
  python3 Tools/ue.py -c "MODE='clean'; exec(open('Tools/mh_play.py').read())"

VIEW is (camera, target, fov) in her own frame: x forward, y to her right, z up from the floor
under her (cm). BONE='head' moves that frame's origin onto a bone. LOD=n forces the
MetaHuman's LOD Sync to n for the shot (-1 gives it back). The capture skips the post grade: it shows shape and framing, not the look
(for the look use the gaps tour's -visexshots). Nothing is saved into the level.
Only stops a Play session this script started.
"""
import os
import unreal

TAG = "MHPlay"
OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Saved", "MH")
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def tagged_editor():
    return [a for a in eas.get_all_level_actors() if TAG in [str(t) for t in a.tags]]


def game():
    world = ues.get_game_world()
    assert world, "not in Play"
    return world


def her(world):
    return unreal.GameplayStatics.get_player_character(world, 0)


def figure(world):
    """The MetaHuman body and face meshes while the cvar is on, else (None, None)."""
    body = face = None
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        if a.get_class().get_name().startswith("BP_MHC_"):
            for c in a.get_components_by_class(unreal.SkeletalMeshComponent):
                body = c if c.get_name() == "Body" else body
                face = c if c.get_name() == "Face" else face
    return body, face


def prepare():
    assert not les.is_in_play_in_editor(), "stop Play first"
    for a in tagged_editor():
        eas.destroy_actor(a)
    cap = eas.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(0.0, 0.0, 300.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
    cap.set_editor_property("tags", [TAG])
    cap.set_actor_label(TAG + "_Capture")
    rt = unreal.RenderingLibrary.create_render_target2d(ues.get_editor_world(), 3840, 2160, unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
    c = cap.capture_component2d
    c.set_editor_property("texture_target", rt)
    c.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_TONE_CURVE_HDR)
    c.set_editor_property("capture_every_frame", False)
    c.set_editor_property("capture_on_movement", False)
    c.set_editor_property("always_persist_rendering_state", True)
    print("capture placed (level left unsaved)")


def play():
    assert not les.is_in_play_in_editor(), "a Play session is already running: not ours, leave it"
    les.editor_request_begin_play()
    unreal.mh_play_started = True
    print("play requested")


def stop():
    assert getattr(unreal, "mh_play_started", False), "this script did not start the Play session"
    les.editor_request_end_play()
    unreal.mh_play_started = False
    print("stop requested")


def clean():
    assert not les.is_in_play_in_editor(), "stop Play first"
    n = len(tagged_editor())
    for a in tagged_editor():
        eas.destroy_actor(a)
    print("removed", n)


def cmd():
    world = game()
    unreal.SystemLibrary.execute_console_command(world, globals()["CMD"])
    print("ran", globals()["CMD"])


def shot():
    world = game()
    h = her(world)
    origin = h.mesh.get_world_location()
    bone = globals().get("BONE")
    if bone:
        # frame a body part: the origin is that bone on the body that is drawn
        origin = (figure(world)[0] or h.mesh).get_socket_location(bone)
    fwd, right = h.get_actor_forward_vector(), h.get_actor_right_vector()
    pos, target, fov = globals()["VIEW"]
    at = lambda p: origin + fwd * p[0] + right * p[1] + unreal.Vector(0.0, 0.0, p[2])
    cap = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SceneCapture2D) if TAG in [str(t) for t in a.tags]][0]
    cap.set_actor_location_and_rotation(at(pos), unreal.MathLibrary.find_look_at_rotation(at(pos), at(target)), False, True)
    if "LOD" in globals():
        for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
            for sync in a.get_components_by_class(unreal.LODSyncComponent):
                sync.set_editor_property("forced_lod", int(globals()["LOD"]))
    c = cap.capture_component2d
    c.set_editor_property("fov_angle", float(fov))
    c.capture_scene()
    c.capture_scene()
    os.makedirs(OUT, exist_ok=True)
    name = globals().get("NAME", "shot") + ".png"
    unreal.RenderingLibrary.export_render_target(world, c.get_editor_property("texture_target"), OUT, name)
    print("shot", os.path.join(OUT, name))


def bones():
    """Debug: MetaHuman body bones that sit far from their parent compared with the reference pose."""
    world = game()
    body, face = figure(world)
    assert body, "the MetaHuman body is not shown (visex.Body.MetaHuman 1)"
    asset = body.get_editor_property("skeletal_mesh_asset")
    print("body", asset.get_name(), "lod", body.get_predicted_lod_level(), "face lod", face.get_predicted_lod_level() if face else None)
    rows = []
    for i in range(body.get_num_bones()):
        n = body.get_bone_name(i)
        p = body.get_parent_bone(n)
        if str(p) == "None":
            continue
        t = body.get_socket_transform(n, unreal.RelativeTransformSpace.RTS_COMPONENT)
        tp = body.get_socket_transform(p, unreal.RelativeTransformSpace.RTS_COMPONENT)
        d = (t.translation - tp.translation).length()
        rows.append((d, str(n), str(p), t.translation))
    rows.sort(reverse=True)
    for d, n, p, t in rows[:int(globals().get("TOP", 12))]:
        print("%7.2f cm  %s <- %s   at (%.1f, %.1f, %.1f)" % (d, n, p, t.x, t.y, t.z))


MODE = globals().get("MODE", "")
{"prepare": prepare, "play": play, "stop": stop, "clean": clean, "cmd": cmd, "shot": shot, "bones": bones}[MODE]()
