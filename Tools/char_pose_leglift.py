"""Leg lift: she sits down, lies onto her left side propped on her elbow, head in her hand,
then lifts the top leg high and reaches up along it. Idempotent.

  python3 Tools/ue.py Tools/char_pose_leglift.py     # builds three assets, then shoots a strip

Writes /Game/Characters/Poses/A_Pose_LegLift, _In and _Out. Lying, she runs along her own
X axis: head toward +X (where her left was), legs toward -X, facing forward (+Y).
The sit-down, the solver and the camera come from char_pose_mermaid.py (everything before
its "the seat" marker). Globals: BUILD=False, SHOTS=[...] or [], TWEAK={...} for the last key.
"""
import os
import unreal

BUILD = globals().get("BUILD", True)
SHOTS = globals().get("SHOTS", [0.0, 1.1, 1.8, 2.55, 3.0, 3.55])
_tools = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools")
_kit = open(os.path.join(_tools, "char_pose_mermaid.py")).read().split("# ---- the seat: mermaid ----")[0]
_keep = dict(BUILD=BUILD, SHOTS=SHOTS)
exec(compile(_kit, "char_pose_mermaid.py", "exec"), globals())
globals().update(_keep)

OUT_NAME = "A_Pose_LegLift"

def lerp(a, b, t): return tuple(x + (y - x) * t for x, y in zip(a, b))

def leglift_arms(P):
    """down: 0 left hand on the ground beside her, 1 elbow down, head in the hand.
    touch: 0 right hand on the top thigh, 1 reaching up along the lifted shin."""
    d, t = min(max(P["down"], 0.0), 1.0), min(max(P["touch"], 0.0), 1.0)
    sh, hd = pos("upperarm_l"), pos("head")
    under_head = add(hd, P["cradle"])
    reach("l", lerp((sh[0] + P["prop"][0], sh[1] + P["prop"][1], P["prop"][2]), under_head, d),
          lerp((0.2, -1.0, 0.2), (0.1, 0.3, -1.0), d))
    to_head = norm(sub(hd, pos("hand_l")))
    set_hand("l", norm(lerp((0.80, 0.55, -0.12), to_head, d)),
             norm(lerp((0.10, 0.10, -1.0), perp(norm(sub(hd, pos("lowerarm_l"))), to_head), d)))
    share_twist("l")
    for f, s in (("index", -9.0), ("middle", -2.0), ("ring", 5.0), ("pinky", 13.0)):
        straighten("l", f, s)
    for f in ("index", "middle", "ring", "pinky"):
        curl("l", f, (10.0 * d, 18.0 * d, 10.0 * d))
    thigh = lerp(pos("thigh_r"), pos("calf_r"), 0.55)
    shin = lerp(pos("calf_r"), pos("foot_r"), P["along"])
    target = lerp(add(thigh, (0.0, 4.0, 9.0)), add(shin, (0.0, 7.0, 0.0)), t)
    reach("r", target, lerp((0.0, -0.4, 1.0), (0.0, -0.6, -0.8), t))
    fwd = norm(sub(pos("hand_r"), pos("lowerarm_r")))
    set_hand("r", fwd, norm(perp(lerp((0.0, 0.0, -1.0), (0.0, -1.0, 0.0), t), fwd)))
    share_twist("r")
    for f, s in (("index", 6.0), ("middle", 1.0), ("ring", -4.0), ("pinky", -9.0)):
        straighten("r", f, s)
    for f in ("index", "middle", "ring", "pinky"):
        curl("r", f, (6.0, 10.0, 6.0))
ARMS = leglift_arms

for k in (STAND, HALF, KNEEL):
    k.update(down=0.0, touch=0.0, along=0.5, cradle=(-3.0, -2.0, -13.0))

