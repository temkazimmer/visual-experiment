"""Menu Settings sit (Tema, 2026-11-12): she jumps up onto the lean drum and sits on its top,
the right leg hooked round the drum's front (shin on the stone, foot tucked under its bulge),
the left leg hanging toward the menu camera, foot relaxed. Back: she slides off into the lean.
Idempotent.

  python3 Tools/ue.py Tools/char_menu_sit.py     # builds three clips

Writes /Game/Characters/Menu/A_Menu_Sit_Settings (a breathing loop), A_Menu_Sit_Settings_Down
(lean to seat) and A_Menu_Sit_Settings_Up (seat to lean). All in place: the root never moves.
The menu director plays them while Settings is open (VisexMenuDirector.cpp, SitForSettings).
The lean key, the drum and the hands come from char_menu_lean.py; the solver, the game's joint
limits and ik_foot keying from char_pose_mermaid.py.
Her mesh space: X her left, Y forward, Z up. The menu camera is 69 deg to her LEFT (measured in
Play: (255, 98, -14)); a negative pelvis yaw turns her toward it.
Globals: BUILD=False, TWEAK={...} for the seat key, SEAT_DEG, HANG, HOOK, SIT_HAND_DEG, SIT_HAND_X
(not HAND_*: char_menu_lean.py owns those names).
"""
import math
import os
import unreal

if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise RuntimeError("Play mode is running: stop PIE first")
BUILD = globals().get("BUILD", True)
_tools = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools")
_sit_keep = dict(BUILD=BUILD, TWEAK=globals().get("TWEAK", {}))
# the lean script defines LEAN, the drum (CY, CZ, RADIUS, TOP) and the lean's arms; BUILD off writes nothing.
# It sets its own _keep, so ours has another name.
BUILD, SHOTS, TWEAK = False, [], {}
exec(compile(open(os.path.join(_tools, "char_menu_lean.py")).read(), "char_menu_lean.py", "exec"), globals())
globals().update(_sit_keep)
PALM_OFF = globals().get("SIT_PALM_OFF", 4.0)   # 1 cm more than the lean: at Height 145 the crouch's palms touched 0.4 cm off the HD stone
print("drum in her space: axis y %.1f z %.1f, radius %.1f, top %.1f" % (CY, CZ, RADIUS, TOP))

def surface_y(z):
    """The drum's front surface at height z (its nearest point to her), or None above/below it."""
    dz = z - CZ
    return CY + math.sqrt(RADIUS * RADIUS - dz * dz) if abs(dz) < RADIUS else None

def clearance(p):
    """How far a point is outside the drum (cm, negative = inside the stone)."""
    return math.hypot(p[1] - CY, p[2] - CZ) - RADIUS

# ---- the seat: on the drum's top, a little toward its front ----
SEAT_DEG = globals().get("SEAT_DEG", 26.0)       # from the top toward her
SEAT_OFF = globals().get("SEAT_OFF", 10.0)       # pelvis bone off the stone
_a = math.radians(SEAT_DEG)
SEAT_PELVIS = (1.0, CY + (RADIUS + SEAT_OFF) * math.sin(_a), CZ + (RADIUS + SEAT_OFF) * math.cos(_a))

# the left leg hangs off the front toward the lens: thigh out over the edge, knee soft, foot relaxed
HANG = globals().get("HANG", dict(ankle=(22.0, 14.0, 30.0), knee=(24.0, 50.0, 75.0), toe=(0.15, 0.4, -0.9)))   # long and out: the knee clears the HD drum's flutes (~8 cm in Play)
# the right leg hooks round the drum: the knee over its front, the shin down the bulge, the foot
# tucked back under it, toes curled toward the stone
HOOK = globals().get("HOOK", dict(ankle=(-15.0, -18.0, 56.0), knee=(-14.0, 25.0, 100.0), toe=(0.0, -0.3, -0.95)))   # knee 35 cm out over the edge, shin 12 cm back onto the stone

