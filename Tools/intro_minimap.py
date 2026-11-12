"""Write Saved/IntroMinimap/minimap.html: a top-down plan of the plaza to draw the intro on.

  python3 Tools/intro_minimap.py      (needs Tools/.bridge/minimap.json, dumped from the editor)

Tema draws her path, camera spots and notes; window.getDrawing() returns them in world cm.
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
data = json.load(open(os.path.join(ROOT, "Tools/.bridge/minimap.json")))
tpl = open(os.path.join(ROOT, "Tools/intro_minimap.html")).read()
out = os.path.join(ROOT, "Saved/IntroMinimap/minimap.html")
open(out, "w").write(tpl.replace("/*DATA*/null", json.dumps(data)))
print("wrote", out)
