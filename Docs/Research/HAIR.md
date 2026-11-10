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

## Reference: Baldur's Gate 3 (added 2026-11-10, Tema's target for look and feel)

What is verifiable, from [80.lv](https://80.lv/articles/creating-characters-for-baldur-s-gate-3) and [Adobe Substance](https://www.adobe.com/products/substance3d/magazine/bringing-life-to-legends-character-art-in-baldurs-gate-3.html):

- Larian moved from clay-sculpted hair (Divinity: Original Sin 2) to a strand approach, which needed a new in-house hair shading model from their graphics programmers. Their style is "more realistic" than in DOS2.
- A Maya tool aligns the strands and controls vertex colours; the same tool does fur and feathers. Early hairstyle designs were outsourced to set the quality bar.
- Overall rule for characters: follow PBR, "but bend them, not too much, just a bit". Eyes get faked highlights so they don't look dead. Skin comes from scans, with fantasy colours for non-human races.

Not found: whether the strands are cards or true curves, texture sizes, card counts, shader maths. Treat anything more specific as unverified.

What that means for us:

- BG3 reads as **grounded realism with a light painterly bend**: soft clumped strands, warm skin-like light wrap, readable value structure, no hard toon bands.
- That **contradicts the current hair direction** (blue-black spikes, solid locks, 90s anime) and step 1 above (hard-edged anime highlight band). It leans towards the card route: hundreds of cards, a baked strand texture, flow-map anisotropic highlight, root-to-tip colour, flyaway cards, a soft tint instead of near-black.
- It also pulls on skin and eyes, not only hair. The aerogel body and "statue" eyes are not a BG3 look.

## Card hair physics: how others do it (added 2026-11-10)

Tema chose the long, Frieren-style cut built by `Tools/blender_hair_cards.py` (fringe to the brow, two face-framing side locks in front of the elf ears, the rest combed behind them). It is cards, so it moves the way card hair moves in shipped games: on bones.

| Option | How it works | Who uses it | Verdict for us |
|---|---|---|---|
| **Kawaii Physics** | One AnimGraph node sways bone chains. A simple solver without PhysX that keeps bone length, so chains never stretch. Sphere, capsule, box and plane colliders; wind and gravity ([docs](https://pafuhana1213.github.io/KawaiiPhysics-Portal/en/docs), [GitHub](https://github.com/pafuhana1213/KawaiiPhysics)). MIT licence, UE 5.3 to 5.8. | Stellar Blade and Granblue Fantasy Versus Rising (listed on the plugin's portal); Clair Obscur: Expedition 33 ([PC Gamer](https://www.pcgamer.com/games/rpg/clair-obscur-expedition-33-uses-a-plugin-called-kawaiiphysics-a-free-engine-built-to-cutely-animate-things-thats-used-in-a-ton-of-popular-games-from-gachas-to-soulslikes)); Wuthering Waves is only a technical artist's guess ([Automaton](https://automaton-media.com/en/column/wuthering-waves-is-full-of-beautiful-technical-headaches-that-would-make-the-average-ue-developer-say-please-dont-make-me-do-this/)). | **Recommended.** It is the de facto standard for stylized card hair in UE. |
| AnimDynamics | Built-in AnimGraph node with its own light solver ([Epic](https://dev.epicgames.com/documentation/unreal-engine/animation-blueprint-animdynamics-in-unreal-engine)). | Common in forum setups. | Fallback with no plugin. Forum reports say chains of 4 or more bones twitch ([thread](https://forums.unrealengine.com/t/physics-for-short-stylized-anime-hair/243622)). Our locks need 4 to 6. |
| RigidBody node and Physics Asset | Real rigid bodies per bone. | Generic. | Chains stretch in fast moves (same thread). Costlier to tune. |
| Chaos Cloth on cards | Paint tips free and roots fixed ([Reallusion's setup](https://manual.reallusion.com/CC_and_IC_Auto_Setup_Plugin/ENU/CC_and_iC_Auto_Setup/1.2/02_for_Unreal/Hair-Physics.htm)). | Character Creator exports. | Works, but forum users call it far below groom. Cards collapse and clip. |
| Groom strands with simulation | Guides simulate and strands follow ([Epic](https://dev.epicgames.com/documentation/unreal-engine/hair-physics-in-unreal-engine---overview)). | MetaHumans. | Not for cards. Generated cards do not follow the sim. |

**Plan for her:**
1. In Blender, add bone chains to the hair: about 6 behind (5 bones each), one per side lock (5 bones), 3 short ones in the fringe (2 bones). Bind each card to the chain nearest its root, weighted along its length.
2. Export the hair as **its own skeletal mesh** (root bone = `head`), attached to the character's `head` socket, with a small AnimBP running Kawaii Physics. This avoids adding bones to the shared Quinn skeleton, which the character branch owns.
3. Colliders: a head sphere, a neck capsule, shoulder and upper-back capsules kept 1 cm out (the clothes agent's strap clearance).
4. Tune for weight, not bounce: high damping and low stiffness at the roots, so the hair swings late and settles, matching [ANIMATION.md](ANIMATION.md).

Open points: Kawaii Physics is a C++ plugin, so the Mac build has to compile it (the project already builds C++). Adding a plugin is Tema's call.

## What I could not verify

- **No named AA studio's hair method.** I searched for Black Myth: Wukong, Hellblade 2 and Stellar Blade. I found no source describing their hair, so none is claimed here. The studio-level information above is Epic's docs, 80.lv and blog posts.
- **Strand hair on macOS/Metal.** Epic's platform support page returned no content. Do not plan around strands on this machine until tested.
- **Substrate toon shader in UE 5.8** is from one blog post, not Epic's release notes.
- **The "awkward" diagnosis** is from the code. I did not open the editor or look at a capture. A 4K capture of the head from front, side and back would confirm which of the five causes matter most.
- [Wuthering Waves-style hair shader](https://www.patreon.com/posts/ue5-wuthering-144355942) is paid and behind a login; I did not read it.
