"""Shared layout of the intro shot (plain Python: read by the Blender and the editor scripts).

She walks a straight line from ORIGIN along HEADING (degrees, UE yaw) up an open dune ridge
south of the ruins, toward the low sun (sun azimuth -55, so the light travels toward yaw 125).
Distances along the path are 'u' (cm), across it 'v' (cm, +v = her left).
"""
import math

ORIGIN = (24000.0, -32000.0)
HEADING = -35.0
U_START = 300.0            # where she stands at t = 0
U_CREST = 1850.0           # the ridge line
PATCH_U = (100.0, 2500.0)  # hero sand extent along the path...
PATCH_V = 500.0            # ...and half-width across it
CORE_V = 330.0             # full ripple relief within this |v|
STEP = 1.0                 # cm grid

_c, _s = math.cos(math.radians(HEADING)), math.sin(math.radians(HEADING))


def world(u, v):
    """Path coordinates -> world xy (works on numpy arrays too)."""
    return ORIGIN[0] + u * _c - v * _s, ORIGIN[1] + u * _s + v * _c


CENTER = world(0.5 * (PATCH_U[0] + PATCH_U[1]), 0.0)
