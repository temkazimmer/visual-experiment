"""Reads the jitter tour's per-frame CSVs (Saved/QA/jitter_<spot>.csv) and says where the
jitter enters: the off-line distance (mm) of each foot at each stage of the frame (the graph's
foot, after the layers, after the pin, final), and of the signals that feed the plant pass.

  python3 Docs/Research/idle_jitter/analyse.py Saved/QA/jitter_dune25x.csv [--worst 3]
"""
import csv
import math
import sys


def load(path):
    rows = list(csv.DictReader(open(path)))
    return {k: [float(r[k]) for r in rows] for k in rows[0]}


def vec(d, name):
    return list(zip(d[name + "_x"], d[name + "_y"], d[name + "_z"]))


def offline(points, dt):
    """Per frame: distance (mm) from the time-weighted line between the frames either side."""
    out = [0.0] * len(points)
    for i in range(1, len(points) - 1):
        d0, d1 = dt[i], dt[i + 1]
        if d0 <= 1e-5 or d1 <= 1e-5:
            continue
        t = d0 / (d0 + d1)
        a, b, c = points[i - 1], points[i], points[i + 1]
        out[i] = 10.0 * math.sqrt(sum((b[k] - (a[k] + (c[k] - a[k]) * t)) ** 2 for k in range(3)))
    return out


def scalar(values):
    return [(0.0, 0.0, v) for v in values]


def stat(off):
    n = max(1, len(off) - 2)
    return math.sqrt(sum(x * x for x in off) / n), max(off)


def main():
    path = sys.argv[1]
    worst_n = int(sys.argv[sys.argv.index("--worst") + 1]) if "--worst" in sys.argv else 3
    d = load(path)
    dt, t = d["dt"], d["t"]
    print("%s: %d frames, %.1f s, dt %.1f..%.1f ms" % (path.split("/")[-1], len(t), t[-1] - t[0], 1000 * min(dt), 1000 * max(dt)))
    print("  stage / signal        rms mm   worst mm")
    tracks = [("pelvis graph", vec(d, "pelvisg")), ("pelvis final", vec(d, "pelvis")), ("pelvis fix (cm->mm)", scalar(d["pelvis_fix"]))]
    for leg in "lr":
        for label, key in (("foot graph", "footg"), ("foot layers", "footlay"), ("foot pin", "footpin"), ("foot final", "foot"),
                           ("ball final", "ball"), ("knee final", "knee"), ("hip final", "hip"), ("plant trace hit", "trace")):
            tracks.append(("%s %s" % (label, leg.upper()), vec(d, "%s_%s" % (key, leg))))
        for label, key in (("sole under", "soleunder"), ("plant ask", "plantask"), ("plant fix", "plantfix"), ("pin hold", "pinhold")):
            tracks.append(("%s %s" % (label, leg.upper()), scalar(d["%s_%s" % (key, leg)])))
    offs = {}
    for name, pts in tracks:
        offs[name] = offline(pts, dt)
        rms, worst = stat(offs[name])
        print("  %-22s %7.3f %9.2f" % (name, rms, worst))
    for leg in "lr":
        flips = {k: sum(1 for i in range(1, len(t)) if (d["%s_%s" % (k, leg)][i] > 0.5) != (d["%s_%s" % (k, leg)][i - 1] > 0.5)) for k in ("heelw", "ballw", "plantw", "pinw", "ground")}
        print("  %s flips: %s; extension %.3f..%.3f; gap %.2f..%.2f cm" % (leg.upper(), flips, min(d["ext_" + leg]), max(d["ext_" + leg]), min(d["gap_" + leg]), max(d["gap_" + leg])))
    # the worst frames of the final feet and knees, with what each stage did around them
    for leg in "lr":
        key = "knee final %s" % leg.upper()
        order = sorted(range(len(t)), key=lambda i: -max(offs[key][i], offs["foot final %s" % leg.upper()][i]))
        seen = []
        for i in order:
            if any(abs(i - j) < 30 for j in seen):
                continue
            seen.append(i)
            if len(seen) > worst_n:
                break
            print("  worst %s at t %.2f s (frame %d): knee %.2f mm, foot %.2f mm off line" % (leg.upper(), t[i], i, offs[key][i], offs["foot final %s" % leg.upper()][i]))
            print("    frame   dt  footg_z footlay_z footpin_z foot_z  foot_x  foot_y  knee_x  knee_y  knee_z   ext  heelw ballw pinhold  gap  plantask plantfix pelfix trace_x trace_y trace_z soleunder")
            for j in range(max(0, i - 3), min(len(t), i + 4)):
                def g(k):
                    # "foot_z" is the column foot_<leg>_z; a plain name is <name>_<leg>
                    base, _, axis = k.rpartition("_")
                    key = "%s_%s_%s" % (base, leg, axis) if axis in ("x", "y", "z") and base else "%s_%s" % (k, leg)
                    return d[key][j]
                print("    %5d %4.1f %8.3f %8.3f %8.3f %7.3f %7.2f %7.2f %7.2f %7.2f %7.2f %.4f %5.2f %5.2f %6.3f %6.3f %7.3f %7.3f %6.3f %7.2f %7.2f %7.3f %7.3f" % (
                    j, 1000 * dt[j], g("footg_z"), g("footlay_z"), g("footpin_z"), g("foot_z"), g("foot_x"), g("foot_y"), g("knee_x"), g("knee_y"), g("knee_z"),
                    g("ext"), g("heelw"), g("ballw"), g("pinhold"), g("gap"), g("plantask"), g("plantfix"), d["pelvis_fix"][j], g("trace_x"), g("trace_y"), g("trace_z"), g("soleunder")))


main()


def knees(path):
    """Which step of the frame pops the knee: the knee's off-line distance after each step, and
    of each step's own change to it (the step's output minus its input)."""
    d = load(path)
    dt = d["dt"]
    names = ["k0graph", "k1catwalk", "k2alive", "k3toes", "k4reshape", "k5contact", "k6limits", "k7head", "k8pin", "k9plant"]
    if "k0graph_l_x" not in d:
        return
    print("  knee after each step     rms mm  worst mm | the step's own change: rms mm  worst mm")
    for leg in "lr":
        prev = None
        for n in names:
            pts = vec(d, "%s_%s" % (n, leg))
            rms, worst = stat(offline(pts, dt))
            line = "  %s %-12s %9.3f %9.2f" % (leg.upper(), n[2:], rms, worst)
            if prev is not None:
                own = [tuple(p[k] - q[k] for k in range(3)) for p, q in zip(pts, prev)]
                r2, w2 = stat(offline(own, dt))
                line += "   | %9.3f %9.2f" % (r2, w2)
            print(line)
            prev = pts


if "--knees" in sys.argv:
    knees(sys.argv[1])
