"""The Body page's shape sliders on the MetaHuman body: morph targets on the assembled body mesh.

The assembly writes SKM_MHC_Her_BodyMesh without morph targets and a re-assembly rewrites it, so this
runs after every assembly (like Tools/mh_bare_skin.py). In the editor, through the bridge:

  python3 Tools/ue.py -c "STEP='apply'; exec(open('<project>/Tools/mh_morphs.py').read())"

STEP
  apply     (default) add every morph in SourceArt/Character/MH/Morphs/mh_morphs.json to body LOD 0
            and save the mesh (only that asset). Stops if the body's vertices are not the ones the
            deltas were made on (a new shape: make the deltas again, see below).
  base      write the body's LOD 0 vertices and triangles, in the mesh's own vertex order, to
            Saved/MH/morphs/base.json (what the Blender side matches its copy of the skin to)
  variants  one Creator body variant on a scratch character (VARIANT='bust', of VARIANTS below; run
            'base' first): a fresh copy of the preset shaped once, its preview body read in the same
            vertex order -> Saved/MH/morphs/var_<name>.json

Making the deltas (only for a new body shape; the json is committed):
  1. STEP='base', then STEP='variants' once per name in VARIANTS    (editor)
  2. Blender -b --factory-startup --python Tools/blender_outfit.py -- SourceArt/Character/MH/mh_skin.fbx \
         SourceArt/Character/MH/Outfits --body=mh --mh-morphs      (Tools/mh_morph_build.py: variants and the
         old body's shape keys through the kit's landmark map -> mh_morphs.json)
  3. STEP='apply', then Tools/mh_vertex_order.py and the spike tour
Tools/mh_after_build.sh runs all of it in order after an assembly.
"""
import json
import os
import time
import unreal

STEP = globals().get("STEP", "apply")
BODY = "/Game/Character/MH/Built/MHC_Her/Body/SKM_MHC_Her_BodyMesh"
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
WORK = os.path.join(PROJECT, "Saved", "MH", "morphs")
MORPHS = os.path.join(PROJECT, "SourceArt", "Character", "MH", "Morphs", "mh_morphs.json")
AU = unreal.GeometryScript_AssetUtils
GL = unreal.GeometryScript_List
os.makedirs(WORK, exist_ok=True)


def body_mesh():
    """(skeletal mesh, dynamic mesh of LOD 0's source model, its vertex positions)."""
    skm = unreal.load_asset(BODY)
    dm = unreal.DynamicMesh()
    lod = unreal.GeometryScriptMeshReadLOD()
    lod.set_editor_property("lod_type", unreal.GeometryScriptLODType.SOURCE_MODEL)
    lod.set_editor_property("lod_index", 0)
    AU.copy_mesh_from_skeletal_mesh(skm, dm, unreal.GeometryScriptCopyMeshFromAssetOptions(), lod)
    P = list(GL.convert_vector_list_to_array(unreal.GeometryScript_MeshQueries.get_all_vertex_positions(dm, False)[1]))
    return skm, dm, P


def step_base():
    skm, dm, P = body_mesh()
    T = GL.convert_triangle_list_to_array(unreal.GeometryScript_MeshQueries.get_all_triangle_indices(dm, False)[1])
    out = {"v": [(round(p.x, 4), round(p.y, 4), round(p.z, 4)) for p in P], "t": [(t.x, t.y, t.z) for t in T]}
    json.dump(out, open(os.path.join(WORK, "base.json"), "w"))
    print("MORPHS base:", len(out["v"]), "vertices", len(out["t"]), "triangles ->", os.path.join(WORK, "base.json"))


def step_apply():
    data = json.load(open(MORPHS))
    skm, dm, P = body_mesh()
    assert len(P) == data["vertices"], "the body has %d vertices, the deltas were made on %d" % (len(P), data["vertices"])
    worst = max(abs(P[i].x - x) + abs(P[i].y - y) + abs(P[i].z - z) for i, x, y, z in data["check"])
    assert worst < 0.02, "the body's vertices moved (%.2f cm at a check vertex): a new shape needs new deltas" % worst
    opts = unreal.GeometryScriptCopyMorphTargetToAssetOptions()
    opts.set_editor_property("overwrite_existing_target", True)
    opts.set_editor_property("emit_transaction", False)
    lod = unreal.GeometryScriptMeshWriteLOD()
    lod.set_editor_property("lod_index", 0)
    t0 = time.time()
    for name, m in data["morphs"].items():
        Q = [unreal.Vector(p.x, p.y, p.z) for p in P]
        d = m["d"]
        far = 0.0
        for k, i in enumerate(m["i"]):
            Q[i] = unreal.Vector(P[i].x + d[3 * k], P[i].y + d[3 * k + 1], P[i].z + d[3 * k + 2])
            far = max(far, (d[3 * k] ** 2 + d[3 * k + 1] ** 2 + d[3 * k + 2] ** 2) ** 0.5)
        unreal.GeometryScript_MeshEdits.set_all_mesh_vertex_positions(dm, GL.convert_array_to_vector_list(Q))
        out = AU.copy_morph_target_to_skeletal_mesh(dm, skm, name, opts, lod)
        assert out[1] == unreal.GeometryScriptOutcomePins.SUCCESS, "could not add " + name
        print("MORPHS %-18s %6d vertices, %.2f cm at most" % (name, len(m["i"]), far))
    unreal.EditorAssetLibrary.save_loaded_asset(skm, only_if_is_dirty=False)     # this asset only
    names = [t.get_name() for t in skm.get_editor_property("morph_targets")]
    user = [u.get_class().get_name() for u in (skm.get_editor_property("asset_user_data") or []) if u]
    print("MORPHS applied %d in %.0f s; the mesh now has %d: %s; user data %s" % (len(data["morphs"]), time.time() - t0, len(names), names, user))
    assert "DNAAsset" in user, "the body lost its DNA user data: RigLogic would stop driving the correctives"


