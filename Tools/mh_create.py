"""Her as a MetaHuman (Docs/AGENT_METAHUMAN.md, stage 1). Re-runnable; each step is safe to repeat.

  python3 Tools/ue.py Tools/mh_create.py                      # create + shape + look
  python3 Tools/ue.py -c "STEPS=['rig']; exec(open(unreal.Paths.project_dir() + 'Tools/mh_create.py').read())"

STEPS (default ['create', 'shape', 'look']):
  fresh     delete the Creator asset first: a new body shape is always a clean pass
            (STEPS=['fresh','create','shape','look','hair','rig','textures','build'])
  create    copy a MetaHuman Creator preset to /Game/Character/MH/MHC_Her (the face is a
            placeholder: PRESET picks it, Tema designs the real one later)
  shape     drive the parametric body toward her measured numbers (TARGETS, in cm)
  look      take the preset's garment off and set a pale skin tone (SKIN_UV)
  hair      wear a MetaHuman groom in black (HAIR, default Hair_L_StraightBangs; HAIR_COLOUR), brows to match
  rig       auto-rig through Epic's cloud (needs an Epic sign-in inside the editor)
  textures  download the high-resolution skin textures from Epic's cloud
  build     assemble game-ready assets under /Game/Character/MH/Built (PIPELINE, QUALITY)
Every step saves the character asset, so a later step can run in a fresh editor.
"""
import unreal

CHAR_DIR = "/Game/Character/MH"
CHAR_NAME = globals().get("CHAR_NAME", "MHC_Her")     # another name makes a scratch copy to try shapes on
CHAR_PATH = CHAR_DIR + "/" + CHAR_NAME
PRESET = globals().get("PRESET", "Ada")
STEPS = globals().get("STEPS", ["create", "shape", "look"])

# Her default body in the MetaHuman Creator's own body constraints (cm). The Creator's tape
# is not ours (its Inseam, Hip and arm lengths are measured differently), so these were
# tuned until Tools/mh_measure.py reads the same on both bodies: see the findings log.
# Constraints not listed stay free and the parametric model fills them in.
TARGETS = globals().get("TARGETS", {
    "Height": 166.8,            # the Creator's height is 2.4 cm more than sole to crown
    "Chest": 93.8,              # our tape reads 93.2
    "Waist": 56.6,
    "Hip": 92.5,
    "Inseam": 70.5,             # reads ~6 cm lower than the crotch height we measure
    "Across Shoulder": 24.3,
    "Upper Arm Length": 32.0,   # shoulder point to elbow, not joint to joint
    "Lower Arm Length": 23.5,
    # Stage 3, "slimmer, keep realism": the limbs and ribcage stage 1 left free. With the
    # tape matched the free model filled them in heavier than her (upper arm 27.7 against
    # her 23.0). The Creator reads its girths 1.5-2 cm under our tape.
    "Thigh": 60.0,              # our tape 61.7 (stage 1: 61.0 -> 62.8)
    "Calf": 37.6,               # (stage 1: 38.2)
    "Knee": 30.5,               # free it sat at 32.4
    "Bicep": 21.5,              # free 25.5; our tape 27.7 -> 23.6
    "Forearm": 19.3,            # free 20.1
    "Wrist": 12.8,              # free 13.5
    "Underbust": 66.5,          # free 67.7; lower and the bust (Chest stays) projects too far
    "Bust Span": 20.5,          # pinned where it sat: free, a lower Underbust pulls it to 18.6
    "Muscularity": -1.0,        # an axis, not cm (-2.3..2.3); free -0.6
    "Hand Circumference": 17.0, # free 19.2: a narrower palm; hand length has no control
})
# Left free on purpose: Fat (-2.8..2.8) and Masculine/Feminine (-2..2). Pinned away from
# where the model puts them, Fat moves volume into the seat and hollows the collarbones
# (Neck Base falls under the Creator's own minimum), and neither changes hand or foot length.

mh = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
eal = unreal.EditorAssetLibrary


def log(*a):
    print("MH", *a)


def save(character):
    eal.save_loaded_asset(character, only_if_is_dirty=False)


