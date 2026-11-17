#!/bin/zsh
# Stage 8 clip: the photo Walk, legs close, under a raking spot that travels with her, so the calf and
# quad firing reads (AVisexQATour::BuildClip with -visexclipcam / -visexcliplight). Bare legs by default.
#   Tools/mh_flex_clip.sh <name> [frames, default 300] [extra UnrealEditor args, e.g. -visexcvar=...Flex=0]
# -> Saved/QA/<name>_clip_NNNN.png (3840x2160) and Saved/QA/<name>.mp4 (30 fps)
set -e
cd "$(dirname "$0")/.."
UE="/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
NAME="${1:-fc}"; FRAMES="${2:-300}"; shift 2 2>/dev/null || true
CVAR="visex.Body.MetaHuman=1"
for a in "$@"; do [[ "$a" == -visexcvar=* ]] && CVAR="${a#-visexcvar=}"; done
[ "$(pgrep -x UnrealEditor | wc -l)" -lt 4 ] || { echo "4 editors already running"; exit 1; }
# camera: 62 degrees round from her front to her right, 310 cm away, aimed 50 cm over her feet, 31 degrees
# across. Light: from further round behind her (140 degrees), so it rakes across what the camera sees;
# 200 candela (6000 burnt her white: the level's exposure is set for its own dim, dusty light).
"$UE" "$PWD/visex.uproject" /Game/VisEx/Lvl_Dust -game -windowed -ResX=1920 -ResY=1080 -nosplash -nosound -benchmark -fps=30 \
  -visexqa -visexclip "-visexcvar=$CVAR" -visexshots="$NAME" -visexoutfit=None \
  "-visexclipcam=62,310,50,31,$FRAMES" "-visexcliplight=${LIGHT:-140,320,90,200}" "${(@)@:#-visexcvar=*}" > "Saved/QA/${NAME}_game.log" 2>&1
n=$(ls Saved/QA/${NAME}_clip_*.png 2>/dev/null | wc -l | tr -d ' ')
echo "frames: $n"
[ "$n" -gt 1 ] && ffmpeg -loglevel error -y -framerate 30 -i "Saved/QA/${NAME}_clip_%04d.png" -c:v libx264 -pix_fmt yuv420p -crf 18 "Saved/QA/${NAME}.mp4" && echo "Saved/QA/${NAME}.mp4"
