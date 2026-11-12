"""Menu Settings sit (Tema's sketch, 2026-11-12): she climbs from the lean up onto the drum and
perches on its top-front edge, right foot down on the sand on its ball, left leg stretched out long
toward the menu camera (69 deg to her left, measured in Play), toes up, sole to the lens, toes up, the sole to the lens. Hands on the stone behind her hips. Idempotent.

  python3 Tools/ue.py Tools/char_menu_sit.py     # builds three clips, then shoots stills

Writes /Game/Characters/Menu/A_Menu_Sit_Settings (a breathing loop), A_Menu_Sit_Settings_Down
(lean to perch) and A_Menu_Sit_Settings_Up (perch to lean). All in place: the root never moves.
The menu director plays them while Settings is open (VisexMenuDirector.cpp, SitForSettings).
The lean key, the column and the hands come from char_menu_lean.py; the solver from
char_pose_mermaid.py.
Globals: BUILD=False, SHOTS=[...] (seconds into _Down) or [], TWEAK={...} for the perch key,
CAM_DEG (where the lens is, deg from her forward, + toward her left), LEG_DEG (where the
raised leg points), REACH, FOOT_Z.
"""
import math
import os
import unreal

if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise RuntimeError("Play mode is running: stop PIE first")
BUILD = globals().get("BUILD", True)
SHOTS = globals().get("SHOTS", [0.0, 0.8, 1.6, 2.6])
_tools = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools")
_sit_keep = dict(BUILD=BUILD, SHOTS=SHOTS, TWEAK=globals().get("TWEAK", {}), VIEWS=globals().get("VIEWS", ("side", "front", "left")))
# the lean script defines LEAN, the column (CY, CZ, RADIUS) and the lean's arms; with BUILD off it writes nothing
BUILD, SHOTS, TWEAK = False, [], {}
exec(compile(open(os.path.join(_tools, "char_menu_lean.py")).read(), "char_menu_lean.py", "exec"), globals())
globals().update(_sit_keep)   # the lean script has its own _keep
print("column in her space: axis y %.1f z %.1f, radius %.1f, top %.1f" % (CY, CZ, RADIUS, TOP))

# where the menu lens is, seen from her (deg in the ground plane from her forward, + toward her left)
# (measured in Play 2026-11-12, her mesh space: the menu camera at (255, 98, -14), 69 deg to her left)
def _dir(deg): return (math.sin(math.radians(deg)), math.cos(math.radians(deg)), 0.0)   # X is her left
CAM_DEG = globals().get("CAM_DEG", 69.0)
LEG_DEG = globals().get("LEG_DEG", 35.0)              # the leg a little off the lens line: the foot beside her, not over her
TO_CAM, TO_LEG = _dir(CAM_DEG), _dir(LEG_DEG)
E, PL = ("r", "l") if CAM_DEG < 0 else ("l", "r")     # the raised leg on the lens's side, the other carries her
SX = 1.0 if PL == "l" else -1.0                         # the planted side's X sign

# ---- the perch ----
# higher up the drum than the lean: glutes on its top-front edge, pelvis bone ~11 cm off the stone
PERCH_DEG = globals().get("PERCH_DEG", 29.0)           # from the drum's top toward her
PERCH_OFF = globals().get("PERCH_OFF", 10.0)
_a = math.radians(PERCH_DEG)
PELVIS = (1.0, CY + (RADIUS + PERCH_OFF) * math.sin(_a), CZ + (RADIUS + PERCH_OFF) * math.cos(_a))

# the raised leg out long toward the lens, the foot raised, toes up, sole to the camera
REACH = globals().get("REACH", 0.99)                    # of the leg's full length, hip to ankle: long, the knee just soft
KNEE_UP = globals().get("KNEE_UP", 10.0)
FOOT_Z = globals().get("FOOT_Z", 42.0)                  # the ankle's height over the sand
build(LEAN)
_hip = pos("thigh_" + E)
_leg = length(sub(pos("calf_" + E), pos("thigh_" + E))) + length(sub(pos("foot_" + E), pos("calf_" + E)))
_hip_p = add(_hip, sub(PELVIS, LEAN["pelvis"]))         # where the hip joint will be on the perch (near enough)
_dz = FOOT_Z - _hip_p[2]
_flat = math.sqrt(max((_leg * REACH) ** 2 - _dz * _dz, 1.0))
ANKLE_X = (_hip_p[0] + TO_LEG[0] * _flat, _hip_p[1] + TO_LEG[1] * _flat, FOOT_Z)
TOE_X = norm(add(mul(TO_CAM, 0.25), (0.0, 0.0, 1.0)))   # toes up, a little toward the lens
SOLE_X = norm(perp(TO_CAM, TOE_X))                      # the sole faces the lens
KNEE_X = add(lerp(_hip_p, ANKLE_X, 0.5), (0.0, 0.0, KNEE_UP))   # the knee bends up, never down

