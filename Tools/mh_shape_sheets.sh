#!/bin/zsh
# Stage 7b sheets: the shape sliders on the MetaHuman body, min | default | max per slider, from two
# sliders tours (Tools/mh_sliders.sh with -visexsetups=shape -visexframe=body+torso+side+back; the
# sheets use the lean and stand passes, and LegLift and Walk for the last one):
#   Tools/mh_shape_sheets.sh <suit run> <bare run> [out prefix, default stage7b] [run with the LegLift and Walk passes, default the suit run]
# <suit run> has the Silversuit on, <bare run> was shot with -visexoutfit=None (her skin, bare top).
# -> Docs/Research/metahuman/<prefix>_shape_sheet_1..5.jpg
set -e
cd "$(dirname "$0")/.."
S="${1:-m7}"; B="${2:-m7b}"; PRE="${3:-stage7b}"; W="${4:-$S}"
Q="$PWD/Saved/QA"; D="$PWD/Docs/Research/metahuman"; T="$Q/${S}_sheets"; mkdir -p "$T"
typeset -A LABEL
LABEL=(Bust "Bust · 0 | 0.80 (default) | 1" BustLift "Bust height · -1 | 0.5 (default) | 1"
       BustApart "Bust spacing · -1 | 0.8 (default) | 1" Glutes "Glutes · 0 | 0.5 (default) | 1"
       Abs "Abs · 0 flat | 0.5 (default) | 1 six-pack" Fat "Body fat · 0 | 0.38 (default) | 1"
       ThighGap "Thigh gap · 0 (min, the default) | 0 | 1")
# cell <run> <pose> <slider> <frame> [note]: the three frames, the torso framings cut to her middle
crop() {   # the tour's PNGs make this ffmpeg complain and still decode
  out="$T/$(basename "$1" .png).jpg"
  if [[ "$3" == thighs ]]; then out="${out%.jpg}_thighs.jpg"; ffmpeg -loglevel quiet -y -i "$1" -vf "crop=760:864:260:880" -q:v 2 "$out"
  elif [[ "$2" == body ]]; then ffmpeg -loglevel quiet -y -i "$1" -q:v 2 "$out"
  else ffmpeg -loglevel quiet -y -i "$1" -vf "crop=1100:1250:90:470" -q:v 2 "$out"; fi
  echo "$out"
}
cell() {
  printf '%s%s\t\t%s\t%s\t%s\n' "${LABEL[$3]}" "${5:+ · ${5//_/ }}" "$(crop "$Q/$1_$2_$3_min_$4.png" $4 $5)" "$(crop "$Q/$1_$2_default_$4.png" $4 $5)" "$(crop "$Q/$1_$2_$3_max_$4.png" $4 $5)"
}
sheet() {   # sheet <n> <title> <cells...>, each cell "run pose slider frame [note]"
  n=$1; title=$2; shift 2
  : > "$T/m_$n.txt"
  for c in "$@"; do cell ${=c} >> "$T/m_$n.txt"; done
  swift Tools/sheet.swift "$D/${PRE}_shape_sheet_$n.jpg" 3840 2 "$title" "$T/m_$n.txt"
}
sheet 1 "Shape sliders on the MetaHuman body (menu lean, Silversuit, from her front right): min | default | max  ·  1 of 5" \
  "$S lean Bust torso" "$S lean BustLift torso" "$S lean BustApart torso" "$S lean Abs torso" "$S lean Fat torso" "$S lean ThighGap torso"
sheet 2 "Shape sliders on the MetaHuman body (standing in the reference pose, Silversuit): min | default | max  ·  2 of 5" \
  "$S stand Bust side profile" "$S stand BustLift side profile" "$S stand BustApart torso front_right" "$S stand Abs torso front_right" \
  "$S stand Glutes side profile" "$S stand Glutes back behind" "$S stand Fat torso front_right" "$S stand ThighGap body thighs"
sheet 3 "Shape sliders on her bare skin (menu lean, no outfit, from her front right): min | default | max  ·  3 of 5" \
  "$B lean Bust torso" "$B lean BustLift torso" "$B lean BustApart torso" "$B lean Abs torso" "$B lean Fat torso" "$B lean ThighGap torso"
sheet 4 "Shape sliders on her bare skin (standing in the reference pose, no outfit): min | default | max  ·  4 of 5" \
  "$B stand Bust side profile" "$B stand BustLift side profile" "$B stand BustApart torso front_right" "$B stand Abs torso front_right" \
  "$B stand Glutes side profile" "$B stand Glutes back behind" "$B stand Fat torso front_right" "$B stand ThighGap body thighs"
sheet 5 "Shape sliders in the photo LegLift and the photo Walk (Silversuit, full figure): min | default | max  ·  5 of 5" \
  "$W leglift Bust body LegLift" "$W leglift Glutes body LegLift" "$W leglift Fat body LegLift" "$W leglift ThighGap body LegLift" \
  "$W walk Bust body Walk" "$W walk Glutes body Walk" "$W walk Fat body Walk" "$W walk ThighGap body Walk"
ls -la "$D"/${PRE}_shape_sheet_*.jpg