SEAT = dict(LEAN, pelvis=SEAT_PELVIS, roll=0.0, pitch=-4.0, yaw=-12.0,
            knee_l=HANG["knee"], knee_r=HOOK["knee"],
            lean_left=-1.0, lean_fwd=-6.0, twist_left=8.0, head_tilt=-4.0, head_down=12.0, head_left=-30.0,   # down and round toward the low lens (negative: toward it)
            hand_deg=globals().get("SIT_HAND_DEG", 34.0), hand_x=globals().get("SIT_HAND_X", 26.0),   # ~5 cm off the stone, no wrist past its range
            arms=1.0, floor=0.0)
SEAT.update(foot("l", HANG["ankle"], HANG["toe"], (0.1, -0.9, -0.3)))
SEAT.update(foot("r", HOOK["ankle"], HOOK["toe"], (0.0, 0.9, -0.3)))
SEAT.update(globals().get("TWEAK", {}))
carry_hands(SEAT)

def report(tag):
    """The pose's contacts: joints near the stone, the feet, what the game will clamp."""
    pts = {b: pos(b) for b in ("pelvis", "thigh_l", "calf_l", "foot_l", "ball_l", "thigh_r", "calf_r", "foot_r", "ball_r", "hand_l", "hand_r")}
    print("%s: pelvis %s | clearance cm: %s | over limits %s | hand miss l %.1f r %.1f" % (
        tag, [round(v, 1) for v in pts["pelvis"]],
        " ".join("%s %.0f" % (b, clearance(p)) for b, p in pts.items() if b != "pelvis"),
        OVER[:4], ARM_MISS["l"], ARM_MISS["r"]))

build(SEAT)
report("seat")

def breathing(seconds=12.0):
    """The seat, alive: three breaths, a slow sway, the hanging foot swinging a little (loops)."""
    frames = []
    n = int(round(seconds * FPS))
    for f in range(n + 1):
        w = 2.0 * math.pi * f / n
        b = 3.0 * w
        P = dict(SEAT)
        P["lean_fwd"] = SEAT["lean_fwd"] - 1.2 * math.sin(b) - 1.0 * math.sin(w + 0.3)
        P["pelvis"] = add(SEAT["pelvis"], (0.8 * math.sin(w + 0.6), 0.0, 0.3 * math.sin(b)))
        P["roll"] = SEAT["roll"] + 1.0 * math.sin(w + 0.6)
        P["twist_left"] = SEAT["twist_left"] + 1.5 * math.sin(w + 2.0)
        P["head_left"] = SEAT["head_left"] + 3.0 * math.sin(w + 1.2) + 1.5 * math.sin(2 * w + 0.4)
        P["head_tilt"] = SEAT["head_tilt"] + 1.5 * math.sin(2 * w)
        P["ball_l"] = add(SEAT["ball_l"], (1.0 * math.sin(2 * w + 0.9), 2.5 * math.sin(2 * w + 0.2), 0.0))   # the dangling foot swings
        build(P)
        frames.append({b2: (local[b2][0], local[b2][1], local[b2][2]) for b2 in bones})
    return frames

# ---- the jump up: a real two-foot spring, hands pressing on the stone ----
# She gathers (both feet flat under her, hands back onto the drum), sinks into a crouch, springs:
# the hips rise past the seat and back onto the top while both feet leave the sand and tuck,
# then she lands seated, the hips compress 3 cm and settle while the legs swing into the
# hook and the hang. Timings from a bench hop-up: ~0.3 s crouch, ~0.25 s in the air.
def feet(K, l, r):
    K.update(foot("l", l[0], l[1], l[2] if len(l) > 2 else (0.0, 0.0, -1.0)))
    K.update(foot("r", r[0], r[1], r[2] if len(r) > 2 else (0.0, 0.0, -1.0)))
    return K
FLAT_L, FLAT_R = ((13.0, -4.0, 8.5), (0.15, 0.98, -0.1)), ((-13.0, -4.0, 8.5), (-0.15, 0.98, -0.1))
GATHER = feet(dict(LEAN, pelvis=add(LEAN["pelvis"], (0.0, 4.0, -4.0)), pitch=6.0, yaw=-6.0, lean_fwd=4.0,
                   head_down=6.0, head_left=0.0, hand_deg=40.0, hand_x=28.0, knee_l=(16.0, 40.0, 50.0), knee_r=(-16.0, 40.0, 50.0)),
              FLAT_L, FLAT_R)
