"""Head turnaround of a hairstyle in the open editor, without Play (hair agent). Through the bridge:

  python3 Tools/ue.py -c "STYLE='short'; TAG='v1'
exec(open('<abs>/Tools/hair_preview.py').read())"

Stands her skin and the style's hair (rest shape, no physics) in the open air above the menu
spot, under the level's own sun and haze, and shoots front, side, back, top and 3/4 to
Tools/.bridge/shots/prev_<TAG>_<STYLE>_<view>.png. The temporary actors are removed again and
the level is not saved. SceneCapture skips the post grade: judge the hair, not the grade.
Use Tools/hair_shots.py for the same views in Play, with physics.
"""
import os
import unreal

STYLE = globals().get("STYLE", "short")
TAG = globals().get("TAG", "v1")
VIEWS = globals().get("VIEWS", ("front", "side", "back", "top", "front34"))
RES = globals().get("RES", (3840, 2160))
SPOT = globals().get("SPOT", (9500.0, -900.0, 700.0))
YAW = globals().get("YAW", 0.0)                     # her facing, degrees (0: front = +Y)
MESH = {"long": "SKM_HairCards", "elf": "SKM_HairElf", "short": "SKM_HairShort"}[STYLE]

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not les.is_in_play_in_editor(), "Play is running: this shoots the editor world"
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
made = []
try:
    rot = unreal.Rotator(roll=0, pitch=0, yaw=YAW)
    skin = eas.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(*SPOT), rot)
    made.append(skin)
    sc = skin.skeletal_mesh_component
    sc.set_skeletal_mesh_asset(unreal.load_asset("/Game/Character/Android/SKM_AndroidSkin"))
    sc.set_collision_profile_name("NoCollision")
    for i, slot in enumerate(sc.get_material_slot_names()):     # the skin's own old hair: hidden, as in game
        if "hair" in str(slot).lower():
            sc.show_material_section(i, i, False, 0)
    hair = eas.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(*SPOT), rot)
    made.append(hair)
    hc = hair.skeletal_mesh_component
    hc.set_skeletal_mesh_asset(unreal.load_asset("/Game/Character/Hair/" + MESH))
    hc.set_collision_profile_name("NoCollision")
    for c_ in (sc, hc):
        c_.set_editor_property("bounds_scale", 3.0)
        # full-resolution textures before the first shot: a freshly imported strand atlas streams
        # in late, and its low mips turn the cards into solid feathers (2026-11-14)
        c_.prestream_textures(30.0, True, 0)
    head = sc.get_socket_location("head")
    ctr = head + unreal.Vector(0, 0, globals().get("UP", 7.0))
    f, r, up = skin.get_actor_right_vector(), skin.get_actor_forward_vector() * -1.0, unreal.Vector(0, 0, 1)
    D = globals().get("DIST", 110.0)
    eyes = dict(front=ctr + f * D, side=ctr + r * D, back=ctr - f * D + up * 4, top=ctr + up * D * 0.85 + f * D * 0.45,
                front34=ctr + (f * 0.72 - r * 0.7) * D + up * 8)
    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools", ".bridge", "shots")
    os.makedirs(out, exist_ok=True)
    rt = unreal.RenderingLibrary.create_render_target2d(world, RES[0], RES[1], unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
    cap = eas.spawn_actor_from_class(unreal.SceneCapture2D, eyes["front"])
    made.append(cap)
    c = cap.capture_component2d
    c.set_editor_property("texture_target", rt)
    c.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_TONE_CURVE_HDR)
    c.set_editor_property("fov_angle", globals().get("FOV", 30.0))
    c.set_editor_property("capture_every_frame", False)
    c.set_editor_property("capture_on_movement", False)
    for name in VIEWS:
        cam = eyes[name]
        cap.set_actor_location_and_rotation(cam, unreal.MathLibrary.find_look_at_rotation(cam, ctr), False, False)
        for _ in range(8):                          # let the temporal dither and Lumen settle
            c.capture_scene()
        unreal.RenderingLibrary.export_render_target(world, rt, out, "prev_%s_%s_%s.png" % (TAG, STYLE, name))
        print("shot", name)
finally:
    for a in made:
        eas.destroy_actor(a)
