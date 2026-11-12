"""Trace SM_Dunes on a 25 cm grid around the intro path -> SourceArt/VisEx/Intro/dune_heights.json
(the base surface for Tools/intro_sand_mesh.py). Run live: python3 Tools/ue.py Tools/intro_bake_dune.py"""
import json
import os
import sys
import unreal

root = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
sys.path.insert(0, os.path.join(root, "Tools"))
import importlib
import intro_params as P
importlib.reload(P)

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def H(x, y):
    hits = unreal.SystemLibrary.line_trace_multi(
        world, unreal.Vector(x, y, 20000), unreal.Vector(x, y, -8000),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
    for h in hits:
        t = h.to_tuple()
        if t[9] and t[9].get_actor_label() == "Dunes":
            return t[4].z
    return None


corners = [P.world(u, v) for u in P.PATCH_U for v in (-P.PATCH_V, P.PATCH_V)]
S = 25
x0 = int(min(c[0] for c in corners) // S * S) - 2 * S
x1 = int(max(c[0] for c in corners) // S * S) + 3 * S
y0 = int(min(c[1] for c in corners) // S * S) - 2 * S
y1 = int(max(c[1] for c in corners) // S * S) + 3 * S
g = [[H(x, y) for x in range(x0, x1 + 1, S)] for y in range(y0, y1 + 1, S)]
miss = sum(v is None for r in g for v in r)
g = [[v if v is not None else 0.0 for v in r] for r in g]
json.dump({"x0": x0, "y0": y0, "step": S, "z": g}, open(os.path.join(root, "SourceArt/VisEx/Intro/dune_heights.json"), "w"))
print("dune grid", len(g), "x", len(g[0]), "missing", miss)
