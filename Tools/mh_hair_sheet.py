"""MetaHuman hairstyles on her, for the contact sheet (stage 5). Runs in the editor through the bridge,
on a scratch copy of the Creator asset that is never saved; MHC_Her is not touched. No assembly: the
Creator's preview actor wears each style.

  MODE='list'      wardrobe items per groom slot (what the Creator can put on her)
  MODE='prepare'   scratch copy MHC_HairTry, registered for editing, preview actor in the level
  MODE='style'     STYLE='Hair_L_Straight' (or SLOT='Eyebrows'; STYLE='Eyebrows_M_Thin'): wear it, black
  MODE='shot'      NAME='Hair_L_Straight': front 3/4 and back, PNGs in Tools/.bridge/hair/
  MODE='clean'     preview actor and scratch copy go (nothing was saved)

Wear and shoot in separate jobs: the groom binds a few frames after the selection.
Driver (host side): Tools/mh_hair_sheet.sh; the sheets: Tools/sheet.swift.
"""
import os
import unreal

eal = unreal.EditorAssetLibrary
mh = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

SRC = "/Game/Character/MH/MHC_Her"
TRY = "/Game/Character/MH/MHC_HairTry"
BINDINGS = "/MetaHumanCharacter/Optional/Grooms/Bindings/"
SLOT_DIRS = {"Hair": "Hair", "Eyebrows": "Eyebrows", "Eyelashes": "Eyelashes", "Peachfuzz": "Peachfuzz",
             "Beard": "Beards", "Mustache": "Mustaches"}
MODE = globals().get("MODE", "list")
SLOT = globals().get("SLOT", "Hair")
OUT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()) + "Tools/.bridge/hair"

# her black: all melanin, no red, no grey. Lightness is left as the style ships it.
BLACK = globals().get("BLACK", {"Melanin": 1.0, "Redness": 0.0, "Whiteness": 0.0})


def log(*a):
    print("MHHAIR", *a)


def items(slot):
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    flt = unreal.ARFilter(package_paths=[BINDINGS + SLOT_DIRS[slot]],
                          class_paths=[unreal.MetaHumanWardrobeItem.static_class().get_class_path_name()])
    return sorted(str(a.asset_name)[3:] for a in reg.get_assets(flt))


def list_all():
    for slot in SLOT_DIRS:
        names = items(slot)
        log(slot, len(names), " ".join(names))


def character():
    c = unreal.load_asset(TRY) if eal.does_asset_exist(TRY) else eal.duplicate_asset(SRC, TRY)
    if c is None:
        raise RuntimeError("no scratch copy of " + SRC)
    if not mh.is_object_added_for_editing(c) and not mh.try_add_object_to_edit(c):
        raise RuntimeError("cannot edit the scratch copy")
    return c


def preview(c):
    act = getattr(unreal, "s5_preview", None)
    try:
        act.get_actor_location()
    except Exception:
        act = mh.spawn_meta_human_actor(c)
        unreal.s5_preview = act
    return act


def prepare():
    c = character()
    act = preview(c)
    # facing +X, soles on the floor (the preview carries them 2.4 cm over its origin)
    act.set_actor_location_and_rotation(unreal.Vector(0.0, 0.0, -2.4), unreal.Rotator(roll=0.0, pitch=0.0, yaw=-90.0), False, True)
    for a in eas.get_all_level_actors():
        label = a.get_actor_label()
        if a != act and (isinstance(a, (unreal.SkeletalMeshActor, unreal.Character)) or "Mannequin" in label or "Quinn" in label):
            a.set_is_temporarily_hidden_in_editor(True)
    for comp in act.get_components_by_class(unreal.MeshComponent):
        comp.set_editor_property("bounds_scale", 3.0)
    # the main view has to look at her: it is the one that computes the strands the captures show
    eye = unreal.Vector(220.0, 150.0, 150.0)
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(
        eye, unreal.MathLibrary.find_look_at_rotation(eye, unreal.Vector(0.0, 0.0, 140.0)))
    log("preview", act.get_actor_label(), [(k.get_name(), k.get_class().get_name()) for k in act.get_components_by_class(unreal.SceneComponent)])


def wear(slot, style):
    """Select a wardrobe item in a slot on the scratch copy and colour it black."""
    c = character()
    inst = c.internal_collection.default_instance
    keys = getattr(unreal, "s5_keys", None)
    if keys is None:
        keys = unreal.s5_keys = {}
    if (slot, style) not in keys:
        item = unreal.load_asset(BINDINGS + SLOT_DIRS[slot] + "/WI_" + style)
        if item is None:
            raise RuntimeError("no wardrobe item WI_" + style)
        keys[(slot, style)] = c.internal_collection.try_add_item_from_wardrobe_item(slot, item)
    key = keys[(slot, style)]
    if key is None:
        raise RuntimeError("the collection refused WI_" + style)
    inst.set_single_slot_selection(slot, key)
    mh.assemble_for_preview(character=c)
    was = {}
    for p in inst.get_instance_parameters(item_path=unreal.MetaHumanPaletteItemPath(item_key=key)):
        name = str(p.name)
        if name in BLACK:
            was[name] = round(p.get_float(), 3)
            p.set_float(value=BLACK[name])
        elif name in ("Lightness", "Roughness"):
            was[name] = round(p.get_float(), 3)
    log("wears", slot, style, "shipped colour", was, "->", BLACK)


