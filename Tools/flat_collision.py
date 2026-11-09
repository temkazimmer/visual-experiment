"""Clean walking collision for the stairs and plaza slabs.

The steps and slabs are chipped, worn stone, and apply_geo.py cooks their collision
from the full-res mesh. Running across a worn nosing or a slab joint, the capsule
snagged on the small sharp facets and stopped dead. This gives each Step_* and
PlazaSlabs_* mesh one box (its own bounds, so the top sits on the tread) and makes
the player collide with that. The chips stay visual; traces (foot IK, the camera)
still see the detailed surface.

Works alongside add_step_ramp.py and doesn't replace it. That script makes the Step_*
actors ignore the Pawn channel, so the player walks the stairs on the hidden StepRamp,
and these step boxes only affect other channels. The slabs have no ramp, so for the
player the slab boxes are the fix.

Idempotent. Run after apply_geo.py:  python3 Tools/ue.py Tools/flat_collision.py
"""
import unreal

assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), \
    "Play mode is running: stop PIE before changing assets"

FLAT = ("SM_Step_", "SM_PlazaSlabs_")
DEST = "/Game/VisEx/Geo"

sme = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
paths = [p for p in unreal.EditorAssetLibrary.list_assets(DEST, recursive=True, include_folder=False)
         if p.split("/")[-1].split(".")[0].startswith(FLAT)]
for p in paths:
    m = unreal.load_asset(p)
    if not isinstance(m, unreal.StaticMesh):
        continue
    sme.remove_collisions(m)
    sme.add_simple_collisions(m, unreal.ScriptCollisionShapeType.BOX)
    body = m.get_editor_property("body_setup")
    body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    print("flat", m.get_name())
print("done", len(paths))
