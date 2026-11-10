# Study: Jels Studio "OoMH-05 Skirt and Corset" (Fab)

Clothes agent, 2026-11-10. Steps 1–3 of the study plan: geometry and textures, measured from the FBX
in headless Blender. **The pack is licensed and gitignored; this file holds only our measurements
and conclusions, no data from it.** Steps 4–5 (Chaos cloth setup, skirt weights) need the MetaHuman
package, which is saved by UE 5.8 and does not open in our 5.7: on hold.

## What the FBX is

- Two **static** meshes: `Skorzet` (corset) and `Skirt`. No skeleton, no skin weights, no shape keys,
  no cloth data. Authored in cm, Y-up, for a MetaHuman-sized body (~170 cm).
- Clean **all-quad** topology at a very even **5.5 mm** edge length (render density, not sim density).
- **Single-sided panels.** Every garment is many separate sheet pieces: corset 45, skirt 51.
  Thickness and depth come from *stacking pieces*, not from solid walls.
- Four 4K maps per garment: albedo (greyscale, alpha = UV-island mask), normal, roughness, and a
  binary mask (1–2% of texels: the hardware).

## 1. Corset: fit and construction

| Height (cm) | Hull perimeter (cm) | Width × depth (cm) | Reads as |
|---|---|---|---|
| 110 | 73 | 26 × 20 | bottom edge, on the natural waist |
| 118 | 85 | 29 × 21 | under the bust |
| 124 | 95 | 32 × 23 | across the bust (widest) |
| 132 | 88 | 31 × 17 | upper chest |
| 138 | 76 | 30 × 15 | top edge |

- **An overbust that ends on the waist**, 33 cm tall. Bust to waist is **95:73 = 1.30**.
- **Pieces, all mirrored left and right:**
  - two **cups** (160 cm² each, 43° off centre front) and **six body panels** (130–150 cm²);
  - a flat **centre-back panel**, 18.5 cm wide;
  - two short **front-waist panels** (64 cm²);
  - a **front top band**, 20.6 × 4.6 cm.
- **Boning:** six pairs of thin strips, 11–14 cm long and 1–2 cm wide, spread round the waist at
  ±10°, ±23°, ±67°, ±112° and ±140°, plus centre-front channels. All sit in the 108–122 cm band,
  so the bones stiffen only the waist and stop under the cups.
- **Edge bindings:** separate narrow pieces, 1.5–2.2 cm tall, along the bottom edge and under the cups.
- **Layering:** 55% of vertices have another piece within 1.5 cm. Seams touch (median gap 0), and
  bones and bindings sit **up to 0.8 cm** proud (90th percentile). That proud layer is where the
  corset's depth comes from.
- **Small hardware** (a bow or clasp cluster and side hooks) is separate little pieces, flagged white
  in the mask.

## 2. Skirt: drape, hem and waistband

| Height (cm) | Hull perimeter (cm) | Width × depth (cm) |
|---|---|---|
| 107 | 77 | 27 × 22 (waist) |
| 99 | 92 | 33 × 21 |
| 91 | 105 | 37 × 24 (hip, buckles) |
| 83 | 122 | 39 × 28 |
| 75 | 156 | 43 × 33 |
| 69 | 176 | 45 × 35 (hem) |

- **Shape:** an A-line flare from 77 cm at the waist to 176 cm at the hem, 43 cm long (to mid-thigh).
  The flare is **2.3×**, most of it below the hip.
- **Folds are sculpted in.** Round the hem the radius swings with a peak-to-peak of **10 cm**
  (spread 1.5 cm) and about **5 broad lobes** round the hem. The rest pose is already draped: it is
  not a flat cone waiting for simulation.
- **Panels:** a big front panel (741 cm²), side and back panels (340–485 cm²), and a lower ring of
  four gores (257–272 cm²).
- **Waistband: two identical pieces 0.3 cm apart** (inside and outside, 6 cm tall), plus a
  2.2 cm binding on top. Median gap between stacked pieces: **0.3 cm**.
- **Straps:** four hanging straps (16.7 cm, at ±70° and ±120°) end in buckle clusters at
  91 cm: three nested pieces each, of 8.0, 5.3 and 2.7 cm². The front waistband has one more cluster.

## 3. Fabric surface (textures)

- **Texel density:** corset **72.5 px/cm** (0.14 mm per texel), skirt **44.7 px/cm** (0.22 mm).
  About a third of each sheet is unused.
- **Albedo:**
  - **Greyscale**, median luminance 0.26–0.27, so the colour comes from the material tint.
  - The **weave is in the albedo**: the skirt has a fine stripe of about 0.5 mm.
  - The corset albedo shows a coarser diagonal twill in a 512 px crop, but my FFT period there
    (~12 mm) probably mixes in panel edges; treat it as unverified.
- **Roughness is almost flat.** Fabric is **0.8** (64–74% of texels). The only other values are the
  empty sheet at 0 and a few percent between 0.4 and 0.7 on the hardware.
- **Normal maps carry the construction, not the weave:**
  - Corset: 11% of texels are tilted (8% strongly). Piping and seam bevels are **2.5–5.5 mm**
    wide, with stitch-channel rows repeating every **2.6 mm**.
  - Skirt: the normal map's flat value is not 0.5 (mean 0.40), so it is encoded differently.
    Check in UE before reading anything into it.
- **Mask:** binary. White marks only the hardware (2.3% of skirt texels, 1.3% of corset), so
  hardware gets its own metal response.

## What this upgrades in our builder (`Tools/blender_outfit.py`)

1. **Depth from stacked pieces.** Their depth is layers 0.3–0.8 cm apart. Our pieces sit at
   0.05–0.12 cm offsets, and the proud bits (bindings at +0.015) are 10 to 50 times thinner.
   - **Rule:** structural trim (bindings, boning, waistbands) sits **0.3 cm** proud of the panel
     under it.
   - Hardware sits **0.5–0.8 cm** proud.
   - Raised **boning strips** (1–2 cm wide, 11–14 cm long, waist band only) are the next piece to
     add, for a corset or bodice.
2. **Construction lives in the normal map, the weave in albedo, roughness stays flat.**
   - Our `T_Outfit_Detail_M` and `T_Outfit_Weave_N` put the weave in the normal.
   - **Rule:** add a seam and piping channel, i.e. 2.5–5.5 mm bevels along cut edges and seams,
     baked from our own seam lines into a per-outfit normal.
   - Keep fabric roughness near-constant (satin 0.3–0.45, matte cotton 0.8). Only metal gets its
     own mask.
3. **Texel budget:** ~70 px/cm for close-up corsetry, ~45 px/cm for a skirt. Our outfit textures are
   tiling (24 cm UV period), so this applies when we bake per-outfit seam normals.
4. **A skirt is a new garment type.** We have only skin-tight pieces.
   - **Rule for its rest shape:** sculpt the drape in, with about 5 lobes round the hem and ±5 cm of
     radial swing. A clean cone looks wrong even without simulation.
   - **Weights:** the skirt must **not** copy the nearest skin, because it spans the gap between the
     legs. That rule waits for step 5 (their skirt weights) or our own pelvis-to-thigh blend.
5. **Bust-to-waist read:** their overbust is 95:73 (1.30). Use it as the target when we cut a corset
   on our default body.

## Not answered here (needs the 5.8 package)

- Chaos cloth: sim mesh against render mesh, max-distance and backstop maps, leg collision, LODs.
- Skirt and corset skin weights on the MetaHuman skeleton, and whether to transfer them to Quinn.
