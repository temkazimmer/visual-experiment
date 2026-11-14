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
for mode in 1 2 0 1; do                 # groom, none, cards, groom again (drift check)
  cmd "visex.Body.MetaHuman.Hair $mode"; nap 6
  cmd "csvprofile start"; nap "$SECS"; cmd "csvprofile stop"; nap 3
  f=$(ls -t Saved/Profiling/CSV/*.csv | head -1); mv "$f" "Saved/Profiling/CSV/hair_$mode_$(date +%s).csv"
  echo "MODE $mode $(ls -t Saved/Profiling/CSV/*.csv | head -1)"
done
kill $GAME
