#!/bin/bash
# Packages vis-ex for Mac and wraps it in an installable DMG (drag VIS-EX into Applications).
#   Tools/make_dmg.sh                 # Shipping build
#   CONFIG=Development Tools/make_dmg.sh
# Output: Build/VIS-EX-<build>.dmg, where <build> is the commit count the menu title shows.
# Signing is ad-hoc (no Developer ID on this Mac), so another Mac needs right-click > Open once.
set -euo pipefail
cd "$(dirname "$0")/.."
PROJECT="$PWD/visex.uproject"
UE="/Users/Shared/Epic Games/UE_5.7"
CONFIG="${CONFIG:-Shipping}"
BUILD=$(printf %04d "$(git rev-list --count HEAD)")
NAME="VIS-EX-$BUILD"

# a clean cook: an iterative one once left a shader library that didn't match (crash at start)
rm -rf Saved/Cooked/Mac Saved/StagedBuilds/Mac
"$UE/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun -project="$PROJECT" -platform=Mac \
  -clientconfig="$CONFIG" -build -cook -map=/Game/VisEx/Lvl_Dust -stage -pak \
  -nop4 -utf8output -unattended -nocompileeditor -ubtargs="-WaitMutex"

# the archive step copies a content-less app on Mac: take the staged one
SRC=$(ls -d Saved/StagedBuilds/Mac/*.app | head -1)
OUT="Build/dmg/$NAME"
rm -rf "$OUT" && mkdir -p "$OUT"
ditto "$SRC" "$OUT/VIS-EX.app"
codesign --force --deep --sign - "$OUT/VIS-EX.app"
ln -s /Applications "$OUT/Applications"

rm -f "Build/$NAME.dmg"
hdiutil create -volname "VIS-EX $BUILD" -srcfolder "$OUT" -fs APFS -format ULFO "Build/$NAME.dmg"
rm -rf "$OUT"
echo "built $PWD/Build/$NAME.dmg ($(du -h "Build/$NAME.dmg" | cut -f1))"
