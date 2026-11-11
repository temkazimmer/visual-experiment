"""A low stone ledge for the photo pose "Ledge" (she kneels on it, down on her forearms, one
leg up behind her). A weathered slab kit-bashed from the props' rubble block (stone_geo.py)
in the colonnade's sand-covered stone, 220 x 100 cm and 60 cm out of the sand, on the flat
patch of plaza in front-left of the menu spot, out of the menu camera's frame.

Tagged PhotoLedge: AVisexPhotoMode finds it by that tag and traces its top. Its collision is
custom: query only, blocking Visibility (her floor and contact traces) and Camera, and
ignoring pawns, so it never stops the player capsule or the spawn. Rerunnable.

Run live: python3 Tools/ue.py Tools/add_ledge.py
"""
import unreal

TAG = "PhotoLedge"
LABEL = "PhotoLedge"
SPOT = (8849, -354)          # flat (1 cm over the footprint), 47 deg off the menu camera's axis
YAW = 180                    # long axis along the colonnade; she faces -X, shot from the -Y side
SIZE = (220, 100, 66)        # cm; sunk SINK into the sand, so 60 cm shows
SINK = 6
MESH = "/Game/VisEx/Geo/SM_Rubble_%d"
MAT = "/Game/VisEx/M_SandCover"

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
CH = unreal.CollisionChannel

for a in eas.get_all_level_actors():
    if a.actor_has_tag(TAG) or a.get_actor_label().startswith(LABEL):
        eas.destroy_actor(a)


def trace(x, y, ignore=()):
    hit = unreal.SystemLibrary.line_trace_single(
        world, unreal.Vector(x, y, 3000), unreal.Vector(x, y, -3000), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
        True, list(ignore), unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit else None


x, y = SPOT
floor = min(z for z in (trace(x + dx, y + dy) for dx in (-110, 0, 110) for dy in (-50, 50)) if z is not None)
centre = unreal.Vector(x, y, floor + SIZE[2] / 2 - SINK)
grid = [(u, v) for u in (-85, -40, 0, 40, 85) for v in (-35, 0, 35)]   # her knees to forearms


def spawn(i):
    a = eas.spawn_actor_from_object(unreal.load_asset(MESH % i), centre, unreal.Rotator(roll=0, pitch=0, yaw=YAW))
    a.set_actor_label(LABEL + "_try")   # a failed run leaves nothing a rerun misses
    a.set_actor_scale3d(unreal.Vector(SIZE[0] / 100, SIZE[1] / 100, SIZE[2] / 100))
    c = a.static_mesh_component
    c.set_material(0, unreal.load_asset(MAT))
    # custom collision: traces and the camera find it, her capsule walks through it
    c.set_collision_profile_name("Custom")
    c.set_collision_enabled(unreal.CollisionEnabled.QUERY_ONLY)
    c.set_collision_object_type(CH.ECC_WORLD_STATIC)
    c.set_collision_response_to_all_channels(unreal.CollisionResponseType.ECR_IGNORE)
    c.set_collision_response_to_channel(CH.ECC_VISIBILITY, unreal.CollisionResponseType.ECR_BLOCK)
    c.set_collision_response_to_channel(CH.ECC_CAMERA, unreal.CollisionResponseType.ECR_BLOCK)
    c.set_editor_property("can_ever_affect_navigation", False)
    c.set_editor_property("can_character_step_up_on", unreal.CanBeCharacterBase.ECB_NO)
    return a


# the six rubble blocks are weathered differently: keep the one whose top is flattest
best = None
for i in range(6):
    a = spawn(i)
    tops = [trace(x - u, y + v) for u, v in grid]     # YAW 180: local +X is world -X
    spread = max(tops) - min(tops)
    if best is None or spread < best[0]:
        if best:
            eas.destroy_actor(best[2])
        best = (spread, i, a, max(tops))
    else:
        eas.destroy_actor(a)
spread, i, a, top = best
a.set_actor_label(LABEL)
a.tags = [unreal.Name(TAG)]
c = a.static_mesh_component
print("ledge: SM_Rubble_%d at (%d, %d), sand %.0f, top %.0f (%.0f cm up, %.1f cm uneven), profile %s, pawn %s" % (
    i, x, y, floor, top, top - floor, spread, c.get_collision_profile_name(),
    c.get_collision_response_to_channel(CH.ECC_PAWN)))