CROUCH = dict(GATHER, pelvis=add(LEAN["pelvis"], (0.0, 12.0, -14.0)),   # forward: low down the drum's face comes out to meet her
             pitch=10.0, lean_fwd=14.0, head_down=10.0, hand_deg=36.0)
# in the air: hips up past the seat, knees tucked, feet off the sand; the hands leave the stone at the
# top (nothing can reach it from there), arms half back toward hanging
AIR = feet(dict(SEAT, pelvis=add(SEAT["pelvis"], (0.0, 4.0, 7.0)), pitch=4.0, yaw=-6.0, lean_fwd=10.0, twist_left=2.0,
                head_down=6.0, head_left=-10.0, hand_deg=30.0, arms=0.5, knee_l=(16.0, 40.0, 70.0), knee_r=(-16.0, 40.0, 70.0)),
           ((14.0, -6.0, 26.0), (0.1, 0.5, -0.86)), ((-14.0, -8.0, 30.0), (-0.1, 0.45, -0.89)))
LAND = dict(SEAT, pelvis=add(SEAT["pelvis"], (0.0, 1.0, -3.0)), lean_fwd=4.0, head_down=8.0)   # weight arrives: hips compress
carry_hands(GATHER, CROUCH, AIR, LAND)
DOWN_KEYS = [(0.0, LEAN), (0.45, GATHER), (0.80, CROUCH), (1.08, AIR), (1.35, LAND), (1.95, SEAT)]

# ---- the way down: she slides forward off the edge, lands on both feet, rises into the lean ----
SLIP = feet(dict(SEAT, pelvis=add(SEAT["pelvis"], (0.0, 8.0, -2.0)), pitch=0.0, yaw=-6.0, lean_fwd=8.0, head_down=8.0, head_left=-6.0,
                  hand_deg=40.0, knee_l=(16.0, 40.0, 60.0), knee_r=(-16.0, 40.0, 60.0)),
             ((14.0, 0.0, 16.0), (0.1, 0.6, -0.8)), ((-14.0, -2.0, 18.0), (-0.1, 0.6, -0.8)))
TOUCH = dict(CROUCH, pelvis=add(LEAN["pelvis"], (0.0, 10.0, -10.0)), lean_fwd=10.0, hand_deg=44.0)
carry_hands(SLIP, TOUCH)
UP_KEYS = [(0.0, SEAT), (0.45, SLIP), (0.80, TOUCH), (1.45, LEAN)]

def snappy(keys, scale=0.35, pad=0.12):
    """clip() with the lead/follow offsets cut to a third: a jump is quick, every part on nearly one clock."""
    global LEAD, PAD
    keep = LEAD, PAD
    LEAD, PAD = {k: v * scale for k, v in keep[0].items()}, pad
    try:
        return clip(keys)
    finally:
        LEAD, PAD = keep

def ground_check(frames, tag):
    """The lowest ball and ankle over the clip, and the closest any joint comes to the stone."""
    lo, worst = 99.0, (99.0, "")
    for fr in frames[::3]:
        for b in bones:
            local[b] = [fr[b][0], fr[b][1], fr[b][2]]
        _cache.clear()
        lo = min(lo, pos("ball_l")[2], pos("ball_r")[2])
        for j in ("calf_l", "calf_r", "foot_l", "foot_r", "ball_l", "ball_r", "hand_l", "hand_r", "thigh_l", "thigh_r"):
            c = clearance(pos(j))
            if c < worst[0]:
                worst = (c, j)
    print("%s: %.2f s, lowest ball %.1f cm, closest to the stone %.1f cm (%s)" % (tag, (len(frames) - 1) / float(FPS), lo, worst[0], worst[1]))

frames_down, frames_up = snappy(DOWN_KEYS), snappy(UP_KEYS)
ground_check(frames_down, "jump up"); ground_check(frames_up, "slide down")

if BUILD:
    write("A_Menu_Sit_Settings", breathing(), MENU_DIR)
    write("A_Menu_Sit_Settings_Down", frames_down, MENU_DIR)
    write("A_Menu_Sit_Settings_Up", frames_up, MENU_DIR)
