#!/bin/zsh
# Stage 8 sheets: the flex layer off | on, same pose and camera, on bare skin and in the Silversuit.
#   Tools/mh_flex_sheets.sh shoot <bare run> <suit run> [extra UnrealEditor args]   two flex tours (Tools/mh_flex.sh)
#   Tools/mh_flex_sheets.sh sheets <bare run> <suit run>                           -> Docs/Research/metahuman/stage8_flex_onoff_{bare,suit}.jpg
# Poses: the menu lean (the arms she rests on, the leg that carries her), the photo Idle (weight-bearing
# against free leg), then the reference pose with one thing changed: the right elbow bent 95 degrees,
# both feet on tiptoe (35 degrees), the chest turned 34 degrees. The clips are held while a pair is shot.
set -e
cd "$(dirname "$0")/.."
POSES='arm=ref;lowerarm_r:0,0,95|tiptoe=ref;foot_r:0,0,35;foot_l:0,0,35|twist=ref;spine_02:-12,0,0;spine_03:-12,0,0;spine_04:-10,0,0'
CAMS='lean:lowerarm_r:130,130,10:20g:0,0,12+lean:pelvis:260,120,-40:26g:0,0,-40+idle:pelvis:300,80,-45:24g:0,0,-45+arm:upperarm_r:20,220,-5:16g:5,0,-14+tiptoe:calf_r:-200,120,-10:18g:0,-10,-18+twist:spine_03:230,40,0:22g'
MODE="$1"; B="${2:-fb}"; S="${3:-fs}"; shift 3 2>/dev/null || true
if [[ "$MODE" == shoot ]]; then
  Tools/mh_flex.sh "$B" "$POSES" "$CAMS" -visexoutfit=None "$@"
  Tools/mh_flex.sh "$S" "$POSES" "$CAMS" "$@"
  exit 0
fi
Q="$PWD/Saved/QA"; D="$PWD/Docs/Research/metahuman"; T="$Q/flex_sheets"; mkdir -p "$T"
half() { out="$T/$(basename "$1" .png).jpg"; ffmpeg -loglevel quiet -y -i "$1" -vf "crop=1920:2160:960:0" -q:v 2 "$out"; echo "$out"; }
cell() { printf '%s\t\t%s\t%s\n' "$2" "$(half "$Q/$1_flex0_$3.png")" "$(half "$Q/$1_flex1_$3.png")"; }
for pair in "${B}:bare:her bare skin" "${S}:suit:the Silversuit"; do
  run=${pair%%:*}; rest=${pair#*:}; tag=${rest%%:*}; what=${rest#*:}
  {
    cell $run "Bent arm, elbow 95: biceps, forearm" arm_close0_upperarm_r
    cell $run "Photo Idle: weight-bearing leg, free leg" idle_close0_pelvis
    cell $run "Tiptoe, 35 degrees: calves" tiptoe_close0_calf_r
    cell $run "Menu lean: the arm she rests on" lean_close0_lowerarm_r
    cell $run "Menu lean: the leg that carries her" lean_close1_pelvis
    cell $run "Chest turned 34 degrees: abs, obliques" twist_close0_spine_03
  } > "$T/$tag.txt"
  swift Tools/sheet.swift "$D/stage8_flex_onoff_$tag.jpg" 3840 2 "The flex layer on $what: off | on in every cell (visex.Body.MetaHuman.Flex 0 | 1), same pose and camera" "$T/$tag.txt"
done