# Creator body variants. Each is one fresh copy of the preset, shaped once like the build
# (TARGETS of Tools/mh_create.py), with some controls let go ("free") and some moved ("set", cm or
# axis units, added to what the built body measures). A morph is one variant less another
# (Tools/mh_morph_build.py pairs them), so both ends come from the same kind of solve.
VARIANTS = globals().get("VARIANTS", {
    "base":   {},                                                       # the built body again
    "bust":   {"set": {"Chest": -9.0}},                                 # Underbust, Bust Span, Waist stay pinned
    "span":   {"set": {"Bust Span": 3.0}},
    "apex":   {"set": {"Shoulder to Apex": 3.0}},
    "hip":    {"set": {"Hip": 7.0, "High Hip": 0.0}},                   # seat out, the high hip held
    "thigh":  {"set": {"Thigh": -6.0}},
    "fat0":   {"free": "girths", "set": {"Fat": 0.0}},                  # girths let go so fat has somewhere to go
    "fat1":   {"free": "girths", "set": {"Fat": 1.2}},
})
GIRTHS = ("Chest", "Waist", "Hip", "Thigh", "Calf", "Knee", "Bicep", "Forearm", "Wrist", "Underbust", "Bust Span", "Hand Circumference")
PRESET = "/MetaHumanCharacter/Optional/Presets/Ada"
SCRATCH = "/Game/Character/MH/Scratch"


def _targets():
    """TARGETS of Tools/mh_create.py, read from its source so the two cannot drift."""
    src = open(os.path.join(PROJECT, "Tools", "mh_create.py")).read()
    a = src.index('TARGETS = globals().get("TARGETS", {') + len('TARGETS = globals().get("TARGETS", ')
    return eval(src[a:src.index("})", a) + 1])


def step_variant(name):
    mh = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
    eal = unreal.EditorAssetLibrary
    spec = VARIANTS[name]
    measured = json.load(open(os.path.join(WORK, "var_base.json")))["m"] if name != "base" else {}
    want = dict(_targets())
    for k in (GIRTHS if spec.get("free") == "girths" else spec.get("free", ())):
        want.pop(k, None)
    for k, dv in spec.get("set", {}).items():
        want[k] = measured[k] + dv
    path = "%s/MHV_%s_%d" % (SCRATCH, name, int(time.time()))
    character = eal.duplicate_asset(PRESET, path)
    assert character and mh.try_add_object_to_edit(character), "no scratch character for " + name
    actor = None
    try:
        cs = {str(c.name): c for c in mh.get_body_constraints(character)}
        for c in cs.values():
            c.is_active = False
        for k, v in want.items():
            cs[k].is_active = True
            cs[k].target_measurement = max(cs[k].min_measurement, min(cs[k].max_measurement, v))
            if abs(cs[k].target_measurement - v) > 0.05:
                print("MORPHS %s: %s clamped, wanted %.1f, range %.1f..%.1f" % (name, k, v, cs[k].min_measurement, cs[k].max_measurement))
        mh.set_body_constraints(character, list(cs.values()))
        mh.commit_body_state(character)
        got = {str(c.name): round(c.target_measurement, 3) for c in mh.get_body_constraints(character)}
        actor = mh.spawn_meta_human_actor(character)
        body = next(c for c in actor.get_components_by_class(unreal.SkeletalMeshComponent) if c.get_name() == "Body")
        dm = unreal.DynamicMesh()
        lod = unreal.GeometryScriptMeshReadLOD()
        lod.set_editor_property("lod_type", unreal.GeometryScriptLODType.SOURCE_MODEL)
        lod.set_editor_property("lod_index", 0)
        AU.copy_mesh_from_skeletal_mesh(body.get_skinned_asset(), dm, unreal.GeometryScriptCopyMeshFromAssetOptions(), lod)
        P = GL.convert_vector_list_to_array(unreal.GeometryScript_MeshQueries.get_all_vertex_positions(dm, False)[1])
        bones = {b: tuple(round(x, 3) for x in body.get_socket_location(b).to_tuple()) for b in ("pelvis", "spine_05", "thigh_l", "upperarm_l", "head")}
        out = {"v": [(round(p.x, 4), round(p.y, 4), round(p.z, 4)) for p in P], "m": got, "want": want, "bones": bones}
        json.dump(out, open(os.path.join(WORK, "var_%s.json" % name), "w"))
        print("MORPHS variant %s: %d vertices; %s" % (name, len(out["v"]), {k: (round(measured.get(k, 0), 1), got[k]) for k in spec.get("set", {})}))
    finally:
        if actor:
            unreal.get_editor_subsystem(unreal.EditorActorSubsystem).destroy_actor(actor)
        mh.remove_object_to_edit(character)
        del character
        eal.delete_asset(path)


if STEP == "base":
    step_base()
elif STEP == "variants":
    step_variant(globals().get("VARIANT", "base"))      # one per bridge job: VARIANT='bust'
elif STEP == "apply":
    step_apply()
