"""Menu lean: she perches against a fallen column, glutes on its rounded top-front edge,
hands on the stone beside her hips, breathing; then pushes off into her standing idle,
and the way back. Idempotent.

  python3 Tools/ue.py Tools/char_menu_lean.py     # builds three assets, then shoots stills

Writes /Game/Characters/Menu/A_Menu_Lean (a 12 s loop), A_Menu_Lean_Up (lean to idle) and
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
# the drum she leans on in Lvl_Dust (SM_Drum_0, measured in Play 2026-11-10, in her mesh space,
# i.e. divided by her body scale 0.935). The column is main's: a tagged "LeanColumn" actor
# whose dimensions are written down in Docs/Research/LEAN_COLUMN.md (world cm). Those numbers
# are the source here; if main changes them, change COLUMN and rerun this script.
BODY_SCALE = 0.935
COLUMN = {"surface_radius": 47.2, "top_z": 155.4, "axis_z": 108.3, "nominal_radius": 45.0, "gap": 21.8}
FEET_Z = COLUMN["axis_z"] - COLUMN["nominal_radius"] + 3.0          # her feet: axis = feet + 45 - 3 (LEAN_COLUMN.md)
GAP = globals().get("GAP", COLUMN["gap"] / BODY_SCALE)              # her root to the column's nearest point, cm
RADIUS = globals().get("RADIUS", COLUMN["surface_radius"] / BODY_SCALE)
TOP = globals().get("TOP", (COLUMN["top_z"] - FEET_Z) / BODY_SCALE)
print("column in her space: gap %.1f radius %.1f top %.1f" % (GAP, RADIUS, TOP))
CY, CZ = -(GAP + RADIUS), TOP - RADIUS      # the column's axis, in mesh space (Y forward)
PROPS = [("/Engine/BasicShapes/Cylinder", (0.0, CY, CZ), (1.0, 0.0, 0.0), (RADIUS / 50.0, RADIUS / 50.0, 3.0))]

# hands near the top of the drum, where its collision matches the stone (the contact probe
# traces against collision), with room for the palm under the wrist bone
HAND_DEG = globals().get("HAND_DEG", 48.0)     # on the drum's front, beside her hips, in MenuCam's view: at -15 (the far side)
#   the low camera lost the hands behind the drum's edge (QA 0320 cmp_hand), and the game's shoulder limit
#   pulled them 13 cm off it; at 20 the stone's own bulge still hid them
HAND_X = globals().get("HAND_X", 34.0)         # hands this far out from her centre; wider = the arms angle out, not hang
PALM_OFF = globals().get("PALM_OFF", 3.0)
FINGER_DIP = globals().get("FINGER_DIP", 0.12)      # 0.25 put the fingertips into the stone, 0 lifted them
FINGER_CURL = globals().get("FINGER_CURL", (10.0, 14.0, 6.0))   # draped over the drum's curve, not laid out
FINGER_OUT = globals().get("FINGER_OUT", 0.6)        # fingers out along the drum: 0.8 folded the wrist back 90 deg
FINGER_SPREAD = globals().get("FINGER_SPREAD", (-4.0, -1.0, 2.0, 6.0))   # a resting hand, fingers nearly together
FINGER_YAW = globals().get("FINGER_YAW", -150.0)  # fingers down the drum's front, a little turned: a bench grip, every joint in range
ARM_SLACK = globals().get("ARM_SLACK", 0.93)   # the arm reaches at most this much of its length: a soft elbow

def lerp(a, b, t): return tuple(x + (y - x) * t for x, y in zip(a, b))
SLIDE = globals().get("SLIDE", 16.0)      # degrees a hand may slide on the stone toward its shoulder
SOLVED = {}
ARM_MAX = globals().get("ARM_MAX", 0.985)   # the most of its length an arm ever reaches

def on_stone(x, deg):
    """A point on the column's surface, `deg` from its top toward her."""
    a = math.radians(deg)
    return (x, CY + RADIUS * math.sin(a), CZ + RADIUS * math.cos(a)), (0.0, math.sin(a), math.cos(a))

