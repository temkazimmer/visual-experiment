#!/bin/bash
# usage: shoot.sh <level> <out.png>
# Leaves the game window open to look at; the next run closes it first.
pkill -f "visex.uproject.* -game" 2>/dev/null; sleep 1
P="/Users/tema/Documents/Unreal Projects/visex"; D="$P/Saved/Screenshots/MacEditor"
UE="/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
mkdir -p "$D"; touch /tmp/.visex_mark 2>/dev/null || touch "$(dirname "$2")/.mark"
M="$(dirname "$2")/.mark"; touch "$M"
"$UE" "$P/visex.uproject" "$1" -game -windowed -ResX=1920 -ResY=1080 -nosplash -ExecCmds="HighResShot 1920x1080" > "$(dirname "$2")/game.log" 2>&1 &
PID=$!
for i in $(seq 1 60); do sleep 3; f=$(find "$D" -name '*.png' -newer "$M" | head -1); [ -n "$f" ] && break; done
[ -n "$f" ] && cp "$f" "$2" && echo "OK $f" || { echo "NO SHOT"; tail -20 "$(dirname "$2")/game.log"; }