# the other foot down on the sand on its ball, heel up, a little out, carrying her
ANKLE_P = globals().get("ANKLE_P", (SX * 16.0, -20.0, 11.5))   # under the hip: the leg just reaches
TOE_P = norm((SX * 0.2, 0.55, -0.8))

PERCH = dict(LEAN, pelvis=PELVIS, roll=-3.0, pitch=-9.0, yaw=SX * 20.0,   # hips turned toward the lens
             **{"knee_" + PL: (SX * 22.0, 40.0, 50.0), "knee_" + E: KNEE_X},
             lean_left=-2.0, lean_fwd=-13.0, twist_left=4.0, head_tilt=4.0, head_down=4.0, head_left=6.0,
             hand_deg=globals().get("HAND_DEG_SIT", 30.0), hand_x=globals().get("HAND_X_SIT", 34.0),
             arms=1.0, floor=0.0)
PERCH.update(foot(PL, ANKLE_P, TOE_P))
PERCH.update(foot(E, ANKLE_X, TOE_X, SOLE_X))
PERCH.update(globals().get("TWEAK", {}))

# on the way up: hands press, the hips rise and slide back onto the top, both feet still down
CLIMB = dict(LEAN, pelvis=lerp(LEAN["pelvis"], PELVIS, 0.25),   # the lean's hip tilt, low enough that both feet stay on the sand
             lean_fwd=4.0, head_down=4.0, hand_deg=0.5 * (LEAN["hand_deg"] + PERCH["hand_deg"]), arms=1.0, floor=0.0)
# the raised leg starts to lift, knee first
LIFT = dict(PERCH, lean_fwd=-2.0, head_down=6.0)
LIFT["knee_" + E] = add(KNEE_X, (0.0, -12.0, 16.0))
LIFT.update(foot(E, add(lerp(sub(LEAN["ball_" + E], mul(LEAN["toe_" + E], FOOT[E])), ANKLE_X, 0.5), (0.0, 0.0, 10.0)),
                 lerp(LEAN["toe_" + E], TOE_X, 0.4), (0.0, 0.6, -0.8)))

carry_hands(LEAN, CLIMB, LIFT, PERCH)

DOWN_KEYS = [(0.0, LEAN), (0.75, CLIMB), (1.45, LIFT), (2.15, PERCH)]
UP_KEYS = [(0.0, PERCH), (0.65, LIFT), (1.30, CLIMB), (2.00, LEAN)]

def breathing(seconds=12.0):
    """The perch, alive: three breaths, a slow sway, and the raised foot easing a little (whole
    cycles, so it loops)."""
    frames = []
    n = int(round(seconds * FPS))
    for f in range(n + 1):
        w = 2.0 * math.pi * f / n
        b = 3.0 * w
        P = dict(PERCH)
        P["lean_fwd"] = PERCH["lean_fwd"] - 1.2 * math.sin(b) - 1.2 * math.sin(w + 0.3)
        P["pelvis"] = add(PERCH["pelvis"], (1.0 * math.sin(w + 0.6), 0.0, 0.3 * math.sin(b)))
        P["roll"] = PERCH["roll"] + 1.2 * math.sin(w + 0.6)
        P["twist_left"] = PERCH["twist_left"] + 1.5 * math.sin(w + 2.0)
        P["head_left"] = PERCH["head_left"] + 3.0 * math.sin(w + 1.2) + 1.5 * math.sin(2 * w + 0.4)
        P["head_tilt"] = PERCH["head_tilt"] + 1.5 * math.sin(2 * w)
        P["ball_" + E] = add(PERCH["ball_" + E], (1.5 * math.sin(w + 0.9), 0.0, 1.2 * math.sin(2 * w + 0.2)))
        P["toe_" + E] = norm(add(PERCH["toe_" + E], mul(TO_CAM, 0.06 * math.sin(2 * w + 1.4))))   # the foot flexes a touch
        build(P)
        frames.append({b2: (local[b2][0], local[b2][1], local[b2][2]) for b2 in bones})
    return frames

