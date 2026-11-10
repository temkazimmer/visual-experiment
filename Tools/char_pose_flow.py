"""Floor to floor: when she is already down and another floor pose is picked, she moves
into it without standing up. Idempotent.

  UE_WAIT=6000 python3 Tools/ue.py Tools/char_pose_flow.py

Every floor pose has a way down to one shared seat (the mermaid's legs, hands quiet) and
back up from it. A transition is the first pose's way down plus the second pose's way up,
cross-faded where they meet. Writes /Game/Characters/Poses/A_Pose_<From>_To_<To>; the
pose wheel plays one when it exists, instead of getting up and sitting down again.
Each pose script runs in its own namespace, so each keeps its own hands.
Globals: PAIRS=[("Glance","LegLift"), ...] to build only some; SHOTS=[...] seconds into
the first pair's clip, for stills.
"""
import math
import os
import unreal

if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise RuntimeError("Play mode is running: stop PIE first")
TOOLS = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools")

def load(script):
    g = dict(BUILD=False, SHOTS=[], __name__="flow")
    exec(compile(open(os.path.join(TOOLS, script)).read(), script, "exec"), g)
    return g

NS = {"Mermaid": load("char_pose_mermaid.py"), "Glance": load("char_pose_glance.py"),
      "LegLift": load("char_pose_leglift.py"), "FeetUp": load("char_pose_prone.py")}

# the shared seat: the mermaid's legs and body, arms nearly at rest
SEAT = NS["Mermaid"]["SIT"]
BODY = [k for k in NS["Mermaid"]["STAND"] if k not in ("arms", "prop")]
def hub(g):
    h = dict(g["KNEEL"])
    h.update({k: SEAT[k] for k in BODY})
    h["arms"] = 0.15
    return h

# each pose's way down to the seat, from its held key
def down(name):
    g = NS[name]
    H = hub(g)
    if name == "Mermaid":
        return [(0.0, g["SIT"]), (0.45, H)]
    if name == "Glance":
        return [(0.0, g["GLANCE"]), (0.35, g["SETTLE"]), (0.90, H)]
    if name == "LegLift":
        return [(0.0, g["LIFT"]), (0.45, g["BEND"]), (0.90, g["LIE"]), (1.55, H)]
    return [(0.0, g["FEETUP"]), (0.60, g["BELLY"]), (1.40, g["ALLFOURS"]), (2.00, g["KNEEL"]), (2.75, H)]

def up(name):
    keys = down(name)
    end = keys[-1][0]
    return [(end - t, k) for t, k in reversed(keys)]

def nlerp(a, b, t):
    if sum(x * y for x, y in zip(a, b)) < 0:
        b = tuple(-v for v in b)
    q = tuple(x + (y - x) * t for x, y in zip(a, b))
    n = math.sqrt(sum(v * v for v in q))
    return tuple(v / n for v in q)

def blend(fa, fb, t):
    out = {}
    for b, (pa, qa, sa) in fa.items():
        pb, qb, sb = fb[b]
        out[b] = (tuple(x + (y - x) * t for x, y in zip(pa, pb)), nlerp(qa, qb, t),
                  tuple(x + (y - x) * t for x, y in zip(sa, sb)))
    return out

def flow(a, b, overlap=9):
    fa, fb = NS[a]["clip"](down(a)), NS[b]["clip"](up(b))
    mid = []
    for i in range(overlap):
        x = i / (overlap - 1.0)
        mid.append(blend(fa[len(fa) - overlap + i], fb[i], x * x * (3 - 2 * x)))
    return fa[:-overlap] + mid + fb[overlap:]

names = list(NS)
PAIRS = globals().get("PAIRS", [(a, b) for a in names for b in names if a != b])
W = NS["Mermaid"]
first = None
for a, b in PAIRS:
    frames = flow(a, b)
    W["write"]("A_Pose_%s_To_%s" % (a, b), frames)
    first = first or frames

SHOTS = globals().get("SHOTS", [])
if SHOTS and first:
    W.update(VIEWS=globals().get("VIEWS", ("front3qr",)))
    W["shoot"]("flow", first, SHOTS, W["VIEWS"])
