#!/bin/bash
# What the MetaHuman hair costs per frame at the menu camera (stage 5). Run with NO editor open in
# this worktree (a -game run answers the folder bridge too). She walks in and leans; then, for the
# groom, no hair and the card hair in turn, the CSV profiler records SECS seconds.
# The built-in display is 3456x2234, so 3840x2160 is rendered inside a 1920x1080 window at
# r.ScreenPercentage 200 (the scene is drawn at 4K, then scaled down to the window).
#   Tools/mh_hair_cost.sh [extra UnrealEditor args]      ->  prints mean ms per mode
cd "$(dirname "$0")/.." || exit 1
UE="/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
SECS=${SECS:-10}
rm -f Tools/.bridge/in/*.py Saved/Profiling/CSV/*.csv
"$UE" "$PWD/visex.uproject" /Game/VisEx/Lvl_Dust -game -windowed -ResX=1920 -ResY=1080 -nosplash -nosound "$@" \
  > Tools/.bridge/hair_cost_game.log 2>&1 &
GAME=$!
cmd() { UE_WAIT=${2:-100} python3 Tools/ue.py -c "import unreal
w = unreal.find_object(None, '/Game/VisEx/Lvl_Dust.Lvl_Dust')
for c in '''$1'''.split(';'):
    unreal.SystemLibrary.execute_console_command(w, c.strip())
print('ran', '''$1''')"; }
nap() { python3 -c "import time; time.sleep($1)"; }
cmd "visex.Body.MetaHuman 1; t.MaxFPS 0; r.VSync 0; r.ScreenPercentage 200" 1800 || { kill $GAME; exit 1; }
nap 25                                  # the walk in, the lean, shaders and the groom settle
n=0
for mode in 1 2 0 1 nosim; do           # groom, none, cards, groom again (drift check), groom with its physics off
  if [ "$mode" = nosim ]; then cmd "r.HairStrands.Simulation 0"; else cmd "visex.Body.MetaHuman.Hair $mode"; fi
  nap 6
  cmd "csvprofile start"; nap "$SECS"; cmd "csvprofile stop"; nap 3
  n=$((n + 1)); f=$(ls -t Saved/Profiling/CSV/*.csv | head -1); mv "$f" "Saved/Profiling/CSV/hair_${n}_${mode}.csv"
done
kill $GAME
python3 - <<'PY'
import csv, glob
for path in sorted(glob.glob("Saved/Profiling/CSV/hair_*.csv")):
    rows = list(csv.reader(open(path)))
    head = rows[0]
    cols = {k: head.index(k) for k in ("FrameTime", "GameThreadTime", "RenderThreadTime", "GPUTime") if k in head}
    data = [r for r in rows[1:] if len(r) == len(head) and r[0] != head[0]][30:]   # the file repeats its header at the end; the first frames carry the switch
    def mean(k):
        v = [float(r[cols[k]]) for r in data if r[cols[k]] not in ("", "0")]
        return sum(v) / len(v) if v else float("nan")
    print("%-22s %4d frames  " % (path.split("/")[-1], len(data)) + "  ".join("%s %.2f ms" % (k, mean(k)) for k in cols))
PY
