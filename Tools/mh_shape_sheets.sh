#!/bin/zsh
# Stage 7b sheets: the shape sliders on the MetaHuman body, min | default | max per slider, from two
# sliders tours (Tools/mh_sliders.sh with -visexsetups=shape -visexframe=body+torso+side+back):
#   Tools/mh_shape_sheets.sh <suit run> <bare run> [out prefix, default stage7b]
# <suit run> has the Silversuit on, <bare run> was shot with -visexoutfit=None (her skin, bare top).
# -> Docs/Research/metahuman/<prefix>_shape_sheet_1..5.jpg
set -e
cd "$(dirname "$0")/.."
S="${1:-m7}"; B="${2:-m7b}"; PRE="${3:-stage7b}"
Q="$PWD/Saved/QA"; D="$PWD/Docs/Research/metahuman"; T="$Q/${S}_sheets"; mkdir -p "$T"
typeset -A LABEL
LABEL=(Bust "Bust · 0 | 0.80 (default) | 1" BustLift "Bust height · -1 | 0.5 (default) | 1"
       BustApart "Bust spacing · -1 | 0.8 (default) | 1" Glutes "Glutes · 0 | 0.5 (default) | 1"
       Abs "Abs · 0 flat | 0.5 (default) | 1 six-pack" Fat "Body fat · 0 | 0.38 (default) | 1"
       ThighGap "Thigh gap · 0 (min, the default) | 0 | 1")
# cell <run> <pose> <slider> <frame> [note]
cell() {
  printf '%s%s\t\t%s\t%s\t%s\n' "${LABEL[$3]}" "${5:+ · $5}" "$Q/$1_$2_$3_min_$4.png" "$Q/$1_$2_default_$4.png" "$Q/$1_$2_$3_max_$4.png"
}
sheet() {   # sheet <n> <title> <cells...>, each cell "run pose slider frame [note]"
  n=$1; title=$2; shift 2
  : > "$T/m_$n.txt"
  for c in "$@"; do cell ${=c} >> "$T/m_$n.txt"; done
  swift Tools/sheet.swift "$D/${PRE}_shape_sheet_$n.jpg" 3840 2 "$title" "$T/m_$n.txt"
}
sheet 1 "Shape sliders on the MetaHuman body (standing, Silversuit, from her front right): min | default | max  ·  1 of 5" \
  "$S idle Bust torso" "$S idle BustLift torso" "$S idle BustApart torso" "$S idle Abs torso"
sheet 2 "Shape sliders on the MetaHuman body (standing, Silversuit): min | default | max  ·  2 of 5" \
  "$S idle Glutes back from_behind" "$S idle Glutes side profile" "$S idle Fat torso" "$S idle ThighGap body full_figure"
sheet 3 "Shape sliders in the menu lean (Silversuit): min | default | max  ·  3 of 5" \
  "$S lean Bust torso" "$S lean Bust side profile" "$S lean BustApart torso" "$S lean Fat torso"
sheet 4 "Shape sliders on her bare skin (standing, no outfit): min | default | max  ·  4 of 5" \
  "$B idle Bust torso" "$B idle Bust side profile" "$B idle BustLift torso" "$B idle BustApart torso" "$B idle Abs torso" "$B idle Fat torso" \
  "$B idle Glutes back from_behind" "$B idle ThighGap body full_figure"
sheet 5 "Shape sliders in the photo LegLift and the photo Walk (Silversuit, full figure): min | default | max  ·  5 of 5" \
  "$S leglift Bust body LegLift" "$S leglift Glutes body LegLift" "$S leglift Fat body LegLift" "$S leglift ThighGap body LegLift" \
  "$S walk Bust body Walk" "$S walk Glutes body Walk" "$S walk Fat body Walk" "$S walk ThighGap body Walk"
ls -la "$D"/${PRE}_shape_sheet_*.jpg