def fresh():
    """Throw the Creator asset away so 'create' starts from the preset again. A character
    shaped several times does not come back the same: the solve leans on the state it starts
    from and the head stops following the body at the shoulders (see the stage 3 log). A new
    shape is one pass: fresh, create, shape, look, rig, textures, build."""
    if not eal.does_asset_exist(CHAR_PATH):
        return
    old = unreal.load_asset(CHAR_PATH)
    if mh.is_object_added_for_editing(old):
        mh.remove_object_to_edit(old)
    del old
    if not eal.delete_asset(CHAR_PATH):
        raise RuntimeError("could not delete " + CHAR_PATH)
    log("deleted", CHAR_PATH)
    if unreal.find_object(None, CHAR_PATH + "." + CHAR_NAME):
        # an editor session that assembled this character keeps it in memory after the delete,
        # and 'create' would then pick up the dead object instead of copying the preset
        raise RuntimeError("deleted on disk but still loaded: restart the editor, then run the same STEPS again")


def create():
    if eal.does_asset_exist(CHAR_PATH):
        log("exists", CHAR_PATH)
        return unreal.load_asset(CHAR_PATH)
    src = "/MetaHumanCharacter/Optional/Presets/" + PRESET
    character = eal.duplicate_asset(src, CHAR_PATH)
    if character is None:
        raise RuntimeError("could not copy preset " + src)
    log("created", CHAR_PATH, "from preset", PRESET)
    return character


def constraints(character):
    return {str(c.name): c for c in mh.get_body_constraints(character)}


def shape(character):
    cs = constraints(character)
    before = {k: c.target_measurement for k, c in cs.items()}
    for c in cs.values():           # release what the preset pinned (High Hip, Knee)
        c.is_active = False
    for name, value in TARGETS.items():
        c = cs[name]
        c.is_active = True
        c.target_measurement = max(c.min_measurement, min(c.max_measurement, value))
        if abs(c.target_measurement - value) > 0.05:
            log("clamped %s: wanted %.1f, range %.1f..%.1f" % (name, value, c.min_measurement, c.max_measurement))
    mh.set_body_constraints(character, list(cs.values()))
    mh.commit_body_state(character)
    after = constraints(character)
    for name, c in after.items():
        mark = "*" if c.is_active else " "
        log("%s %-24s %7.1f -> %7.1f   (%.1f..%.1f)" % (mark, name, before[name], c.target_measurement,
                                                      c.min_measurement, c.max_measurement))


SKIN_UV = globals().get("SKIN_UV", (0.12, 0.35))    # the Creator's skin tone picker: pale, like hers


def look(character):
    """Bare body in her skin tone: the preset's garment hides the torso under it."""
    inst = character.internal_collection.default_instance
    inst.set_single_slot_selection("Outfits", unreal.MetaHumanPaletteItemKey())
    settings = character.skin_settings
    skin = settings.skin
    skin.u, skin.v = SKIN_UV
    settings.skin = skin
    mh.commit_skin_settings(character, settings)
    mh.assemble_for_preview(character)
    log("slots:", [str(d.selection.slot_name) for d in inst.get_slot_selection_data()])
    log("skin:", character.skin_settings.skin.u, character.skin_settings.skin.v)


# Her hair is MetaHuman's own (Tema, stage 5): a groom from the Creator's wardrobe, coloured black.
# StraightBangs is the longest straight one (to the mid-back) and has her fringe; the other long straight
# style stops at the shoulders (Docs/Research/metahuman/stage5_hair_pick.jpg). The preset's brows and
# lashes stay and take the same colour.
HAIR = globals().get("HAIR", "Hair_L_StraightBangs")
HAIR_COLOUR = globals().get("HAIR_COLOUR", {"Melanin": 1.0, "Redness": 0.0, "Whiteness": 0.0})
GROOMS = "/MetaHumanCharacter/Optional/Grooms/Bindings/"


