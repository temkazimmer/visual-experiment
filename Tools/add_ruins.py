"""Ruin pass for Lvl_Dust: snapped columns with jagged tops, two toppled columns
lying as drums in the sand, tilted plinths, rubble at the bases of everything.
Resets columns from their build spec first, so it's safe to re-run.

Run live: python3 Tools/ue.py Tools/add_ruins.py
"""
import math
import random

import unreal

SEED = 11
COL_Y, COL_D, COL_H, PLINTH = 1900, 1000, 10000, (1400, 1400, 700)   # from build_dust.py
TOPPLED = {4, 9}           # column indices that fell
BREAK_CHANCE = 0.45        # of the rest, how many are snapped

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
rng = random.Random(SEED)
by = {a.get_actor_label(): a for a in eas.get_all_level_actors()}
mat = unreal.load_asset("/Game/VisEx/M_SandCover")
CUBE = unreal.load_asset("/Engine/BasicShapes/Cube")
CYL = unreal.load_asset("/Engine/BasicShapes/Cylinder")

for label, a in by.items():
    if label.startswith("Ruin_"):
        eas.destroy_actor(a)

count = [0]


def piece(mesh, loc, size, yaw=0.0, pitch=0.0, roll=0.0):
    a = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*loc),
                                   unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw))
    a.set_actor_label("Ruin_%03d" % count[0])
    count[0] += 1
    a.static_mesh_component.set_static_mesh(mesh)
    a.static_mesh_component.set_material(0, mat)
    a.set_actor_scale3d(unreal.Vector(size[0] / 100, size[1] / 100, size[2] / 100))
    return a


def rubble(cx, cy, cz, spread, n, big=250):
    for _ in range(n):
        s = rng.uniform(40, big)
        piece(CUBE, (cx + rng.gauss(0, spread), cy + rng.gauss(0, spread), cz + s * 0.15),
              (s * rng.uniform(0.7, 1.6), s * rng.uniform(0.6, 1.3), s * rng.uniform(0.4, 0.9)),
              yaw=rng.uniform(0, 360), pitch=rng.uniform(-25, 25), roll=rng.uniform(-25, 25))


columns = sorted((l for l in by if l.startswith("Column_")), key=lambda l: int(l.split("_")[1]))
for label in columns:
    i = int(label.split("_")[1])
    col, plinth = by[label], by.get("Plinth_%02d" % i)
    x = col.get_actor_location().x
    top = PLINTH[2]
    if plinth:   # a few plinths have settled unevenly
        plinth.set_actor_rotation(unreal.Rotator(roll=rng.uniform(-3, 3), pitch=rng.uniform(-2, 2), yaw=rng.uniform(-4, 4)), False)

    if i in TOPPLED:
        # a stump, and the shaft lying across the plaza as separate drums
        stump = rng.uniform(900, 1600)
        col.set_actor_scale3d(unreal.Vector(COL_D / 100, COL_D / 100, stump / 100))
        col.set_actor_location(unreal.Vector(x, COL_Y, top + stump / 2), False, False)
        direction = rng.uniform(-35, -15)          # falls along the plaza, away from the steps
        dx, dy = math.cos(math.radians(direction)), math.sin(math.radians(direction))
        dist = stump + 600
        for d in range(4):
            length = rng.uniform(1300, 2000)
            cx, cy = x + dx * (dist + length / 2), COL_Y + dy * (dist + length / 2)
            piece(CYL, (cx + rng.uniform(-120, 120), cy + rng.uniform(-120, 120), COL_D * 0.32),
                  (COL_D, COL_D, length), yaw=direction + rng.uniform(-12, 12), pitch=90 + rng.uniform(-6, 6))
            dist += length + rng.uniform(80, 500)
        rubble(x + dx * 1500, COL_Y + dy * 1500, 0, 900, 14)
        continue

    col.set_actor_scale3d(unreal.Vector(COL_D / 100, COL_D / 100, COL_H / 100))
    col.set_actor_location(unreal.Vector(x, COL_Y, top + COL_H / 2), False, False)
    if rng.random() < BREAK_CHANCE:
        h = rng.uniform(0.3, 0.75) * COL_H
        col.set_actor_scale3d(unreal.Vector(COL_D / 100, COL_D / 100, h / 100))
        col.set_actor_location(unreal.Vector(x, COL_Y, top + h / 2), False, False)
        # jagged break: a tilted broken drum and splinters on top
        piece(CYL, (x + rng.uniform(-80, 80), COL_Y + rng.uniform(-80, 80), top + h + 150),
              (COL_D * 0.97, COL_D * 0.97, 400), pitch=rng.uniform(-14, 14), roll=rng.uniform(-14, 14))
        for _ in range(3):
            s = rng.uniform(200, 420)
            piece(CUBE, (x + rng.uniform(-300, 300), COL_Y + rng.uniform(-300, 300), top + h + 300 + s * 0.3),
                  (s, s * 0.7, s * 1.3), yaw=rng.uniform(0, 360), pitch=rng.uniform(-30, 30), roll=rng.uniform(-30, 30))
        rubble(x - 600, COL_Y - 400, 0, 600, 8)
    else:
        rubble(x - 500, COL_Y - 300, 0, 350, 3, big=150)

# monoliths: a cracked-off slab leaning against each, rubble at the base
for label in (l for l in by if l.startswith("Monolith_")):
    m = by[label]
    o, e = m.get_actor_bounds(False)
    side = rng.choice((-1, 1))
    base = 0 if o.y > -1500 else -1000        # plaza top vs lower ground
    piece(CUBE, (o.x - e.x - 300, o.y + side * e.y * 0.4, base + 1400),
          (500, e.y * 0.8, 3200), roll=side * rng.uniform(8, 16), pitch=rng.uniform(10, 18))
    rubble(o.x - e.x - 600, o.y, base, 1200, 10, big=400)

print("ruins: %d pieces; saved %s" % (count[0], unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()))
