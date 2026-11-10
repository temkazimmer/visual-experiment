"""Menu lean: she perches against a fallen column, glutes on its rounded top-front edge,
hands on the stone beside her hips, breathing; then pushes off into her standing idle,
and the way back. Idempotent.

  python3 Tools/ue.py Tools/char_menu_lean.py     # builds three assets, then shoots stills

Writes /Game/Characters/Menu/A_Menu_Lean (a 4 s loop), A_Menu_Lean_Up (lean to idle) and
A_Menu_Lean_Down (idle to lean). All in place: the root never moves.
The column (LeanColumnGap on the menu director) is a lying cylinder, radius 45 cm, top at
90 cm, its axis across her (mesh X), behind her: its nearest point is GAP cm behind her root.
Stills show a temporary cylinder of that size; nothing is added to the level.
The solver and the camera come from char_pose_mermaid.py (everything before "the seat").
Globals: BUILD=False, SHOTS=[...] (seconds into _Up) or [], TWEAK={...} for the lean key.
"""
import math
import os
import unreal

BUILD = globals().get("BUILD", True)
SHOTS = globals().get("SHOTS", [0.0, 0.4, 0.8, 1.4])
_tools = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools")
_kit = open(os.path.join(_tools, "char_pose_mermaid.py")).read().split("# ---- the seat: mermaid ----")[0]
_keep = dict(BUILD=BUILD, SHOTS=SHOTS)
exec(compile(_kit, "char_pose_mermaid.py", "exec"), globals())
globals().update(_keep)

MENU_DIR = "/Game/Characters/Menu"
GAP = globals().get("GAP", 24.0)            # her root to the column's nearest point, cm
RADIUS, TOP = 45.0, 90.0
CY, CZ = -(GAP + RADIUS), TOP - RADIUS      # the column's axis, in mesh space (Y forward)
PROPS = [("/Engine/BasicShapes/Cylinder", (0.0, CY, CZ), (1.0, 0.0, 0.0), (RADIUS / 50.0, RADIUS / 50.0, 3.0))]

def lerp(a, b, t): return tuple(x + (y - x) * t for x, y in zip(a, b))

def on_stone(x, deg):
    """A point on the column's surface, `deg` from its top toward her."""
    a = math.radians(deg)
    return (x, CY + RADIUS * math.sin(a), CZ + RADIUS * math.cos(a)), (0.0, math.sin(a), math.cos(a))

def lean_arms(P):
    """Hands flat on the stone beside the hips, fingers over the front edge."""
    for s, side in (("l", 1.0), ("r", -1.0)):
        pt, n = on_stone(side * P["hand_x"], P["hand_deg"])
        wrist = add(pt, mul(n, 3.5))
        fingers = norm(lerp(cross((1.0, 0.0, 0.0), n), (0.0, 0.0, -1.0), 0.25))
        fingers = norm(add(fingers, (side * -0.15, 0.0, 0.0)))
        reach(s, wrist, (side * 0.6, -1.0, 0.2))
        set_hand(s, fingers, neg(n))
        share_twist(s)
        spread = (-9.0, -2.0, 5.0, 13.0) if s == "l" else (9.0, 2.0, -5.0, -13.0)
        for fn, sp in zip(("index", "middle", "ring", "pinky"), spread):
            straighten(s, fn, sp)
        for fn in ("index", "middle", "ring", "pinky"):
            curl(s, fn, (10.0, 14.0, 8.0))
ARMS = lean_arms

for k in (STAND, HALF, KNEEL):
    k.update(hand_x=24.0, hand_deg=22.0)

def foot(side, ankle, toe, sole_hint=(0.0, 0.0, -1.0)):
    toe = norm(toe)
    return {"ball_" + side: ball_from(side, ankle, toe), "toe_" + side: toe, "sole_" + side: norm(perp(sole_hint, toe))}