lift = build(PERCH)
print("perch: lift %.1f, pelvis %s, raised ankle %s (asked %s), planted ball %s, hands %s %s, over limits %s" % (
    lift, [round(v, 1) for v in pos("pelvis")], [round(v, 1) for v in pos("foot_" + E)], [round(v, 1) for v in ANKLE_X],
    [round(v, 1) for v in pos("ball_" + PL)], [round(v, 1) for v in pos("hand_l")], [round(v, 1) for v in pos("hand_r")], OVER[:4]))
print("hand miss l %.1f r %.1f cm" % (ARM_MISS["l"], ARM_MISS["r"]))
frames_down = clip(DOWN_KEYS)
if BUILD:
    write("A_Menu_Sit_Settings", breathing(), MENU_DIR)
    write("A_Menu_Sit_Settings_Down", frames_down, MENU_DIR)
    write("A_Menu_Sit_Settings_Up", clip(UP_KEYS), MENU_DIR)
shoot("sit", frames_down, SHOTS, VIEWS)

def shoot_menu(frame, name="sit_menu", dist=200.0, height=14.0, look=70.0, pan=-16.0, fov=30.0):
    """One still of `frame` from where the menu's floor camera stands (CAM_DEG, dist, height,
    looking at `look` cm over her feet, turned by `pan`), to Tools/.bridge/<name>.png."""
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    stand = {a.get_actor_label(): a for a in eas.get_all_level_actors()}["MenuModel"]
    bp = unreal.load_asset(BP_PATH)
    hh = unreal.get_default_object(bp.generated_class()).get_editor_property("capsule_component").get_unscaled_capsule_half_height()
    stand.set_is_temporarily_hidden_in_editor(True)
    still = write("A_Shot_M", [frame], "/Game/Characters/Scratch", False)
    char, props = None, []
    try:
        char = eas.spawn_actor_from_class(bp.generated_class(), stand.get_actor_location() + unreal.Vector(0, 0, hh),
                                          unreal.Rotator(roll=0, pitch=0, yaw=stand.get_actor_rotation().yaw + 90.0))
        body = char.get_editor_property("mesh")
        body.override_animation_data(still)
        xf = body.get_world_transform()
        for mesh_path, centre, axis, scale in PROPS:
            prop = eas.spawn_actor_from_class(unreal.StaticMeshActor, xf.transform_location(unreal.Vector(*centre)),
                                              unreal.MathLibrary.make_rot_from_z(xf.transform_direction(unreal.Vector(*axis))))
            prop.static_mesh_component.set_static_mesh(unreal.load_asset(mesh_path))
            prop.set_actor_scale3d(unreal.Vector(*scale))
            props.append(prop)
        feet = xf.transform_location(unreal.Vector(0, 0, 0))
        d = xf.transform_direction(unreal.Vector(*TO_CAM)); d.z = 0.0; d = d.normal()
        cam = feet + d * dist + unreal.Vector(0, 0, height)
        aim = feet + unreal.Vector(0, 0, look) - cam
        p = math.radians(pan); aim = unreal.Vector(aim.x * math.cos(p) - aim.y * math.sin(p), aim.x * math.sin(p) + aim.y * math.cos(p), aim.z)
        tgt = cam + aim
        g = {"__name__": "__bridge__", "VIEW": ((cam.x, cam.y, cam.z), (tgt.x, tgt.y, tgt.z), fov)}
        exec(compile(open(os.path.join(TOOLS, "capture.py")).read(), "capture.py", "exec"), g)
        shutil.move(os.path.join(TOOLS, ".bridge", "capture.png"), os.path.join(TOOLS, ".bridge", name + ".png"))
    finally:
        for prop in props:
            eas.destroy_actor(prop)
        if char:
            eas.destroy_actor(char)
        unreal.EditorAssetLibrary.delete_asset("/Game/Characters/Scratch/A_Shot_M")
        stand.set_is_temporarily_hidden_in_editor(False)

if globals().get("MENU_SHOT", True):
    build(PERCH)
    shoot_menu({b: (local[b][0], local[b][1], local[b][2]) for b in bones})
