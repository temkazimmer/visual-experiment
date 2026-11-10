"""Measure a movement clip: the numbers behind weight, stride, sway and overlap.

  python3 Tools/ue.py -c "CLIPS=['/Game/Study/Strut/AS_Walk_Strut_1']; exec(open('Tools/char_study.py').read())"
  python3 Tools/ue.py -c "RECORD=4.0; exec(open('Tools/char_study.py').read())"   # in Play: start recording her
  python3 Tools/ue.py -c "REPORT='rec'; exec(open('Tools/char_study.py').read())" # ...then measure the recording

Works on any clip on a UE5-mannequin skeleton (ours or a pack's own copy: the bone names
match), sampled in component space. RECORD samples her live mesh each frame in Play, so the
catwalk layer and everything after the Anim Blueprint is included: that is the only way to
measure our walk as seen. Everything is in cm, degrees, seconds and frames at 30 fps.
Printed as one table; with OUT='name' also written to Tools/.bridge/study_<name>.json.

What it reports, per clip (see Docs/Research/ANIMATION.md §9):
  speed     travel of the root per second, or (in place) the planted foot's backward speed
  cadence   steps per second; step = speed / cadence
  width     sideways distance between the feet at plant; line = mean distance of a planted
            foot from the pelvis's line (0 = on the midline, the catwalk ideal)
  pelvis    height mean and bob (peak to peak); dip = how far it drops after a contact
  hips      yaw and roll of the hip line, peak to peak
  chest     yaw of the shoulder line, peak to peak; counter = -corr(hips, chest) (1 = fully opposed)
  head      bob and sway, peak to peak; lean = spine pitch from vertical (+ forward)
  arms      hand swing forward and back relative to the pelvis, peak to peak
  lag       frames by which chest yaw follows hip yaw, and head height follows pelvis height
"""
import json
import math
import os
import unreal

TOOLS = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools")
BONES = ["root", "pelvis", "thigh_l", "thigh_r", "foot_l", "foot_r", "ball_l", "ball_r", "spine_05",
         "upperarm_l", "upperarm_r", "hand_l", "hand_r", "neck_01", "head"]
FPS = 30.0
REC = os.path.join(TOOLS, ".bridge", "study_rec.json")

# ---- sources: a clip, or a recording of her in Play ----
def from_clip(path):
    seq = unreal.load_asset(path)
    if not seq:
        raise RuntimeError("no clip at " + path)
    APE = unreal.AnimPoseExtensions
    opt = unreal.AnimPoseEvaluationOptions()
    opt.set_editor_property("extract_root_motion", False)
    L = seq.get_play_length()
    frames = []
    for i in range(int(L * FPS) + 1):
        p = APE.get_anim_pose_at_time(seq, min(i / FPS, L), opt)
        fr = {}
        for b in BONES:
            t = APE.get_bone_pose(p, b, unreal.AnimPoseSpaces.WORLD).translation
            fr[b] = (t.x, t.y, t.z)
        frames.append(fr)
    return frames

def start_recording(seconds):
    gw = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not gw:
        raise RuntimeError("RECORD needs Play running")
    ch = unreal.GameplayStatics.get_player_character(gw, 0)
    mesh = ch.mesh
    rec = {"frames": [], "t": []}
    t0 = unreal.SystemLibrary.get_game_time_in_seconds(gw)
    def tick(dt):
        t = unreal.SystemLibrary.get_game_time_in_seconds(gw) - t0
        fr = {}
        for b in BONES:
            v = mesh.get_socket_transform(b, unreal.RelativeTransformSpace.RTS_COMPONENT).translation
            fr[b] = (v.x, v.y, v.z)
        rec["frames"].append(fr); rec["t"].append(t)
        if t >= seconds:
            unreal.unregister_slate_post_tick_callback(handle[0])
            with open(REC, "w") as f:
                json.dump(rec, f)
            unreal.log("study: recorded %d frames" % len(rec["frames"]))
    handle = [unreal.register_slate_post_tick_callback(tick)]
    print("recording %.1f s" % seconds)

def from_recording():
    rec = json.load(open(REC))
    # resample to 30 fps so lags read in frames
    ts, src = rec["t"], rec["frames"]
    out, j = [], 0
    for i in range(int(ts[-1] * FPS) + 1):
        t = i / FPS
        while j + 1 < len(ts) - 1 and ts[j + 1] < t:
            j += 1
        a, b = src[j], src[min(j + 1, len(src) - 1)]
        k = 0.0 if ts[min(j + 1, len(ts) - 1)] == ts[j] else min(max((t - ts[j]) / (ts[j + 1] - ts[j]), 0.0), 1.0)
        out.append({n: tuple(x + (y - x) * k for x, y in zip(a[n], b[n])) for n in BONES})
    return out

# ---- the measures ----
def p2p(v): return max(v) - min(v) if v else 0.0
def mean(v): return sum(v) / len(v) if v else 0.0
def corr(a, b):
    ma, mb = mean(a), mean(b)
    num = sum((x - ma) * (y - mb) for x, y in zip(a, b))
    den = math.sqrt(sum((x - ma) ** 2 for x in a) * sum((y - mb) ** 2 for y in b)) or 1.0
    return num / den
def lag(a, b, most=8, sign=1.0):
    """Frames by which b follows a (b[t] ~ sign * a[t - lag])."""
    best, at = -2.0, 0
    for k in range(0, most + 1):
        c = corr(a[:len(a) - k], [sign * x for x in b[k:]])
        if c > best:
            best, at = c, k
    return at