def lean_arms(P):
    """Hands flat on the stone beside the hips, fingers over the front edge."""
    for s, side in (("l", 1.0), ("r", -1.0)):
        up, lo, h = "upperarm_" + s, "lowerarm_" + s, "hand_" + s
        arm = (length(sub(pos(lo), pos(up))) + length(sub(pos(h), pos(lo)))) * ARM_SLACK
        def over(d):                             # how far past a soft elbow's reach the spot at d is
            pt, n = on_stone(side * P["hand_x"], d)
            return length(sub(add(pt, mul(n, PALM_OFF)), pos(up))) - arm   # wrist: palm thickness off the stone
        # Her trunk is turned: a spot one shoulder reaches with a soft elbow locks the other arm, so
        # that hand sits further up the drum, toward its shoulder, just as far as it must (at most
        # SLIDE degrees; past that it lets go and the arm straightens toward the spot). The angle
        # is solved once per key (carry_hands) and splined between keys like every other number,
        # so a planted hand stays planted through the breaths. The old per-frame 2-degree search
        # walked away from the shoulder and flung the arm to the far end of its range whenever a
        # breath crossed the limit: a 7 cm hand pop, 12 cm at the elbow.
        deg = P.get("deg_" + s)
        if deg is None:
            deg = P["hand_deg"]
            if over(deg) > 0.0:
                far = deg - SLIDE
                if over(far) <= 0.0:
                    near = deg
                    for _ in range(30):          # bisect: over(near) > 0 >= over(far)
                        mid = 0.5 * (near + far)
                        if over(mid) > 0.0: near = mid
                        else: far = mid
                deg = far
        SOLVED[s] = deg
        pt, n = on_stone(side * P["hand_x"], deg)
        wrist = add(pt, mul(n, PALM_OFF))
        # past a soft elbow the reach eases out toward ARM_MAX instead of hitting the arm's full
        # length: an elbow that snaps straight and then bends again is a kink in its path
        v = sub(wrist, pos(up)); d = length(v); full = arm / ARM_SLACK
        if d > arm:
            room = (ARM_MAX - ARM_SLACK) * full
            wrist = add(pos(up), mul(v, (arm + room * math.tanh((d - arm) / room)) / d))
        back = norm(cross((1.0, 0.0, 0.0), n))           # along the stone, away from her
        if FINGER_YAW is None:
            fingers = norm(lerp(back, (0.0, 0.0, -1.0), FINGER_DIP))
            fingers = norm(add(fingers, (side * FINGER_OUT, 0.0, 0.0)))   # fingers outward-back: the wrist rests at ~55 deg, not folded
        else:                                            # turned on the stone from 'away from her', outward positive
            fingers = norm(lerp(rot(axis_angle(n, -side * FINGER_YAW), back), (0.0, 0.0, -1.0), FINGER_DIP))
        reach(s, wrist, (side * 0.6, -1.0, 0.2))
        set_hand(s, fingers, neg(n))
        share_twist(s)
        spread = FINGER_SPREAD if s == "l" else tuple(-v for v in FINGER_SPREAD)
        for fn, sp in zip(("index", "middle", "ring", "pinky"), spread):
            straighten(s, fn, sp)
        for fn in ("index", "middle", "ring", "pinky"):
            curl(s, fn, FINGER_CURL)                     # relaxed, following the stone
ARMS = lean_arms

for k in (STAND, HALF, KNEEL):
    k.update(hand_x=HAND_X, hand_deg=HAND_DEG)

def foot(side, ankle, toe, sole_hint=(0.0, 0.0, -1.0)):
    toe = norm(toe)
    return {"ball_" + side: ball_from(side, ankle, toe), "toe_" + side: toe, "sole_" + side: norm(perp(sole_hint, toe))}

# the lean: weight on the left leg, the right knee forward and in, on the ball of the foot
SEAT = globals().get("SEAT", -4.0)          # pelvis bone height against the drum's top (skin on stone)
# The pelvis rides the drum's curve at SEAT_R from its axis (the old seat: 36 out, SEAT down
# from the top, skin on stone). At 38 deg from vertical her ankles sat 2-6 cm out of reach:
# straight legs, feet hanging above the sand. At 46 every key reaches, knees softly bent.
SEAT_R = math.hypot(36.0, RADIUS + SEAT)
SEAT_ANGLE = math.radians(globals().get("SEAT_ANGLE", 46.0))
LEAN = dict(STAND, pelvis=(1.0, CY + SEAT_R * math.sin(SEAT_ANGLE), CZ + SEAT_R * math.cos(SEAT_ANGLE)),
            roll=4.0, pitch=12.0, yaw=-18.0,
            knee_l=(18.0, 60.0, 50.0), knee_r=(-2.0, 70.0, 55.0),
            lean_left=2.0, lean_fwd=-11.0, twist_left=6.0, head_tilt=5.0, head_down=-2.0, head_left=10.0,
            arms=1.0, floor=5.5)
