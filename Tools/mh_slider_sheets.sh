#!/bin/zsh
# Stage 7 sheets from a sliders tour's portrait frames (Tools/mh_sliders.sh <name>):
#   Tools/mh_slider_sheets.sh <name> [pose, default lean] [out prefix, default stage7] [which: sliders,profiles,tone]
# -> Docs/Research/metahuman/<prefix>_sliders_sheet_1..4.jpg  one cell per slider: min | default | max
#    Docs/Research/metahuman/<prefix>_profiles.jpg            small | default | tall
#    Docs/Research/metahuman/<prefix>_tone.jpg                skin tone 0 | 0.25 (default) | 0.5 | 0.75 | 1
set -e
cd "$(dirname "$0")/.."
N="${1:-s7}"; POSE="${2:-lean}"; PRE="${3:-stage7}"; WHICH="${4:-sliders,profiles,tone}"
Q="$PWD/Saved/QA"; D="$PWD/Docs/Research/metahuman"; T="$Q/${N}_sheets"; mkdir -p "$T"
img() { echo "$Q/${N}_${POSE}_$1_body.png"; }
row() { printf '%s\t\t%s\t%s\t%s\n' "$1" "$(img $2_min)" "$(img default)" "$(img $2_max)"; }
rows=(
  "Height · 145 | 166 (default) | 185 cm:Height"
  "Head · 0.85 | 1.04 (default) | 1.15:Head"
  "Neck · 0.80 | 0.90 (default) | 1.30:Neck"
  "Shoulders · 0.80 | 0.88 (default) | 1.20:Shoulders"
  "Ribcage · 0.85 | 1.0175 (default) | 1.20:Chest"
  "Waist · 0.60 | 0.85 (default) | 1.20:Waist"
  "Hips · 0.85 | 0.954 (default) | 1.50:Hips"
  "Leg length · 0.90 | 0.9836 (default) | 1.12:Legs"
  "Thighs · 0.80 | 1.208 (default) | 1.40:Thighs"
  "Calves · 0.80 | 1.16 (default) | 1.25:Calves"
  "Arm length · 0.92 | 0.976 (default) | 1.08:ArmLength"
  "Arms · 0.80 | 1.00 (default) | 1.25:Arms"
  "Hands · 0.85 (min, the default) | 0.85 | 1.15:Hands"
  "Feet · 0.85 | 0.955 (default) | 1.15:Feet"
)
if [[ $WHICH == *sliders* ]]; then
sheet=1; n=0; : > "$T/m_$sheet.txt"
for r in $rows; do
  row "${r%%:*}" "${r##*:}" >> "$T/m_$sheet.txt"
  n=$((n + 1))
  if [ $((n % 4)) -eq 0 ] && [ $n -lt ${#rows} ]; then sheet=$((sheet + 1)); : > "$T/m_$sheet.txt"; fi
done
for s in $(seq 1 $sheet); do
  swift Tools/sheet.swift "$D/${PRE}_sliders_sheet_$s.jpg" 3840 2 "Body sliders on the MetaHuman body ($POSE, Silversuit): min | default | max  ·  sheet $s of $sheet" "$T/m_$s.txt"
done
fi
if [[ $WHICH == *profiles* ]]; then
printf 'small and slight | default | tall and strong\t\t%s\t%s\t%s\n' "$(img small)" "$(img default)" "$(img tall)" > "$T/m_p.txt"
swift Tools/sheet.swift "$D/${PRE}_profiles.jpg" 3840 1 "Combined profiles on the MetaHuman body ($POSE)" "$T/m_p.txt"
fi
if [[ $WHICH == *tone* ]]; then
h() { echo "$Q/${N}_${POSE}_$1_head.png"; }
printf 'Skin tone · 0 | 0.25 (default) | 0.5 | 0.75 | 1\t\t%s\t%s\t%s\t%s\t%s\n' "$(h Skin_000)" "$(h default)" "$(h Skin_050)" "$(h Skin_075)" "$(h Skin_100)" > "$T/m_t.txt"
swift Tools/sheet.swift "$D/${PRE}_tone.jpg" 3840 1 "Skin tone on the MetaHuman skin (body, face, bare skin under the suit)" "$T/m_t.txt"
fi
ls -la "$D"/${PRE}_*.jpg
