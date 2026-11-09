"""Folder bridge: lets scripts outside the editor run Python inside it.

Drop a .py file into Tools/.bridge/in/; the editor runs it on its next tick and
writes stdout (or the traceback) to Tools/.bridge/out/<same name>.out.
Loaded automatically at editor start (Content/Python/init_unreal.py).
"""
import contextlib
import io
import os
import time
import traceback

import unreal

_ROOT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools", ".bridge")
_IN, _OUT = os.path.join(_ROOT, "in"), os.path.join(_ROOT, "out")
os.makedirs(_IN, exist_ok=True)
os.makedirs(_OUT, exist_ok=True)
_last = [0.0]


def _tick(_dt):
    if time.time() - _last[0] < 0.25:
        return
    _last[0] = time.time()
    for name in sorted(os.listdir(_IN)):
        if not name.endswith(".py"):
            continue
        path = os.path.join(_IN, name)
        with open(path) as f:
            code = f.read()
        os.remove(path)
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            try:
                exec(compile(code, name, "exec"), {"__name__": "__bridge__"})
            except Exception:
                buf.write("\nBRIDGE_ERROR\n" + traceback.format_exc())
        tmp = os.path.join(_OUT, name + ".tmp")
        with open(tmp, "w") as f:
            f.write(buf.getvalue())
        os.replace(tmp, os.path.join(_OUT, name[:-3] + ".out"))


if "_visex_bridge_handle" not in globals():
    _visex_bridge_handle = unreal.register_slate_post_tick_callback(_tick)
    unreal.log("visex bridge: watching " + _IN)
