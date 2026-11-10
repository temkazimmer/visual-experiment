"""Build /Game/Character/Hair/ABP_HairCards: Copy Pose From Mesh (attached parent) -> Kawaii Physics -> Output.

  python3 Tools/ue.py Tools/char_hair_physics.py

Needs the visexTools editor module (unreal.VisexAnimGraphTools) and the KawaiiPhysics plugin.
Tuned for weight, not bounce: the hair swings late and settles (Docs/Research/HAIR.md).
The hair mesh goes on the character as a child of the body mesh with this anim class.
"""
import unreal

PHYS = dict(
    damping=0.12,           # velocity loss per step (higher = calmer)
    stiffness=0.04,         # pull back to the animated shape
    world_damp_loc=0.75,    # how much the hair ignores world motion of the head
    world_damp_rot=0.75,
    radius=1.2,             # bone collision radius, cm
    limit_angle=40.0,       # max bend per bone, degrees
    gravity=(0, 0, -400),   # cm/s^2, on top of the rest shape
)
ROOTS = ["hair_b%d_01" % i for i in range(6)] + ["hair_s0_01", "hair_s1_01"] + ["hair_f%d_01" % i for i in range(3)]
# Colliders in component space (cm, UE axes: -Y is her front), measured off SKM_AndroidSkin with
# ~1 cm to spare (the clothes clearance). The script converts them to offsets on the driving bone.
# (bone, centre, radius) and (bone, centre, axis "Z" vertical / "X" across, radius, length)
SPHERES = [("head", (0, 0.5, 168.5), 8.0)]
CAPSULES = [
    ("neck_01", (0, -1.5, 155), "Z", 5.5, 10.0),
    ("spine_05", (0, -1.0, 141), "X", 9.5, 24.0),     # shoulders and upper back, across
    ("spine_04", (0, 0.0, 129), "Z", 9.3, 10.0),      # lower back
]
DIR = "/Game/Character/Hair"
NAME = "ABP_HairCards"

assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "stop Play first"
T = unreal.VisexAnimGraphTools
eal = unreal.EditorAssetLibrary
path = DIR + "/" + NAME
if eal.does_asset_exist(path):
    abp = unreal.load_asset(path)
else:
    f = unreal.AnimBlueprintFactory()
    f.set_editor_property("target_skeleton", unreal.load_asset(DIR + "/SK_HairCards"))
    f.set_editor_property("preview_skeletal_mesh", unreal.load_asset(DIR + "/SKM_HairCards"))
    f.set_editor_property("parent_class", unreal.AnimInstance)
    abp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, DIR, unreal.AnimBlueprint, f)
assert abp, "could not create " + path

T.clear_anim_graph(abp)
out = T.get_output_node(abp)
copy = T.add_node(abp, unreal.load_class(None, "/Script/AnimGraph.AnimGraphNode_CopyPoseFromMesh"), -900, 0)
kawaii = T.add_node(abp, unreal.load_class(None, "/Script/KawaiiPhysicsEd.AnimGraphNode_KawaiiPhysics"), -400, 0)
assert out and copy and kawaii, "node creation failed"


def setv(node, p, v):
    assert T.set_node_value(node, p, v), "could not set %s = %s" % (p, v)


def vec(v):
    return "(X=%g,Y=%g,Z=%g)" % v


def rot(r):          # (pitch, yaw, roll)
    return "(Pitch=%g,Yaw=%g,Roll=%g)" % r


# ---- colliders: component space -> driving-bone offsets (Kawaii adds them in parent-bone space)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
probe = eas.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(0, 0, -50000))
pc = probe.skeletal_mesh_component
pc.set_skeletal_mesh_asset(unreal.load_asset(DIR + "/SKM_HairCards"))
CS = unreal.RelativeTransformSpace.RTS_COMPONENT


def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)


def qt(t):
    q = t.rotation
    return (q.x, q.y, q.z, q.w)


def offset(bone, centre, axis="Z"):
    """(offset location, offset rotator) that puts the bone's collider at centre, axis along Z or X."""
    parent = pc.get_parent_bone(bone)
    P_ = pc.get_socket_transform(parent, CS)
    B = pc.get_socket_transform(bone, CS)
    rot = unreal.Rotator(roll=0, pitch=90 if axis == "X" else 0, yaw=0)
    D = unreal.Transform(unreal.Vector(*centre), rot, unreal.Vector(1, 1, 1))
    L = unreal.MathLibrary.make_relative_transform(B, P_)
    L2 = unreal.MathLibrary.make_relative_transform(D, P_)
    off = L2.translation - L.translation
    lx, ly, lz, lw = qt(L)
    ro = qmul(qt(L2), (-lx, -ly, -lz, lw))
    r = unreal.Quat(*ro).rotator()
    return (off.x, off.y, off.z), (r.pitch, r.yaw, r.roll)


sph = [(b, offset(b, c)[0], r) for b, c, r in SPHERES]
cap = [(b,) + offset(b, c, ax) + (rad, ln) for b, c, ax, rad, ln in CAPSULES]
eas.destroy_actor(probe)

setv(copy, "Node.bUseAttachedParent", "True")
setv(copy, "Node.bCopyCurves", "False")
setv(kawaii, "Node.RootBone", '(BoneName="%s")' % ROOTS[0])
setv(kawaii, "Node.AdditionalRootBones", "(" + ",".join('(RootBone=(BoneName="%s"))' % b for b in ROOTS[1:]) + ")")
setv(kawaii, "Node.PhysicsSettings",
     "(Damping=%g,Stiffness=%g,WorldDampingLocation=%g,WorldDampingRotation=%g,Radius=%g,LimitAngle=%g)" % (
         PHYS["damping"], PHYS["stiffness"], PHYS["world_damp_loc"], PHYS["world_damp_rot"], PHYS["radius"],
         PHYS["limit_angle"]))
setv(kawaii, "Node.Gravity", vec(PHYS["gravity"]))
setv(kawaii, "Node.SphericalLimits", "(" + ",".join(
    '(DrivingBone=(BoneName="%s"),OffsetLocation=%s,Radius=%g)' % (b, vec(o), r) for b, o, r in sph) + ")")
setv(kawaii, "Node.CapsuleLimits", "(" + ",".join(
    '(DrivingBone=(BoneName="%s"),OffsetLocation=%s,OffsetRotation=%s,Radius=%g,Length=%g)' % (b, vec(o), rot(r), rad, ln)
    for b, o, r, rad, ln in cap) + ")")

assert T.link_pose(copy, kawaii), "copy -> kawaii link failed"
assert T.link_pose(kawaii, out), "kawaii -> output link failed"
unreal.BlueprintEditorLibrary.compile_blueprint(abp)
eal.save_loaded_asset(abp)
print("built", abp.get_path_name())
