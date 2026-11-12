"""Height map for the intro's close-up sand, from the Megascans rippled-sand normal map.

The scan ships no displacement, so the 8K normal map is integrated (Frankot-Chellappa,
FFT) into a height field and scaled to the scan's 4 cm relief. The result is licensed-
derived, so it stays in SourceArt/Megascans (gitignored).

Run: Blender -b -P Tools/intro_sand_height.py
"""
import os
import bpy
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "SourceArt/Megascans/RippledSand_vd3lecfs/Textures/T_vd3lecfs_8K_N.png")
OUT = os.path.join(ROOT, "SourceArt/Megascans/RippledSand_vd3lecfs/height_2k.npy")
RES = 2048          # 2 m tile -> ~1 mm per texel
RELIEF_CM = 4.0     # scan height per the Megascans metadata
GREEN_DOWN = False  # DirectX-style normal map (UE convention); flip if ripples come out inverted

img = bpy.data.images.load(SRC)
w, h = img.size
px = np.empty(w * h * 4, dtype=np.float32)
img.pixels.foreach_get(px)
px = px.reshape(h, w, 4)[::-1]          # Blender stores bottom row first
f = w // RES
n = px[:, :, :3].reshape(RES, f, RES, f, 3).mean(axis=(1, 3)) * 2.0 - 1.0
nx, ny, nz = n[..., 0], n[..., 1], np.clip(n[..., 2], 0.05, None)
if GREEN_DOWN:
    ny = -ny
# slopes in texel units: dz/dx = -nx/nz; image rows go down, so dz/drow = +ny/nz
p = -nx / nz
q = ny / nz
wx = np.fft.fftfreq(RES) * 2 * np.pi
wy = np.fft.fftfreq(RES) * 2 * np.pi
WX, WY = np.meshgrid(wx, wy)
den = WX ** 2 + WY ** 2
den[0, 0] = 1.0
Z = (-1j * WX * np.fft.fft2(p) - 1j * WY * np.fft.fft2(q)) / den
Z[0, 0] = 0
z = np.real(np.fft.ifft2(Z))
lo, hi = np.percentile(z, 0.5), np.percentile(z, 99.5)
z = np.clip((z - lo) / (hi - lo), 0, 1) * RELIEF_CM
np.save(OUT, z.astype(np.float32))
print("height", z.shape, float(z.min()), float(z.max()), "->", OUT)