# 1. the mermaid's seat, hands quiet: left hand on the ground, right on the thigh
ANKLE_L, TOE_L = (-25.8, 11.0, 5.5), (-0.95, -0.30, 0.0)
ANKLE_R, TOE_R = (-33.3, -9.8, 5.6), (-0.70, -0.70, -0.05)
SIT = dict(KNEEL, pelvis=(18.0, -13.0, 17.0), roll=28.0, pitch=8.0, yaw=0.0,
           ball_l=ball_from("l", ANKLE_L, TOE_L), toe_l=TOE_L, sole_l=(0.3, -0.9, 0.3), knee_l=(13.0, 27.0, 5.5),
           ball_r=ball_from("r", ANKLE_R, TOE_R), toe_r=TOE_R, sole_r=(0.5, -0.6, 0.6), knee_r=(-8.0, 24.0, 9.0),
           lean_left=-16.0, lean_fwd=4.0, twist_left=0.0, head_tilt=0.0, head_down=8.0, head_left=0.0,
           arms=1.0, floor=0.0)
# 2. down onto the left side: legs long, one on the other, propped on the left elbow
TOE_LIE, SOLE_LIE = norm((-0.85, 0.45, -0.1)), norm((-0.45, -0.85, 0.0))
LIE = dict(SIT, pelvis=(8.0, 0.0, 17.0), roll=88.0, pitch=0.0, yaw=0.0,
           ball_l=ball_from("l", (-80.0, 6.0, 7.0), TOE_LIE), toe_l=TOE_LIE, sole_l=SOLE_LIE, knee_l=(-36.0, 30.0, 8.0),
           ball_r=ball_from("r", (-76.0, 14.0, 15.0), TOE_LIE), toe_r=TOE_LIE, sole_r=SOLE_LIE, knee_r=(-36.0, 34.0, 22.0),
           lean_left=-22.0, lean_fwd=0.0, head_tilt=0.0, head_down=0.0, down=1.0)
# 3. the top knee drawn up first, foot pointed beside the other knee
TOE_B = norm((-0.7, -0.2, -0.68))
BEND = dict(LIE, ball_r=ball_from("r", (-30.0, 18.0, 38.0), TOE_B), toe_r=TOE_B, sole_r=norm(perp((-0.2, -0.96, 0.49), TOE_B)),
            knee_r=(0.0, 50.0, 60.0), touch=0.0, along=0.2)
# 4. the top leg up, toes pointed, the right hand climbing the shin
TOE_UP, SOLE_UP = norm((0.15, 0.1, 0.98)), norm((0.0, -0.995, 0.1))
LIFT = dict(LIE, ball_r=ball_from("r", (16.0, 8.0, 104.0), TOE_UP), toe_r=TOE_UP, sole_r=SOLE_UP,
            knee_r=(12.0, 50.0, 62.0), touch=1.0, along=0.45)
LIFT.update(globals().get("TWEAK", {}))

IN_KEYS = [(0.0, STAND), (0.60, HALF), (1.10, KNEEL), (1.80, SIT), (2.55, LIE), (3.00, BEND), (3.55, LIFT)]
OUT_KEYS = [(0.0, LIFT), (0.45, BEND), (0.90, LIE), (1.60, SIT), (2.20, KNEEL), (2.70, HALF), (3.30, STAND)]

lift = build(LIFT)
print("leglift: hips lifted %.1f, elbow_l %s hand_l %s head %s, foot_r %s hand_r %s" % (
    lift, [round(v, 1) for v in pos("lowerarm_l")], [round(v, 1) for v in pos("hand_l")], [round(v, 1) for v in pos("head")],
    [round(v, 1) for v in pos("foot_r")], [round(v, 1) for v in pos("hand_r")]))
hold = [{b: (local[b][0], local[b][1], local[b][2]) for b in bones}]
frames_in = clip(IN_KEYS)
if BUILD:
    write(OUT_NAME, hold)
    write(OUT_NAME + "_In", frames_in)
    write(OUT_NAME + "_Out", clip(OUT_KEYS))
shoot("leglift", frames_in, SHOTS, globals().get("VIEWS", ("front", "side")))
