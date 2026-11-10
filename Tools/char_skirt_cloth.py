"""Build the skirt's Chaos cloth asset (CA_Skirt) and its Dataflow graph (DF_Skirt).

  python3 Tools/ue.py Tools/char_skirt_cloth.py

Needs the ChaosClothAsset/ChaosClothAssetEditor plugins and the visexEditor module (the
VisexHeightWeightMap node). Source mesh: SKM_Outfit_Skirt (Tools/char_outfit.py, ONLY=["Skirt"]).
Settings follow the reference skirt read in UE 5.8 (Docs/Research/STUDY_SKIRT_SIM.md).
To rebuild, first delete CA_Skirt, DF_Skirt and PA_Skirt with the editor closed: deleting them
from Python can stop the editor on a references dialog. The factory's unused template graph
(DF_Skirt_*) is left for the same reason; delete it afterwards too.
"""
import unreal

DIR = "/Game/Character/Clothes"
MESH = DIR + "/SKM_Outfit_Skirt"
PHYS = DIR + "/PA_Skirt"            # tapered leg capsules fitted to her skin; no arms, so her hands do not catch the hem
SKIN = "/Game/Character/Android/SKM_AndroidSkin"
FIT = (0.5, 1.0)                    # percentile, scale per end: higher picks up the glutes the thigh drives, and the fat capsule pushed the hem out in a cuff
LEGS = (["thigh_l", "thigh_r", "calf_l", "calf_r"], ["calf_l", "calf_r", "foot_l", "foot_r"])
# cm in the reference pose: pinned over the hip, a little free by the hem (LM: crotch 85.5, hem 79.5).
# Tema 2026-11-10: the skirt follows her shape, only the hem moves, and only slightly.
TOP_Z, BOTTOM_Z, EXPONENT = 93.0, 79.5, 1.5
MAX_DISTANCE = 4.0
BACKSTOP_RADIUS = 50.0              # a big sphere behind each particle: a near-flat wall at its skinned position

eal = unreal.EditorAssetLibrary
lib = unreal.DataflowEditorBlueprintLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

for name in ("CA_Skirt", "DF_Skirt", "PA_Skirt"):
    if eal.does_asset_exist(DIR + "/" + name):
        raise RuntimeError(name + " exists: delete it with the editor closed, then run again")


def step(text):
    unreal.log_warning("char_skirt_cloth: " + text)

pa_asset = eal.duplicate_asset("/Game/Characters/Mannequins/Rigs/PA_Mannequin", PHYS)
print(unreal.VisexClothLibrary.fit_capsules(pa_asset, unreal.load_asset(SKIN), LEGS[0], LEGS[1], FIT[0], FIT[1]))
eal.save_loaded_asset(pa_asset)
step("physics asset done")

df = tools.create_asset("DF_Skirt", DIR, unreal.Dataflow, unreal.DataflowAssetFactory())
x = [0]


def add(kind, label):
    x[0] += 400
    n = lib.add_dataflow_node(df, kind, label, unreal.Vector2D(x[0], 0))
    if not n or str(n) == "None":
        raise RuntimeError("could not add " + kind)
    return n


def prop(node, name, value):
    if not lib.set_dataflow_node_property(df, node, name, value):
        raise RuntimeError("could not set %s.%s = %s" % (node, name, value))


def wv(lo, hi, wmap="", anim=True):
    s = '(bIsAnimatable=%s,Low=%f,High=%f' % (anim, lo, hi)
    return s + (',WeightMap="%s")' % wmap if wmap else ')')


def link(a, b, out="Collection", inp="Collection"):
    if not lib.connect_dataflow_nodes(df, a, out, b, inp):
        raise RuntimeError("could not connect %s.%s -> %s.%s" % (a, out, b, inp))


chain = []
imp = add("FChaosClothAssetSkeletalMeshImportNode_v2", "SkirtImport")
prop(imp, "SkeletalMesh", "/Script/Engine.SkeletalMesh'%s.%s'" % (MESH, MESH.rsplit("/", 1)[1]))
prop(imp, "UVChannel", "-1")                          # no 2D pattern: 3D rest lengths only
chain.append(imp)