def hair(character):
    """Wear HAIR in the Hair slot, in HAIR_COLOUR. Part of the one pass, before rig and build.
    The colour is an instance parameter and only exists once the preview has been assembled."""
    coll = character.internal_collection
    inst = coll.default_instance
    item = unreal.load_asset(GROOMS + "Hair/WI_" + HAIR)
    if item is None:
        raise RuntimeError("no wardrobe item for " + HAIR)
    key = coll.try_add_item_from_wardrobe_item("Hair", item)
    if key is None:
        raise RuntimeError("the collection refused " + HAIR)
    inst.set_single_slot_selection("Hair", key)     # replaces the preset's; try_add would wear both
    mh.assemble_for_preview(character)
    seen = {}
    for p in inst.get_instance_parameters(item_path=unreal.MetaHumanPaletteItemPath(item_key=key)):
        if str(p.name) in HAIR_COLOUR:
            p.set_float(value=HAIR_COLOUR[str(p.name)])
            seen[str(p.name)] = round(p.get_float(), 3)
    if len(seen) != len(HAIR_COLOUR):
        raise RuntimeError("hair colour parameters not found: %s of %s" % (seen, HAIR_COLOUR))
    log("hair:", HAIR, seen)
    for d in inst.get_slot_selection_data():         # brows and lashes in the same black, where they have a colour
        slot = str(d.selection.slot_name)
        if slot in ("Eyebrows", "Eyelashes"):
            got = {}
            for p in inst.get_instance_parameters(item_path=unreal.MetaHumanPaletteItemPath(item_key=d.selection.selected_item)):
                if str(p.name) in HAIR_COLOUR:
                    p.set_float(value=HAIR_COLOUR[str(p.name)])
                    got[str(p.name)] = round(p.get_float(), 3)
            log(slot, d.selection.selected_item.to_asset_name_string(), got or "no colour parameters")
    log("slots:", [(str(d.selection.slot_name), d.selection.selected_item.to_asset_name_string()) for d in inst.get_slot_selection_data()])


# Cloud steps. BLOCKING=True (default) holds the editor until Epic's service answers, which
# is what a batch re-run wants. The first time, run with BLOCKING=False and KEEP=True: if
# no Epic sign-in is cached the plugin opens a browser login and a blocking call would
# freeze the editor behind it. Then poll with STEPS=['status'].
BLOCKING = globals().get("BLOCKING", True)
KEEP = globals().get("KEEP", False)       # leave the character registered for editing


def rig(character):
    params = unreal.MetaHumanCharacterAutoRiggingRequestParams()
    params.blocking = BLOCKING
    params.report_progress = not BLOCKING
    params.rig_type = unreal.MetaHumanRigType.JOINTS_ONLY   # blendshapes: face work, later
    mh.request_auto_rigging(character, params)


def textures(character):
    params = unreal.MetaHumanCharacterTextureRequestParams()
    params.blocking = BLOCKING
    params.report_progress = not BLOCKING
    mh.request_texture_sources(character, params)


def status(character):
    log("high-res textures:", character.has_high_resolution_textures)
    log("can build:", mh.can_build_meta_human(character, True), "(the reason, if not, is in the editor log)")


BUILD_DIR = CHAR_DIR + "/Built"
PIPELINE = globals().get("PIPELINE", "CINEMATIC")     # or OPTIMIZED (+ QUALITY HIGH/MEDIUM/LOW)
QUALITY = globals().get("QUALITY", "HIGH")


def build(character):
    """Assemble game-ready assets: skeletal meshes, materials, textures and a Blueprint."""
    if not mh.can_build_meta_human(character, True):
        raise RuntimeError("not ready to build: needs the rig and the textures steps (see the editor log)")
    params = unreal.MetaHumanCharacterEditorBuildParameters()
    params.pipeline_type = getattr(unreal.MetaHumanDefaultPipelineType, PIPELINE)
    if PIPELINE == "OPTIMIZED":
        params.pipeline_quality = getattr(unreal.MetaHumanQualityLevel, QUALITY)
    params.absolute_build_path = BUILD_DIR
    params.common_folder_path = CHAR_DIR + "/Common"
    params.enable_wardrobe_item_validation = False
    mh.build_meta_human(character, params)
    # the assembly leaves ~240 unsaved packages; save this folder only, never "save all"
    unreal.EditorAssetLibrary.save_directory(CHAR_DIR, only_if_is_dirty=False, recursive=True)
    log("built into", BUILD_DIR, unreal.EditorAssetLibrary.list_assets(BUILD_DIR, True, False)[:6])


def run():
    if "fresh" in STEPS:
        fresh()
    character = create() if "create" in STEPS else unreal.load_asset(CHAR_PATH)
    if not mh.is_object_added_for_editing(character) and not mh.try_add_object_to_edit(character):
        raise RuntimeError("cannot edit %s: is it open in the MetaHuman editor?" % CHAR_PATH)
    try:
        for step in STEPS:
            if step in ("create", "fresh"):
                continue
            log("step", step)
            globals()[step](character)
            if step != "status" and not (KEEP and not BLOCKING):
                save(character)
    finally:
        if not KEEP and mh.is_object_added_for_editing(character):
            mh.remove_object_to_edit(character)
    log("done", STEPS)


run()
