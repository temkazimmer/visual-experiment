#!/bin/bash
# Shoots every MetaHuman hairstyle (then brows and lashes) on her for the stage 5 contact sheets.
# Needs the editor open in this worktree (any level; Lvl_Studio was used) with the bridge up.
#   Tools/mh_hair_sheet.sh            all hair, brows, lashes  ->  Tools/.bridge/hair/*.png
#   Tools/mh_hair_sheet.sh Hair       one slot
# Then: swift Tools/sheet.swift ... (see Docs/AGENT_METAHUMAN.md, stage 5).
cd "$(dirname "$0")/.." || exit 1
P="exec(open(unreal.Paths.project_dir() + 'Tools/mh_hair_sheet.py').read())"
job() { UE_WAIT=3000 python3 Tools/ue.py -c "import unreal; $1; $P" 2>&1 | cut -c1-600; }
FACE="VIEWS={'face': (20.0, 300.0, 156.5, 3.6)}; SIZE=(1200, 750)"
job "MODE='list'" | tee Tools/.bridge/hair_list.txt
job "MODE='prepare'; SIZE=(1080, 1350)"
for slot in ${1:-Hair Eyebrows Eyelashes}; do
  [ "$slot" = Hair ] || SHOT="$FACE"
  [ "$slot" = Hair ] || job "MODE='rig'; $FACE"
  for style in $(grep "^MHHAIR $slot " Tools/.bridge/hair_list.txt | cut -d' ' -f4-); do
    job "MODE='style'; SLOT='$slot'; STYLE='$style'" | cut -c1-120
    for try in 1 2 3; do                        # the groom shows a few frames after it is worn
      python3 -c "import time; time.sleep(4)"
      r=$(job "MODE='shot'; NAME='$style'; ${SHOT:-SIZE=(1080, 1350)}"); echo "$r"
      case "$r" in *SKIN*) ;; *) break ;; esac
    done
  done
done
