"""Stand the MetaHuman next to the current body in Lvl_Studio and shoot both at 4K.
Nothing is saved: everything is spawned in a Play session and goes away with it.

Run in this order from the worktree (each line is one bridge job):
  python3 Tools/ue.py -c "MODE='prepare'; exec(open(unreal.Paths.project_dir() + 'Tools/mh_stage.py').read())"
  python3 Tools/ue.py -c "MODE='play';    exec(...)"      # starts Play (refuses if one is running)
  python3 Tools/ue.py -c "MODE='setup';   exec(...)"      # places both figures and the camera
  python3 Tools/ue.py -c "MODE='shot'; NAME='full'; exec(...)"   # full | hands | feet | any VIEW
  python3 Tools/ue.py -c "MODE='stop';    exec(...)"      # ends the Play session this script started
  python3 Tools/ue.py -c "MODE='clean';   exec(...)"      # removes the temporary stage actors

Why Play: her default body only exists at runtime (the body component and the anim instance
apply the sliders), so the editor world would show the raw 177.5 cm sculpt instead.
The MetaHuman comes from the assembled Blueprint under /Game/Character/MH/Built if it
exists; otherwise from the Creator's preview meshes (no Epic cloud needed): 'prepare'
asks the MetaHuman subsystem for them and keeps the character registered for editing.
"""
import math
import os
import unreal

CHAR_PATH = "/Game/Character/MH/MHC_Her"
BUILT_BP = "/Game/Character/MH/Built/MHC_Her/BP_MHC_Her"
MODE = globals().get("MODE", "prepare")
GAP = globals().get("GAP", 80.0)            # cm between the two figures
TAG = "MHStage"
YAW = globals().get("YAW", -90.0)       # the assembled Blueprint's facing, if it differs
SHOTS = {       # camera position, look-at point, field of view; she is at y = -GAP/2
    "full": ((560.0, 0.0, 90.0), (0.0, 0.0, 84.0), 33.0),
    "hands": ((170.0, 0.0, 78.0), (0.0, 0.0, 76.0), 34.0),
    "feet": ((150.0, 0.0, 32.0), (0.0, 0.0, 6.0), 36.0),
}

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
mh = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
out_dir = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools", ".bridge")


def spawn(cls, loc, yaw=0.0):
    """Editor world, before Play. Python cannot spawn into a running game world, so the
    stage actors are placed first and Play carries them over. The level is never saved."""
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    a = eas.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
    a.set_editor_property("tags", [TAG])
    name = cls.get_name() if isinstance(cls, unreal.Class) else cls.__name__
    a.set_actor_label("%s_%s_%d" % (TAG, name, len(tagged_editor())))
    return a


def tagged_editor():
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    return [a for a in eas.get_all_level_actors() if TAG in [str(t) for t in a.tags]]


def tagged(world, cls):
    return [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, cls) if TAG in [str(t) for t in a.tags]]


def prepare():
    """Editor world, before Play: the MetaHuman and the capture camera, as temporary actors."""
    assert not les.is_in_play_in_editor(), "stop Play first"
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in tagged_editor():
        eas.destroy_actor(a)
    world = ues.get_editor_world()
    ground = floor_z(world, 0.0, GAP / 2)
    comps = []
    if unreal.EditorAssetLibrary.does_asset_exist(BUILT_BP):
        a = spawn(unreal.load_asset(BUILT_BP).generated_class(), (0.0, GAP / 2, ground), YAW)
        comps = list(a.get_components_by_class(unreal.SkeletalMeshComponent))
        print("MetaHuman: assembled Blueprint", BUILT_BP)
    else:
        character = unreal.load_asset(CHAR_PATH)
        if not mh.is_object_added_for_editing(character):
            assert mh.try_add_object_to_edit(character), "cannot register the character for editing"
        # The Creator's own preview actor (body, face, grooms). The subsystem owns it; Play
        # carries a copy over, which setup() moves into place.
        preview = mh.spawn_meta_human_actor(character)
        comps = list(preview.get_components_by_class(unreal.SkeletalMeshComponent))
        print("MetaHuman: Creator preview actor", [c.get_name() for c in comps])
    for c in comps:
        c.set_editor_property("bounds_scale", 3.0)
    cap = spawn(unreal.SceneCapture2D, (560.0, 0.0, 92.0))
    rt = unreal.RenderingLibrary.create_render_target2d(world, 3840, 2160, unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
    c = cap.capture_component2d
    c.set_editor_property("texture_target", rt)
    c.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_TONE_CURVE_HDR)
    c.set_editor_property("capture_every_frame", False)
    c.set_editor_property("capture_on_movement", False)
    c.set_editor_property("always_persist_rendering_state", True)
    print("stage ready, floor at", ground)