LEAN.update(foot("l", (12.0, 0.0, 8.5), (0.2, 0.96, -0.2)))
LEAN.update(foot("r", (-4.0, 12.0, 12.5), (0.08, 0.5, -0.86), (0.0, -1.0, -0.3)))
LEAN.update(globals().get("TWEAK", {}))

# pushing off: hands press, hips come forward off the stone, chest forward
# the trunk goes first (Park Life Bench_Sit_Stand_Sit: the trunk tips forward ~0.8 s before the hips
# leave the seat, the head comes over the feet, and the lean peaks just after the hips are up)
PREP = dict(LEAN, lean_fwd=3.0, head_down=5.0, head_left=6.0, arms=1.0)
PUSH = dict(LEAN, pelvis=(1.0, CY + 46.0, TOP + SEAT + 5.0), pitch=2.0, lean_fwd=9.0, head_down=4.0, head_left=4.0, arms=0.85)
# over her feet, the right foot stepping through to its idle place
STEP = dict(STAND, pelvis=(1.5, -6.0, 92.0), yaw=-6.0, lean_fwd=3.0, arms=0.25, hand_x=24.0, hand_deg=HAND_DEG)
STEP.update(foot("l", (12.0, 0.0, 8.5), (0.2, 0.96, -0.2)))
_mid_r = lerp(sub(LEAN["ball_r"], mul(LEAN["toe_r"], FOOT["r"])), sub(I["ball_r"], mul(I_TOE["r"], FOOT["r"])), 0.5)
STEP.update(foot("r", add(_mid_r, (0.0, 0.0, 7.0)), (0.0, 0.7, -0.7), (0.0, -1.0, -0.4)))
STEP["knee_r"] = (-8.0, 60.0, 50.0)

def carry_hands(*keys):
    """Solve each key's hand spots on its own pose and keep them in the key (deg_l, deg_r)."""
    for K in keys:
        K.pop("deg_l", None); K.pop("deg_r", None)
        build(K)
        K.update(deg_l=SOLVED["l"], deg_r=SOLVED["r"])
carry_hands(LEAN, PREP, PUSH, STEP, STAND)
print("hand spots, deg from the top: " + ", ".join("%s %.1f/%.1f" % (n, K["deg_l"], K["deg_r"]) for n, K in (("lean", LEAN), ("push", PUSH), ("step", STEP))))

UP_KEYS = [(0.0, LEAN), (0.40, PUSH), (0.80, STEP), (1.40, STAND)]
DOWN_KEYS = [(0.0, STAND), (0.70, STEP), (1.35, PUSH), (1.95, LEAN)]   # settling back, unhurried

def breathing(seconds=12.0):
    """The lean, alive: three breaths, one slow sway of the hips and trunk, and the right knee
    easing in and out, all whole cycles so it loops. Park Life's standing leans move the trunk
    6-13 deg and the hips 5-12 cm over half a minute; this keeps a calm third of that."""
    frames = []
    n = int(round(seconds * FPS))
    for f in range(n + 1):
        w = 2.0 * math.pi * f / n                     # once per loop
        b = 3.0 * w                                   # the breath, every 4 s
        P = dict(LEAN)
        P["lean_fwd"] = LEAN["lean_fwd"] - 1.4 * math.sin(b) - 1.8 * math.sin(w + 0.3)
        P["pelvis"] = add(LEAN["pelvis"], (2.2 * math.sin(w + 0.6) + 0.6 * math.sin(2 * w), 0.0, 0.35 * math.sin(b)))
        P["roll"] = LEAN["roll"] + 1.8 * math.sin(w + 0.6)
        P["twist_left"] = LEAN["twist_left"] + 2.0 * math.sin(w + 2.0)
        P["head_left"] = LEAN["head_left"] + 3.0 * math.sin(w + 1.2) + 2.0 * math.sin(2 * w + 0.4)
        P["head_tilt"] = LEAN["head_tilt"] + 1.5 * math.sin(2 * w)
        P["knee_r"] = add(LEAN["knee_r"], (3.0 * math.sin(2 * w + 1.0), 0.0, 0.0))
        build(P)
        frames.append({b2: (local[b2][0], local[b2][1], local[b2][2]) for b2 in bones})
    return frames

