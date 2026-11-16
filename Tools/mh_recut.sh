#!/bin/zsh
# The short loop after a change to SourceArt/Character/MH/Morphs/mh_morphs.json alone (new flex
# sizes, a slider's reach): the morphs back on the body mesh, then the Silversuit re-cut so it
# carries them. Editor open on this folder; Tools/mh_after_build.sh is the full run after an assembly.
set -e
cd "$(dirname "$0")/.."
P="$PWD"; BL=/Applications/Blender.app/Contents/MacOS/Blender
ue() { UE_WAIT=${UE_WAIT:-5500} python3 Tools/ue.py -c "$1; exec(open('$P/Tools/$2').read())"; }
ue "STEP='apply'" mh_morphs.py | grep "base correction\|applied" | cut -c1-120
"$BL" -b --factory-startup --python Tools/blender_outfit.py -- SourceArt/Character/MH/mh_skin.fbx SourceArt/Character/MH/Outfits \
  --only=Silversuit --body=mh 2>&1 | grep "mh morphs on\|pokesim.*past 0.1\|Traceback" | grep -v "(  0 past" | cut -c1-700 || true
ue "OUTFIT='Silversuit'" mh_outfit.py | tail -1
