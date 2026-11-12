"""Stand her idle in Play, log her posture, feet and hip hand, and shoot 4K stills.

  python3 Tools/ue.py Tools/char_idle_pie.py; python3 Tools/ue.py Tools/char_idle_check.py
  Globals: TAG ("before" / "after"), SECONDS (20), SHOTS {seconds: [views]}.
Results: Tools/.bridge/out/idle_<TAG>.txt and Tools/.bridge/shots/idle_<TAG>_<view>_<s>.png.
Angles in her mesh space (Y forward, Z up), degrees, + = forward.
"""
import math
import os
import unreal

TAG = globals().get("TAG", "after")
SECONDS = globals().get("SECONDS", 20.0)
DIST = globals().get("DIST", 800.0)
SHOTS = globals().get("SHOTS", {3.0: ["front34", "side"], 9.0: ["front34"]})
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
if not world:
    raise RuntimeError("Play mode isn't running")
pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
mesh = pawn.get_editor_property("mesh")
for c in pawn.get_components_by_class(unreal.SkeletalMeshComponent):
    c.set_editor_property("bounds_scale", 3.0)
skin = next((c for c in pawn.get_components_by_class(unreal.SkeletalMeshComponent) if c.get_name() == "Shell"), mesh)
root = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools", ".bridge")
shot_dir = os.path.join(root, "shots")
rt = unreal.RenderingLibrary.create_render_target2d(world, 3840, 2160, unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
cap = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SceneCapture2D)[0]
comp = cap.capture_component2d
comp.set_editor_property("texture_target", rt)
comp.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_TONE_CURVE_HDR)
comp.set_editor_property("fov_angle", 24.0)
unreal.SystemLibrary.execute_console_command(world, "t.IdleWhenNotForeground 0")
CS = unreal.RelativeTransformSpace.RTS_COMPONENT


def p(b, space=CS):
    v = mesh.get_socket_transform(b, space).translation
    return (v.x, v.y, v.z)


def lean(a, b):
    pa, pb = p(a), p(b)
    return math.degrees(math.atan2(pb[1] - pa[1], pb[2] - pa[2]))


def yaw(b):
    q = mesh.get_socket_transform(b, CS).rotation
    return q.rotator().yaw


def head_turn(q0):
    q = mesh.get_socket_transform("head", CS).rotation
    f = q.multiply(q0.inversed()).rotate_vector(unreal.Vector(0, 1, 0))
    return math.degrees(math.atan2(-f.x, f.y))


def gap(side):
    lib = getattr(unreal, "VisexSkinProbeLibrary", None)
    if lib and hasattr(lib, "hand_body_gap"):
        return lib.hand_body_gap(skin, side == "l")
    return -1.0


def shoot(view, t):
    at = pawn.get_actor_location()
    tl, tr = (unreal.Vector(*p("thigh_" + s, WS)) for s in "lr")
    left = tl - tr
    left.z = 0.0
    left = left.normal()
    fwd = unreal.Vector(0, 0, 1).cross(left)  # her facing, from her hips
    right = left * -1.0
    d = {"front34": fwd * 0.75 + right * -0.66, "side": right * -1.0, "front": fwd, "side_r": right}[view]
    eye = at + d * DIST + unreal.Vector(0, 0, 5)
    look = at + unreal.Vector(0, 0, -3)
    cap.set_actor_location_and_rotation(eye, unreal.MathLibrary.find_look_at_rotation(eye, look), False, False)
    comp.capture_scene()
    name = "idle_%s_%s_%s.png" % (TAG, view, t if isinstance(t, str) else "%02d" % int(t))
    unreal.RenderingLibrary.export_render_target(world, rt, shot_dir, name)


state = {"t": 0.0, "next": 0.0, "rows": [], "done": set(), "q0": None,
         "w0": unreal.GameplayStatics.get_time_seconds(world)}   # game time: the slate's dt runs slow in Play
WS = unreal.RelativeTransformSpace.RTS_WORLD