lift = build(LEAN)
print("lean: hips lifted %.1f, pelvis %s, hands %s %s, knees %s %s" % (
    lift, [round(v, 1) for v in pos("pelvis")], [round(v, 1) for v in pos("hand_l")], [round(v, 1) for v in pos("hand_r")],
    [round(v, 1) for v in pos("calf_l")], [round(v, 1) for v in pos("calf_r")]))
# ---- getting up and walking off: the push, three steps of the real walk, a closing step ----
WALK = "/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd"
ROOT_KIDS = [b for b in bones if parent[b] == "root"]
# She walks off at the game's slow walk (calm_walk.py: a light stick plays the walk at rate 0.5 and
# Stride Warping trims the stride to about half), so the clip plays the walk at RATE and re-solves
# the legs onto a stride of STRIDE: short, unhurried steps, about 90 cm/s. The arms swing half as
# far (the catwalk layer keeps only a third of the mocap swing in play).
RATE = globals().get("RATE", 0.55)
STRIDE = globals().get("STRIDE", 0.55)
LIFT = globals().get("LIFT", 0.75)          # how much of the walk's foot lift a short step keeps
ARM_MIX = globals().get("ARM_MIX", 0.5)     # the arms, this far from the walk's swing toward the idle's hang
WALK_FOOT_Z = 7.3                           # the foot bone's height when the foot is flat on the ground
build(STAND); STAND_ARMS = {b: tuple(local[b][1]) for b in ARM}

def snap():
    return {b: (tuple(local[b][0]), tuple(local[b][1]), tuple(local[b][2])) for b in bones}

def qmix(a, b, t):
    if sum(x * y for x, y in zip(a, b)) < 0:
        b = tuple(-v for v in b)
    q = tuple(x + (y - x) * t for x, y in zip(a, b)); n = math.sqrt(sum(v * v for v in q))
    return tuple(v / n for v in q)

def walk_pose(T, W, opt):
    """The walk at unwrapped time T (s), its stride shortened; returns (pose, its root's forward travel)."""
    L = W.get_play_length()
    p = APE.get_anim_pose_at_time(W, T % L, opt)
    for b in bones:
        if "toe_0" in b:
            continue                                 # the walk has no toe tracks: keep Quinn's (char_pose_peace.py)
        x = APE.get_bone_pose(p, b, unreal.AnimPoseSpaces.LOCAL); r = x.rotation
        local[b] = [(x.translation.x, x.translation.y, x.translation.z), qnorm((r.x, r.y, r.z, r.w)),
                    (x.scale3d.x, x.scale3d.y, x.scale3d.z)]
    _cache.clear()
    travel = local["root"][0][1] + math.floor(T / L) * WALK_CYCLE
    local["root"][0] = (0.0, 0.0, 0.0)
    _cache.clear()
    if STRIDE < 1.0:
        pv = pos("pelvis")
        for s in "lr":
            f, knee = "foot_" + s, pos("calf_" + s)
            toe0, sole0 = toe_dir(s), rot(world(f)[1], SOLE[s])
            a = pos(f)
            target = (a[0], pv[1] + (a[1] - pv[1]) * STRIDE, WALK_FOOT_Z + (a[2] - WALK_FOOT_Z) * LIFT)
            hint = (knee[0], pv[1] + (knee[1] - pv[1]) * STRIDE, knee[2])
            limb("thigh_" + s, "calf_" + s, f, target, hint)
            rotate(f, align2(toe_dir(s), rot(world(f)[1], SOLE[s]), toe0, sole0))   # the foot keeps its own angle
    for b in ARM:
        local[b][1] = qmix(tuple(local[b][1]), STAND_ARMS[b], ARM_MIX)
    _cache.clear()
    return snap(), travel * STRIDE

def placed(pose, anchor, root):
    """`pose` drawn with its root at `anchor` (cm forward), re-expressed for a root at `root`."""
    out = dict(pose)
    for b in ROOT_KIDS:
        t, q, sc = pose[b]
        out[b] = ((t[0], t[1] + anchor - root, t[2]), q, sc)
    out["root"] = ((0.0, root, 0.0), pose["root"][1], pose["root"][2])
    return out

