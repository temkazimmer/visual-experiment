# vis-ex animation buy list

For Tema to buy himself. Nothing here has been purchased. Prices were read from the live Fab listings on **2026-11-10**: GBP, excluding tax, so the basket may add VAT. Rating means Fab stars. Most of these listings have 0 to 5 ratings, so treat the preview video as the real quality signal. I could not watch videos, so check them before buying.

Our skeleton is the UE5 Manny/Quinn mannequin, which Quinn at 159 cm shares. "Native" means authored on it, so no retarget.

## How these packs are used (Tema, 2026-11-10)

The aim is to **learn from the clips and build the knowledge into our own model**, not to drop assets in. Every pack here is a study reference. The movement agent extracts timing, weight, arcs and foot placement, writes them down as rules, and implements them in our own clips and layers (see `ANIMATION.md` and `GUIDELINES.md`). Shipping a bought clip unchanged is the exception, and needs a reason.

## Direction (from Tema, 2026-11-10)

Proper anatomy. A calm but playful character with a **proud** bearing. So rank by:
- upright chest and level head (pride), unhurried weight (calm);
- small playful accents on top: a head tilt, a hair tuck, a spin, a half-smile of the body. Never bounce or mugging.
- Prefer swagger and confident gaits over cute or timid ones. Cute packs (UwU, happy cheers) are accent sources only.
- Anatomy means mocap over keyframe where weight and ribcage/pelvis counter-rotation matter. Keyframed packs (Girl Walk&Run) are for silhouette study.

## Week 1 basket (about £95)

| Pack | £ | Why |
|---|---|---|
| Gamma Movement | 29.75 | Female locomotion on our skeleton, no retarget |
| Park Life | 33.47 | Feminine seated and leaning poses, stand-to-sit |
| Girl Walk&Run | 23.80 | The only catwalk/strut walk I found |
| Female Everyday Idles | 5.20 | Weight shifts and fidgets |
| Lie Down | 2.96 | Floor poses for 3 pounds |
| **Total** | **95.18** | |

## Must buy

