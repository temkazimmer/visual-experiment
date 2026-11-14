"""Procedural strand-clump atlas for hair cards (no external tools).

make_atlas(path, tiles=8, w=256, h=1024) writes one RGBA PNG:
  A = strand coverage (the mask), R = per-strand brightness variation, G = strand id,
  B = 0 at the root .. 1 at the tip. u runs across a card, v along it (root v=0).
"""
import numpy as np


def _tile(rng, w, h, nstrands=26, strand_w=(3.0, 5.2), root_rag=0.012):
    v = np.linspace(0, 1, h)[:, None]                      # (h,1)
    u = (np.arange(w)[None, :] + 0.5) / w                    # (1,w)
    alpha = np.zeros((h, w), np.float32)
    var = np.zeros((h, w), np.float32)
    sid = np.zeros((h, w), np.float32)
    # clump envelope: strands gather toward the middle, thin out at the edges
    for k in range(nstrands):
        x0 = np.clip(rng.normal(0.5, 0.17 if nstrands < 30 else 0.24), 0.05, 0.95)
        length = rng.uniform(0.62, 1.0)
        amp = rng.uniform(0.004, 0.02)
        freq = rng.uniform(1.0, 3.2)
        ph = rng.uniform(0, 6.28)
        pinch = rng.uniform(0.15, 0.55)                      # strands converge toward the tip
        xc = 0.5 + (x0 - 0.5) * (1 - pinch * v) + amp * np.sin(freq * 6.28 * v + ph)
        wid = rng.uniform(*strand_w) / w                     # wider strands: cards read solid (QA 0278)
        taper = np.clip((length - v) / 0.18, 0, 1) ** 0.8    # sharp tip
        root = np.clip(v / 0.02, 0, 1)                       # opens at the very root
        half = wid * taper * (0.55 + 0.45 * root)
        d = np.abs(u - xc)
        a = np.clip(1.0 - (d - half * 0.35) / (half * 0.65 + 1e-6), 0, 1)
        a *= (v <= length)
        a *= (v >= rng.uniform(0.0, root_rag))               # ragged roots: no straight card edge at the hairline
        # (0.035 opened the elf cut's part into a 1.5 cm bald stripe: its cards are ~85 cm long)
        bright = rng.uniform(0.35, 1.0)
        m = a > alpha
        var = np.where(m, bright, var)
        sid = np.where(m, rng.uniform(), sid)
        alpha = np.maximum(alpha, a)
    return alpha, var, sid


def make_atlas(path, tiles=8, w=256, h=1024, seed=3, fine=False):
    """fine: a third more strands, a little thinner, and roots ragged over the first 7% (short
    hair: lighter colours show each strand, and the roots of its upper layers are in plain view).
    Not finer than that: strands far under a pixel at head-shot distance average into solid,
    feather-like cards (tried at twice the count and half the width, 2026-11-14)."""
    import bpy
    rng = np.random.default_rng(seed)
    out = np.zeros((h, w * tiles, 4), np.float32)
    vv = np.linspace(0, 1, h)[:, None] * np.ones((1, w))
    for t in range(tiles):
        dense = t < tiles // 2                      # first half: coverage tiles, packed
        if fine:
            a, var, sid = _tile(rng, w, h, nstrands=int(rng.integers(88, 108) if dense else rng.integers(46, 58)),
                                strand_w=(2.5, 4.4), root_rag=0.07)
        else:
            a, var, sid = _tile(rng, w, h, nstrands=int(rng.integers(64, 80) if dense else rng.integers(34, 44)))
        # card borders: coverage fades to nothing at the tile's sides and is cut before the tip
        # row, or the card's rectangle shows as a pale outline (mip bleed across tiles, and the
        # tip row wrapping onto the root row; QA 0286)
        uu = (np.arange(w) + 0.5) / w
        side = np.clip(np.minimum(uu - 0.04, 0.96 - uu) / 0.08, 0, 1)
        side = side * side * (3 - 2 * side)
        tip = (np.linspace(0, 1, h) < 0.985)[:, None]
        a = a * side[None, :] * tip
        sl = slice(t * w, (t + 1) * w)
        out[:, sl, 0] = var
        out[:, sl, 1] = sid
        out[:, sl, 2] = vv
        out[:, sl, 3] = a
    # Blender image rows run bottom (v 0) to top, which is how the array is laid out
    img = bpy.data.images.new("hair_atlas", w * tiles, h, alpha=True, float_buffer=False)
    img.pixels.foreach_set(np.clip(out, 0, 1).ravel())
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    return path