def play():
    assert not les.is_in_play_in_editor(), "a Play session is already running: not ours, leave it"
    les.editor_request_begin_play()
    unreal.mh_stage_started_play = True
    print("play requested")


def stop():
    assert getattr(unreal, "mh_stage_started_play", False), "this script did not start the Play session"
    les.editor_request_end_play()
    unreal.mh_stage_started_play = False
    print("stop requested")


def floor_z(world, x, y):
    hit = unreal.SystemLibrary.line_trace_single(
        world, unreal.Vector(x, y, 300.0), unreal.Vector(x, y, -100.0),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit else 0.0


def setup():
    """In Play: put her next to the MetaHuman, without the suit, and hide the Quinn stand-in."""
    assert les.is_in_play_in_editor() and getattr(unreal, "mh_stage_started_play", False), "run MODE='play' first"
    world = ues.get_game_world()
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SkeletalMeshActor):
        if not tagged(world, unreal.SkeletalMeshActor).count(a):
            a.set_actor_hidden_in_game(True)
    her = unreal.GameplayStatics.get_player_character(world, 0)
    at = her.get_actor_location()
    lift = at.z - floor_z(world, at.x, at.y)
    her.get_component_by_class(unreal.VisexBodyComponent).set_outfit("None")   # bodies, not the suit
    her.set_actor_location_and_rotation(unreal.Vector(0.0, -GAP / 2, floor_z(world, 0.0, -GAP / 2) + lift),
                                        unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0), False, True)
    for c in her.get_components_by_class(unreal.SkeletalMeshComponent):
        c.set_editor_property("bounds_scale", 3.0)
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        if a.get_class().get_name() == "MetaHumanDefaultEditorPipelineActor":
            # the preview body carries its soles 2.4 cm above its origin
            a.set_actor_location_and_rotation(unreal.Vector(0.0, GAP / 2, floor_z(world, 0.0, GAP / 2) - 2.4),
                                              unreal.Rotator(roll=0.0, pitch=0.0, yaw=YAW), False, True)
            print("MetaHuman preview at", a.get_actor_location())
    print("her at", her.get_actor_location(), "stage actors in Play:", len(tagged(world, unreal.Actor)))


def shot():
    """One 4K frame through the stage capture. NAME picks a SHOTS entry, or pass VIEW."""
    world = ues.get_game_world()
    name = globals().get("NAME", "full")
    pos, target, fov = globals().get("VIEW") or SHOTS[name]
    cap = tagged(world, unreal.SceneCapture2D)[0]
    loc = unreal.Vector(*pos)
    cap.set_actor_location_and_rotation(
        loc, unreal.MathLibrary.find_look_at_rotation(loc, unreal.Vector(*target)), False, True)
    c = cap.capture_component2d
    c.set_editor_property("fov_angle", fov)
    for _ in range(globals().get("FRAMES", 16)):     # let Lumen and the temporal AA settle
        c.capture_scene()
    # Play duplicated the render target along with the level: export the one this camera owns
    unreal.RenderingLibrary.export_render_target(world, c.get_editor_property("texture_target"), out_dir, "mh_%s.png" % name)
    print("shot", os.path.join(out_dir, "mh_%s.png" % name))


def clean():
    assert not les.is_in_play_in_editor(), "stop Play first"
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in tagged_editor():
        eas.destroy_actor(a)
    character = unreal.load_asset(CHAR_PATH)
    if not globals().get("KEEP") and mh.is_object_added_for_editing(character):
        mh.remove_object_to_edit(character)      # also removes the Creator's preview actor
    print("stage cleared; the level is dirty but unchanged: do not save it")


{"prepare": prepare, "play": play, "setup": setup, "shot": shot, "stop": stop, "clean": clean}[MODE]()