### 1. Gamma Studio, Movement Animation | Vicon Mocap: £29.75
[Listing](https://www.fab.com/listings/f3fe5267-5658-4891-a8c0-1023a35287ff)
- **Clips:** 169, at 120 FPS.
- **Skeleton:** Native Manny/Quinn. No retarget needed.
- **Root motion:** Yes, on locomotion, turns, transitions and direction changes.
- **Capture:** Vicon optical mocap, hand-cleaned, female performer. No ratings yet.
- **Contents:** Unarmed walk, jog and run in 8 directions, each with start, loop and stop. Also walk/run transitions, turns in place, idles, gestures, hit reactions.
- **Why it fits:** It is the closest thing to a female "Game Animation Sample" on our skeleton. Starts, stops and turns are the gaps in our hand-built clips.
- **What it teaches us:** How a real performer leans into a start, settles into a stop and plants during a turn. Read the start-clip timing and the stop-foot placement.
- **Note:** The unarmed set is one of three in the [Female Locomotion Bundle](https://www.fab.com/bundles/16f20a78-2dfb-4d51-baae-90b2d198dec6). The other two are pistol and rifle, which we don't need, so buy the single pack.
- **Free taster:** the [Gamma Free Animation Pack](https://www.fab.com/listings/8de31c5d-93bc-4bd4-9606-ca789ce91b99) is £0 with 24 clips on the same skeleton. It is mostly male and rifle, so use it only to test the rig and retarget path.

### 2. MoCap Online, Park Life: £33.47
[Listing](https://www.fab.com/listings/5f1df0df-2211-4911-92d4-adeb8d0fa218) (listing text read, no video watched).
- **Clips:** 65+, about 30 s each, all seamless loops.
- **Skeleton:** Not stated explicitly. Retargeted to Manny with IK Retargeters included. Expect a small retarget job.
- **Root motion:** Not mentioned.
- **Capture:** Mocap. Rating 5.0 from 2 ratings.
- **Contents:** Bench sitting with feminine variants (legs together or crossed). Standing poses leaning on a wall or tree. Stand-to-sit transitions.
- **Why it fits:** It covers priorities 2 and 3 (seated idles and poses) in a feminine, non-sexual way.
- **What it teaches us:** How leg and ankle crossing and hand placement carry personality. It is also the best source for stand-to-sit timing.

### 3. Digital Kinetics, Female Everyday Actions Vol. 1 (Idle Collection): £5.20
[Listing](https://www.fab.com/listings/2bcb4ad1-dabf-4713-a519-2fa76db0be80)
- **Clips:** 20.
- **Skeleton:** Not specified. The page says "any humanoid rig". Treat it as a retarget job and test one clip first.
- **Root motion:** Both root-motion and in-place versions.
- **Capture:** Vicon mocap, female actor. No ratings yet.
- **Contents:** Left and right weight shifts, hands on hips, crossed arms, adjust hair, tuck hair behind ear, head tilt, deep breath, look left, right and up.
- **Why it fits:** Priority 2, and cheap. These are exactly the micro-animations the menu idle needs.
- **What it teaches us:** How little a good idle moves. Compare the weight-shift clips' hip drift against ours.

## Strong

### 4. Inu Games, Girl Walk&Run Anims: £23.80
[Listing](https://www.fab.com/listings/efda1f4a-cfee-46a4-bcfc-2c6f2b40121e)
- **Clips:** 17 (13 walk, 4 run), including a catwalk strut and three strut variants.
- **Skeleton:** Standard UE5 mannequin (Quinn). Source FBX and Cascadeur files are included.
- **Root motion:** Not stated.
- **Capture:** Hand-keyed ("hand made"), not mocap. Rating 4.5 from 4 ratings.
- **Why it fits:** It is the only catwalk walk I found. Hand-keyed means exaggerated and readable, which suits a stylised android.
- **What it teaches us:** The cross-step and hip sway of a catwalk. The Cascadeur files let our agent open the keys and see the arc.

### 5. XM3 Studios, Walk Cycle Animations Pack #01: £21.56
[Listing](https://www.fab.com/listings/b216a371-a939-45c7-b3c0-b883ea0ec3c3)
- **Clips:** 20 walk cycles, 30 to 60 s each, at 60 fps.
- **Skeleton:** Native UE5 Manny.
- **Root motion:** Not stated. The root is fixed at world 0 between the feet.
- **Capture:** Vicon mocap, full body with fingers. Rating 5.0 from 1.
- **Contents:** Confident, cautious and casual gaits, plus swagger, timid and energetic walks.
- **Why it fits:** It shows what "character in a walk" means. Swagger and confident are the relevant ones. It is not female-specific.
- **What it teaches us:** Posture as identity. Compare how spine and head carry differ between gaits at the same speed.
- **Caution:** Long clips mean you have to cut loops yourself.
- **Alternative:** [Walk Cycle Pack #02](https://www.fab.com/listings/b2f2e094-e98d-443f-8226-f8c4335692f9) (£21.56) is mostly exaggerated and impaired gaits. Skip it.

### 6. MoCap Central, MC Core Motion Animation Pack: £37.19
[Listing](https://www.fab.com/listings/9657e6b9-a480-43a8-a0f0-55abd806ce85)
- **Clips:** 659 across 30+ sets, male and female.
- **Skeleton:** Standard UE5 mannequin.
- **Root motion:** No-root-motion versions are included for the two locomotion sets. The default versions' status isn't stated.
- **Capture:** Studio mocap. Rating 4.7 from 27 (the structured data says 4.8 from 20).
- **Contents:** Idles with fidgets, conversation and gestures, waving and pointing, dancing, phone, jump start, fall and land, and Locomotion V2 with start, stop and pivot.
- **Why it fits:** One pack covers priorities 5 and 6 (jumps, gestures) and part of 7 (dance). It also has the best reviews of anything I found.
- **What it teaches us:** Jump anticipation, landing recovery and pivots. Pick the female sets and ignore the rest.

### 7. Ailive, Lie Down Animation Pack: £2.96
[Listing](https://www.fab.com/listings/622020b8-1c8d-4c42-978c-99bb6e69f442)
- **Clips:** 5. Based on Manny. Lightly cleaned mocap.
- **Contents:** Dozing, face down, side-lying, sprawling, stretching.
- **Why it fits:** It is cheap enough to try. It gives a first look at how mocap lies down. Check that "face down" and "sprawling" stay on the pin-up side.

## Optional

| Pack | £ | Notes |
|---|---|---|
| [Infectedfury, Lean Lay Sit Stand bundle](https://www.fab.com/listings/e86d4294-c9bf-46ef-9555-4f9f89e34135) | 52.07 | Claims 3,200+ clips: lean (~100), laying (900+), sitting (1,200+) and "Strike A Pose" (600+ with transitions). Skeleton, root motion and capture method are all **not stated**, with 1 rating. Potentially the only source of getting down and up, but I can't verify it. Read the preview and message the seller about the skeleton before buying. |
| [ITBortArt, Female Mocap Dance Vol.1](https://www.fab.com/listings/2358e5ac-abc7-4c39-aea7-34581f634208) | 14.87 | 20 dances (10 UE5). Skeleton "default UE5". Priority 7. Names are not listed. |
| [Mobility Pro, MoCap Online](https://www.fab.com/listings/52288b17-3c7f-44df-a5c8-3ca37692c288) | 74.39 | 223+ locomotion clips, but on the **UE4 skeleton**, so you need the IK retarget project. Rating 4.0 from 31. Expensive and redundant next to Gamma. |
| [Kalungis_Arcana, Quick Motion Happy Women](https://www.fab.com/listings/6c059932-d121-43b3-8766-eb489d85aec8) | 2.96 | 33 in-place clips: 7 happy poses, spins, waves, laughs. Too cheerful for the calm base, but a cheap source of the playful accents (a spin, a small wave). |
| [Ailive, Various Walk Pack](https://www.fab.com/listings/f03e7d7d-08df-40b7-ab8d-2ca94d111ec9) | 3.71 | 6 mocap walks on Manny, including a Swagger Walk. Cheap proud-gait reference. |
| [lumiflo, Self Cam Pose Bundle](https://www.fab.com/listings/c51003a2-275c-4946-a4c1-aba95f7be92c) | 59.51 | Only 8 clips (looking at the camera, hand gestures). Mocap, standard UE5 mannequin. It is about photo-mode gaze, but at £7 per clip it is overpriced. |
| [MC Seated Animation Pack](https://www.fab.com/listings/7c1ac5c5-9da0-4e47-b7a5-36ce1b55a21d) | 55.79 | 250+ chair clips from a male-led performance. Park Life covers the feminine side. |
| [XM3 Sitting Idle Pack #01](https://www.fab.com/listings/005be6a1-9e87-468b-8057-c44130ebab7a) | 21.56 | 20 long seated idles on Manny, not female-specific. |
| [HandMade, Female Locomotion Animations](https://www.fab.com/listings/3df5b4e7-5a62-4d05-b3a5-54d70f8cc9f0) | 7.43 | Walk/run/turn with root motion. Skeleton not stated and clip count not stated. Rating 4.6 from 7. Cheap test of "cute" walk. |

## Skip

- **Gamma Ultimate bundle** (£185.98, 1,451 clips): [listing](https://www.fab.com/bundles/62c3612b-8e7f-4d1a-8481-efdffd7d23a0). It is mostly combat, crouch and injured. Over budget and off-topic.
- **Gamma Pistol/Rifle/Crouch packs:** combat and stealth. We have no use.
- **Digital Kinetics Male and Female Locomotion bundle** (£96.71): [listing](https://www.fab.com/listings/607032cc-ecac-47ac-8342-c95c6735f736). Rating 3.0 from 1.
- **Female Horror Survivor** (£37.19) and **Lumiflo Two of Hearts** (£52.07): wrong mood and wrong use.
- **Anime Girl Standing** and similar pose packs: built on the UE4 mannequin and flagged as retargeting "approximately".

## Free and other sources

- **Epic Game Animation Sample (GASP):** free on Fab, 500+ mocap clips and motion matching, updated for 5.7. See [Epic's 5.7 post](https://www.unrealengine.com/tech-blog/explore-the-updates-to-the-game-animation-sample-project-in-ue-5-7). It is the study reference for starts, stops, pivots, slopes and stairs. Movement already splices MF_Unarmed clips from it.
- **Mixamo:** free with an Adobe account. A [third-party July 2026 guide](https://app.cinevva.com/ko/guides/free-character-animations-rigging) says it is up but in maintenance mode, and I found no statement from Adobe. Retargeting to Manny needs an IK Retargeter, and the free [mixamo2unreal](https://github.com/arbitrarygames/mixamo2unreal) repo was MetaHuman-only when I looked. Quality is generic, so it is fine for pose blocking and rough poses only.
- **MoCap Online** also has a free UE5 sampler. Its pages say stock UE5 IK Retargeters need their custom ones.
- **Not verified (could not load a price or contents):**
  - ActorCore/Reallusion. The [Female Sit Animation](https://marketplace.reallusion.com/female-sit-animation) page (chairs, sofas, bar stools, floor) is promising, but I could not read its price or whether it exports to the UE5 skeleton.
  - Rokoko/Kinetic.
  - Poses in Heels (a [Marketplace listing](https://docs.unrealengine.com/marketplace/en-US/product/poses-in-heels-mocap): 20 posing mocaps with the performer in heels). It returned 403 and may only be in the Launcher vault.
  - Digital Kinetics' 54-clip Female Locomotion Pack.

## Gaps

Floor transitions (priority 4) and stylish jumps (priority 5) are the weak spots on Fab: I found no single clean female pack for getting down and getting up. Park Life's stand-to-sit and the Infectedfury bundle are the only candidates. Expect to author these with our pose-wheel tooling, studying the sit clips for timing.

## Suggested order

| Week | Buy | Cost |
|---|---|---|
| 1 | Gamma Movement, Park Life, Girl Walk&Run, Female Idles, Lie Down | £95.18 |
| 2 | MC Core, XM3 Walk #01 | £58.75 |
| 3 | Dance Vol.1 (£14.87), then Infectedfury (£52.07) only after the skeleton is confirmed | £66.94 |

The "Strong" and "Optional" totals are roughly £120 and £260 if you bought everything. A plain Must-buy-only basket is £68.42.

## Content line

All picks above are fashion/pin-up-compatible. I flagged none as sexual. I excluded the sexual-themed material that appears in searches, and excluded "Characters - Stylized Modular Teenager" under character design below, since the listing is a minor.

## Character design study packs

Evidence here is thin; I could not verify topology or deformation quality from listings.

1. **[Stylized Girl Base Mesh, ArtStation](https://www.artstation.com/marketplace/p/8LNDe/girl-base-mesh-game-ready):** fully rigged to the Epic skeleton, 3 blend shapes. Price not read. Best match for "clean base on our skeleton".
2. **[3DScanStore UE5 Rigged Characters](https://www.3dscanstore.com/unreal-engine-stylised-characters/ue5-rigged-character-kat):** the page returned 403, but the search summary says the line is Manny-skeleton, UE 5.4+, built from scans for accurate anatomy. Price not read. Check for nudity before buying.
3. **Female Base Body 1 (ARKit 52), [RenderHub](https://www.renderhub.com/dzib3d/female-base-body-1-rigged-ue-character-arkit-52):** rigged to Manny with 52 face blendshapes. The base-body name implies nude, so check the content line first.
- **Rejected:** [Characters - Stylized Modular Teenager (£37.19)](https://www.fab.com/listings/15ff9a79-a7e3-41aa-ace4-28cac30d3914) fits the skeleton but the listing says teenager.

## What I could not verify

- Video quality of every pack.
- Any price for ActorCore, Rokoko/Kinetic and the character-design packs.
- Root motion on XM3, Park Life and Inu Games.
- Links were matched to names by fetch order; open each before paying and confirm the title.
