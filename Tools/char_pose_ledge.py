"""Ledge (bundle 02, Tema's reference): she kneels, goes onto all fours, lowers her chest onto
her forearms with her hips held high and her back arched, then raises her right leg straight
up behind her, toe pointed, the left shin flat on the ground, her head turned away. Idempotent.

  python3 Tools/ue.py Tools/char_pose_ledge.py     # builds three assets, then shoots a strip

Writes /Game/Characters/Poses/A_Pose_Ledge, _In and _Out (the pose wheel lists A_Pose_* on its
own). Face down she runs along her own Y axis: head forward (+Y), legs back (-Y).
The kneel, the solver and the camera come from char_pose_mermaid.py (everything before its
"the seat" marker); the forearm prop is char_pose_prone.py's. Globals: BUILD=False,
SHOTS=[...] or [], TWEAK={...} for the last key, VIEWS=(...).
"""
import os
import unreal

BUILD = globals().get("BUILD", True)
SHOTS = globals().get("SHOTS", [0.0, 1.9, 2.6, 3.3, 4.1])
_tools = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools")
_kit = open(os.path.join(_tools, "char_pose_mermaid.py")).read().split("# ---- the seat: mermaid ----")[0]
_keep = dict(BUILD=BUILD, SHOTS=SHOTS)
exec(compile(_kit, "char_pose_mermaid.py", "exec"), globals())
globals().update(_keep)

OUT_NAME = "A_Pose_Ledge"

def lerp(a, b, t): return tuple(x + (y - x) * t for x, y in zip(a, b))

def prone_arms(P):
    """fore: 0 straight arms, palms flat under the shoulders; 1 down on the forearms,
    hands meeting in front of her (as char_pose_prone.py)."""
    f = min(max(P["fore"], 0.0), 1.0)
    for s, side in (("l", 1.0), ("r", -1.0)):
        sh = pos("upperarm_" + s)
        palm_down = (sh[0] - side * 2.0, sh[1] + 6.0, 4.0)
        forearm = (sh[0] - side * P["inward"], sh[1] + P["ahead"], 5.0)
        reach(s, lerp(palm_down, forearm, f), lerp((0.0, -1.0, 0.2), (side * 0.3, -0.3, -1.0), f))
        set_hand(s, norm(lerp((-side * 0.15, 1.0, -0.05), (-side * 0.6, 0.8, -0.05), f)), (0.0, 0.0, -1.0))
        share_twist(s)
        spread = (-9.0, -2.0, 5.0, 13.0) if s == "l" else (9.0, 2.0, -5.0, -13.0)
        for fn, sp in zip(("index", "middle", "ring", "pinky"), spread):
            straighten(s, fn, sp)
        for fn in ("index", "middle", "ring", "pinky"):
            curl(s, fn, (6.0 * f, 14.0 * f, 8.0 * f))
ARMS = prone_arms

for k in (STAND, STEP, HALF, KNEEL):
    k.update(fore=0.0, inward=12.0, ahead=26.0)

# 1. onto all fours: hands under the shoulders, knees under the hips, toes tucked
ALLFOURS = dict(KNEEL, pelvis=(1.0, -6.0, 52.0), roll=0.0, pitch=-78.0, yaw=-22.0,
                ball_l=(10.0, -48.0, 0.5), toe_l=(0.0, 0.2, -1.0), sole_l=(0.0, -1.0, -0.2), knee_l=(11.0, 30.0, -20.0),
                ball_r=(-9.0, -48.0, 0.5), toe_r=(0.0, 0.2, -1.0), sole_r=(0.0, -1.0, -0.2), knee_r=(-9.0, 30.0, -20.0),
                lean_left=0.0, lean_fwd=-6.0, twist_left=0.0, head_tilt=0.0, head_down=-10.0, head_left=0.0,
                arms=1.0, fore=0.0, floor=5.0)
# 2. chest down onto the forearms, hips high over the knees, back arched, insteps flat
TOE_FLAT, SOLE_FLAT = norm((0.0, -0.95, -0.3)), norm((0.0, -0.3, 0.95))
DOWN = dict(ALLFOURS, pelvis=(1.0, -4.0, 50.0), pitch=-122.0, yaw=-22.0,
            ball_l=ball_from("l", (9.0, -48.0, 14.0), TOE_FLAT), toe_l=TOE_FLAT, sole_l=SOLE_FLAT,
            ball_r=ball_from("r", (-8.0, -48.0, 14.0), TOE_FLAT), toe_r=TOE_FLAT, sole_r=SOLE_FLAT,
            lean_fwd=-16.0, head_down=-45.0, fore=1.0, inward=9.0, ahead=24.0)
# 3. the right leg raised straight up behind her, toe pointed, head turned away to her left
RISE = 55.0                                                   # the raised leg's angle above level, deg
_c, _s = math.cos(math.radians(RISE)), math.sin(math.radians(RISE))
TOE_UP, SOLE_UP = norm((0.0, -_c, _s)), norm((0.0, _s, _c))
LEDGE = dict(DOWN, ball_r=ball_from("r", (-10.0, -4.0 - 86.0 * _c, 50.0 + 86.0 * _s), TOE_UP), toe_r=TOE_UP, sole_r=SOLE_UP,
             knee_r=(-10.0, -35.0, -5.0), head_left=40.0, head_tilt=-8.0, head_down=-60.0, lean_fwd=-22.0, floor=4.0)
LEDGE.update(globals().get("TWEAK", {}))

IN_KEYS = [(0.0, STAND), (0.55, STEP), (1.25, HALF), (1.85, KNEEL), (2.50, ALLFOURS), (3.20, DOWN), (3.95, LEDGE)]
OUT_KEYS = [(0.0, LEDGE), (0.65, DOWN), (1.35, ALLFOURS), (2.00, KNEEL), (2.60, HALF), (3.25, STEP), (3.95, STAND)]

def report(name, P):
    lift = build(P)
    print("%s: hips lifted %.1f, pelvis %s, shoulders z %.1f %.1f, elbows z %.1f %.1f, knees %s %s, balls %s %s, head %s" % (
        name, lift, [round(v, 1) for v in pos("pelvis")], pos("upperarm_l")[2], pos("upperarm_r")[2],
        pos("lowerarm_l")[2], pos("lowerarm_r")[2], [round(v, 1) for v in pos("calf_l")], [round(v, 1) for v in pos("calf_r")],
        [round(v, 1) for v in pos("ball_l")], [round(v, 1) for v in pos("ball_r")], [round(v, 1) for v in pos("head")]))
report("down", DOWN)
report("ledge", LEDGE)
hold = [{b: (local[b][0], local[b][1], local[b][2]) for b in bones}]
frames_in = clip(IN_KEYS)
if BUILD:
    write(OUT_NAME, hold)
    write(OUT_NAME + "_In", frames_in)
    write(OUT_NAME + "_Out", clip(OUT_KEYS))
shoot("ledge", frames_in, SHOTS, globals().get("VIEWS", ("side", "front")))
