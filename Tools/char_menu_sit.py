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
print("drum in her space: axis y %.1f z %.1f, radius %.1f, top %.1f" % (CY, CZ, RADIUS, TOP))

def surface_y(z):
    """The drum's front surface at height z (its nearest point to her), or None above/below it."""
    dz = z - CZ
    return CY + math.sqrt(RADIUS * RADIUS - dz * dz) if abs(dz) < RADIUS else None

def clearance(p):
    """How far a point is outside the drum (cm, negative = inside the stone)."""
    return math.hypot(p[1] - CY, p[2] - CZ) - RADIUS

# ---- the seat: on the drum's top, a little toward its front ----
SEAT_DEG = globals().get("SEAT_DEG", 16.0)       # from the top toward her
SEAT_OFF = globals().get("SEAT_OFF", 10.0)       # pelvis bone off the stone
_a = math.radians(SEAT_DEG)
SEAT_PELVIS = (1.0, CY + (RADIUS + SEAT_OFF) * math.sin(_a), CZ + (RADIUS + SEAT_OFF) * math.cos(_a))

# the left leg hangs off the front toward the lens: thigh out over the edge, knee soft, foot relaxed
HANG = globals().get("HANG", dict(ankle=(30.0, -8.0, 40.0), knee=(30.0, 10.0, 90.0), toe=(0.25, 0.35, -0.9)))
# the right leg hooks round the drum: the knee over its front, the shin down the bulge, the foot
# tucked back under it, toes curled toward the stone
HOOK = globals().get("HOOK", dict(ankle=(-16.0, -16.0, 50.0), knee=(-14.0, 0.0, 110.0), toe=(0.0, -0.35, -0.94)))   # ankle high and close: the knee folds out over the edge

SEAT = dict(LEAN, pelvis=SEAT_PELVIS, roll=0.0, pitch=-4.0, yaw=-12.0,
            knee_l=HANG["knee"], knee_r=HOOK["knee"],
            lean_left=-1.0, lean_fwd=-6.0, twist_left=8.0, head_tilt=-4.0, head_down=12.0, head_left=-30.0,   # down and round toward the low lens (negative: toward it)
            hand_deg=globals().get("SIT_HAND_DEG", 34.0), hand_x=globals().get("SIT_HAND_X", 26.0),   # ~5 cm off the stone, no wrist past its range
            arms=1.0, floor=0.0)
SEAT.update(foot("l", HANG["ankle"], HANG["toe"], (0.1, -0.9, -0.3)))
SEAT.update(foot("r", HOOK["ankle"], HOOK["toe"], (0.0, 0.8, -0.5)))
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

# ---- getting up there and back (placeholders until the jump lands: straight key blends) ----
DOWN_KEYS = globals().get("DOWN_KEYS") or [(0.0, LEAN), (1.6, SEAT)]
UP_KEYS = globals().get("UP_KEYS") or [(0.0, SEAT), (1.4, LEAN)]

if BUILD:
    write("A_Menu_Sit_Settings", breathing(), MENU_DIR)
    write("A_Menu_Sit_Settings_Down", clip(DOWN_KEYS), MENU_DIR)
    write("A_Menu_Sit_Settings_Up", clip(UP_KEYS), MENU_DIR)