# camera: degrees round her from the front, distance, height, horizontal field of view; the frame
# runs from over the crown to the armpits (z 120..180), so a long style shows where it ends
VIEWS = globals().get("VIEWS", {"front": (35.0, 420.0, 150.0, 6.6), "back": (180.0, 420.0, 150.0, 6.6)})
SIZE = globals().get("SIZE", (720, 900))
FRAMES = globals().get("FRAMES", 16)     # captures per shot: temporal AA resolves the strands
SKIN = globals().get("SKIN", 0.30)       # back-of-the-head luminance over this is bare skin
GREY = globals().get("GREY", 0.25)       # the card's emissive level
BACK = 120.0      # cm from her to the grey card


def unthrottle(on=True):
    """A scene capture never runs the groom's strand interpolation (the renderer skips it for
    captures), it shows what the main view last computed. A background editor does not draw its
    viewport ("Use Less CPU when in Background"), so the head stays bald in every capture until
    that is off. Set on the settings object in memory only, and put back by 'clean'."""
    perf = unreal.find_object(None, "/Script/UnrealEd.Default__EditorPerformanceSettings")
    perf.set_editor_property("bThrottleCPUWhenNotForeground", not on)
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_set_viewport_realtime(True)


def rig():
    """A capture camera per view, kept between shots (its history lets the strands resolve), and a
    grey card behind her in each: black hair on the studio's black reads as nothing."""
    import math
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    made = getattr(unreal, "s5_rig", None) or {}
    for a in [x for pair in made.values() for x in pair[:2]]:
        try:
            eas.destroy_actor(a)
        except Exception:
            pass
    made = {}
    for view, (deg, dist, z, fov) in VIEWS.items():
        d = unreal.Vector(math.cos(math.radians(deg)), math.sin(math.radians(deg)), 0.0)
        loc = unreal.Vector(dist * d.x, dist * d.y, z)
        rot = unreal.MathLibrary.find_look_at_rotation(loc, unreal.Vector(0.0, 0.0, z))
        cap = eas.spawn_actor_from_class(unreal.SceneCapture2D, loc, rot)
        rt = unreal.RenderingLibrary.create_render_target2d(world, SIZE[0], SIZE[1], unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
        comp = cap.capture_component2d
        comp.set_editor_property("texture_target", rt)
        comp.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_TONE_CURVE_HDR)
        comp.set_editor_property("fov_angle", fov)
        comp.set_editor_property("capture_every_frame", False)
        comp.set_editor_property("capture_on_movement", False)
        comp.set_editor_property("always_persist_rendering_state", True)
        card = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(-BACK * d.x, -BACK * d.y, z),
                                          unreal.Rotator(roll=0.0, pitch=-90.0, yaw=deg))
        smc = card.static_mesh_component
        smc.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
        smc.set_collision_profile_name("NoCollision")
        smc.set_cast_shadow(False)
        mid = smc.create_dynamic_material_instance(0, unreal.load_asset("/Engine/EngineMaterials/EmissiveMeshMaterial"))
        mid.set_vector_parameter_value("Color", unreal.LinearColor(GREY, GREY, GREY, 1.0))
        card.set_actor_scale3d(unreal.Vector(4.0, 4.0, 1.0))
        made[view] = (cap, card, rt)
    unreal.s5_rig = made
    log("rig", list(made))


def shot(name):
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    os.makedirs(OUT, exist_ok=True)
    made = unreal.s5_rig
    for view, (cap, card, rt) in made.items():
        for other, pair in made.items():      # only this view's card, or the other stands in front of her
            pair[1].set_is_temporarily_hidden_in_editor(other != view)
        for _ in range(FRAMES):
            cap.capture_component2d.capture_scene()
        unreal.RenderingLibrary.export_render_target(world, rt, OUT, "%s_%s.png" % (name, view))
    log("shot", name, list(made), "back of the head:", back_of_head())


def back_of_head():
    """HAIR or SKIN, read from the back view where the skull is (5 cm over the frame's middle). The
    groom shows a few frames after it is worn, and a shot taken too early is bald: the driver
    shoots again while this says SKIN (a shaved style says SKIN every time; three tries, then on)."""
    made = unreal.s5_rig
    if "back" not in made:
        return "?"
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    rt = made["back"][2]
    tall = VIEWS["back"][1] * 2.0 * __import__("math").tan(__import__("math").radians(VIEWS["back"][3] / 2.0)) * SIZE[1] / SIZE[0]
    lum = []
    for dx in (-0.03, 0.0, 0.03):
        for up in (2.0, 5.0, 8.0):
            c = unreal.RenderingLibrary.read_render_target_raw_uv(world, rt, 0.5 + dx, 0.5 - up / tall)
            lum.append((0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b) / 255.0)
    mean = sum(lum) / len(lum)
    return "%s (%.3f)" % ("SKIN" if mean > SKIN else "HAIR", mean)


def clean():
    unthrottle(False)
    for pair in (getattr(unreal, "s5_rig", None) or {}).values():
        for a in pair[:2]:
            try:
                eas.destroy_actor(a)
            except Exception:
                pass
    unreal.s5_rig = None
    act = getattr(unreal, "s5_preview", None)
    try:
        eas.destroy_actor(act)
    except Exception:
        pass
    unreal.s5_preview = None
    unreal.s5_keys = None
    if eal.does_asset_exist(TRY):
        c = unreal.load_asset(TRY)
        if mh.is_object_added_for_editing(c):
            mh.remove_object_to_edit(c)
        del c
        log("scratch copy deleted:", eal.delete_asset(TRY))


if MODE == "list":
    list_all()
elif MODE == "prepare":
    unthrottle()
    prepare()
    rig()
elif MODE == "rig":
    rig()
elif MODE == "style":
    wear(SLOT, globals()["STYLE"])
elif MODE == "shot":
    shot(globals().get("NAME", "try"))
elif MODE == "clean":
    clean()
