"""A table from a sliders tour's report lines (Tools/mh_sliders.sh, AVisexQATour::BuildSliders).

  python3 Tools/mh_slider_table.py <report block> [pose ...]     # default: lean leglift walk

Per setup and pose: body stretch / creases / grew, garment stretch, fabric under drawn skin at a hem
(count, deepest cm), the soles' vertex gap L / R (standing poses), and how far the MetaHuman's hand
and foot joints sit from the driver's. Ends with the worst of each column and the setup it is on.
"""
import re
import sys

text = open(sys.argv[1]).read().splitlines()
poses = sys.argv[2:] or ["lean", "leglift", "walk"]
rows = {}
for line in text:
    m = re.match(r"VisexSpikes: (\w+) (\w+) \((\d+) frames\): (.*)", line)
    if m and m.group(1) in poses:
        r = rows.setdefault((m.group(1), m.group(2)), {})
        rest = m.group(4)
        for part in ("body", "face", "garment"):
            p = re.search(part + r" (pass|FAIL): stretch ([\d.]+)x \(.*? on (\w+)\), (\d+) over 3x; creases ([\d.]+)x .*?; grew ([\d.]+) cm", rest)
            if p:
                r[part] = (p.group(1), float(p.group(2)), float(p.group(5)), float(p.group(6)), p.group(3))
        p = re.search(r"poke (pass|FAIL): where skin is drawn .*? (\d+) fabric vertices under it, deepest ([\d.]+) cm on (\w+); under clipped skin (\d+), deepest ([\d.]+)", rest)
        if p:
            r["poke"] = (p.group(1), int(p.group(2)), float(p.group(3)), p.group(4), float(p.group(6)))
    m = re.match(r"VisexFeet: STILL (\w+) (\w+): sole vertex gap L ([-\d.]+) \(.*?\) R ([-\d.]+)", line)
    if m and m.group(1) in poses:
        rows.setdefault((m.group(1), m.group(2)), {})["gap"] = (float(m.group(3)), float(m.group(4)))
    m = re.match(r"VisexSliders: (\w+) \| (\w+) \| capsule half ([\d.]+) radius ([\d.]+).*?hand L ([\d.]+) R ([\d.]+), foot L ([\d.]+) R ([\d.]+), head ([\d.]+), pelvis ([\d.]+)", line)
    if m:
        rows.setdefault((m.group(1), m.group(2)), {})["joint"] = tuple(float(m.group(i)) for i in range(3, 11))

worst = {}
def note(key, value, where):
    if key not in worst or value > worst[key][0]:
        worst[key] = (value, where)

for pose in poses:
    print("\n%s\n%-14s %-22s %-18s %-18s %-12s %-18s %s" % (pose, "setup", "body str/crease/grew", "garment", "hem poke n, cm", "sole gap L/R", "hand L/R foot L/R", "capsule"))
    for (p, name), r in rows.items():
        if p != pose:
            continue
        b, g, k, gap, j = r.get("body"), r.get("garment"), r.get("poke"), r.get("gap"), r.get("joint")
        if b:
            note("body stretch", b[1], pose + " " + name); note("body creases", b[2], pose + " " + name); note("body grew cm", b[3], pose + " " + name)
        if r.get("face"):
            note("face stretch", r["face"][1], pose + " " + name)
        if g:
            note("garment stretch", g[1], pose + " " + name)
        if k:
            note("hem poke cm", k[2], pose + " " + name); note("hem poke vertices", k[1], pose + " " + name)
        if gap and pose == "lean":
            note("sole gap cm (lean)", max(abs(gap[0]), abs(gap[1])), pose + " " + name)
        if j:
            note("hand off cm", max(j[2], j[3]), pose + " " + name); note("foot off cm", max(j[4], j[5]), pose + " " + name)
        print("%-14s %-22s %-18s %-18s %-12s %-18s %s" % (
            name,
            "%s %.2f/%.2f/%.2f" % (b[0], b[1], b[2], b[3]) if b else "-",
            "%s %.2f/%.2f" % (g[0], g[1], g[3]) if g else "-",
            "%s %d, %.2f %s" % (k[0], k[1], k[2], k[3] if k[1] else "") if k else "-",
            "%.2f/%.2f" % gap if gap else "-",
            "%.1f/%.1f %.1f/%.1f" % (j[2], j[3], j[4], j[5]) if j else "-",
            "%.1f r %.1f" % (j[0], j[1]) if j else "-"))
print("\nworst:")
for key, (value, where) in worst.items():
    print("  %-22s %.2f  (%s)" % (key, value, where))
