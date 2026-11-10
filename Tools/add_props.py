"""Human-scale props at the column bases, so the colonnade's size reads in the haze:
stone benches, bollards, urns, stacked blocks and a leaning stele. They're kit-bashed
from the weathered rubble and drum meshes (stone_geo.py) at person-sized scales,
dropped onto whatever is under them (slabs or banked sand) and sunk a little.
A cluster sits in front of each plinth whose base the dunes haven't buried,
none near the menu set.
Labelled Prop_NN (photo mode hides them like the other ruins). Idempotent.

Run live: python3 Tools/ue.py Tools/add_props.py  (after apply_geo.py)
"""
import math
import random

import unreal

GEO = "/Game/VisEx/Geo/"
PLINTH_Y, PLINTH_HALF = 1900, 700         # from build_dust.py: plinth row, half size
MENU_X = (6000, 13000)                    # keep the menu shot's backdrop as it is
SINK = 4                                  # cm into the floor
MAX_FLOOR = 120                           # cm: skip plinths whose base is under the dunes
# kind: (mesh family, size cm (x, y, z), pitch range)
KINDS = {
    "bench":   ("Rubble", (220, 55, 45), 0),
    "block":   ("Rubble", (90, 60, 50), 4),
    "wall":    ("Rubble", (160, 70, 75), 3),
    "bollard": ("Drum", (42, 42, 95), 0),
    "urn":     ("Drum", (55, 55, 80), 0),
    "stele":   ("Rubble", (18, 90, 170), 0),
}
CLUSTERS = [  # (kind, dx, dy, yaw) relative to the plinth's front-centre
    [("bench", -250, -260, 0), ("bollard", 420, -180, 0), ("urn", 520, -60, 0)],
    [("block", -300, -220, 18), ("block", -290, -230, 40), ("stele", 380, -120, 8), ("urn", -560, -90, 0)],
    [("wall", 150, -260, -6), ("bollard", -420, -200, 0), ("bollard", -420, -560, 0)],
    [("bench", 200, -300, 3), ("urn", -380, -120, 0), ("block", -480, -350, 65)],
]

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
mat = unreal.load_asset("/Game/VisEx/M_SandCover")
rng = random.Random(11)

for a in eas.get_all_level_actors():
    if a.get_actor_label().startswith("Prop_"):
        eas.destroy_actor(a)

plinths = sorted((a for a in eas.get_all_level_actors() if a.get_actor_label().startswith("Plinth_")),
                 key=lambda a: a.get_actor_location().x)
placed = []


def floor_z(x, y):
    hit = unreal.SystemLibrary.line_trace_single(
        world, unreal.Vector(x, y, 3000), unreal.Vector(x, y, -3000), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
        True, [], unreal.DrawDebugTrace.NONE, True)     # props count: a block lands on a block
    return hit.to_tuple()[4].z if hit else 0.0


n = 0
for i, p in enumerate(plinths):
    px = p.get_actor_location().x
    front = PLINTH_Y - PLINTH_HALF
    # only where the column base is still clear: further out the dunes have buried it
    if MENU_X[0] < px < MENU_X[1] or floor_z(px, front - 250) > MAX_FLOOR:
        continue
    for kind, dx, dy, yaw in CLUSTERS[i % len(CLUSTERS)]:
        family, size, tilt = KINDS[kind]
        x, y = px + dx + rng.uniform(-40, 40), front + dy + rng.uniform(-30, 30)
        z = floor_z(x, y)
        mesh = unreal.load_asset(GEO + "SM_%s_%d" % (family, rng.randrange(6 if family == "Rubble" else 4)))
        a = eas.spawn_actor_from_object(mesh, unreal.Vector(x, y, z + size[2] / 2 - SINK))
        a.set_actor_label("Prop_%02d" % n)
        a.set_actor_scale3d(unreal.Vector(size[0] / 100, size[1] / 100, size[2] / 100))
        pitch = rng.uniform(-tilt, tilt) + (12 if kind == "stele" else 0)
        a.set_actor_rotation(unreal.Rotator(roll=rng.uniform(-tilt, tilt), pitch=pitch, yaw=yaw + rng.uniform(-6, 6)), False)
        a.static_mesh_component.set_material(0, mat)
        placed.append(a)
        n += 1
print("props: %d" % n)
