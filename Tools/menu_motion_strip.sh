#!/bin/zsh
# Join the menu motion check's 5 frames side by side:
#   Tools/menu_motion_strip.sh body in   ->  Tools/.bridge/shots/motion_body_in.jpg
D="$(cd "$(dirname "$0")" && pwd)/.bridge/shots"
P="$D/motion_$1_$2"
ffmpeg -loglevel error -y -i "${P}_0.png" -i "${P}_1.png" -i "${P}_2.png" -i "${P}_3.png" -i "${P}_4.png" \
  -filter_complex "hstack=inputs=5" -q:v 3 "$P.jpg" && echo "$P.jpg"
