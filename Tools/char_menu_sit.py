"""Menu Settings sit: she slides down off the lean onto the sand, back against the same drum,
legs out long toward the menu camera, ankles crossed, soles and toes to the lens. Idempotent.

  python3 Tools/ue.py Tools/char_menu_sit.py     # builds three clips, then shoots stills

Writes /Game/Characters/Menu/A_Menu_Sit_Settings (a breathing loop), A_Menu_Sit_Settings_Down
(lean to sit) and A_Menu_Sit_Settings_Up (sit to lean). All in place: the root never moves.
The menu director plays them while Settings is open (VisexMenuDirector.cpp, SitForSettings)
and drops its camera onto her feet at SettingsFeet (mesh space), so keep the two in step.
The lean key and the column come from char_menu_lean.py; the sit's way down from
char_photoseq_sit.py; the solver, the game's joint limits and the camera from
char_pose_mermaid.py.
Globals: BUILD=False, SHOTS=[...] (seconds into _Down) or [], TWEAK={...} for the sit key.
"""
import math
import os
import unreal

if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise RuntimeError("Play mode is running: stop PIE first")
BUILD = globals().get("BUILD", True)
SHOTS = globals().get("SHOTS", [0.0, 1.0, 2.0, 3.0, 4.2])
_tools = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools")
_keep = dict(BUILD=BUILD, SHOTS=SHOTS, TWEAK=globals().get("TWEAK", {}))
# the lean script defines LEAN, the column (CY, CZ, RADIUS) and the lean's arms; with BUILD off it writes nothing
BUILD, SHOTS, TWEAK = False, [], {}
exec(compile(open(os.path.join(_tools, "char_menu_lean.py")).read(), "char_menu_lean.py", "exec"), globals())
globals().update(_keep)
LEAN_ARMS, LEAN_KEY, STAND_KEY = lean_arms, dict(LEAN), dict(STAND)
print("column in her space: axis y %.1f z %.1f, radius %.1f" % (CY, CZ, RADIUS))
