#!/bin/bash
# Stage 6 face sheets: her current head and the MetaHuman head from the same cameras under one light.
# Needs the editor open in this worktree on Lvl_Studio (nothing is saved), not in Play.
#   Tools/mh_face_sheets.sh            ->  Docs/Research/metahuman/stage6_face_match.jpg, stage6_ears.jpg,
#                                          stage6_expressions.jpg, stage6_eyes_lods.jpg (3840x2160 each)
# Rows: her current head (visex.Body.MetaHuman 0) over the MetaHuman (1), hair hidden on both.
cd "$(dirname "$0")/.." || exit 1
P="$PWD"; M="$P/Saved/MH"; D="$P/Docs/Research/metahuman"
play() { python3 Tools/ue.py -c "import unreal; $1; exec(open('$P/Tools/mh_play.py').read())" 2>&1 | tail -1 | cut -c1-200; }
shots() { UE_WAIT=1800 python3 Tools/ue.py -c "import unreal; $1; exec(open('$P/Tools/mh_face_shots.py').read())" 2>&1 | grep "FACE\|rror" | cut -c1-200; }
nap() { python3 -c "import time; time.sleep($1)"; }
png() { for f in "$@"; do sips -s format png "$M/$f.png" --out "$M/fix_$f.png" >/dev/null 2>&1; done; }   # ffmpeg rejects the export as it is

# grooms (brows, lashes) only show in a capture if the main view draws them: keep the background editor drawing
python3 Tools/ue.py -c "import unreal; unreal.find_object(None, '/Script/UnrealEd.Default__EditorPerformanceSettings').set_editor_property('bThrottleCPUWhenNotForeground', False)" >/dev/null 2>&1
play "MODE='prepare'"
shots "MODE='lights'"
play "MODE='play'"; nap 8
play "MODE='cmd'; CMD='visex.Body.MetaHuman.Hair 2'"
WIDE="SIZE=(2560, 2160); FOV=12.0"

play "MODE='cmd'; CMD='visex.Body.MetaHuman 0'"; nap 12
shots "NAME='m0'; VIEWS=['front', 'three', 'side']; $WIDE"
shots "NAME='e0'; VIEWS=['ear', 'earback', 'earfront']; $WIDE; FOV=13.0"
play "MODE='cmd'; CMD='visex.Body.MetaHuman 1'"; nap 35     # the grooms (brows, lashes) take a while to show
shots "NAME='m1'; VIEWS=['front', 'three', 'side']; $WIDE"
shots "NAME='e1'; VIEWS=['ear', 'earback', 'earfront']; $WIDE; FOV=13.0"

# the face rig: neutral, a blink, a smile (visex.Body.MetaHuman.Face drives rig curves), then the eyes per LOD
face() { play "MODE='cmd'; CMD='visex.Body.MetaHuman.Face $1'"; nap 2; shots "NAME='$2'; VIEWS=['front']"; }
shots "NAME='x_neutral'; VIEWS=['front']"
face "CTRL_expressions_eyeBlinkL=1,CTRL_expressions_eyeBlinkR=1" x_blink
face "CTRL_expressions_mouthCornerPullL=1,CTRL_expressions_mouthCornerPullR=1,CTRL_expressions_mouthCheekRaiseL=0.6,CTRL_expressions_mouthCheekRaiseR=0.6" x_smile
play "MODE='cmd'; CMD='visex.Body.MetaHuman.Face off'"
for lod in 0 1 2 3; do
  play "MODE='cmd'; CMD='visex.Body.MetaHuman.LOD $lod'"; nap 2
  shots "NAME='lod$lod'; VIEWS=['eyes']; SIZE=(1920, 1080); FOV=14.0; FRAME='driver'"
done
play "MODE='cmd'; CMD='visex.Body.MetaHuman.LOD -1'"
play "MODE='cmd'; CMD='visex.Body.MetaHuman.Hair 1'"
play "MODE='cmd'; CMD='visex.Body.MetaHuman 0'"
play "MODE='stop'"; nap 4
play "MODE='clean'"

grid() {    # out, cell size, then six pictures: three over three
  out="$1"; cell="$2"; shift 2; png "$@"
  ffmpeg -y -loglevel error -i "$M/fix_$1.png" -i "$M/fix_$2.png" -i "$M/fix_$3.png" -i "$M/fix_$4.png" -i "$M/fix_$5.png" -i "$M/fix_$6.png" \
    -filter_complex "[0]scale=$cell[a];[1]scale=$cell[b];[2]scale=$cell[c];[3]scale=$cell[d];[4]scale=$cell[e];[5]scale=$cell[f];[a][b][c]hstack=3[t];[d][e][f]hstack=3[u];[t][u]vstack" -q:v 2 "$out"
}
grid "$D/stage6_face_match.jpg" 1280:1080 m0_front m0_three m0_side m1_front m1_three m1_side
grid "$D/stage6_ears.jpg" 1280:1080 e0_ear e0_earback e0_earfront e1_ear e1_earback e1_earfront
png x_neutral_front x_blink_front x_smile_front lod0_eyes lod1_eyes lod2_eyes lod3_eyes
ffmpeg -y -loglevel error -i "$M/fix_x_neutral_front.png" -i "$M/fix_x_blink_front.png" -i "$M/fix_x_smile_front.png" -filter_complex "[0][1][2]hstack=3" -q:v 2 "$D/stage6_expressions.jpg"
ffmpeg -y -loglevel error -i "$M/fix_lod0_eyes.png" -i "$M/fix_lod1_eyes.png" -i "$M/fix_lod2_eyes.png" -i "$M/fix_lod3_eyes.png" -filter_complex "[0][1]hstack[t];[2][3]hstack[u];[t][u]vstack" -q:v 2 "$D/stage6_eyes_lods.jpg"
ls -la "$D"/stage6_*.jpg | cut -c26-
