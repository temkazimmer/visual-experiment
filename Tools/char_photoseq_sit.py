"""Photo sequence sit: she sits on the sand, back against the fallen column, knees up.

  python3 Tools/ue.py Tools/char_photoseq_sit.py     # builds three clips, then shoots stills

Writes /Game/Characters/PhotoSeq/A_PhotoSeq_Sit (a breathing loop), A_PhotoSeq_SitDown
(stand to sit) and A_PhotoSeq_StandUp (sit to stand). All in place: the root never moves.
The way down is a squat with her back to the stone: hips drop behind her heels, one hand
reaches back to the sand, then the feet slide out and she settles back onto the drum.
The column is a lying cylinder behind her, axis across her (mesh X) at COLUMN_Y / COLUMN_Z,
in her mesh space (world / 0.935). Photo mode places her root COLUMN_Y * 0.935 cm in front
of the stone's axis (VisexPhotoMode.cpp, SitGap).
The solver and the camera come from char_pose_mermaid.py (everything before "the seat").
Globals: BUILD=False, SHOTS=[...] (seconds into _SitDown) or [].
"""
import os
import unreal

BUILD = globals().get("BUILD", True)
SHOTS = globals().get("SHOTS", [0.0, 0.9, 1.7, 2.6, 3.4])
_tools = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools")
_kit = open(os.path.join(_tools, "char_pose_mermaid.py")).read().split("# ---- the seat: mermaid ----")[0]
_keep = dict(BUILD=BUILD, SHOTS=SHOTS)
exec(compile(_kit, "char_pose_mermaid.py", "exec"), globals())
globals().update(_keep)

PHOTO_DIR = "/Game/Characters/PhotoSeq"
RADIUS = 50.0                      # the drum, 47 cm world
COLUMN_Y, COLUMN_Z = -86.0, 45.0   # its axis behind her root, and above the sand
PROPS = [("/Engine/BasicShapes/Cylinder", (0.0, COLUMN_Y, COLUMN_Z), (1.0, 0.0, 0.0), (RADIUS / 50.0, RADIUS / 50.0, 3.0))]


def sit_arms(P):
    """Left hand flat on the sand beside her hip; right forearm resting over the right knee."""
    sh = pos("upperarm_l")
    reach("l", (sh[0] + P["prop"][0], sh[1] + P["prop"][1], P["prop"][2]), (0.3, -1.0, 0.0))
    set_hand("l", (0.35, 0.90, -0.10), (0.05, 0.05, -1.0))
    share_twist("l")
    for f, s in (("index", -8.0), ("middle", -2.0), ("ring", 4.0), ("pinky", 11.0)):
        straighten("l", f, s)
    knee = pos("calf_r")
    reach("r", add(knee, P["drape"]), (-1.0, -0.2, -0.3))
    set_hand("r", (0.15, 0.45, -0.88), (0.25, -0.55, -0.80))
    share_twist("r")
    for f in ("index", "middle", "ring", "pinky"):
        curl("r", f, (18.0, 25.0, 12.0))
ARMS = sit_arms
LEAD["drape"] = 0.30

# ---- the key poses (mesh space: X her left, Y forward, Z up, sand at 0) ----
FLAT = (0.0, 0.0, -1.0)
BASE_P = dict(STAND, prop=(10.0, -26.0, 4.0), drape=(-2.0, 10.0, -6.0), floor=0.0)
# hips back and down behind the heels, trunk forward to balance, feet where they stood
CROUCH = dict(BASE_P, pelvis=(1.7, -16.0, 50.0), pitch=-8.0,
              knee_l=(16.0, 45.0, 52.0), knee_r=(-14.0, 45.0, 52.0),
              lean_fwd=24.0, head_down=10.0, arms=0.45)
# the left hand reaches the sand and takes her weight; the hips touch down
LOW = dict(CROUCH, pelvis=(3.0, -12.0, 16.0), pitch=6.0, roll=-4.0,
           knee_l=(17.0, 40.0, 50.0), knee_r=(-15.0, 40.0, 50.0),
           lean_fwd=14.0, head_down=6.0, arms=1.0)
# feet slide out, knees up, and she settles back until her shoulders rest on the stone
def _feet(P, reach_y, knee_z):
    return dict(P, ball_l=(19.0, reach_y, 0.4), toe_l=(0.05, 0.97, -0.2), sole_l=(0.0, -0.2, -0.98), knee_l=(17.0, 24.0, knee_z),
                ball_r=(-17.0, reach_y - 6.0, 0.4), toe_r=(-0.10, 0.97, -0.2), sole_r=(0.0, -0.2, -0.98), knee_r=(-16.0, 20.0, knee_z))
SIT = _feet(dict(LOW, pelvis=(3.0, -10.0, 13.0), pitch=24.0, roll=-3.0, yaw=4.0,
                 lean_fwd=-4.0, lean_left=-3.0, twist_left=-5.0, head_down=-8.0, head_left=6.0, head_tilt=-4.0),
            48.0, 46.0)
SIT.update(globals().get("TWEAK", {}))
# the breath: chest lifts, head eases, a few cm of the hips
BREATH = dict(SIT, pelvis=(3.0, -10.5, 13.3), pitch=22.5, lean_fwd=-5.5, head_down=-10.0, head_left=8.0)

DOWN_KEYS = [(0.0, BASE_P), (0.9, CROUCH), (1.8, LOW), (2.7, _feet(LOW, 40.0, 50.0)), (3.5, SIT)]
UP_KEYS = [(0.0, SIT), (0.7, _feet(LOW, 40.0, 50.0)), (1.5, LOW), (2.3, CROUCH), (3.2, BASE_P)]
LOOP_KEYS = [(0.0, SIT), (2.6, BREATH), (5.2, SIT)]

lift = build(SIT)
print("sit: hips lifted %.1f, pelvis %s, spine_05 %s, head %s" % (
    lift, [round(v, 1) for v in pos("pelvis")], [round(v, 1) for v in pos("spine_05")], [round(v, 1) for v in pos("head")]))
_old_pad = PAD
frames_down = clip(DOWN_KEYS)
if BUILD:
    write("A_PhotoSeq_SitDown", frames_down, folder=PHOTO_DIR)
    write("A_PhotoSeq_StandUp", clip(UP_KEYS), folder=PHOTO_DIR)
    PAD = 0.0                       # a loop: no rest padding, first frame = last frame
    write("A_PhotoSeq_Sit", clip(LOOP_KEYS), folder=PHOTO_DIR)
    PAD = _old_pad
shoot("photoseq_sit", frames_down, SHOTS)
