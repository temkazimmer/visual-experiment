"""4K head shots of the card hair in the open editor (hair agent). Through the bridge:

  python3 Tools/ue.py -c "TAG='before'; STYLE='long'
exec(open('<abs>/Tools/hair_shots.py').read())"

Run with Play going in this editor (hair and skin only go on in a game world). Shoots the
player character's head from top, front, 3/4,
side and back to Tools/.bridge/shots/hair_<TAG>_<STYLE>_<view>.png. SceneCapture skips the
post grade: use these to judge the hair, not the overall look.
"""
import os
import unreal

TAG = globals().get("TAG", "before")
STYLE = globals().get("STYLE", "long")
VIEWS = globals().get("VIEWS", ("top", "front", "front34", "side", "back"))
RES = globals().get("RES", (3840, 2160))
MESH = {"long": "/Game/Character/Hair/SKM_HairCards", "elf": "/Game/Character/Hair/SKM_HairElf",
        "short": "/Game/Character/Hair/SKM_HairShort"}[STYLE]

world = unreal.EditorLevelLibrary.get_game_world()
if world is None:
    raise RuntimeError("start Play in this editor first: the hair and skin only go on in a game world")
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
char = unreal.GameplayStatics.get_player_character(world, 0)
comps = {c.get_name(): c for c in char.get_components_by_class(unreal.SkeletalMeshComponent)}
body = comps["Shell"]
hair = comps.get("Hair")
if hair and hair.get_skeletal_mesh_asset().get_path_name().split(".")[0] != MESH:
    # the mesh and its own physics AnimBP, as UVisexBodyComponent::ApplyHair does
    hair.set_skeletal_mesh_asset(unreal.load_asset(MESH))
    hair.set_anim_instance_class(unreal.load_class(None, MESH.replace("SKM_HairCards", "ABP_HairCards").replace("SKM_", "ABP_") + "." + MESH.split("/")[-1].replace("SKM_HairCards", "ABP_HairCards").replace("SKM_", "ABP_") + "_C"))
# FIXBOUNDS: borrow the body's bounds (leader pose: the locks hang at rest, no physics), so a
# shot shows the shading even where the hair's own rest-pose bounds would cull it
if globals().get("FIXBOUNDS", True):
    hair.set_leader_pose_component(body)
else:
    hair.set_bounds_scale(4.0)                        # physics stays on; the bounds just grow
cap =next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SceneCapture2D)
           if a.get_actor_label() == "HairCam")   # placed in the editor level before Play: Python can't spawn into Play
try:
    head = body.get_socket_location("head")
    f, r = char.get_actor_forward_vector(), char.get_actor_right_vector()
    up = unreal.Vector(0, 0, 1)
    ctr = head + unreal.Vector(0, 0, 6)
    eyes = dict(top=ctr + up * 95 + f * 25, front=ctr + f * 95, front34=ctr + (f * 0.7 + r * 0.7) * 95 + up * 12,
                side=ctr + r * 95, back=ctr - f * 95 + up * 10)
    for k_, (d_, dist_) in globals().get("EXTRA", {}).items():          # name -> ((f, r, up), cm)
        v_ = f * d_[0] + r * d_[1] + up * d_[2]
        eyes[k_] = ctr + v_ * (dist_ / max(1e-3, v_.length()))
    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools", ".bridge", "shots")
    os.makedirs(out, exist_ok=True)
    rt = unreal.RenderingLibrary.create_render_target2d(world, RES[0], RES[1], unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
    for name in VIEWS:
        cam = eyes[name]
        rot = unreal.MathLibrary.find_look_at_rotation(cam, ctr)
        cap.set_actor_location_and_rotation(cam, rot, False, False)
        c = cap.capture_component2d
        c.set_editor_property("texture_target", rt)
        c.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_TONE_CURVE_HDR)
        c.set_editor_property("fov_angle", globals().get("FOV", 28.0))
        for _ in range(8):                      # let the temporal dither settle
            c.capture_scene()
        unreal.RenderingLibrary.export_render_target(world, rt, out, "hair_%s_%s_%s.png" % (TAG, STYLE, name))
        print("shot", name)
finally:
    pass
