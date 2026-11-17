"""Her default bust, softened (stage 8, Tema: "Yes, soften it"): a correction baked into the base of
the assembled MetaHuman body, so outfits are cut on it and every slider works around it.

  python3 Tools/mh_base_fix.py            (plain Python, no numpy; run after Tools/mh_morph_build.py)

Why a correction and not new TARGETS. At the default her bust read as two high round balls. The
Creator's controls cannot change that alone: Shoulder to Apex +6 cm (with Chest -2) does lower and
fill it, but the same solve moves hips and legs 0.4 cm on average (1.0 at most), the shoulder joints
1 cm out and the head 0.6 cm up, and Tema's call is that everything else stays exactly as it is.
So the shape is taken from the same Creator variants the sliders use, inside the bust's footprint
only (the morphs of Tools/mh_morph_build.py), and added to the body's own vertices:

    fix = FIX_LIFT x BustLift  +  (FIX_BUST + FIX_UPPER x above-the-tip) x Bust

BustLift is the Creator's Shoulder to Apex direction (lower, fuller below, a longer upper slope),
Bust its Chest direction (projection); the last term takes a little more off the upper pole, where
the ball started at the collarbone. Nothing else moves, and nothing within 2.6 cm of the neck
opening (the morphs are faded there).

In:  SourceArt/Character/MH/Morphs/mh_morphs.json as Tools/mh_morph_build.py wrote it
Out: the same file with
       built      the body's vertices as the assembly writes them
       base       built + fix: what the mesh holds after Tools/mh_morphs.py STEP='apply', and what the
                  outfit cut and Tools/mh_flex_build.py match the skin FBX to
       fix        the correction (indices, deltas), check / check_built
       fix_src    BustLift and Bust at their first size (so this can be run again)
     and the BustLift and Bust morphs resized by RANGE: the two sliders now move round a default that
     already sits lower and projects less, so their ends are pulled in (see the stage 8 log).
"""
import json
import os

FIX_LIFT = -1.0       # units of the BustLift morph as first sized (2.0 cm of tip travel a unit; minus is down)
FIX_BUST = -0.28      # units of the Bust morph (5.6 cm of projection a unit)
FIX_UPPER = -0.15     # more of the Bust morph above the tip, fading in over 6 cm
# The two sliders' reach round the new default (their morphs are resized by these). Bust height: the
# default already sits 2 cm lower, and at 0.67 its minimum (4 cm under the old default) put the lower
# pole over the Silversuit's keyhole hem (poke 0.17-0.20 cm in the lean, LegLift and walk; limit 0.10);
# at 0.40 still 0.12 in LegLift; 0.30 ends 2.9 cm under the old default, where the old slider's
# minimum was, and reaches 0.9 cm down and 0.3 up from the new one. Bust: so that 0 is the
# small bust it was (0.8 units under the old default), not that less the correction again, which
# dented the inner breast.
RANGE = {"BustLift": 0.30, "Bust": 0.65}

PROJECT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PATH = os.path.join(PROJECT, "SourceArt", "Character", "MH", "Morphs", "mh_morphs.json")


def dense(m, n):
    out = [None] * n
    d = m["d"]
    for k, i in enumerate(m["i"]):
        out[i] = (d[3 * k], d[3 * k + 1], d[3 * k + 2])
    return out


def tip(V):
    """The bust's most forward vertex on her left: (index, position)."""
    i = max((i for i, p in enumerate(V) if p[0] > 3.0 and 112.0 < p[2] < 132.0), key=lambda i: V[i][1])
    return i, V[i]


def main():
    data = json.load(open(PATH))
    n = data["vertices"]
    built = data.get("built") or data["base"]
    src = data.get("fix_src") or {}
    for k in RANGE:                          # the morphs at their first size, kept so this can be run again
        src.setdefault(k, data["morphs"][k])
    lift, bust = dense(src["BustLift"], n), dense(src["Bust"], n)
    ti, tp = tip(built)
    fix_i, fix_d, base, far = [], [], [], 0.0
    for i, p in enumerate(built):
        d = [0.0, 0.0, 0.0]
        if lift[i]:
            d = [d[k] + FIX_LIFT * lift[i][k] for k in range(3)]
        if bust[i]:
            t = min(max((p[2] - (tp[2] - 1.0)) / 6.0, 0.0), 1.0)
            w = FIX_BUST + FIX_UPPER * t * t * (3.0 - 2.0 * t)
            d = [d[k] + w * bust[i][k] for k in range(3)]
        size = (d[0] ** 2 + d[1] ** 2 + d[2] ** 2) ** 0.5
        if size > 0.003:
            fix_i.append(i)
            fix_d += [round(x, 4) for x in d]
            far = max(far, size)
            base.append([round(p[k] + round(d[k], 4), 3) for k in range(3)])
        else:
            base.append([round(x, 3) for x in p])
    data["built"] = built
    data["base"] = base
    data["fix"] = {"i": fix_i, "d": fix_d}
    data["fix_src"] = src
    data["check_built"] = [[i] + list(built[i]) for i in range(0, n, 97)]
    data["check"] = [[i] + list(base[i]) for i in range(0, n, 97)]
    for k, r in RANGE.items():
        data["morphs"][k] = {"i": src[k]["i"], "d": [round(x * r, 4) for x in src[k]["d"]]}
    json.dump(data, open(PATH, "w"), separators=(",", ":"))
    t2 = tip(base)[1]
    print("mh base fix: %d vertices moved, %.2f cm at most; the bust's tip %.1f cm forward, %.1f up -> %.1f forward, %.1f up; "
          "sliders resized %s" % (len(fix_i), far, tp[1], tp[2], t2[1], t2[2], RANGE))


if __name__ == "__main__":
    main()