def measure(frames):
    n = len(frames)
    dur = (n - 1) / FPS
    g = lambda b, i: [f[b][i] for f in frames]
    # forward: the root's travel, else the direction a planted foot slides (backwards)
    travel = (frames[-1]["root"][0] - frames[0]["root"][0], frames[-1]["root"][1] - frames[0]["root"][1])
    if math.hypot(*travel) > 20.0:
        fwd = (travel[0] / math.hypot(*travel), travel[1] / math.hypot(*travel))
        speed = math.hypot(*travel) / dur
        in_place = False
    else:
        fwd, in_place = (0.0, 1.0), True
    side = (fwd[1], -fwd[0])
    F = lambda p: p[0] * fwd[0] + p[1] * fwd[1]
    S = lambda p: p[0] * side[0] + p[1] * side[1]
    rel = lambda b, i: tuple(x - y for x, y in zip(frames[i][b], frames[i]["root"]))

    # plants: the ball of a foot at its lowest, and not moving forward
    plants = []
    for s in "lr":
        z = [frames[i]["ball_" + s][2] for i in range(n)]
        floor = min(z) + 2.5
        down = False
        for i in range(1, n):
            low = z[i] <= floor
            if low and not down:
                plants.append((i, s))
            down = low
    plants.sort()
    if in_place:   # speed from a planted foot sliding back under her
        slides = []
        for i, s in plants:
            j = min(i + 4, n - 1)
            slides.append(-(F(frames[j]["ball_" + s]) - F(frames[i]["ball_" + s])) / ((j - i) / FPS or 1))
        speed = mean([v for v in slides if v > 0]) if slides else 0.0
    alt = [p for k, p in enumerate(plants) if k == 0 or p[1] != plants[k - 1][1]]
    cadence = (len(alt) - 1) / ((alt[-1][0] - alt[0][0]) / FPS) if len(alt) > 2 else 0.0
    pelvis_side = mean([S(rel("pelvis", i)) for i in range(n)])
    widths, line = [], []
    for k in range(1, len(alt)):
        i, s = alt[k]; j = alt[k - 1][0]
        widths.append(abs(S(rel("foot_l", i)) - S(rel("foot_r", i))))
    for i, s in plants:
        line.append(abs(S(rel("foot_" + s, i)) - pelvis_side))

    pz = g("pelvis", 2)
    dips = []
    for i, s in plants:
        before = max(pz[max(0, i - 3):i + 1])
        after = min(pz[i:min(n, i + 9)])
        dips.append(before - after)

    def yaw_of(a, b):
        out = []
        for f in frames:
            v = tuple(x - y for x, y in zip(f[a], f[b]))
            out.append(math.degrees(math.atan2(F(v), S(v))))
        return out
    def roll_of(a, b):
        return [math.degrees(math.atan2(f[a][2] - f[b][2], math.hypot(f[a][0] - f[b][0], f[a][1] - f[b][1]))) for f in frames]
    hip_yaw, hip_roll = yaw_of("thigh_l", "thigh_r"), roll_of("thigh_l", "thigh_r")
    chest_yaw = yaw_of("upperarm_l", "upperarm_r")
    hz = g("head", 2)
    head_side = [S(rel("head", i)) for i in range(n)]
    lean = mean([math.degrees(math.atan2(F(tuple(x - y for x, y in zip(f["neck_01"], f["pelvis"]))),
                                          f["neck_01"][2] - f["pelvis"][2])) for f in frames])
    swing = mean([p2p([F(rel("hand_" + s, i)) - F(rel("pelvis", i)) for i in range(n)]) for s in "lr"])
    return dict(
        seconds=round(dur, 2), in_place=in_place, speed=round(speed), cadence=round(cadence, 2),
        step=round(speed / cadence) if cadence else 0, width=round(mean(widths), 1), line=round(mean(line), 1),
        pelvis_z=round(mean(pz), 1), pelvis_bob=round(p2p(pz), 1), dip=round(mean(dips), 1),
        hip_yaw=round(p2p(hip_yaw), 1), hip_roll=round(p2p(hip_roll), 1),
        chest_yaw=round(p2p(chest_yaw), 1), counter=round(-corr(hip_yaw, chest_yaw), 2),
        head_bob=round(p2p(hz), 1), head_sway=round(p2p(head_side), 1), lean=round(lean, 1),
        arm_swing=round(swing, 1),
        lag_chest=lag(hip_yaw, chest_yaw, sign=-1.0), lag_head=lag(pz, hz))

COLS = ["seconds", "speed", "cadence", "step", "width", "line", "pelvis_bob", "dip", "hip_yaw", "hip_roll",
        "chest_yaw", "counter", "head_bob", "head_sway", "lean", "arm_swing", "lag_chest", "lag_head"]
def table(rows):
    print("%-28s " % "clip" + " ".join("%9s" % c[:9] for c in COLS))
    for name, m in rows:
        print("%-28s " % name[-28:] + " ".join("%9s" % m[c] for c in COLS))

if globals().get("RECORD"):
    start_recording(float(RECORD))
else:
    rows = []
    for path in globals().get("CLIPS", []):
        rows.append((path.split("/")[-1], measure(from_clip(path))))
    if globals().get("REPORT"):
        rows.append((REPORT, measure(from_recording())))
    table(rows)
    if globals().get("OUT"):
        with open(os.path.join(TOOLS, ".bridge", "study_%s.json" % OUT), "w") as f:
            json.dump(dict(rows), f, indent=1)
