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

**Status (2026-11-10):** Kawaii Physics 1.21.0 is in `Plugins/` and builds on Mac. `Tools/char_hair_cards.py` imports `SKM_HairCards` (own skeleton `SK_HairCards`, 11 chains, 0.01 scale like the skin) with `M_HairCards`. First UE captures: it sits on the head, ears clear. It reads darker than the Blender renders; the highlight needs tuning.

**Physics (2026-11-10):** `ABP_HairCards` is built by `Tools/char_hair_physics.py` through the editor-only `visexTools` module (Python can't wire AnimGraph nodes). The colliders are measured off the body. Tuning is damping 0.25, stiffness 0.1 and gravity -980: it hangs, lags a turn and settles. Test in Simulate with editor background throttling off; at 3 fps the solver explodes. If the game ever drops that low, turn on Kawaii's fixed substepping (project setting).

The manual equivalent, for reference:
1. Right-click `SK_HairCards` → Create → Anim Blueprint → `ABP_HairCards`.
2. AnimGraph: **Copy Pose From Mesh** (tick *Use Attached Parent*) → **Kawaii Physics** → Output Pose.
3. Kawaii Physics node: Root Bone `hair_b0_01`, then add the other 10 roots as extra chains (`hair_b1..b5_01`, `hair_s0_01`, `hair_s1_01`, `hair_f0..f2_01`). Damping 0.8, Stiffness 0.05, World Damping Location 0.8, Gravity -980.
4. Colliders: sphere on `head` (radius ~11), capsules on `neck_01`, both shoulders and `spine_05`, each 1 cm larger than the body.
5. On the character: add a Skeletal Mesh component `Hair` under the body mesh, mesh `SKM_HairCards`, anim class `ABP_HairCards`, and hide the `Hair` section of `SKM_AndroidSkin`. This touches the character branch (`UVisexBodyComponent` or the skin chain), so it is that agent's change.

## Hairlines, partings and what "finished" card hair has (added 2026-11-11)

Tema: the hair "looks halfway there". Asked: how do others build the hairline. What the
breakdowns agree on, and where ours differs.

**1. Cards go on in layers, and each layer has a job.** The clearest breakdown
([80.lv, real-time Chinese hairstyle in Blender](https://80.lv/articles/designing-a-real-time-traditional-chinese-hairstyle-in-blender))
uses six, in this order:

| Layer | Cards | Job |
|---|---|---|
| Hairline | dense, opaque, snapped onto the scalp around the hairline **and at the part** | softens where skin becomes hair |
| Base shape | the densest, opaque, side by side or just overlapping | blocks the scalp |
| Volume and breakup | semi-sparse crossed with sparse | body; sparse cards can overlap without showing edges |
| Sparse strands | two sparse cards crossed, some against the flow | irregularity |
| Flyaways | loose single strands | breaks the silhouette |
| Baby hair | very thin, short, soft, at the front and the nape | the last softening of the hairline |

Others say the same in fewer words: an opaque base for scalp coverage, then 1 or 2 more passes of
less opaque cards, and the final layers "work out the transition areas along the hairline"
([80.lv, tips on hair for games](https://80.lv/articles/tips-tricks-on-hair-for-games/)); card
type 1 is "dense, nearly white alpha" and covers the scalp, types 2 to 6 get sparser, placed
from the scalp outward ([80.lv, hair for real-time characters](https://80.lv/articles/creating-hair-for-real-time-game-characters/)).

**2. The scalp is never left as bare skin under the hair.** Two mechanisms:
- The dense base layer (above).
- The skin itself is darkened where hair grows. Epic's groom tools generate a *follicle mask*, "a
  small distance field of the hair roots", so the head's material can blend hair into its surface
  ([Epic, generating groom textures](https://dev.epicgames.com/documentation/unreal-engine/generating-groom-textures-in-unreal-engine)).
  Card-hair packs ship a separate *cap* mesh with its own UVs and alpha for the same purpose
  (seen in marketplace listings; I found no studio write-up that explains the cap, so the
  explanation is mine: a shell on the scalp, hair-coloured, fading out at the hairline).

**3. Roots are softened in the shader.** The Paragon hair material's "eyebrow option" adds pixel
depth offset at the roots "for a softer blend into the scalp" (same Chinese-hairstyle article).

**4. Depth comes from baked occlusion, not only from lighting.** "Ambient occlusion is baked into
the vertices of the hair cards on a layer-by-layer basis" (80.lv tips article). Inner layers are
darker, so the mass reads as layered instead of as one flat colour.

**5. Clumps are 2 to 3 crossed cards**, not single ribbons, so a lock has thickness from every angle
(80.lv tips article; the Chinese-hairstyle article crosses one opaque card with two semi-sparse).

### Ours against that list

| Practice | Ours before today | Now |
|---|---|---|
| Hairline layer, cards at the part | none: the part was a 1.5 cm stripe of bare scalp | not done as cards; the cap closes the part, and strand roots are less ragged |
| Scalp darkened under the hair | none: every gap showed bright skin | a cap shell on the scalp in the hair mesh (`M_HairCap`), fading at the hairline |
| Dense base, sparser on top | yes (dense tiles under, sparse on top) | unchanged |
| Flyaways | 4% of the back cards | unchanged |
| Baby hair | none | not done; next for the front hairline |
| Soft roots (depth offset) | none | not done: Python can't reach that material pin in 5.7 |
| Baked occlusion per layer | none: the hair is one flat black | not done; the next thing to try for depth |
| Crossed-card clumps | single ribbons, slightly arched | not done; matters most for the face locks |

So the missing half is mostly at the scalp (cap, hairline, part) and in depth (occlusion,
crossed clumps). Shape and motion were the half we had.

### Why black hair went white in backlight (found 2026-11-11)

Every QA round reported pale or white strands at the hair's edge against the haze, and Tema's
first Body-page shot had white card edges. Reproduced in the editor: sun in front of her, camera
behind. Sun off: gone. Sky light off: still there. Specular 0, a quarter of the base colour, the
Backlit pin at 0: no change. Only a pure black base colour removed it.

The engine source explains all of it (`Engine/Shaders/Private/HairBsdf.ush`,
`HairShadingCommon.ush`): the Hair shading model always adds the TT lobe, light transmitted
through the fibre, when light and viewer are on opposite sides. For card hair
`bUseBacklit` is false, so the material's Backlit pin is ignored (it only works for strand
grooms), Specular doesn't scale that lobe, and its tint is `pow(BaseColor, small exponent)`, so
near-black hair still transmits a lot.

Fix: `M_HairCards` is now Default Lit with Anisotropy -0.8 and the tangent along the strands (a
highlight band across the hair, no transmission). Roughness 0.58, specular 0.12. On this model the
cards' normals matter, which the Hair model hid: flat cards can show as facets in a hard light.

## Hairstyles (added 2026-11-11)

Two styles, each its own mesh and AnimBP; one skeleton (`SK_HairCards`), atlas and material for both.
To put one on her: mesh + anim class, attached to the body mesh, as `ApplyHair` does.

| Style | Mesh | AnimBP | Look |
|---|---|---|---|
| long | `SKM_HairCards` | `ABP_HairCards_C` | loose, straight, wispy fringe, face-framing locks |
| elf | `SKM_HairElf` | `ABP_HairElf_C` | centre part, front and sides swept behind the ears and gathered at the back, one thin lock a side, no fringe |

Build a style: `HAIR_STYLE=elf ... blender_hair_cards.py -- <out> rig`, copy the FBX and atlas to
`SourceArt/Character/Hair/`, then in the editor run `char_hair_cards.py` and `char_hair_physics.py`
with `STYLE='elf'` set first (see the scripts' headers).

- The elf cut is Tema's fix for locks cutting into the body: whatever collided is tied back, and the
  two face locks have their own chains (`hair_s0`, `hair_s1`) with colliders on the collarbones and bust.
- A new style's bones must be a subset of the skeleton's, or the importer adds them to `SK_HairCards`
  (fine, but then commit the skeleton too).
- UE's importer puts a new style's mesh on the existing hair skeleton by itself; don't rename it.

## What I could not verify

- **No named AA studio's hair method.** I searched for Black Myth: Wukong, Hellblade 2 and Stellar Blade. I found no source describing their hair, so none is claimed here. The studio-level information above is Epic's docs, 80.lv and blog posts.
- **Strand hair on macOS/Metal.** Epic's platform support page returned no content. Do not plan around strands on this machine until tested.
- **Substrate toon shader in UE 5.8** is from one blog post, not Epic's release notes.
- **The "awkward" diagnosis** is from the code. I did not open the editor or look at a capture. A 4K capture of the head from front, side and back would confirm which of the five causes matter most.
- [Wuthering Waves-style hair shader](https://www.patreon.com/posts/ue5-wuthering-144355942) is paid and behind a login; I did not read it.
