#!/bin/zsh
# The flex layer off and on, same pose and camera (AVisexQATour::BuildFlex): the menu lean, the photo
# Idle and standing test poses, each shot with visex.Body.MetaHuman.Flex 0 and then 1.
#   Tools/mh_flex.sh <name> <poses> <cameras> [extra UnrealEditor args, e.g. -visexoutfit=None]
#   <poses>    name=<TestPose>|name=<TestPose>      (visex.Body.MetaHuman.TestPose: "ref;bone:roll,pitch,yaw;...")
#   <cameras>  the tour's -visexclose list: <pose>:<bone>:<x,y,z>:<fov>[g]:<tx,ty,tz>+...   (pose: lean, idle or a name above)
# 4K frames go to Saved/QA/<name>_flex0_<pose>_close<n>_<bone>.png and <name>_flex1_...; the drivers'
# numbers once a second to ~/Library/Logs/visex/visex.log (lines "VisexFlex:").
# Tools/mh_flex_sheets.sh has the poses and cameras of the stage 8 sheets.
set -e
cd "$(dirname "$0")/.."
UE="/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
NAME="$1"; POSES="$2"; CAMS="$3"; shift 3
[ "$(pgrep -x UnrealEditor | wc -l)" -lt 4 ] || { echo "4 editors already running"; exit 1; }
"$UE" "$PWD/visex.uproject" /Game/VisEx/Lvl_Dust -game -windowed -ResX=1920 -ResY=1080 -nosplash -nosound -benchmark -fps=30 \
  -visexqa -visexflex "-visexcvar=visex.Body.MetaHuman=1+visex.Body.MetaHuman.FlexTest=log" -visexshots="$NAME" \
  "-visexflexposes=$POSES" "-visexclose=$CAMS" "$@" > "Saved/QA/${NAME}_game.log" 2>&1
grep "VisexFlex:" ~/Library/Logs/visex/visex.log > "Saved/QA/${NAME}_flex.log" || true
echo "frames: $(ls Saved/QA/${NAME}_flex?_*.png 2>/dev/null | wc -l | tr -d ' '), driver lines: $(wc -l < Saved/QA/${NAME}_flex.log | tr -d ' ')"
