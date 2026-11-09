"""Send the big masses up into the sky: the colonnade's back wall, the monoliths and the
sloped mass grow taller with their bases where they were. Columns are regenerated
taller by stone_geo.py (VISEX_TALL), then placed by apply_geo.py.

Reads the original transforms from Tools/.bridge/actors.json, so it's safe to re-run.
Run live: python3 Tools/ue.py Tools/go_tall.py
"""
import json
import os

import unreal

# label prefix -> height multiplier
TALL = {"ColonnadeWall": 5.0, "Monolith_": 4.0, "SlopedMass": 1.0}

src = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools", ".bridge", "actors.json")
orig = {a["label"]: a for a in json.load(open(src))}
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
n = 0
for actor in eas.get_all_level_actors():
    L = actor.get_actor_label()
    k = next((p for p in TALL if L.startswith(p)), None)
    if not k or L not in orig:
        continue
    o, f = orig[L], TALL[k]
    sx, sy, sz = o["scale"]
    if L == "SlopedMass":            # tilted: grow it whole, pivot stays
        actor.set_actor_scale3d(unreal.Vector(sx, sy * f, sz * f))
    else:                            # upright: grow upward from the base
        x, y, z = o["loc"]
        base = z - sz * 50
        actor.set_actor_scale3d(unreal.Vector(sx, sy, sz * f))
        actor.set_actor_location(unreal.Vector(x, y, base + sz * f * 50), False, False)
    n += 1
print("made %d masses taller" % n)