# the lean: weight on the left leg, the right knee forward and in, on the ball of the foot
LEAN = dict(STAND, pelvis=(1.0, CY + 36.0, 86.0), roll=4.0, pitch=12.0, yaw=-18.0,
            knee_l=(18.0, 60.0, 50.0), knee_r=(-2.0, 70.0, 55.0),
            lean_left=2.0, lean_fwd=-11.0, twist_left=6.0, head_tilt=5.0, head_down=-2.0, head_left=10.0,
            arms=1.0, floor=5.5)
LEAN.update(foot("l", (12.0, 0.0, 8.5), (0.2, 0.96, -0.2)))
LEAN.update(foot("r", (-4.0, 12.0, 12.5), (0.08, 0.5, -0.86), (0.0, -1.0, -0.3)))
LEAN.update(globals().get("TWEAK", {}))

# pushing off: hands press, hips come forward off the stone, chest forward
PUSH = dict(LEAN, pelvis=(1.0, CY + 46.0, 91.0), pitch=2.0, lean_fwd=6.0, head_down=4.0, head_left=4.0, arms=0.85)
# over her feet, the right foot stepping through to its idle place
STEP = dict(STAND, pelvis=(1.5, -6.0, 92.0), yaw=-6.0, lean_fwd=3.0, arms=0.25, hand_x=24.0, hand_deg=22.0)
STEP.update(foot("l", (12.0, 0.0, 8.5), (0.2, 0.96, -0.2)))
_mid_r = lerp(sub(LEAN["ball_r"], mul(LEAN["toe_r"], FOOT["r"])), sub(I["ball_r"], mul(I_TOE["r"], FOOT["r"])), 0.5)
STEP.update(foot("r", add(_mid_r, (0.0, 0.0, 7.0)), (0.0, 0.7, -0.7), (0.0, -1.0, -0.4)))
STEP["knee_r"] = (-8.0, 60.0, 50.0)

UP_KEYS = [(0.0, LEAN), (0.40, PUSH), (0.80, STEP), (1.40, STAND)]
DOWN_KEYS = [(0.0, STAND), (0.55, STEP), (1.05, PUSH), (1.50, LEAN)]

def breathing(seconds=4.0):
    """The lean, alive: a breath and a slow sway, both whole cycles so it loops."""
    frames = []
    n = int(round(seconds * FPS))
    for f in range(n + 1):
        w = 2.0 * math.pi * f / n
        P = dict(LEAN)
        P["lean_fwd"] = LEAN["lean_fwd"] - 1.4 * math.sin(w)
        P["pelvis"] = add(LEAN["pelvis"], (0.8 * math.sin(w + 0.6), 0.0, 0.35 * math.sin(w)))
        P["roll"] = LEAN["roll"] + 1.2 * math.sin(w + 0.6)
        P["head_left"] = LEAN["head_left"] + 3.0 * math.sin(w + 1.2)
        P["head_tilt"] = LEAN["head_tilt"] + 1.5 * math.sin(2 * w)
        build(P)
        frames.append({b: (local[b][0], local[b][1], local[b][2]) for b in bones})
    return frames

lift = build(LEAN)
print("lean: hips lifted %.1f, pelvis %s, hands %s %s, knees %s %s" % (
    lift, [round(v, 1) for v in pos("pelvis")], [round(v, 1) for v in pos("hand_l")], [round(v, 1) for v in pos("hand_r")],
    [round(v, 1) for v in pos("calf_l")], [round(v, 1) for v in pos("calf_r")]))
frames_up = clip(UP_KEYS)
if BUILD:
    write("A_Menu_Lean", breathing(), MENU_DIR)
    write("A_Menu_Lean_Up", frames_up, MENU_DIR)
    write("A_Menu_Lean_Down", clip(DOWN_KEYS), MENU_DIR)
shoot("lean", frames_up, SHOTS, globals().get("VIEWS", ("front3qr", "right")))
