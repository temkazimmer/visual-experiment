#!/bin/zsh
# Outfit Kit: kit.sh new|draw|export <Name>   (see Tools/outfit_kit/sketch.py)
set -e
cd "$(dirname "$0")/../.."
B=/Applications/Blender.app/Contents/MacOS/Blender
MODE=$1; NAME=$2
[[ -z "$NAME" ]] && { echo "usage: kit.sh new|draw|export <Name>"; exit 1; }
F="SourceArt/Character/OutfitKit/$NAME/sketch.blend"
case $MODE in
  new)    "$B" -b --factory-startup --python Tools/outfit_kit/sketch.py -- new "$NAME" ;;
  draw)   [[ -f "$F" ]] || "$B" -b --factory-startup --python Tools/outfit_kit/sketch.py -- new "$NAME"
          "$B" "$F" --python Tools/outfit_kit/sketch.py -- open "$NAME" > "${TMPDIR:-/tmp}/outfit_kit_draw.log" 2>&1 & ;;
  export) "$B" -b "$F" --factory-startup --python Tools/outfit_kit/sketch.py -- export "$NAME" ;;
  *) echo "usage: kit.sh new|draw|export <Name>"; exit 1 ;;
esac
