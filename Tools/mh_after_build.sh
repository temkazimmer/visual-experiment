#!/bin/zsh
# Everything that hangs on her assembled MetaHuman, put back after an assembly. Run with the editor
# still open on this folder, right after Tools/mh_create.py's one pass (... 'build'):
#   Tools/mh_after_build.sh            the body's vertices did not move (a look change: makeup, hair, tone)
#   Tools/mh_after_build.sh shape      new body shape (TARGETS changed): the morph deltas are made again too
# Steps, each of them re-runnable on its own:
#   1  bare-skin maps for outfits            Tools/mh_bare_skin.py (export, Blender, import)
#   2  the skin FBX the outfits are cut on   Tools/mh_skin_export.py
#   3  (shape) the shape sliders' deltas     Tools/mh_morphs.py base + variants, Tools/mh_morph_build.py
#   4  morph targets on the body mesh        Tools/mh_morphs.py apply (the assembly writes none)
#   5  Silversuit re-cut with the morphs     Tools/blender_outfit.py --body=mh, Tools/mh_outfit.py
# Afterwards: quit the editor, then the spike tour (-visexqa -visexspikes) and, for a new shape, the
# shape tour (see Tools/mh_sliders.sh). Tools/mh_vertex_order.py before and after tells which case it is.
set -e
cd "$(dirname "$0")/.."
P="$PWD"; BL=/Applications/Blender.app/Contents/MacOS/Blender
ue() { UE_WAIT=${UE_WAIT:-3000} python3 Tools/ue.py -c "$1; exec(open('$P/Tools/$2').read())"; }
echo "== 1 bare-skin maps"
ue "STEP='export'" mh_bare_skin.py | tail -1
"$BL" -b --factory-startup --python Tools/mh_bare_skin.py -- Saved/MH/tex SourceArt/Character/MH/Bare 2>&1 | grep wrote
ue "STEP='import'" mh_bare_skin.py | tail -1
echo "== 2 skin export"
ue "pass" mh_skin_export.py | head -1
if [ "$1" = shape ]; then
  echo "== 3 morph deltas for the new shape"
  ue "STEP='base'" mh_morphs.py
  for v in base bust span apex hip thigh fat0 fat1; do ue "STEP='variants'; VARIANT='$v'" mh_morphs.py; done
  "$BL" -b --factory-startup --python Tools/blender_outfit.py -- SourceArt/Character/MH/mh_skin.fbx SourceArt/Character/MH/Outfits \
    --body=mh --mh-morphs 2>&1 | grep "mh morph"
fi
echo "== 4 morph targets on the body mesh"
ue "STEP='apply'" mh_morphs.py | tail -1
echo "== 5 Silversuit"
"$BL" -b --factory-startup --python Tools/blender_outfit.py -- SourceArt/Character/MH/mh_skin.fbx SourceArt/Character/MH/Outfits \
  --only=Silversuit --body=mh 2>&1 | grep "mh morphs on\|pokesim.*past 0.1\|Traceback" | grep -v "(  0 past" || true
ue "OUTFIT='Silversuit'" mh_outfit.py | tail -1
echo "done: quit the editor, then run the spike tour"
