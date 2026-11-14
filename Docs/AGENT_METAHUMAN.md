# MetaHuman body rebuild (branch `metahuman-body`)

Worktree `~/Documents/Unreal Projects/visex-metahuman`. Read `CLAUDE.md` and
`Docs/AGENT_PLAYBOOK.md` first.

## Goal (Tema, 2026-11-14)

"I want our character animated like metahuman with muscles contracting properly and looks
stunning. With proper hands and feet. But I want our animations and clothes to work
correctly." He chose route B: a real MetaHuman body inside our pipeline, rebuilt fully on
this branch. If it goes wrong we return to tag `pre-metahuman` (main 8992d06) and keep
what we learned, so **write findings down in the log at the bottom as you go**.

## Rules for this branch

- Nothing here merges into `main` until Tema approves a side-by-side.
- Keep the current body (`SKM_AndroidSkin`, its outfits, clips) working and untouched
  until the swap is approved. New work goes in new files and folders
  (`Content/Character/MH/`, `Tools/mh_*.py`), and the game picks the body by a cvar
  (`visex.Body.MetaHuman`), so both can be shot from the same camera.
- MetaHuman Creator is scriptable: see the engine's examples in
  `Engine/Plugins/MetaHuman/MetaHumanCharacter/Content/Python/examples/` and drive them
  through the bridge (`python3 Tools/ue.py`). Prefer scripts over hand clicks so every
  step can be re-run.
- Generated MetaHuman assets are large. Check sizes before committing; they go to LFS.
- Feet rule stands: one ground-IK owner, drift ≤1 cm, sole gap ≤0.5 cm at real speed.
- Review captures are 4K.

## Stages

1. **Her as a MetaHuman.** Create a MetaHumanCharacter asset, shape the body to her
   default proportions (measure `SKM_AndroidSkin` at default sliders), rig, textures,
   assemble. Done when she stands in `Lvl_Studio` next to the current body in one 4K shot.
2. **Animation.** Retarget Quinn clips onto the MetaHuman skeleton (bake once): walk,
   proud idle, menu lean first. Check which C++ layers in `VisexAnimInstance` find their
   bones by name. Done when walk and lean play in Lvl_Dust with foot numbers measured.
3. **One outfit.** Rebuild Silversuit on the new skin through `Tools/blender_outfit.py`
   (new skin FBX in, new fit data out). Done when it holds through walk, lean and a deep bend.
4. **Spike review with Tema**: 4K side-by-side stills and a short clip, foot numbers, and
   a list of what broke. He decides whether to continue.
5. Body page: bone-scale sliders against the corrective joints; morph sliders rebuilt
   from MetaHuman body variants (same topology, so a variant minus the base is a morph).
6. Remaining outfits, then hair refit to the new head.
7. Muscle flex layer (morphs by joint angle and load), tension wrinkles, face expressions
   for photo mode.

## Known before starting

- MetaHuman has no muscle simulation. Its deformation is corrective and twist joints
  driven by pose (RigLogic from the body DNA, LOD0-1 only). The flex layer in stage 7 is ours.
- MetaHuman Animator and identity conform are not available on Mac. Grooms are limited
  on Mac; we stay on hair cards.
- Unverified: whether the cloud steps (auto-rig, texture download) work on this Mac and
  need Tema signed in to Epic in the editor; whether Quinn clips play without a retarget.
- Outfits are cut from the skin mesh and `skinfit_*.npz` is per skin vertex, so all fit
  data is regenerated for the new mesh, never reused.
- Body LOD0 is about 30,500 vertices with 8 skin influences.

## Findings log

(date, what was tried, what happened, numbers, commit)

### 2026-11-14 · Stage 1 · her numbers (commit 0e4ef85, `Tools/mh_measure.py`)

Measured on the live skin (`Shell`, `SKM_AndroidSkin`, 227k render vertices) in a Play
session in Lvl_Studio with a fresh save, so every default slider is applied (actor scale
0.935 = 166 / 177.5). She was in the proud idle (contrapposto, arms down), so girths are
level slices of a slightly tilted torso: read them as ±2 cm. Lengths are joint to joint.

| cm | her (default sliders) | how |
|---|---|---|
| Height | 162.0 in the idle, **164.3 straightened** | crown to floor; ankle + shin + thigh + trunk |
| Shoulders | 26.6 joint to joint, 39.6 across the deltoids | `upperarm_l/r` |
| Bust | 93.3 | fullest tape below the armpits |
| Waist | 57.6 | narrowest tape, pelvis to bust |
| Hip | 98.1 | fullest tape above the crotch |
| Inseam | 76.4 | where the trunk splits into legs |
| Arm | 48.6 shoulder to wrist (24.7 + 23.8), hand 13.2 | joints; wrist to fingertip |
| Leg | thigh 41.8, shin 38.1, ankle 7.5 high | joints |
| Foot | 22.4 | heel to toe along the foot |
| Thigh / calf girth | 63.7 / 38.6 | 3 cm under the crotch / fullest |

