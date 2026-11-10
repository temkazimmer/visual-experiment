# Design note: a skirt garment type

Clothes agent, 2026-11-10. **Built as `skirt()` in `Tools/blender_outfit.py`, worn in the outfit row "BodystockingSkirt".** It is the first garment that is
not cut from the skin. Numbers come from [STUDY_SKIRT_CORSET.md](STUDY_SKIRT_CORSET.md). Weighting
waits on study steps 4–5, which need the 5.8 package.

## Why it is a new type

Every piece we have is a region of the skin lifted off it, and it copies that skin's weights and
morphs. A skirt **leaves the body** at the hip and spans the gap between the legs. Copying the
nearest skin vertex would split the hem between the two thighs and tear it on every step.

## Shape (rest pose, on the default body)

| | Value | Source |
|---|---|---|
| Waist | sits on `LM["waist"]`, follows the skin (cut like today) | ours |
| Hip | still on the skin down to `hip_top`, then it leaves | ours |
| Length | waist to mid-thigh, about 43 cm on a 170 cm body; scale by her height (153.8) to ~39 cm | study |
| Flare | hull perimeter grows **2.3×** from waist to hem, most of it below the hip | study (77 → 176 cm) |
| Rest folds | **5 broad lobes** round the hem, radial swing **±5 cm** (10 cm peak to peak), fading to 0 at the hip | study |
| Waistband | two layers **0.3 cm** apart, 6 cm tall, plus a 2.2 cm binding on top (TRIM_PROUD) | study |
| Hem | rolled edge from the seam normal (UVSeam.x), no binding | this branch |

**How to build it.** It is a lofted surface in `blender_outfit.py`, not a `region()`:
- Rings every ~1 cm from waist to hem. Down to `hip_top` each ring is the skin hull pushed out
  by the fit offset. Below that, each ring is a superellipse whose perimeter follows the flare
  curve.
- Add `r += 5 * fold(z) * sin(5 * theta + phase(theta))`, where `fold` ramps 0 → 1 from the hip
  to the hem. A small `phase` jitter keeps the lobes from looking machine-made.
- Single-sided, two-sided material (as the study's pieces are). The inner waistband is a copy
  0.3 cm inside.
- Panel seams at the front, sides and back go in `SEAMS`, so the seam normal draws them.
- UV0 stays a garment coordinate in cm: u is arc length round the ring, v is distance down.

**Collision margin.** Keep every ring at least 1.5 cm outside the posed thighs, checked across the
pose wheel. The hull test needs the thigh BVH at each pose, not only the rest pose.

## Weighting (waits on steps 4–5)

- **Above `hip_top`:** take the skin's weights and morphs, as now.
- **Below:** blend from `pelvis` to `thigh_l`/`thigh_r` by angle round the body. Front and back
  centre stay mostly pelvis (≥ 0.7); the sides follow the thigh more towards the hem (thigh
  ≤ 0.5). The two thighs never share a hem vertex above 0.5 each.
- **Morphs:** Glutes, Hips and Fat carry down from the hip ring, scaled by `1 - fold(z)`. Bust
  and the bounce morphs do not apply.
- **Check against their weights first.** When the 5.8 package opens, read the skirt weights and
  sim setup: max distance, backstop, leg collision, LODs. Then decide between this blend and
  Chaos cloth on a coarse sim mesh driving the 5.5 mm render mesh.

## Tema's answers (2026-11-10) and what was built

1. **Length:** shorter than the study, for the photo poses. The hem sits 6 cm below the crotch,
   so the skirt runs about 31 cm from the waist.
2. **Fabric:** black satin, roughness 0.38. The `Satin` slot gets the seam normal: side and
   back seams, rolled hem.
3. **Layering:** worn over the bodystocking. Because a profile wears one mesh, the outfit row
   "BodystockingSkirt" is the bodystocking with the skirt built in, standing 0.75 cm off it.

Built values: flare +9 cm of radius at the hem, 5 lobes of ±5 cm swing, waistband 3.5 cm tall
and 0.3 cm out, binding 0.7 cm wide and 0.3 cm proud.

**Weighting is a stand-in.** Steps 4–5 are now read ([STUDY_SKIRT_SIM.md](STUDY_SKIRT_SIM.md)): the reference uses inpainted weights plus Chaos cloth, so the plan is to keep these weights as the cloth's kinematic base and add a sim.
- above the hip, the skin's weights;
- below it, the hip ring's weights blended towards the leg skin under each point, up to 0.5 at
  the sides at the hem;
- body morphs fade to 0 at the hem.

**Known limit (skinned version):** when she sits or kneels the thighs pass through the front of
the skirt. **Since 2026-11-10 the skirt is Chaos cloth** ([CLOTHES.md](../CLOTHES.md)), which
drapes over the thighs instead; these weights are its skinned anchor.
