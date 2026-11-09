"""Detail textures for the outfit materials, 2048 x 2048, tileable. Outside the editor:

  /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup \
      --python Tools/blender_outfit_textures.py -- SourceArt/Character/Outfits/Textures

  T_Outfit_Weave_N   satin weave, tangent-space normal
  T_Outfit_Latex_N   soft stretch ripples in latex, tangent-space normal
  T_Outfit_Detail_M  linear masks: R smudges (roughness), G fine grain, B soft clouds

Tools/char_outfit.py imports them (normal / masks compression, linear, never streamed).
They tile, and the materials repeat them a whole number of times per 24 cm of garment UV.
"""
import os
import sys

import bpy
import numpy as np

OUT = sys.argv[sys.argv.index("--") + 1]
N = 2048
os.makedirs(OUT, exist_ok=True)
yy, xx = np.mgrid[0:N, 0:N] / N


def band_noise(seed, lo, hi, aniso=(1.0, 1.0)):
    """Tileable noise with features between lo and hi cycles per tile."""
    rng = np.random.default_rng(seed)
    F = np.fft.fft2(rng.standard_normal((N, N)))
    fy = np.fft.fftfreq(N)[:, None] * N * aniso[1]
    fx = np.fft.fftfreq(N)[None, :] * N * aniso[0]
    r = np.sqrt(fx ** 2 + fy ** 2)
    mid, wid = (lo + hi) * 0.5, max((hi - lo) * 0.5, 1e-3)
    h = np.real(np.fft.ifft2(F * np.exp(-((r - mid) / wid) ** 2)))
    return h / h.std()


def unit(h, spread=2.5):
    return np.clip(h / (2 * spread) + 0.5, 0, 1)


def normal_map(h, strength):
    dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5 * N * strength
    dy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5 * N * strength
    n = np.stack([-dx, -dy, np.ones_like(h)], -1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    return n * 0.5 + 0.5


def save(name, rgb):
    img = bpy.data.images.new(name, N, N, alpha=False, is_data=True)
    px = np.concatenate([rgb, np.ones((N, N, 1))], -1).astype(np.float32)
    img.pixels.foreach_set(px.ravel())
    img.filepath_raw = os.path.join(OUT, name + ".png")
    img.file_format = "PNG"
    img.save()
    print("wrote", img.filepath_raw)


# satin: 64 warp threads floating over 4, bound by a short weft step (a 5-end satin look)
T_ = 64
u, v = xx * T_, yy * T_
warp = np.cos(2 * np.pi * u) * 0.5 + 0.5                    # a rounded thread running along v
bind = ((np.floor(u) * 2 + np.floor(v)) % 5 == 0)           # where the weft crosses over
weft = np.cos(2 * np.pi * v) * 0.5 + 0.5
h = np.where(bind, 0.55 * weft + 0.25, warp * (0.85 + 0.15 * np.cos(2 * np.pi * v / 5)))
h = h + 0.05 * band_noise(1, 40, 160)
save("T_Outfit_Weave_N", normal_map(h, 0.0022))

# latex: long soft ripples across the stretch; it must stay smooth or it reads as scales
h = 1.0 * band_noise(2, 2, 6, aniso=(1.0, 0.35)) + 0.06 * band_noise(3, 14, 40)
save("T_Outfit_Latex_N", normal_map(h, 0.0016))

# masks
smudge = unit(band_noise(5, 2, 7) + 0.5 * band_noise(6, 8, 20, aniso=(0.5, 1.0)))
grain = unit(band_noise(7, 150, 600))
cloud = unit(band_noise(8, 5, 16))
save("T_Outfit_Detail_M", np.stack([smudge, grain, cloud], -1))