def mix(fa, fb, t):
    def nl(a, b):
        if sum(x * y for x, y in zip(a, b)) < 0:
            b = tuple(-v for v in b)
        q = tuple(x + (y - x) * t for x, y in zip(a, b)); n = math.sqrt(sum(v * v for v in q))
        return tuple(v / n for v in q)
    return {b: (tuple(x + (y - x) * t for x, y in zip(fa[b][0], fb[b][0])), nl(fa[b][1], fb[b][1]),
                tuple(x + (y - x) * t for x, y in zip(fa[b][2], fb[b][2]))) for b in fa}

def ease(x): x = min(max(x, 0.0), 1.0); return x * x * (3 - 2 * x)

# the stop settle (Gamma Walk_F_End): hips 3 cm low and 2 cm back as the feet stop, chest braked
# ~3 deg back; the hips settle in 0.4 s, the head about half a second after them
DIP, CREEP, BRAKE = globals().get("DIP", 3.0), globals().get("CREEP", 2.0), globals().get("BRAKE", 3.0)
SETTLE_HIPS, SETTLE_HEAD = 0.4, 0.85
BRAKE_AT, STOP_PAST = 0.25, 0.07     # s of walk clock: braking starts this long before T_STOP, ends this long after

def up_and_walk():
    W = unreal.load_asset(WALK)
    opt = unreal.AnimPoseEvaluationOptions(); opt.set_editor_property("extract_root_motion", False)
    global WALK_CYCLE
    WALK_CYCLE = 0.0
    WALK_CYCLE = APE.get_bone_pose(APE.get_anim_pose_at_time(W, W.get_play_length(), opt), "root", unreal.AnimPoseSpaces.LOCAL).translation.y
    # lazily: the trunk tips over 0.6 s, the hips leave the stone at 1.25 s (the bench rise took
    # 0.7), the walk eases in over 0.6 s and runs on its slow clock, and the closing step takes 0.65
    T0, T_STOP, PUSH_T, IN_T, CLOSE_T, HOLD_T = 1.0, 2.1, 1.25, 0.60, 0.65, 0.85
    PREP_T = 0.60
    # the walk is placed so its planted right foot lands where her right foot rests in the lean
    build(LEAN); lean_r = pos("foot_r")[1]
    w0, tr0 = walk_pose(T0, W, opt); base_tr = tr0
    build_walk_r = None
    walk_pose(T0, W, opt); walk_r = pos("foot_r")[1]
    offset = lean_r - walk_r                         # walk root at T0, in her lean's space
    def WP(T):
        pose, tr = walk_pose(T, W, opt)
        return pose, offset + tr - base_tr          # travel is already on the short stride
    # where she stops: the left foot planted at T_STOP, the idle's left foot over it
    stop_pose, stop_root = WP(T_STOP); walk_pose(T_STOP % W.get_play_length(), W, opt)
    left_rel = pos("foot_l")[1]
    build(STAND); stand_pose = snap(); stand_left = pos("foot_l")[1]
    def settled(sec):
        """The stop settle, measured on Gamma's Walk_F_End (learning log 2026-11-10): the feet are
        planted, the hips are still low and a little behind, then rise and come over the feet
        within 0.4 s; the chest braked back and comes forward later, so the head arrives last."""
        hips, chest = ease(sec / SETTLE_HIPS), ease(sec / SETTLE_HEAD)
        P = dict(STAND, pelvis=add(STAND["pelvis"], (0.0, -CREEP * (1 - chest), -DIP * (1 - hips))),
                 lean_fwd=-BRAKE * (1 - chest), head_down=0.5 * BRAKE * (1 - chest))
        build(P); return snap()
    RF = stop_root + left_rel - stand_left
    frames = []
    fps = float(FPS)
    push_keys = [(0.0, LEAN), (PREP_T, PREP), (PUSH_T, PUSH)]
    total = PUSH_T + (T_STOP - T0) / RATE + CLOSE_T + HOLD_T
    # the walk's own clock (running at RATE) slows to a stop over the last step (Gamma: from walking
    # speed to still in ~0.3 s, never past the stop): from BRAKE_AT before T_STOP it runs on a
    # monotone ease that ends STOP_PAST after T_STOP, where her hips sit over the idle's, so nothing
    # has to slide back
    brake_tau = PUSH_T + (T_STOP - T0 - BRAKE_AT) / RATE
    span, budget = BRAKE_AT / RATE + CLOSE_T, BRAKE_AT + STOP_PAST
    m0 = min(RATE * span / budget, 3.0)
    def walk_t(tau):
        if tau <= brake_tau:
            return T0 + (tau - PUSH_T) * RATE
        x = min((tau - brake_tau) / span, 1.0)
        return T_STOP - BRAKE_AT + budget * ((x ** 3 - 2 * x ** 2 + x) * m0 + (-2 * x ** 3 + 3 * x ** 2))
    close_tau = PUSH_T + (T_STOP - T0) / RATE
    _, r_a = WP(walk_t(close_tau)); _, r_b = WP(walk_t(close_tau + 1.0 / fps))
    root_a, v_close = r_a, (r_b - r_a) * fps
    for f in range(int(round(total * fps)) + 1):
        tau = f / fps
        if tau <= PUSH_T:
            build(sample(push_keys, tau)); frames.append(placed(snap(), 0.0, 0.0)); continue
        T = walk_t(tau)
        if tau <= close_tau:
            wp, wr = WP(T)
            x = (tau - PUSH_T) / IN_T
            if x < 1.0:                              # out of the push into the walk
                build(PUSH); push = snap()
                k = ease(x); root = max(0.0, wr) * k
                frames.append(mix(placed(push, 0.0, root), placed(wp, wr, root), k))
            else:
                frames.append(placed(wp, wr, wr))
            continue
        x = (tau - close_tau) / CLOSE_T
        if x <= 1.0:                                 # the closing step, settling into the idle
            wp, wr = WP(T)
            d = RF - root_a
            m1 = min(max(v_close * CLOSE_T, 0.0), 3.0 * d)
            xx = min(x, 1.0)
            root = (2 * xx ** 3 - 3 * xx ** 2 + 1) * root_a + (xx ** 3 - 2 * xx ** 2 + xx) * m1 + (-2 * xx ** 3 + 3 * xx ** 2) * RF
            frames.append(mix(placed(wp, wr, root), placed(settled(0.0), RF, root), ease(x)))
        else:
            frames.append(placed(settled(tau - close_tau - CLOSE_T), RF, RF))
    print("lean up: %.2f s, walks %.0f cm" % (total, RF))
    return frames