hw = add("FVisexHeightWeightMapNode", "MaxDistanceMap")
prop(hw, "TopZ", str(TOP_Z)); prop(hw, "BottomZ", str(BOTTOM_Z)); prop(hw, "Exponent", str(EXPONENT))
chain.append(hw)

md = add("FChaosClothAssetSimulationMaxDistanceConfigNode", "MaxDistance")
prop(md, "MaxDistance", wv(0.0, MAX_DISTANCE, "MaxDistance"))
chain.append(md)

st = add("FChaosClothAssetSimulationStretchConfigNode", "Stretch")      # satin: firm, little give
prop(st, "StretchStiffnessWarp", wv(400.0, 400.0))
prop(st, "StretchStiffnessWeft", wv(450.0, 450.0))
prop(st, "StretchStiffnessBias", wv(150.0, 150.0))
chain.append(st)

be = add("FChaosClothAssetSimulationBendingConfigNode", "Bending")       # soft drape, holds the lobes
prop(be, "BendingStiffnessWarp", wv(2.0, 2.0))
prop(be, "BendingStiffnessWeft", wv(0.1, 0.1))
prop(be, "BendingStiffnessBias", wv(0.15, 0.15))
chain.append(be)

ma = add("FChaosClothAssetSimulationMassConfigNode", "Mass")
prop(ma, "DensityWeighted", "(Low=0.15,High=0.15)")
chain.append(ma)

da = add("FChaosClothAssetSimulationDampingConfigNode", "Damping")
prop(da, "DampingCoefficientWeighted", wv(0.01, 0.01))
chain.append(da)

te = add("FChaosClothAssetSimulationLongRangeAttachmentConfigNode_v2", "Tethers")
chain.append(te)

bs = add("FChaosClothAssetSimulationBackstopConfigNode", "Backstop")     # never inside its skinned surface
prop(bs, "BackstopDistance", wv(0.0, 0.0))
prop(bs, "BackstopRadius", wv(BACKSTOP_RADIUS, BACKSTOP_RADIUS))
chain.append(bs)

co = add("FChaosClothAssetSimulationCollisionConfigNode", "Collision")
prop(co, "CollisionThicknessImported", "(ImportedValue=0.5,bUseImportedValue=False)")
prop(co, "FrictionCoefficientImported", "(ImportedValue=0.8,bUseImportedValue=False)")
prop(co, "bEnableSkinnedTriangleMeshCollisions", "True")
chain.append(co)

so = add("FChaosClothAssetSimulationSolverConfigNode", "Solver")
prop(so, "NumIterations", "2"); prop(so, "MaxNumIterations", "4")
prop(so, "NumSubstepsImported", "(ImportedValue=6,bUseImportedValue=False)")
chain.append(so)

pa = add("FChaosClothAssetSetPhysicsAssetNode", "Physics")
prop(pa, "PhysicsAsset", "/Script/Engine.PhysicsAsset'%s.%s'" % (PHYS, PHYS.rsplit("/", 1)[1]))
chain.append(pa)

for a, b in zip(chain, chain[1:]):
    link(a, b)

term = add("FChaosClothAssetTerminalNode_v2", "SkirtTerminal")
for pin in ("CollectionLods[0]", "CollectionLods_0", "CollectionLods"):
    if lib.connect_dataflow_nodes(df, chain[-1], "Collection", term, pin):
        print("terminal pin", pin)
        break
else:
    raise RuntimeError("could not connect the terminal")
eal.save_loaded_asset(df)
step("graph done")

ca = tools.create_asset("CA_Skirt", DIR, unreal.ChaosClothAsset, unreal.ChaosClothAssetFactory())
made = ca.get_editor_property("dataflow_instance")     # the factory also made a template graph
tmpl = made.get_editor_property("dataflow_asset")
made.set_editor_property("dataflow_asset", df)
made.set_editor_property("dataflow_terminal", term)
ca.set_editor_property("dataflow_instance", made)
step("cloth asset made; unused template graph: %s" % (tmpl.get_path_name() if tmpl else None))
ok = unreal.DataflowBlueprintLibrary.regenerate_asset_from_dataflow(ca)
step("regenerated %s" % ok)
eal.save_loaded_asset(ca)
print("CA_Skirt regenerated", ok, "nodes", chain + [term])
