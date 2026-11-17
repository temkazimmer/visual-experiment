#!/bin/zsh
# Ship 2: the old | new sheets Tema approves the merge from. Frames are the side tour's 4K game-view
# shots, the same steps and cameras for both bodies, both in the Silversuit, at a fixed 60 Hz step:
#   UnrealEditor visex.uproject /Game/VisEx/Lvl_Dust -game -windowed -ResX=1920 -ResY=1080 -visexqa -visexside \
#       -visexshots=s2old -benchmark -fps=60 -dpcvars=visex.Body.MetaHuman=0      (and s2new with =1)
#   Tools/mh_ship2_sheets.sh [old run] [new run]
# -> Docs/Research/metahuman/ship2_side_<shot>.jpg (two 4K frames side by side), ship2_overview.jpg
# The clip: the same tour with -visexside=film (every second frame to Saved/QA/<run>_film/), then
#   Tools/mh_ship2_sheets.sh clip [old run] [new run]  -> ship2_side_walk.mp4
set -e
cd "$(dirname "$0")/.."
Q="$PWD/Saved/QA"; D="$PWD/Docs/Research/metahuman"; T="$Q/ship2_sheets"; mkdir -p "$T"
if [[ "$1" == clip ]]; then
  O="${2:-s2old}"; N="${3:-s2new}"
  ffmpeg -loglevel error -y -framerate 30 -i "$Q/${O}_film/f%05d.png" -framerate 30 -i "$Q/${N}_film/f%05d.png" \
    -filter_complex "[0:v][1:v]hstack=inputs=2" -c:v libx264 -crf 18 -pix_fmt yuv420p "$D/ship2_side_walk.mp4"
  echo "clip $D/ship2_side_walk.mp4"; exit 0
fi
O="${1:-s2old}"; N="${2:-s2new}"
typeset -A LABEL
LABEL=(01_menu "The menu, her lean on the column" 02_body "Character page, body view" 03_face "Character page, face view"
  04_wardrobe "Wardrobe" 05_settings "Settings, the sit on the drum" 06_photo_Bookend "Photo mode, Bookend"
  06_photo_Mermaid "Photo, Mermaid (floor pose)" 06_photo_LegLift "Photo mode, LegLift" 06_photo_FeetUp "Photo, FeetUp (floor pose)"
  07_portrait "Close portrait (photo Idle)" 07_portrait_blink "The face rig: a blink (the old face has none)" 08_hand_r "Right hand (photo Idle)" 09_hand_l "Left hand (photo Idle)" 10_feet "Feet (photo Idle)"
  11_walk_behind "Start: the walk-off, from behind" 12_start_end "The end of Start: arrived"
  13_walk_front "Walking, front three-quarter")
: > "$T/all.txt"
for s in 01_menu 02_body 03_face 04_wardrobe 05_settings 06_photo_Bookend 06_photo_Mermaid 06_photo_LegLift 06_photo_FeetUp \
         07_portrait 07_portrait_blink 08_hand_r 09_hand_l 10_feet 11_walk_behind 12_start_end 13_walk_front; do
  [[ -f "$Q/${O}_$s.png" && -f "$Q/${N}_$s.png" ]] || { echo "missing $s"; continue; }
  printf 'old body\t\t%s\nMetaHuman body\t\t%s\n' "$Q/${O}_$s.png" "$Q/${N}_$s.png" > "$T/one.txt"
  printf '%s\t\t%s\t%s\n' "${LABEL[$s]}" "$Q/${O}_$s.png" "$Q/${N}_$s.png" >> "$T/all.txt"
  swift Tools/sheet.swift "$D/ship2_side_${s#*_}.jpg" 7680 2 "Ship 2 · ${LABEL[$s]} · old body | MetaHuman body · same camera, light and step, 4K game view" "$T/one.txt"
done
swift Tools/sheet.swift "$D/ship2_overview.jpg" 3840 2 "Ship 2 · old body | MetaHuman body, both in the Silversuit · same camera, light and step" "$T/all.txt"
