"""Menu idle beats: small things she does while she leans on the column and nobody has picked
anything yet. Each beat starts and ends in the lean, so the menu director can lay one over the
breathing loop and fade back (AVisexMenuDirector::PlayIdleBeat), with long rests in between.
She is resting and thinking, not waiting: everything is slow, small and finished calmly.

  python3 Tools/ue.py Tools/char_menu_beats.py          # all beats; ONLY = ["Toes"] for some

  A_Menu_Beat_Glance   a slow look over the plaza to her right and back
  A_Menu_Beat_Shift    weight from one leg to the other: the right foot comes down flat
  A_Menu_Beat_Toes     she looks down at her left foot, sets it on its heel, lifts the big toe,
                       plays the others, then swings the forefoot side to side on the heel
Built with the lean's kit (Tools/char_menu_lean.py, which brings Tools/char_pose_mermaid.py).
"""
import math
import os
import unreal

ONLY = globals().get("ONLY")
_here = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools")
_keep = dict(ONLY=ONLY)
BUILD, SHOTS = False, []
exec(compile(open(os.path.join(_here, "char_menu_lean.py")).read(), "char_menu_lean.py", "exec"), globals())
globals().update(_keep)

TOES = ["bigtoe", "indextoe", "middletoe", "ringtoe", "littletoe"]
TOE_BONES = ["%s_0%d_%s" % (t, j, s) for s in "lr" for t in TOES for j in (1, 2)]
assert all(b in bones for b in TOE_BONES), "toe bones missing: run Tools/blender_toes.py and char_toes.py"
REST = {b: tuple(local[b][1]) for b in TOE_BONES}
Zw = (0.0, 0.0, 1.0)


def toe_lift(side, toe, deg, share=0.6):
    """Raise a toe by `deg` (negative curls it under): the knuckle takes it, the tip `share` more."""
    along = norm(sub(pos("%s_02_%s" % (toe, side)), pos("%s_01_%s" % (toe, side))))
    axis = norm(cross(along, Zw))                      # +deg lifts the tip (axis x along has +Z)
    for j, d in ((1, deg), (2, deg * share)):
        rotate("%s_0%d_%s" % (toe, j, side), axis_angle(axis, d))


def toes(side, big=0.0, rest=(0.0, 0.0, 0.0, 0.0)):
    for b in TOE_BONES:
        if b.endswith("_" + side):
            local[b][1] = REST[b]
    _cache.clear()
    toe_lift(side, "bigtoe", big)
    for t, d in zip(TOES[1:], rest):
        toe_lift(side, t, d)


def swayed(P, side, deg):
    """The forefoot swung `deg` about a vertical axis through the heel (heel stays put)."""
    if abs(deg) < 1e-3:
        return P
    P = dict(P)
    toe = norm(P["toe_" + side])
    ball = P["ball_" + side]
    heel = add(sub(ball, mul(norm((toe[0], toe[1], 0.0)), 19.0)), (0.0, 0.0, 0.0))
    a = math.radians(deg)
    rz = lambda v: (v[0] * math.cos(a) - v[1] * math.sin(a), v[0] * math.sin(a) + v[1] * math.cos(a), v[2])
    P["ball_" + side] = add(heel, rz(sub(ball, heel)))
    P["toe_" + side] = rz(toe)
    P["sole_" + side] = rz(P["sole_" + side])
    return P


# ---- the beats: keys over the lean (mesh space: Y forward, X her left) ------------------------
B = dict(LEAN, big=0.0, w1=0.0, w2=0.0, w3=0.0, w4=0.0, sway=0.0)

# Toes. She glances down at her left foot first; the foot follows the gaze.
HEEL_ANKLE = globals().get("HEEL_ANKLE", (13.0, 2.0, 7.0))     # a little forward of the lean's, on its heel
HEEL_TOE = globals().get("HEEL_TOE", (0.22, 0.95, 0.18))      # sole tipped up ~20 deg from flat
LOOK = dict(B, head_down=22.0, head_left=20.0, head_tilt=8.0, twist_left=9.0, lean_fwd=-8.0)
HEEL = dict(LOOK, knee_l=(20.0, 75.0, 50.0))
HEEL.update(foot("l", HEEL_ANKLE, HEEL_TOE))       # slid forward, on its heel, toes up
BIG = dict(HEEL, big=24.0)
W1 = dict(BIG, w1=-14.0, w2=-6.0, w3=4.0, w4=8.0)                   # the small toes play, one after another
W2 = dict(BIG, w1=6.0, w2=-12.0, w3=-10.0, w4=-2.0)
W3 = dict(BIG, w1=-10.0, w2=4.0, w3=-12.0, w4=-12.0)
W4 = dict(BIG, w1=-2.0, w2=-4.0, w3=-2.0, w4=-4.0)
TOES_KEYS = [(0.0, B), (1.4, LOOK), (3.0, HEEL), (4.0, BIG), (4.8, BIG), (5.4, W1), (6.0, W2), (6.6, W3),
             (7.2, W4), (8.4, dict(W4, sway=11.0)), (9.6, dict(W4, sway=-9.0)), (10.8, dict(W4, sway=7.0)),
             (11.8, dict(W4, sway=0.0, big=8.0)), (12.8, dict(LOOK, head_down=12.0)), (14.2, B)]

# Glance: over the plaza to her right, a held look, back.
G1 = dict(B, head_left=-28.0, head_down=-4.0, twist_left=-3.0, head_tilt=0.0)
GLANCE_KEYS = [(0.0, B), (1.6, G1), (3.6, dict(G1, head_left=-34.0, head_down=-2.0)),
               (5.2, dict(B, head_left=-6.0, head_down=3.0)), (6.6, B)]

# Shift: the right foot comes down flat and takes some weight, the left heel eases up.
SHIFT_ANKLE_R = globals().get("SHIFT_ANKLE_R", (-8.0, 0.0, 6.0))
SH = dict(B, knee_r=(-6.0, 60.0, 48.0), roll=-1.0, lean_left=-1.0, head_left=4.0,
          pelvis=add(LEAN["pelvis"], (-1.5, 0.0, 0.0)))
SH.update(foot("r", SHIFT_ANKLE_R, (-0.15, 0.97, -0.2)))
SH.update(foot("l", (12.0, 1.0, 10.5), (0.2, 0.9, -0.4)))
SHIFT_KEYS = [(0.0, B), (1.8, SH), (4.6, dict(SH, head_left=0.0, twist_left=4.0)), (6.6, B)]


def beat(keys):
    frames = []
    for f in range(int(round((keys[-1][0] + 2 * PAD) * FPS)) + 1):
        P = sample(keys, f / FPS - PAD)
        build(swayed(P, "l", P["sway"]))
        toes("l", P["big"], (P["w1"], P["w2"], P["w3"], P["w4"]))
        toes("r")
        frames.append({b: (local[b][0], local[b][1], local[b][2]) for b in bones})
    return frames


for name, keys in (("Toes", TOES_KEYS), ("Glance", GLANCE_KEYS), ("Shift", SHIFT_KEYS)):
    if ONLY and name not in ONLY:
        continue
    for t, P in keys:
        build(swayed(P, "l", P["sway"]))
        print("%-6s %5.1f s  foot_l z %.1f ball_l z %.1f | foot_r z %.1f ball_r z %.1f" % (
            name, t, pos("foot_l")[2], pos("ball_l")[2], pos("foot_r")[2], pos("ball_r")[2]))
    if globals().get("WRITE", True):
        write("A_Menu_Beat_" + name, beat(keys), MENU_DIR)
