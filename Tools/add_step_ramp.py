"""Smooth walking on the plaza steps: the chipped stone steps stop blocking the
player (Pawn channel ignored; traces and the camera still see them) and an
invisible ramp, 10 cm under the nosing line, carries the character instead —
no catching on chipped edges. Feet sit within ~10 cm of each tread. Idempotent.

Run live: python3 Tools/ue.py Tools/add_step_ramp.py  (after apply_geo.py)
"""
import math

import unreal

STEPS, RISE, RUN = 20, 20, 220          # from build_dust.py
TOP_Y, X0, X1 = -1500, -5000, 35000     # plaza edge; the steps' x span
DROP, LENGTH = STEPS * RISE, STEPS * RUN
SINK, THICK = 10, 50                    # ramp top under the nosings; slab thickness

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
CR = unreal.CollisionResponseType

for a in eas.get_all_level_actors():
    L = a.get_actor_label()
    if L == "StepRamp":
        eas.destroy_actor(a)
    elif L.startswith("Step_"):
        a.static_mesh_component.set_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN, CR.ECR_IGNORE)

angle = math.atan2(DROP, LENGTH)        # top surface rises toward +y
mid_y, mid_z = TOP_Y - LENGTH / 2, -DROP / 2 - SINK
# centre sits half the thickness below the top surface, along its normal
cy, cz = mid_y + math.sin(angle) * THICK / 2, mid_z - math.cos(angle) * THICK / 2
ramp = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector((X0 + X1) / 2, cy, cz),
                                  unreal.Rotator(roll=-math.degrees(angle), pitch=0, yaw=0))
ramp.set_actor_label("StepRamp")
smc = ramp.static_mesh_component
smc.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))   # simple box collision
ramp.set_actor_scale3d(unreal.Vector((X1 - X0) / 100, math.hypot(LENGTH, DROP) / 100, THICK / 100))
smc.set_visibility(False)
ramp.set_actor_hidden_in_game(True)
smc.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
smc.set_collision_response_to_all_channels(CR.ECR_IGNORE)
smc.set_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN, CR.ECR_BLOCK)
print("StepRamp at %s rot %s" % (ramp.get_actor_location(), ramp.get_actor_rotation()))
