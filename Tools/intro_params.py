"""Shared layout of the intro shot (plain Python: read by the Blender and the editor scripts).

She walks a straight line from ORIGIN along HEADING (degrees, UE yaw) up an open dune ridge
on open flat sand west of the set (take 3: no hills, Tema).
Distances along the path are 'u' (cm), across it 'v' (cm, +v = her left).
"""
import math

ORIGIN = (-7500.0, -19000.0)       # open flat sand west of the set, lit by a low western sun
HEADING = 90.0                     # she walks north, across the frame
U_START = 0.0              # where she stands at t = 0
U_CREST = 0.0              # (no ridge: Tema wants flat sand)
PATCH_U = (-300.0, 1500.0)  # hero sand extent along the path...
PATCH_V = 500.0            # ...and half-width across it
CORE_V = 280.0             # full ripple relief within this |v|
STEP = 1.0                 # cm grid

_c, _s = math.cos(math.radians(HEADING)), math.sin(math.radians(HEADING))


def world(u, v):
    """Path coordinates -> world xy (works on numpy arrays too)."""
    return ORIGIN[0] + u * _c - v * _s, ORIGIN[1] + u * _s + v * _c


CENTER = world(0.5 * (PATCH_U[0] + PATCH_U[1]), 0.0)