# The stand-up the menu plays when Body, Wardrobe, Photo or Start is picked. Tema (QA 0320): the
# lazy walk-off (up_and_walk, 4.75 s) kept the page waiting 4 s after the click. Now one snappy
# push: the hands press, the hips come off the stone, the right foot steps through and she is
# up in her idle, in place, in 1 s. Every part still leads or follows (LEAD), on a third of
# the clock, so it reads as a push and not as a pose sliding into another.
SNAP_KEYS = [(0.0, LEAN), (0.28, PUSH), (0.55, STEP), (0.80, STAND)]
SNAP_LEAD, SNAP_PAD = 1.0 / 3.0, 0.10

def snap_up():
    global LEAD, PAD
    keep = LEAD, PAD
    LEAD, PAD = {k: v * SNAP_LEAD for k, v in keep[0].items()}, SNAP_PAD
    try:
        return clip(SNAP_KEYS)
    finally:
        LEAD, PAD = keep

frames_up = snap_up() if not globals().get("WALK_OFF") else up_and_walk()
print("lean up: %.2f s" % ((len(frames_up) - 1) / float(FPS)))
if BUILD:
    write("A_Menu_Lean", breathing(), MENU_DIR)
    up_seq = write("A_Menu_Lean_Up", frames_up, MENU_DIR, False)
    walks = bool(globals().get("WALK_OFF"))
    up_seq.set_editor_property("enable_root_motion", walks)  # the walk-off travels; the snap stands up in place
    up_seq.set_editor_property("force_root_lock", False)
    unreal.EditorAssetLibrary.save_loaded_asset(up_seq, only_if_is_dirty=False)
    print("saved %s/A_Menu_Lean_Up %.2f s, root motion %s" % (MENU_DIR, up_seq.get_play_length(), walks))
    write("A_Menu_Lean_Down", clip(DOWN_KEYS), MENU_DIR)
shoot("lean", frames_up, SHOTS, globals().get("VIEWS", ("front3qr", "right")))
