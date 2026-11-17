#!/bin/zsh
# Stage 8 sheets: her default bust before and after the base correction (Tools/mh_base_fix.py), and
# the three bust sliders at their ends on the new base. Frames come from the sliders tour's "stand"
# pass (the reference pose, one camera per framing), shot before and after the correction:
#   Tools/mh_sliders.sh <run> "1+visex.Body.MetaHuman.Flex=0" -benchmark -fps=30 -visexsetups=Bust+BustLift+BustApart \
#       -visexposes=stand -visexframe=torso+side [-visexoutfit=None]
#   Tools/mh_bust_sheets.sh <before dir> <old suit run> <old bare run> <new suit run> <new bare run>
# -> Docs/Research/metahuman/stage8_bust_before_after.jpg, stage8_bust_sliders.jpg
set -e
cd "$(dirname "$0")/.."
OD="${1:-Saved/QA/bust_before}"; OS="${2:-b8a}"; OB="${3:-b8b}"; NS="${4:-n8a}"; NB="${5:-n8b}"
Q="$PWD/Saved/QA"; D="$PWD/Docs/Research/metahuman"; T="$Q/bust_sheets"; mkdir -p "$T"
crop() { out="$T/$3_$(basename "$1" .png).jpg"; ffmpeg -loglevel quiet -y -i "$1" -vf "crop=1100:${2:-1250}:90:330" -q:v 2 "$out"; echo "$out"; }
{
  printf '%s\t\t%s\t%s\n' "Bare skin, front three-quarter · old | new" "$(crop "$OD/${OB}_stand_default_torso.png" 1250 o)" "$(crop "$Q/${NB}_stand_default_torso.png" 1250 n)"
  printf '%s\t\t%s\t%s\n' "Bare skin, profile · old | new" "$(crop "$OD/${OB}_stand_default_side.png" 1250 o)" "$(crop "$Q/${NB}_stand_default_side.png" 1250 n)"
  printf '%s\t\t%s\t%s\n' "Silversuit, front three-quarter · old | new" "$(crop "$OD/${OS}_stand_default_torso.png" 1250 o)" "$(crop "$Q/${NS}_stand_default_torso.png" 1250 n)"
  printf '%s\t\t%s\t%s\n' "Silversuit, profile · old | new" "$(crop "$OD/${OS}_stand_default_side.png" 1250 o)" "$(crop "$Q/${NS}_stand_default_side.png" 1250 n)"
} > "$T/ba.txt"
swift Tools/sheet.swift "$D/stage8_bust_before_after.jpg" 3840 2 "Her default bust, softened: the base body before and after (reference pose, same camera and light)" "$T/ba.txt"
typeset -A LABEL
LABEL=(Bust "Bust · 0 | 0.80 (default) | 1" BustLift "Bust height · -1 | 0.5 (default) | 1" BustApart "Bust spacing · -1 | 0.8 (default) | 1")
: > "$T/sl.txt"
for run in $NB $NS; do for s in Bust BustLift BustApart; do for f in torso side; do
  [[ $s == BustApart && $f == side ]] && continue
  [[ $s != BustApart && $f == torso && $run == $NS ]] && continue
  printf '%s · %s, %s\t\t%s\t%s\t%s\n' "${LABEL[$s]}" "$([[ $run == $NB ]] && echo bare || echo suit)" "$f" \
    "$(crop "$Q/${run}_stand_${s}_min_$f.png" 1000 s)" "$(crop "$Q/${run}_stand_default_$f.png" 1000 s)" "$(crop "$Q/${run}_stand_${s}_max_$f.png" 1000 s)" >> "$T/sl.txt"
done; done; done
swift Tools/sheet.swift "$D/stage8_bust_sliders.jpg" 3840 2 "The bust sliders round her new default (reference pose): min | default | max" "$T/sl.txt"
