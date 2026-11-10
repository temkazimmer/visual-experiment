"""Feet up: she kneels, goes onto all fours, slides down onto her front, props herself on
her forearms and kicks her feet up behind her, ankles crossed. Idempotent.

  python3 Tools/ue.py Tools/char_pose_prone.py     # builds three assets, then shoots a strip

Writes /Game/Characters/Poses/A_Pose_FeetUp, _In and _Out. Lying, she runs along her own
Y axis: head forward (+Y), legs back (-Y), belly down.
The kneel, the solver and the camera come from char_pose_mermaid.py (everything before
its "the seat" marker). Globals: BUILD=False, SHOTS=[...] or [], TWEAK={...} for the last key.
"""
import os
import unreal

BUILD = globals().get("BUILD", True)
SHOTS = globals().get("SHOTS", [0.0, 1.1, 1.7, 2.5, 3.2])
_tools = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools")
_kit = open(os.path.join(_tools, "char_pose_mermaid.py")).read().split("# ---- the seat: mermaid ----")[0]
_keep = dict(BUILD=BUILD, SHOTS=SHOTS)
exec(compile(_kit, "char_pose_mermaid.py", "exec"), globals())
globals().update(_keep)

OUT_NAME = "A_Pose_FeetUp"

def lerp(a, b, t): return tuple(x + (y - x) * t for x, y in zip(a, b))

def prone_arms(P):
    """fore: 0 straight arms, palms flat under the shoulders; 1 down on the forearms,
    hands meeting in front of her."""
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

for k in (STAND, HALF, KNEEL):
    k.update(fore=0.0, inward=12.0, ahead=26.0)

# 1. onto all fours: hands under the shoulders, knees under the hips, toes tucked
# (yaw -22 undoes the idle's twist, which face down would turn into a tilt of the shoulders)
ALLFOURS = dict(KNEEL, pelvis=(1.0, -6.0, 52.0), roll=0.0, pitch=-78.0, yaw=-22.0,
                ball_l=(10.0, -48.0, 0.5), toe_l=(0.0, 0.2, -1.0), sole_l=(0.0, -1.0, -0.2), knee_l=(11.0, 30.0, -20.0),
                ball_r=(-9.0, -48.0, 0.5), toe_r=(0.0, 0.2, -1.0), sole_r=(0.0, -1.0, -0.2), knee_r=(-9.0, 30.0, -20.0),
                lean_left=0.0, lean_fwd=-6.0, twist_left=0.0, head_tilt=0.0, head_down=-10.0, head_left=0.0,
                arms=1.0, fore=0.0, floor=5.0)
# 2. down on the front, legs long behind, toes pointed, propped on the forearms
TOE_FLAT, SOLE_FLAT = norm((0.0, -0.95, -0.3)), norm((0.0, -0.3, 0.95))
BELLY = dict(ALLFOURS, pelvis=(1.0, 4.0, 14.0), pitch=-88.0,
             ball_l=ball_from("l", (8.0, -80.0, 11.5), TOE_FLAT), toe_l=TOE_FLAT, sole_l=SOLE_FLAT, knee_l=(9.0, -40.0, -20.0),
             ball_r=ball_from("r", (-8.0, -80.0, 13.0), TOE_FLAT), toe_r=TOE_FLAT, sole_r=SOLE_FLAT, knee_r=(-9.0, -40.0, -20.0),
             lean_fwd=-42.0, head_down=-30.0, fore=1.0, floor=0.0)
# 3. knees bent, feet up behind her, ankles crossed, head turned a little to her left
TOE_UP, SOLE_UP = norm((0.0, -0.15, 0.99)), norm((0.0, 0.99, 0.15))
FEETUP = dict(BELLY, ball_l=ball_from("l", (-3.0, -38.0, 50.0), TOE_UP), toe_l=TOE_UP, sole_l=SOLE_UP, knee_l=(10.0, -40.0, -5.0),
              ball_r=ball_from("r", (4.0, -42.0, 45.0), TOE_UP), toe_r=TOE_UP, sole_r=SOLE_UP, knee_r=(-10.0, -40.0, -5.0),
              head_left=22.0, head_tilt=-8.0, head_down=-40.0, floor=4.0)
FEETUP.update(globals().get("TWEAK", {}))

IN_KEYS = [(0.0, STAND), (0.60, HALF), (1.10, KNEEL), (1.75, ALLFOURS), (2.55, BELLY), (3.20, FEETUP)]
OUT_KEYS = [(0.0, FEETUP), (0.60, BELLY), (1.40, ALLFOURS), (2.00, KNEEL), (2.50, HALF), (3.10, STAND)]

def report(name, P):
    lift = build(P)
    print("%s: hips lifted %.1f, shoulders %s %s, elbows z %.1f %.1f, hands z %.1f %.1f, knees %s %s, head %s" % (
        name, lift, [round(v, 1) for v in pos("upperarm_l")], [round(v, 1) for v in pos("upperarm_r")],
        pos("lowerarm_l")[2], pos("lowerarm_r")[2], pos("hand_l")[2], pos("hand_r")[2],
        [round(v, 1) for v in pos("calf_l")], [round(v, 1) for v in pos("calf_r")], [round(v, 1) for v in pos("head")]))
report("allfours", ALLFOURS)
report("feetup", FEETUP)
hold = [{b: (local[b][0], local[b][1], local[b][2]) for b in bones}]
frames_in = clip(IN_KEYS)
if BUILD:
    write(OUT_NAME, hold)
    write(OUT_NAME + "_In", frames_in)
    write(OUT_NAME + "_Out", clip(OUT_KEYS))
shoot("prone", frames_in, SHOTS, globals().get("VIEWS", ("side", "front")))
