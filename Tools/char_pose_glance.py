"""Glance back: she sits down sideways, turns her shoulders and looks back over her left
shoulder, left hand on her hip, right hand planted on the ground in front. Idempotent.

  python3 Tools/ue.py Tools/char_pose_glance.py     # builds three assets, then shoots a strip

Writes /Game/Characters/Poses/A_Pose_Glance, _In (stand to sit) and _Out (sit to stand).
In third person the camera is behind her, so the look lands on the player.
The sit-down, the solver and the camera come from char_pose_mermaid.py (everything before
its "the seat" marker); this file only sets the seat and the hands.
Globals: BUILD=False to only shoot, SHOTS=[seconds into _In] or [] to skip, TWEAK={...}.
"""
import os
import unreal

BUILD = globals().get("BUILD", True)
SHOTS = globals().get("SHOTS", [0.0, 0.6, 1.1, 1.6, 2.1])
_tools = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools")
_kit = open(os.path.join(_tools, "char_pose_mermaid.py")).read().split("# ---- the seat: mermaid ----")[0]
_keep = dict(BUILD=BUILD, SHOTS=SHOTS)
exec(compile(_kit, "char_pose_mermaid.py", "exec"), globals())
globals().update(_keep)

OUT_NAME = "A_Pose_Glance"

def glance_arms(P):
    # right arm long and straight, the hand flat on the ground in front of her feet
    reach("r", P["plant"], (-0.7, -0.5, 0.3))
    set_hand("r", (-0.35, 0.93, -0.08), (-0.1, 0.1, -1.0))
    share_twist("r")
    for f, s in (("index", 9.0), ("middle", 2.0), ("ring", -5.0), ("pinky", -13.0)):
        straighten("r", f, s)
    # left hand resting on the outside of the left hip, fingers down and back, elbow out behind
    pv = pos("pelvis")
    reach("l", add(pv, P["hip"]), (0.6, -0.8, 0.3))
    set_hand("l", (0.15, -0.45, -0.88), (-0.9, 0.3, -0.1))
    share_twist("l")
    for f, s in (("index", -6.0), ("middle", -1.0), ("ring", 4.0), ("pinky", 9.0)):
        straighten("l", f, s)
    for f in ("index", "middle", "ring", "pinky"):
        curl("l", f, (8.0, 12.0, 6.0))
ARMS = glance_arms

# the mermaid's legs (hips off to her left, shins swept to her right), body turned back
ANKLE_L, TOE_L = (-25.8, 11.0, 5.5), (-0.95, -0.30, 0.0)
ANKLE_R, TOE_R = (-33.3, -9.8, 5.6), (-0.70, -0.70, -0.05)
for k in (STAND, HALF, KNEEL):
    k.update(plant=(-20.0, 30.0, 6.0), hip=(13.0, -9.0, 7.0))
GLANCE = dict(KNEEL, pelvis=(14.0, -13.0, 16.0), roll=6.0, pitch=-8.0, yaw=-35.0,
              ball_l=ball_from("l", ANKLE_L, TOE_L), toe_l=TOE_L, sole_l=(0.3, -0.9, 0.3), knee_l=(13.0, 27.0, 5.5),
              ball_r=ball_from("r", ANKLE_R, TOE_R), toe_r=TOE_R, sole_r=(0.5, -0.6, 0.6), knee_r=(-8.0, 24.0, 9.0),
              lean_left=-40.0, lean_fwd=12.0, twist_left=35.0, plant=(-30.0, 30.0, 6.0),
              head_tilt=-6.0, head_down=-14.0, head_left=75.0, arms=1.0, floor=0.0)
GLANCE.update(globals().get("TWEAK", {}))

# the turn of the head comes last, a beat after she has settled
SETTLE = dict(GLANCE, twist_left=10.0, head_left=10.0, head_down=4.0, head_tilt=0.0)
IN_KEYS = [(0.0, STAND), (0.60, HALF), (1.10, KNEEL), (1.65, SETTLE), (2.15, GLANCE)]
OUT_KEYS = [(0.0, GLANCE), (0.35, SETTLE), (0.95, KNEEL), (1.45, HALF), (2.05, STAND)]

lift = build(GLANCE)
r_arm = length(sub(pos("lowerarm_r"), pos("upperarm_r"))) + length(sub(pos("hand_r"), pos("lowerarm_r")))
print("glance: hips lifted %.1f, hand_r %s (shoulder %s, arm %.1f), hand_l %s, pelvis %s" % (
    lift, [round(v, 1) for v in pos("hand_r")], [round(v, 1) for v in pos("upperarm_r")], r_arm,
    [round(v, 1) for v in pos("hand_l")], [round(v, 1) for v in pos("pelvis")]))
hold = [{b: (local[b][0], local[b][1], local[b][2]) for b in bones}]
frames_in = clip(IN_KEYS)
if BUILD:
    write(OUT_NAME, hold)
    write(OUT_NAME + "_In", frames_in)
    write(OUT_NAME + "_Out", clip(OUT_KEYS))
shoot("glance", frames_in, SHOTS, globals().get("VIEWS", ("back3q", "side")))