def sample():
    if state["q0"] is None:
        state["q0"] = mesh.get_socket_transform("head", CS).rotation
    pel = p("pelvis")
    sh = [p("upperarm_" + s) for s in "lr"]
    row = dict(t=state["t"], trunk=lean("pelvis", "neck_01"), upper=lean("spine_03", "neck_01"),
               neck=lean("neck_01", "head"), shoulders=(sh[0][1] + sh[1][1]) / 2 - p("spine_05")[1],
               head=head_turn(state["q0"]), pelvis_x=pel[0])
    for s in "lr":
        row["foot_" + s] = p("ball_" + s, WS)
        h = p("hand_" + s)
        row["hand_" + s] = (h[0] - pel[0], h[1] - pel[1], h[2] - pel[2])
        row["gap_" + s] = gap(s)
    state["rows"].append(row)
    # stills keyed to what she is doing, not the clock (the idle's phase at spawn varies)
    when = {"left": row["hand_l"][2] > 8 and abs(row["head"]) < 6 and row["t"] > 2,
            "look": row["hand_l"][2] > 8 and abs(row["head"]) > 28,
            "right": row["hand_r"][2] > 10}
    for name, views in globals().get("STATE_SHOTS", {}).items():
        if when[name] and name not in state["done"]:
            state["done"].add(name)
            for v in views:
                shoot(v, name)
            print_name = "idle %s still at %.1f s" % (name, row["t"])
            unreal.log(print_name)


def report():
    rows = state["rows"]
    out = []
    for k in ("trunk", "upper", "neck", "shoulders", "head", "pelvis_x", "gap_l", "gap_r"):
        v = [r[k] for r in rows]
        out.append("%s mean %.1f min %.1f max %.1f" % (k, sum(v) / len(v), min(v), max(v)))
    for s in "lr":
        f = [r["foot_" + s] for r in rows]
        out.append("ball_%s drift %.2f cm, lifts %.2f cm" % (s, max(math.dist(a, f[0]) for a in f),
                                                          max(a[2] for a in f) - min(a[2] for a in f)))
    for r in rows[::4]:
        out.append("t%.1f trunk %.1f upper %.1f neck %.1f sh %.1f head %.0f pelx %.1f hL(%.0f,%.0f,%.0f) hR(%.0f,%.0f,%.0f) gap %.1f/%.1f" % (
            r["t"], r["trunk"], r["upper"], r["neck"], r["shoulders"], r["head"], r["pelvis_x"],
            *r["hand_l"], *r["hand_r"], r["gap_l"], r["gap_r"]))
    open(os.path.join(root, "out", "idle_%s.txt" % TAG), "w").write("\n".join(out) + "\n")


WALK = globals().get("WALK", False)        # True: walk her forward instead, to compare the walk's posture
_dir = pawn.get_actor_forward_vector()


def tick(dt):
    try:
        if WALK:
            pawn.add_movement_input(_dir, 1.0)
        state["t"] = unreal.GameplayStatics.get_time_seconds(world) - state["w0"]
        if state["t"] >= state["next"]:
            sample()
            state["next"] += 0.5
        loop = globals().get("LOOP", 37.8)   # shots keyed to the idle clip's own clock (she idles from spawn)
        ct = pawn.get_game_time_since_creation() % loop
        for ts, views in globals().get("CLIP_SHOTS", {}).items():
            if ("c", ts) not in state["done"] and ts <= ct < ts + 1.0:
                state["done"].add(("c", ts))
                for v in views:
                    shoot(v, ts)
        for ts, views in SHOTS.items():
            if ts not in state["done"] and state["t"] >= ts:
                state["done"].add(ts)
                for v in views:
                    shoot(v, ts)
        if state["t"] >= SECONDS:
            raise StopIteration
    except Exception as e:
        unreal.unregister_slate_post_tick_callback(handle)
        if not isinstance(e, StopIteration):
            unreal.log_error("idle check: %s" % e)
            open(os.path.join(root, "out", "idle_%s.txt" % TAG), "w").write("error %s\n" % e)
        else:
            report()


handle = unreal.register_slate_post_tick_callback(tick)
print("checking idle", TAG, SECONDS, "s")
