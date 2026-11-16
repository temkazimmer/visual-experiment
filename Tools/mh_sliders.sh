#!/bin/zsh
# The Body page's sliders on the MetaHuman body: every bone-scale slider at both ends, height 145 /
# 185, the skin tone's steps and two combined bodies, each in the menu lean, the photo LegLift and
# the photo Walk (AVisexQATour::BuildSliders). Lines go to Saved/QA/report.txt (VisexSliders,
# VisexSpikes, VisexFeet), portrait frames to Saved/QA/<name>_<pose>_<setup>_{body,head}.png.
#   Tools/mh_sliders.sh <name> [cvar value, default 1] [extra UnrealEditor args]
# Nothing is saved: the profile is changed in memory and loaded back at the end.
set -e
cd "$(dirname "$0")/.."
UE="/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
NAME="${1:-s7}"; CVAR="${2:-1}"; shift 2 2>/dev/null || true
[ "$(ps -axo args | grep 'MacOS/UnrealEditor ' | grep -v grep | wc -l)" -lt 4 ] || { echo "4 editors already running"; exit 1; }
"$UE" "$PWD/visex.uproject" /Game/VisEx/Lvl_Dust -game -windowed -ResX=1920 -ResY=1080 -nosplash -nosound \
  -visexqa -visexsliders -visexcvar=visex.Body.MetaHuman=$CVAR -visexshots="$NAME" "$@" > "Saved/QA/${NAME}_game.log" 2>&1
