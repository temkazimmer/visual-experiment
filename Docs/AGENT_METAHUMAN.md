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