The slider label says 166, but that is the scale of a 177.5 cm reference mesh; with her
default leg (0.98), neck (0.90) and head (1.04) scales she stands about 164.
The skin is on `SK_Mannequin` (Quinn), 181 bones, including our added toe bones.
`copy_mesh_from_component` cannot pose a Leader Pose follower ("Unable to pose the source
mesh"); asking for the render LOD reads the skinned mesh instead.

### 2026-11-14 · Stage 1 · her as a MetaHuman (done)

Shots (3840x2160, MetaHuman left, current body right, Lvl_Studio in Play, nothing saved
into the level): `Docs/Research/metahuman/stage1_full.png`, `stage1_hands.png`,
`stage1_feet.png`, plus `stage1_face.png`. Scripts: `Tools/mh_create.py` (create, shape,
look, rig, textures, build), `Tools/mh_stage.py` (the side-by-side), `Tools/mh_measure.py`.

**What was built.** `/Game/Character/MH/MHC_Her` (the Creator asset, 112 MB), copied from
preset Ada (placeholder face and hair), body driven by constraints, garment removed, pale
skin (`SKIN_UV`). Assembled with the Cine pipeline into `/Game/Character/MH/Built/MHC_Her`
(`BP_MHC_Her`, `Body/SKM_MHC_Her_BodyMesh`, `Face/SKM_MHC_Her_FaceMesh`, grooms; 517 MB)
and `/Game/Character/MH/Common` (shared skeletons, post-process AnimBPs, materials; 185 MB).
814 MB in 242 files.

**Same tape on both** (cm; the MetaHuman is the assembled body in its A-pose):

| | her | MetaHuman | | her | MetaHuman |
|---|---|---|---|---|---|
| Height | 164.3 | 165.6 | Inseam | 76.4 | 76.9 |
| Shoulder joints | 26.6 | 26.8 | Thigh / shin | 41.8 / 38.1 | 41.3 / 38.5 |
| Bust | 93.3 | 92.8 | Upper arm / forearm | 24.7 / 23.8 | 26.4 / 23.9 |
| Waist | 57.6 | 57.9 | Hand | 13.2 | 15.1 |
| Hip | 98.1 | 99.8 | Foot | 22.4 | 24.5 |
| Thigh / calf girth | 63.7 / 38.6 | 62.8 / 38.2 | | | |

The tape matches, the silhouette does not: the parametric body is a real person's, so at
the same girths it has a fuller ribcage, arms, knees and calves and a flatter waist-to-hip
curve than her sculpt. Hands and feet have no constraint and follow height (2 cm longer
than hers). Free constraints worth pinning next: Bicep, Forearm, Wrist, Knee, Underbust,
Neck, and the Fat / Muscularity axes.

**The Creator's tape is not ours.** Its Height reads 2.4 cm more than sole to crown (the
body carries its soles 2.4 cm above the origin), its Inseam about 6 cm less than crotch
height, its Hip about 6 cm less than our seat tape, and Upper Arm Length is shoulder point
to elbow (32.0 gives 26 cm joint to joint). `TARGETS` in `mh_create.py` holds the tuned
values. Ranges that clipped her: Waist minimum 55, Upper Arm Length minimum 25.

**Cloud on this Mac: both steps work.**
- First `request_auto_rigging` found no cached sign-in (`LogEOSAuth: No existing persistent
  auth credentials were found for automatic login.`) and opened Epic's Account Portal in
  the default browser (device code; the log repeated `errors.com.epicgames.account.oauth.
  authorization_pending` four times). The browser session completed it 44 s later
  (`LogMetaHumanAuth: User name is temazimmer`); nobody typed credentials from the agent
  side. After that the sign-in is cached: auto-rig (joints only) takes 3-5 s.
- `request_texture_sources` (2k face and body): 15 s. 4k/8k sources are a parameter.
- Run the first cloud call with `BLOCKING=False; KEEP=True`: a blocking call would freeze
  the editor behind the browser login.
- No offline path to an assembled character: `build_meta_human` refuses without the rig
  ("Character is not rigged.") and the downloaded textures. Offline you only get the
  Creator's preview actor (`spawn_meta_human_actor`), which is enough to measure and shoot.

**Skeleton.** Body: `metahuman_base_skel`, 342 bones. It has 170 of Quinn's 181 names,
including pelvis, spine_01-05, neck_01-02, head, clavicle/upperarm/lowerarm/hand, all
finger bones and metacarpals, thigh/calf/foot/ball, and the same ten toe bones per side
(`bigtoe_01_l` ... `littletoe_02_l`). Missing: `ik_foot_root/l/r`, `ik_hand_root/gun/l/r`,
`weapon_l/r`, `interaction`, `center_of_mass` (11). Extra: 172 corrective, twist and
finger-volume bones driven by `ABP_Body_PostProcess`. Face: `Face_Archetype_Skeleton`,
875 bones, 8 LODs. Body: 4 LODs, 30,455 vertices at LOD0, no morph targets.

**Traps.**
- `build_meta_human` leaves ~240 packages unsaved; `mh_create.py` saves the MH folder only.
- The first character came out with a deformed face (pursed lips, dented neck). It had
  been shaped four times, rigged without blocking and carried through Play sessions as a
  preview actor; the cause was not isolated. A clean create, shape, look, rig in one pass
  is fine, and so was a test copy. Rebuild rather than repair if it shows up again.
- In Play the assembled Blueprint's eyes were blank until LOD Sync was forced to 0: it
  follows the player's view, picked face LOD 2, and the eyes do not render there. The
  editor world shows them. Needs a proper answer in stage 2 (LOD settings, not a force).
- Each `spawn_meta_human_actor` call adds another preview actor; Play copies them all.
- Python cannot spawn into a running game world: place temporary actors before Play.
- A MetaHuman body mesh parks ~30 collapsed vertices on its origin; ignore them in bounds.

**Left for stage 1:** nothing blocking. Tema's calls: the face and hair (Ada's are
placeholders), and how far to push the body toward her stylised silhouette.
