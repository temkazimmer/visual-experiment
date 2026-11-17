"""Did a re-assembly keep the MetaHuman body's vertex order? The follower's sole offsets, every outfit
cut (skinfit, cover masks) and the bare-skin maps are per body vertex, so they are only good for the
mesh they were made from. Runs in the editor through the bridge.

  (run as: python3 Tools/ue.py -c "import unreal; TAG='before'; exec(open(unreal.Paths.project_dir() + 'Tools/mh_vertex_order.py').read())")
  TAG='before'                     dump body and face LOD 0 (positions, triangles) to Tools/.bridge/vorder_before.json
  TAG='after'; COMPARE='before'    dump again and compare: counts, same triangles, how far vertices moved
"""
import json
import os
import unreal

MESHES = {"body": "/Game/Character/MH/Built/MHC_Her/Body/SKM_MHC_Her_BodyMesh",
          "face": "/Game/Character/MH/Built/MHC_Her/Face/SKM_MHC_Her_FaceMesh"}
TAG = globals().get("TAG", "before")
COMPARE = globals().get("COMPARE")
DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()) + "Tools/.bridge/"


def read(path):
    dm = unreal.DynamicMesh()
    lod = unreal.GeometryScriptMeshReadLOD()
    lod.set_editor_property("lod_type", unreal.GeometryScriptLODType.SOURCE_MODEL)
    lod.set_editor_property("lod_index", 0)
    unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(unreal.load_asset(path), dm,
                                                                  unreal.GeometryScriptCopyMeshFromAssetOptions(), lod)
    vl = unreal.GeometryScript_MeshQueries.get_all_vertex_positions(dm, False)[1]
    tl = unreal.GeometryScript_MeshQueries.get_all_triangle_indices(dm, False)[1]
    V = [(round(v.x, 3), round(v.y, 3), round(v.z, 3)) for v in unreal.GeometryScript_List.convert_vector_list_to_array(vl)]
    T = [(t.x, t.y, t.z) for t in unreal.GeometryScript_List.convert_triangle_list_to_array(tl)]
    return {"v": V, "t": T}


now = {name: read(path) for name, path in MESHES.items()}
json.dump(now, open(DIR + "vorder_%s.json" % TAG, "w"))
for name, m in now.items():
    print("VORDER", TAG, name, len(m["v"]), "vertices", len(m["t"]), "triangles")
if COMPARE:
    old = json.load(open(DIR + "vorder_%s.json" % COMPARE))
    for name, m in now.items():
        o = old[name]
        same_n = len(o["v"]) == len(m["v"])
        same_t = [tuple(t) for t in o["t"]] == [tuple(t) for t in m["t"]]
        moved = sorted((sum((a - b) ** 2 for a, b in zip(p, q)) ** 0.5 for p, q in zip(o["v"], m["v"])), reverse=True) if same_n else []
        print("VORDER", name, "count", "same" if same_n else "CHANGED %d -> %d" % (len(o["v"]), len(m["v"])),
              "| triangles", "same" if same_t else "CHANGED",
              "| moved: max %.3f cm, over 0.01 cm: %d, over 0.5 cm: %d" % (moved[0], sum(d > 0.01 for d in moved), sum(d > 0.5 for d in moved)) if moved else "")
        print("VORDER", name, "ORDER KEPT" if same_n and same_t and moved[0] < 0.5 else "VERTEX ORDER OR SHAPE CHANGED: re-export the skin and re-cut the outfits")
