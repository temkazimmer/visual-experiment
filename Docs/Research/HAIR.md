# Hair

Research note, 2026-11-10. Goal: her hair stops looking awkward. Sources are linked inline; what I could not verify is at the end.

## Where we are

`Tools/blender_hair.py` builds a cap on the scalp plus about 92 solid blade-shaped locks in four rows (under, middle, upper, crown). `Tools/char_hair.py` gives them a plain Lit material: near-black base colour, a Fresnel rim, roughness 0.34, anisotropy 0.55. The direction is "short, blue-black, layered spikes, solid locks, not strands" ([CHARACTER_DIRECTION](../CHARACTER_DIRECTION.md)).

Likely reasons it reads awkward (from the code, not yet checked against captures):

1. **No clump structure.** Each lock is one smooth flat blade. Real and stylized hair both read as a lock that splits into two or three sub-locks with offset tips.
2. **No highlight shape.** Anisotropy on a flat Lit surface gives a muddy sheen, not a band. Near-black base colour hides the form.
3. **No root-to-tip change.** One colour end to end.
4. **Rigid.** The hair follows the head bone. Nothing swings when she walks or turns, while her body has springs.
5. **Even silhouette.** Rows of similar-length spikes read as a helmet. No stray locks, no asymmetry.

## What studios do (verified)

Two real routes in UE5, plus a stylized shortcut.

| Route | What it is | Cost | Fits us? |
|---|---|---|---|
| **Strands (Groom)** | Curves simulated and rendered per strand. Best motion and shading. | Epic: [needs an allocated performance budget](https://dev.epicgames.com/documentation/unreal-engine/groom-scalability-and-performance-with-unreal-engine). Curve count drives simulation, interpolation, voxelisation. | No. Wrong look for solid spiky locks; Mac support unverified. |
| **Cards** | Flat strips with a baked strand texture, alpha-masked. Hundreds of cards, 3 to 5 segments each so they bend ([Stray Spark](https://www.strayspark.studio/blog/blender-hair-grooming-to-unreal-engine-pipeline-2026)). | Low. Epic reports [cards on all LODs improve performance by 20 to 30%](https://dev.epicgames.com/documentation/unreal-engine/groom-scalability-and-performance-with-unreal-engine) over strands at LOD0. | Yes, if we want a softer, more real look. |
| **Stylized solid locks** | What we have: chunky geometry, shading does the work. | Lowest. | Yes. It is the direction. It needs better shaping and shading. |

Card pipeline in practice ([80.lv, Marilyn hairstyle](https://80.lv/articles/creating-real-time-marilyn-monroe-hairstyle-in-3d)): sketch over reference, make strands in Blender, bake strips to a plane (alpha, AO, colour, depth, flow, ID, root maps), place cards by hand starting with silhouette shapes, then add supporting strands for breakup. The shader takes flow map, root tint, depth and spec.

Epic's [Hair Card Generator](https://dev.epicgames.com/documentation/unreal-engine/creating-hair-cards-and-lods-using-hair-card-generator) builds cards from a groom in layers: coverage (few cards, hides the scalp), mid, top, flyaways (one card per stray strand), each with its own triangle budget. It is experimental, needs a groom as input, and its docs give an example budget of roughly 2,000 to 25,000 triangles per group.

Card material rules ([Stray Spark](https://www.strayspark.studio/blog/blender-hair-grooming-to-unreal-engine-pipeline-2026)): Masked blend, not Translucent (sorting); two-sided; anisotropic highlight, or default Lit looks like plastic ribbon; keep the base colour near-neutral and tint root to tip in the material.

Anime-style highlights in UE5 ([MooaToon](https://github.com/JasonMa0012/MooaToon), [Epic forum toon thread](https://forums.unrealengine.com/t/toon-shading-models-stylized-rendering-experiments/218895), [Stray Spark toon](https://www.strayspark.studio/blog/substrate-toon-shader-ue5-8-tutorial)): a Kajiya-Kay style highlight stretched along the lock, thresholded into a small hard-edged shape, with specular and rim set separately. A second broader lobe is common.

## Plan for her (recommended: keep solid locks, fix shape, shading, motion)

Order is by visible gain per effort. Each step is one A/B at 4K.

1. **Shading first, no remodel.** In `M_Hair`:
   - Lighten the base a little and add a root-to-tip gradient (UV along the lock): dark at root, a hint of blue at tips.
   - Replace the plain anisotropic specular with a thresholded Kajiya-Kay band: `T` along the lock, `H` from the sun, `sin(T,H)` raised, then `smoothstep` to a hard edge. Tint it cool, keep intensity clamped.
   - Keep the rim, but drive it from the sky colour so it is warm against the dust.
   - Add a root AO gradient so locks sit into the cap.
2. **Reshape the locks** in `blender_hair.py`:
   - Split each lock into 2 or 3 sub-locks with different lengths and a small angle between them.
   - Give each an S-curve and an outward tip flick, not a straight taper.
   - Break the silhouette: vary row lengths, one or two long stray locks, a clear asymmetry at the fringe.
3. **Secondary motion.** Add 2 to 3 bone chains to the fringe and the back locks, driven by the same spring approach as `UVisexBodyComponent` (low stiffness, damped, a little wind from the scene). Match the weight rules in [ANIMATION.md](ANIMATION.md): overlap and a late settle, not floppy. Chain names must be new bones; keep them out of Reshape's scaling list.
4. **Only if it still looks flat:** cards for the soft outer layer (flyaways, nape) in the same material, using the Hair Card Generator or hand-placed strips. This is a bigger change and the pipeline is not set up.

Strands: not recommended. The look conflicts with the direction, and I could not confirm Mac/Metal support (see below).

## What I could not verify

- **No named AA studio's hair method.** I searched for Black Myth: Wukong, Hellblade 2 and Stellar Blade. I found no source describing their hair, so none is claimed here. The studio-level information above is Epic's docs, 80.lv and blog posts.
- **Strand hair on macOS/Metal.** Epic's platform support page returned no content. Do not plan around strands on this machine until tested.
- **Substrate toon shader in UE 5.8** is from one blog post, not Epic's release notes.
- **The "awkward" diagnosis** is from the code. I did not open the editor or look at a capture. A 4K capture of the head from front, side and back would confirm which of the five causes matter most.
- [Wuthering Waves-style hair shader](https://www.patreon.com/posts/ue5-wuthering-144355942) is paid and behind a login; I did not read it.
