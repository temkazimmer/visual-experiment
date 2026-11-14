"""Her as a MetaHuman (Docs/AGENT_METAHUMAN.md, stage 1). Re-runnable; each step is safe to repeat.

  python3 Tools/ue.py Tools/mh_create.py                      # create + shape
  python3 Tools/ue.py -c "STEPS=['rig']; exec(open(unreal.Paths.project_dir() + 'Tools/mh_create.py').read())"

STEPS (default ['create', 'shape']):
  create    copy a MetaHuman Creator preset to /Game/Character/MH/MHC_Her (the face is a
            placeholder: PRESET picks it, Tema designs the real one later)
  shape     drive the parametric body toward her measured numbers (TARGETS, in cm)
  rig       auto-rig through Epic's cloud (needs an Epic sign-in inside the editor)
  textures  download the high-resolution skin textures from Epic's cloud
  build     assemble game-ready assets under /Game/Character/MH/Built
Every step saves the character asset, so a later step can run in a fresh editor.
"""
import unreal

CHAR_DIR = "/Game/Character/MH"
CHAR_NAME = "MHC_Her"
CHAR_PATH = CHAR_DIR + "/" + CHAR_NAME
PRESET = globals().get("PRESET", "Ada")
STEPS = globals().get("STEPS", ["create", "shape"])

# Her default body in the MetaHuman Creator's own body constraints (cm). The Creator's tape
# is not ours (its Inseam, Hip and arm lengths are measured differently), so these were
# tuned until Tools/mh_measure.py reads the same on both bodies: see the findings log.
# Constraints not listed stay free and the parametric model fills them in.
TARGETS = globals().get("TARGETS", {
    "Height": 166.8,            # the Creator's height is 2.4 cm more than sole to crown
    "Chest": 91.5,
    "Waist": 56.6,
    "Hip": 93.7,
    "Inseam": 70.5,             # reads ~6 cm lower than the crotch height we measure
    "Across Shoulder": 24.3,
    "Upper Arm Length": 32.0,   # shoulder point to elbow, not joint to joint
    "Lower Arm Length": 23.5,
    "Thigh": 61.0,
    "Calf": 38.2,
})

mh = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
eal = unreal.EditorAssetLibrary


def log(*a):
    print("MH", *a)


def save(character):
    eal.save_loaded_asset(character, only_if_is_dirty=False)


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


def run():
    character = create() if "create" in STEPS else unreal.load_asset(CHAR_PATH)
    if not mh.try_add_object_to_edit(character):
        raise RuntimeError("cannot edit %s: is it open in the MetaHuman editor?" % CHAR_PATH)
    try:
        for step in STEPS:
            if step == "create":
                continue
            log("step", step)
            globals()[step](character)
            save(character)
    finally:
        if mh.is_object_added_for_editing(character):
            mh.remove_object_to_edit(character)
    save(character)
    log("done", STEPS)


run()
