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
- **Run the spike check on every pose before any capture goes to Tema.** `UnrealEditor visex.uproject
  /Game/VisEx/Lvl_Dust -game -visexqa -visexspikes -visexcvar=visex.Body.MetaHuman=1` walks the arrival,
  the lean, the Settings sit, every photo motion, the Start walk, the idle and height 145/185, and writes
  a line per pose to `Saved/QA/report.txt` (add `-visexshots=<name>` for a 4K frame of each). The TOTAL
  lines (body, face, and with an outfit on: garment, poke) must read `pass`, and look at any pose whose crease count jumps. `visex.MH.Spikes` checks the
  pose on screen. Any re-assembly, new body shape or new pose set: run it again.
- The project must keep `r.GPUSkin.Support16BitBoneIndex=True` (Config/DefaultEngine.ini). Assembling a
  MetaHuman without it silently drops skin weights (see the stage 2 log).

## Stages

1. **Her as a MetaHuman.** Create a MetaHumanCharacter asset, shape the body to her
   default proportions (measure `SKM_AndroidSkin` at default sliders), rig, textures,
   assemble. Done when she stands in `Lvl_Studio` next to the current body in one 4K shot.
2. **Animation.** Done differently from the first plan (no rebake, no graph port): today's
   character stays the driver and the MetaHuman body follows its final pose, behind
   `visex.Body.MetaHuman` (see the stage 2 log). Done when walk and lean play in Lvl_Dust
   with foot numbers measured.
3. **One outfit.** (worn and holding; two hems fail the poke check, see the stage 3 log) Rebuild Silversuit on the new skin through `Tools/blender_outfit.py`
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

### 2026-11-14 · Stage 2 · the MetaHuman body moves in the game (`visex.Body.MetaHuman`)

**How it follows.** No clip was rebaked and the graph was not ported. Today's character stays
the driver: its mesh (`SKM_Quinn_Simple`, never drawn) keeps running `ABP_Unarmed`, every C++
layer and the one ground-IK owner. With the cvar at 1, `UVisexMHBodyComponent` (made at runtime
by `UVisexBodyComponent`, `Source/visex/VisexMHBody.*`) spawns `BP_MHC_Her` on the driver's
origin, hides the skin, outfit, cloth and hair cards, and gives the MetaHuman's Body a native
anim instance with no graph (`UVisexMHBodyAnimInstance`). Each frame, after the driver (a tick
prerequisite), it copies the driver's component-space rotations onto the bones the skeletons
share by name and keeps the MetaHuman's own bone lengths. The mesh's post-process AnimBP
(`ABP_Body_PostProcess`: RigLogic twists and correctives) then runs on that pose, and the Face
copies the body in its own post-process (`ABP_Face_PostProcess` has a Copy Pose From Mesh on the
attached parent). At 0 nothing is spawned or loaded.

Why not the other routes:
- **Leader Pose**: a follower runs no post-process AnimBP, so no correctives.
- **The Blueprint's own options**: the assembled Blueprint has no live-retarget setting. Its Body
  has no anim class; `UMetaHumanComponentUE` only wires the post-process variables. The plugin
  ships `RTG_MH_IKRig` / `IK_MH_IKRig`, but a Retarget Pose From Mesh node needs an AnimBP (Python
  cannot add graph nodes), an IK Rig for our skeleton, and chains would lose our toe bones.
- **Copy Pose From Mesh as it is** copies translations too: the driver is a 177.5 cm skeleton
  scaled by 0.935 with bone scales on top, so the MetaHuman would be stretched to those lengths
  (its arm is 7% longer, its hand 13%).

What the copy does per bone (100 of the driver's 109 bones exist on the MetaHuman, parents agree):
- 56 take the driver's rotation as it is: the two skeletons use the same bone axes (reference
  poses differ by 0-20 degrees of A-pose, not by convention).
- 20 toe bones go through their reference-pose difference: our added toes have other axes (125-140
  degrees off).
- 8 metacarpals take the driver's change from its reference pose, so the palm keeps its own shape.
- 16 twist bones are left to RigLogic, like the 242 bones the driver does not have.
- root and pelvis also take the driver's position (times 166 / 177.5).

**The spike (Tema: "make sure we dont have this broken parts") and the blank eyes: one cause.**
A strip of skin shot out sideways from her hip in every pose. It was 27 vertices of the *right
wrist*, bound 100% to `root`: they stayed where the wrist is in the reference pose while the arm
moved, and the triangles between them and the wrist stretched into a strip (the hip is just where
the A-pose wrist hangs). Their mirror vertices on the left wrist hang on `wrist_outer_l`,
`lowerarm_twist_02_l` and `lowerarm_twist_01_l`; the right ones of those are bones 256, 257 and
258 of the skeleton. Stage 1 assembled the MetaHuman while the project had no
`r.GPUSkin.Support16BitBoneIndex` (a MetaHuman project requirement: it also asks for
`r.GPUSkin.UnlimitedBoneInfluences` and `SkeletalMesh.UseExperimentalChunking`). With 8-bit bone
indices a mesh section holds 256 bones; the assembly dropped influences past that (the body's one
section skins to 285 bones, the face's head to 639), and a vertex with nothing left falls back
to root. The same build chunked the face (875 bones) into
8 sections and wrote its LOD 1 and LOD 2 material maps two and one entries short, so those LODs
drew the eyeballs with the eye-shell and lash materials: the "blank eyes" of stage 1. One face
neck vertex (on `FACIAL_R_12IPV_NeckB4`) was stretching 8.7x the same way.

Not the cause (checked): no corrective or twist joint was far from its parent, the follow step
leaves no shared bone at the origin, there are no collapsed vertices on the origin in the
assembled body (0 within 0.5 cm), and the strip was there with the post-process AnimBP off.

Fix: `r.GPUSkin.Support16BitBoneIndex=True` and `SkeletalMesh.UseExperimentalChunking=1` in
`Config/DefaultEngine.ini`, then the same Creator asset re-assembled
(`STEPS=['build']` of `Tools/mh_create.py`; 105 files under `Content/Character/MH` rewritten).
After it: no vertex on root, face LOD maps `[9,1,2,3,4,5,10,7,8]` and `[11,1,2,3,4,5,10,7]`, eyes
drawn at LOD 0-3 with LOD Sync left alone (`Docs/Research/metahuman/stage2_eyes_lods.jpg`).
Meshes with 256 bones or fewer keep 8-bit indices, so today's body is not rebuilt.
`UnlimitedBoneInfluences` is still off: it changes the skinning path of every mesh, and the body
needs 8 influences at most. The face has more at LOD 0; turn it on when the face work starts.

**The spike check** (`Source/visex/VisexMHSpikes.*`, the rule at the top). It skins body and face
on the CPU at LOD 0 every frame and compares each triangle edge with its reference length:
`stretch` on edges of 1 cm or more (fails past 3x), `creases` on edges of 0.25-1 cm (reported),
`grew` in cm on any edge (fails past 3 cm). Body, worst frame of each pose:

| pose | before: stretch | after: stretch | after: creases | after: grew |
|---|---|---|---|---|
| arrival walk | 208x | 1.75x | 2.29x | 0.82 cm |
| menu lean | 198x | 1.50x | 1.75x | 0.60 cm |
| Settings sit | 243x | 2.38x | 4.06x | 1.66 cm |
| photo Idle / Walk / Run | 157x / 135x / 134x | 1.50x / 1.75x / 2.08x | 2.26x / 2.55x / 3.40x | 0.59 / 0.91 / 1.30 cm |
| photo Bookend / Crown / FeetUp | 121x / 224x / 401x | 1.88x / 1.92x / 2.10x | 2.74x / 4.39x / 4.39x | 0.92 / 1.45 / 1.45 cm |
| photo Glance / Ledge / LegLift | 407x / 358x / 381x | 2.30x / 2.84x / 2.45x | 3.71x / 5.83x / 4.68x | 1.56 / 2.21 / 1.79 cm |
| photo Mermaid / Peace | 243x / 257x | 2.15x / 2.15x | 3.81x / 3.81x | 1.38 / 1.38 cm |
| Start walk | 198x | 1.74x | 2.07x | 0.78 cm |
| proud idle | 157x | 1.48x | 1.98x | 0.55 cm |
| height 145 / 185 | 158x / 151x | 1.47x / 1.46x | 2.08x / 1.87x | 0.54 / 0.54 cm |

"Before" is the first run (edges of 0.02 cm and up in one number, the strip 36-122 cm long).
Face after: stretch 1.35x, creases 2.56x, grew 0.79 cm, every pose. The crease numbers over 3x
are short edges folded into the groin and under the seat (`pelvis`, `thigh_bck_lwr`) in the sits
and leg lifts, and in the armpit (`upperarm_twistCor_01`) with the arms up: 0.26-0.43 cm edges
opening to 1.3-1.9 cm. Looked at in 4K (`-visexshots`), nothing pokes out. They are the
MetaHuman's own skinning in a deep bend, not ours to fix in the follow step.

**Feet.** Copied rotations on other bone lengths leave each ankle within a centimetre of where
it should be, and the MetaHuman's ankle sits higher over its sole (7.69 cm against the driver's
7.26) with a foot 0.6 cm longer to the ball. Fixed in the follow step, with no second ground
pass: each MetaHuman foot is put where its sole meets the driver's sole (one fixed offset in
the foot bone's axes, halfway between matching under the ankle and under the ball, so it adds
no slide of its own), the leg is closed onto it with a two-bone solve, and the pelvis is lifted
by the ankle difference less what the hip joints already sit higher (-0.57 cm) plus the live
leg-length difference. It reads no ground and traces nothing: the driver's Control Rig stays
the one ground-IK owner and this repeats its answer on the other body. `visex.Body.MetaHuman.
FootPin 0` switches it off to compare. The body's soles are 3.4 cm over its own origin (lowest
vertex under the foot; stage 1's 2.4 was the Creator preview), measured from the mesh at start.

Measured by the gaps tour (`-visexqa -visexgaps -visexcvar=visex.Body.MetaHuman=N -benchmark
-fps=60`: a fixed 60 fps step, so both runs walk the same steps; hip bob is identical). With the
cvar on the foot meter reads the MetaHuman body, at 0 the driver's mesh as before.

| cm | cvar 0 | cvar 1 |
|---|---|---|
| arrival walk: drift median / max | 2.87 / 6.21 | 2.80 / 6.21 |
| arrival walk: sole gap median | -0.16 | -0.32 |
| menu lean: drift | 0.31 | 0.32 |
| menu lean: sole vertex gap L / R | 0.19 / 0.19 | 0.35 / 0.27 |
| Start walk: drift median / max | 2.17 / 2.60 | 1.49 / 2.69 |
| Start walk: sole gap median | 0.76 | 0.59 |
| proud idle: drift | 0.36 | 0.38 |
| proud idle: sole vertex gap L / R | 0.19 / 0.18 | 0.24 / -0.09 |

Standing (lean, idle) is inside the targets on both bodies (drift 1 cm, gap 0.5 cm). The walks
are not, on either: 2.2-2.9 cm median drift is today's walk, and the MetaHuman body repeats it
(the same on the arrival, less on the Start walk, where its longer foot plants 16 steps for
13). That is the driver's to fix (root motion and the Control Rig), not the follow.

**Correctives are live.** `stage2_correctives_on.png` / `_off.png` (4K, same camera): the
reference pose with a deep knee and hip bend and the arm overhead
(`visex.Body.MetaHuman.TestPose "ref;calf_r:0,0,-130;thigh_r:0,0,90;upperarm_r:0,120,0"`),
post-process AnimBP on and off (`visex.Body.MetaHuman.Correctives`). Off, the seat goes flat,
the thigh pinches where it meets the hip and the knee and shoulder thin out; on, they keep
their volume. So the follow feeds RigLogic the way a normal anim graph would.

**Captures** (`Docs/Research/metahuman/`, 3840x2160, game view with the grade, same camera and
the same frame of the fixed-step tour): `stage2_lean_cvar0/1.png`, `stage2_walk_cvar0/1.png`
(mid-stride, 4 s after Start), `stage2_idle_cvar0/1.png`; the corrective pair and
`stage2_eyes_lods.jpg` (LOD 0-3) come from a scene capture, so without the grade;
`stage2_walk_lean.mp4` (22 s, 720p): she walks in, leans, goes to the Body page and back with
the cvar on (the feet tour's film mode).

**Frame time.** Not separable at this size: in the real-time tours (1280x720 window, 2 s per
phase) the idle ran 71 fps with the cvar off and 72 on, the lean 56 and 70. At 0 she wears the
Silversuit and hair cards, at 1 the MetaHuman with strand grooms, and the foot meter skins a
mesh on the CPU every frame in both. A real number needs a GPU capture at 4K.

**Traps.**
- A `-game` run of this project answers the folder bridge too (`init_unreal.py` loads there):
  with an editor open in the same folder, jobs go to whichever picks them up first.
- `set_editor_property` on a component of a spawned Blueprint actor re-runs its construction
  script in Play and replaces its components. Hold LODs with `visex.Body.MetaHuman.LOD`.
- An editor killed with unsaved packages shows a modal "Restore Packages" at the next launch
  and the bridge never comes up (frame 0 forever). Move `Saved/Autosaves/PackageRestoreData.json`
  aside before launching; stage 1's wanted to restore engine presets and Lvl_Studio.
- `export_render_target` writes PNGs that `sips` reads and `ffmpeg` rejects; re-save with `sips`.
- The spike ratio on its own is noise below about 0.25 cm of edge: 0.02 cm slivers read 30x.
- Console variables keep their value across Play sessions in one editor (a leftover TestPose).

**Left open.**
- The cvar path loads the Blueprint by path, so it works in the editor and `-game`, not in a
  packaged build until `/Game/Character/MH` is cooked (800 MB; not added to the DMG).
- With the cvar on: no outfit, no hair cards, no body sliders except Height (it scales with the
  driver); Ada's face and groom, and the MetaHuman's baked underwear. Hands land 1-2 cm off the
  driver's contacts (its arm is longer): the lean's hand on the column is not corrected.
- `r.GPUSkin.UnlimitedBoneInfluences` (face quality at LOD 0) and the walks' foot drift.
- Bone scales from the Body page are not passed on: the copy takes rotations only.

### 2026-11-14 · Shape pass · "slimmer, keep realism" (captures `stage3_*`)

Tema looked at `stage1_full.png` and chose **slimmer, keep realism**: same bust, waist and hip,
lighter limbs and ribcage, inside what the Creator's own controls allow, no sculpt on top.

**What the Creator exposes** (`mh.get_body_constraints`, 30 controls; value in cm unless said):
Height, Chest, Bust Span, Underbust, Waist, Rise, Inseam, Hip, High Hip, Neck Base, Neck, Thigh,
Knee, Calf, Front Interscye Length, Upper Arm Length, Lower Arm Length, Bicep, Elbow, Wrist,
Neck to Waist, Across Shoulder, Shoulder Height, Shoulder to Apex, Neck Length, Forearm,
Hand Circumference, and three axes: Fat (-2.8..2.8), Masculine/Feminine (-2..2), Muscularity
(-2.3..2.3). There is no ankle, no hand length and no foot length. Nothing else in the Python
API shapes the body (`conform_body` takes a mesh, which is the sculpt we are not doing).

**Chosen** (`TARGETS` in `Tools/mh_create.py`; free value it replaces in brackets): Bicep 21.5
(25.5), Forearm 19.3 (20.1), Wrist 12.8 (13.5), Knee 30.5 (32.4), Underbust 66.5 (67.7), Bust
Span 20.5 (pinned where it sat), Muscularity -1.0 (-0.6), Hand Circumference 17.0 (19.2), Thigh
60.0 (was 61.0), Calf 37.6 (was 38.2), Chest 93.8 and Hip 92.5 (retuned so bust and hip read
hers). Height, Waist, Inseam, shoulders and arm lengths as in stage 1.

Tried and left out:
- **Fat** below where the model puts it (-0.2): at -0.8 the knees and wrists lose ~1 cm but the
  volume goes into the seat (hip tape +1) and the collarbones hollow; Neck Base falls to 28-31,
  under the Creator's own minimum of 33. Left free.
- **Masculine/Feminine** at 1.7 (free 1.1): feet 0.2 cm shorter, shoulders 0.7 cm narrower,
  the neck thins the same way. Not worth a pin.
- **Underbust lower than 66.5.** With the bust tape held, a smaller ribcage is a bigger cup: at
  65 and 63 the bust stands out further in profile each step and stops reading as a real body.
  Free Bust Span closes the gap between the breasts (18.6), so it is pinned.
- **Her exact thigh and calf** (63.7 / 38.6: Thigh 62, Calf 38.6): the leg came out heavier than
  stage 1 (mid-thigh 59.3). The values above sit 2 cm and 1 cm under her tape, inside the
  +-2 cm the idle pose costs her own reading (61.2 the day this was measured).

**Tape** (`Tools/mh_measure.py`; cm; "her" is the stage 1 reading, limb girths read today in the
idle; the tape now measures limbs square to the bone and takes `WHO='bp:<Blueprint>'`):

| | her | MetaHuman before | MetaHuman now |
|---|---|---|---|
| Height | 164.3 | 165.6 | 165.7 |
| Bust / underbust | 93.3 / 59 | 92.8 / 65.5 | 93.1 / 62.8 |
| Waist | 57.6 | 57.9 | 57.9 |
| Hip | 98.1 | 99.8 | 98.7 |
| Inseam | 76.4 | 76.9 | 77.0 |
| Thigh (3 cm under the crotch) / mid-thigh | 63.7 / 56.0 | 62.8 / 58.5 | 61.8 / 57.4 |
| Knee / calf / ankle | 33.5 / 38.6 / 20.3 | 32.9 / 38.2 / 21.3 | 31.1 / 37.6 / 21.2 |
| Upper arm / forearm / wrist | 23.0 / 22.2 / 13.8 | 27.7 / 21.9 / 14.1 | 23.7 / 21.0 / 13.3 |
| Hand / foot length | 13.2 / 22.4 | 15.1 / 24.5 | 14.9 / 24.6 |

The upper arm was the big one (4.7 cm over hers), then the ribcage. By the tape the old knees
and calves were already hers or under; what read heavy there is a real leg's shape against her
sculpt's long taper, and the hips' breadth (the model puts a 98 cm hip into width, hers is rounder
front to back). That part is not reachable with these controls.

**Hands and feet.** Length has no control. Hand Circumference narrows the palm (19.2 to 17.0
looks right, `stage3_hands.png`) and takes 0.2 cm off the length; down to 15 it takes 0.5. Feet
have no control at all and stay 24.6 (hers 22.4): they follow Height, which is hers already.
Closing those gaps means a bone scale on hand and foot in the follow step, not the Creator.

**One clean pass, and why.** `STEPS=['fresh','create','shape','look','rig','textures','build']`
(blocking, 2 minutes, sign-in cached). `fresh` deletes the Creator asset so `create` copies the
preset again. A character shaped more than once does not come back the same:
- the solve starts from the state it is in: the same targets gave bust 92.8 to 93.8 and
  underbust 62.8 to 63.6 depending on what was tried before;
- after several shapes the head stops following the body: grey flaps stood up from both
  shoulders and a dark seam ran across the collarbones (the scratch copy `MHC_Try`, seen in the
  preview). That is the likely cause of stage 1's deformed face. The clean pass has neither.
Shapes were tried on a scratch copy (`CHAR_NAME='MHC_Try'`, deleted since), never on `MHC_Her`.

**Spike check after the re-assembly** (`-visexqa -visexspikes -visexshots=s3`, cvar on; both
TOTAL lines `pass`; body, worst frame of each pose; last run's crease number in brackets):

| pose | stretch | creases | grew |
|---|---|---|---|
| arrival walk | 1.60x | 2.20x (2.29) | 0.72 cm |
| menu lean | 1.50x | 1.83x (1.75) | 0.60 cm |
| Settings sit | 2.37x | 4.25x (4.06) | 1.63 cm |
| photo Idle / Walk / Run | 1.49x / 1.74x / 2.29x | 2.29x / 2.75x / 4.46x (2.26 / 2.55 / 3.40) | 0.58 / 0.91 / 1.54 cm |
| photo Bookend / Crown / FeetUp | 1.86x / 1.92x / 2.09x | 2.74x / 3.62x / 3.70x (2.74 / 4.39 / 4.39) | 0.93 / 1.24 / 1.30 cm |
| photo Glance / Ledge / LegLift | 2.29x / 2.71x / 2.44x | 3.80x / 6.55x / 4.87x (3.71 / 5.83 / 4.68) | 1.54 / 2.04 / 1.77 cm |
| photo Mermaid / Peace | 2.14x / 2.14x | 3.93x / 3.93x (3.81 / 3.81) | 1.37 / 1.37 cm |
| Start walk | 1.73x | 2.07x (2.07) | 0.76 cm |
| proud idle | 1.43x | 2.00x (1.98) | 0.54 cm |
| height 145 / 185 | 1.42x / 1.41x | 2.03x / 1.88x | 0.52 / 0.51 cm |

Face: stretch 1.35x, creases 2.60x, grew 0.78 cm. The same edges as before lead every line
(`pelvis` and `thigh_bck_lwr` in the groin and under the seat, `upperarm_twistCor_01` in the
armpit); the slimmer thigh folds a little deeper (Ledge 0.26 to 1.69 cm, Run up from 3.40x).
Each pose over 3x looked at in its 4K frame (`Saved/QA/s3_*.png`), shadows lifted where the
frame is backlit:
- **Settings sit, Ledge, LegLift, Mermaid, Peace**: hip and groin folds close cleanly, the seat
  keeps its volume, nothing pokes out or pinches.
- **Crown** (arms overhead): the armpit folds cleanly; the baked top's edge waves there (texture).
- **Run**: clean; the near hand brushes the hip (the MetaHuman's arm is 1.7 cm longer than the
  driver's: pose).
- **FeetUp**: clean; the chest sinks a little into the sand (pose against the dune).
- **Glance**: clean; a heel shows between waist and arm (the pose).
Nothing was a build or setting problem. What is shape: with the ribcage in, the bust reads
large in profile (Ledge, Crown); Underbust is the one value to trade if Tema finds it too much.

Feet on the new body (gaps tour, fixed 60 fps, cvar on): lean drift 0.32 cm, sole vertex gap
L 0.33 R 0.26; idle 0.38, gap L 0.23 R -0.11; Start walk drift median 1.50 max 2.69; arrival
2.80 / 6.21. The same as stage 2: the foot pin measures the soles from the mesh at start.

**Underwear (for the outfit stage; nothing was switched).** The Creator has one switch:
`character.skin_settings.skin.show_top_underwear` ("Show Top Underwear", on by default). Off, the
top goes and the chest is bare skin with no detail (tried on the scratch copy's preview). There
is no switch for the briefs: `FMetaHumanCharacterSkinProperties` holds only the tone (U, V), the
body and face texture index, roughness and that one flag. In the Creator the underwear is a
layer (bare base colour, underwear base colour and normal, one mask: `EBodyTextureType`), but
the assembly bakes it into `Body/Baked/T_Body_BC_VT` and `T_Body_N_VT`, and `MI_Body_Baked_VT`
has no underwear parameter left. So: outfits that cover the hips need nothing; anything cut
lower than the briefs needs the top switched off before assembly and the briefs painted out of
the baked base colour and normal (or a bake of our own from the bare layer). Not tried.

**Traps.**
- Deleting the Creator asset in an editor session that has assembled it leaves the object in
  memory: `create` then finds it "existing" and the edit fails. `fresh` now stops with "restart
  the editor"; after the restart the same STEPS run through.
- Shape and shoot in separate bridge jobs: the preview actor's render mesh updates a frame
  after `commit_body_state`, so a capture in the same job shows the previous shape (the tape
  reads the new one).
- A relaunched editor logged `Cmd: QUIT_EDITOR` two minutes in, right after an assembly (13:52;
  the assets were saved). The one quit job this session sent went to the process before it,
  which took it; where the second came from was not found (Cmd+Q in the window logs the same
  line). Jobs sent after it sat in `Tools/.bridge/in` and would have run in the next process to
  open this folder, the `-game` tour included: clear that folder before every launch.
- The tape's `thigh` reads 0.0 when the thighs touch at the slice (one loop, not two).

**Captures** (`Docs/Research/metahuman/`, 3840x2160): `stage3_slim_full.png` (new MetaHuman
left, current body right, stage 1's camera), `stage3_slim_before_after.png` and
`stage3_slim_before_after_side.png` (old shape left, new right, same camera; the side pair is
brightened), `stage3_hands.png` (both hands, 3/4 from in front and outside, `NAME='hand_l'` /
`'hand_r'` in `Tools/mh_stage.py`), `stage3_lean.png` (menu lean in Lvl_Dust, game view with
the grade, cvar on).

**Left open.** Hands 1.7 cm and feet 2.2 cm longer than hers (bone scale in the follow step, if
wanted). Hip breadth and the leg's real shape against her taper. Bust size in profile
(Underbust). Ada's face and groom are still placeholders.

### 2026-11-14 · Stage 3 · Silversuit on the MetaHuman body (captures `stage3_suit_*`)

With `visex.Body.MetaHuman 1` and the Silversuit worn, she wears a Silversuit cut on the MetaHuman
body: same cut, same material instances. Any other outfit shows nothing yet; with the cvar off
nothing changed. Commits c95dc38 (tools), fb6d1f8 (C++), 35b51f6 (assets) and the three after them.

**The pipeline survives.** `Tools/blender_outfit.py` already measures everything on the skin it is
given (landmarks from joints and surface, fields in cm), so the MetaHuman body became a second skin
source behind `--body=mh` instead of a fork. The old body's outfits build as before: same sizes and
vertex counts, and the FBX differs from the old script's only in the ~6 KB that differ between any
two runs of the same script (checked on Silversuit and Noir, with a control run).

    python3 Tools/ue.py Tools/mh_skin_export.py                         # body LOD 0 -> SourceArt/Character/MH/mh_skin.fbx
    Blender -b --factory-startup --python Tools/blender_outfit.py -- SourceArt/Character/MH/mh_skin.fbx \
        SourceArt/Character/MH/Outfits [<preview_dir>] --only=Silversuit --body=mh
    python3 Tools/ue.py -c "OUTFIT='Silversuit'; exec(open('<project>/Tools/mh_outfit.py').read())"

What `--body=mh` does differently (`Tools/mh_outfit_body.py`, exec'd after the import):
- **The export** comes in metres under a 0.01 empty, split along UV seams (32,334 vertices), with
  UVs in tile 1 (u 1..2). It is flattened to cm, welded to 30,455, and each triangle split in four
  (121,725 vertices, ~0.36 cm apart): the script's smoothing passes and kit fields were tuned on a
  0.27 cm skin and round every opening off at 0.7 cm.
- **Helper joints.** The skin hangs on `thigh_bck`, `calf_knee`, `upperarm_in`, `wrist_inner`,
  `ankle_bck`, `clavicle_pec` ... and not on `thigh`, `calf`, `upperarm`. The script's substring
  groups catch most; added: `wrist_*` is arm, `ankle_*` leg, and `clavicle_pec` / `clavicle_scap`
  are chest and back, not arm (they would have cut the bust out of every torso mask).
- **No morphs.** The bust apex is found on the surface (the most forward chest skin per side) and
  stands in for the Bust morph's reach; there is no design morph set, no fit solve, and the garment
  has no shape keys. The crotch is the lowest centre-line skin that faces down (this body's inner
  thighs meet below it), and the floor is the lowest vertex (its soles are 3.5 cm over the origin).
- **Reference pose kept.** The Blender round trip leaves the 342 bones where they were (worst
  0.0003 cm, 0.14 degrees against the body mesh), which Leader Pose needs.

**Pattern transfer: by landmark.** Silversuit's openings are Figma shapes drawn over the old body's
T-pose views (`OutfitKit/Silversuit/figma.json`). `--kit-ref` on the old skin writes its landmarks
to `OutfitKit/views/ref_profile.json`; on the MetaHuman, `mh_kit_tpose()` raises its arms the same
way and maps each height band onto the old body's (this body -> old, cm): floor 3.5 -> 0, knee
49.8 -> 49.8, crotch 79.2 -> 83.4, pelvis 92.9 -> 98.7, waist 104.0 -> 109.8, bust apex 124.1 ->
128.8, shoulder joint 135.9 -> 142.8, neck 143.6 -> 151.2, with widths scaled by the silhouette's
half-width at each band (1.00 at the hip, 1.07 waist, 0.99 bust, 1.18 at the shoulder joints) and
depths by its front and back. The drawn shapes are then projected exactly as on the old body, so
each hem keeps its place between the same two landmarks: the scoop neck inside the shoulder joints,
the keyhole between and under the bust apexes, the hip cutouts from under the ribs to the hip crease,
the belt (its height mapped back the same way) across them. Ankle and wrist hems come from joints
and weights as before (5 cm over the ankle joint; where the hand's weight starts). Checked against
`OutfitKit/Silversuit/build_front_close.png` in the Blender preview and in the game frames.

**Weights and the follow.** Fabric keeps all 8 influences of the skin it was cut from, helper joints
included, and `SKM_Silversuit_MH` (78,058 vertices, one LOD, on `metahuman_base_skel`, legacy FBX
importer at 0.01) follows the Body by Leader Pose through `UVisexShellComponent`: the leader's final
bones are the post-process AnimBP's output, so the fabric gets the twists and correctives the skin
gets. `UVisexMHBodyComponent::ApplyOutfit` loads `SKM_<Outfit>_MH` and `MI_Body_<Outfit>` by name from
`/Game/Character/MH/Outfits` when the outfit changes; no asset, nothing worn. While a garment is on,
the figure is held at LOD 0 (`ForcedLOD`): the garment has one LOD and is skinned to joints the body
stops evaluating past LOD 1. That also holds the face and grooms at LOD 0 (not measured in ms).

**Skin under the fabric: clipped, not fitted.** There are no fit morphs on this body, so the tool
bakes `cover_<outfit>.png`: the body's UV0, white where an opaque panel covers the skin by more than
`COVER_MARGIN` (0.5 cm in from every edge). `mh_outfit.py` copies the skin material and its two
instances into `/Game/Character/MH/Outfits` (`M_Body_Under`, `MI_Body_Under`, `MI_Body_<Outfit>`;
the assembled originals are not edited), sets the copy to Masked and feeds `1 - Cover Mask` to the
opacity mask. Under a panel there is then no skin to poke through, in any pose and at every LOD, and
the fabric casts the shadow instead. Why not the alternatives: a body mesh with the covered
triangles removed would lose the DNA user data RigLogic reads (no correctives); pushing the skin
in needs morph targets the MetaHuman body does not have. Fabric offsets are the old ones (0.25 cm
panel, 0.5 cuffs, 0.3-0.6 belt), each opaque edge with its rim down to the skin.

**Baked underwear: painted out for outfits.** The Silversuit's scoop neck, keyhole, back and hip
openings showed the grey top and briefs. `Tools/mh_bare_skin.py` exports the baked base colour,
normal and SRMF maps, fills the underwear from the skin around it and imports 4096 copies
(`T_Body_*_Bare`, `T_Head_*_Bare`); every `MI_Body_<Outfit>` uses the body set, and while an outfit
is worn the face mesh's slot 0 gets `MI_Head_Bare` (the head mesh's chest part carries the top's
shoulder straps). The body's mask ships with the plugin in the same UV layout
(`/MetaHumanCharacter/Textures/Shared/1K/T_Underwear_M`, red and green); the head has none, so its
straps are found by colour (grey texels in the lowest eighth of the map). The filled skin is smooth:
no nipples, no navel detail beyond what the surrounding skin carries, which is fine under a suit
and not enough for an outfit that bares the chest or hips fully. With no outfit she still wears
the baked underwear. Better at the next assembly: `skin.show_top_underwear = False` (Epic's own bare
chest on body and head), and keep this fill for the briefs only.

**Checks.** `-visexqa -visexspikes -visexcvar=visex.Body.MetaHuman=1 -visexshots=<name>` now measures
three meshes (body, face, garment) and adds a poke line. Poke: every fabric vertex against the skin
triangle it was cut over, in the pose on screen (CPU skin, LOD 0); "at a hem" is fabric within
0.8 cm of a cut edge, where the skin is still drawn, and fails past 0.1 cm under the skin; further
in the skin is clipped, so depth there is only how far the fabric left the skin's surface.
Body and face read as in the shape pass (TOTAL pass: body stretch 2.83x, creases 6.24x, grew
2.18 cm; face 1.35x). Garment and poke, worst frame of each pose (38,252 fabric vertices compared):

| pose | garment stretch / creases / grew | at a hem: vertices under, deepest | under clipped skin |
|---|---|---|---|
| arrival walk / Start walk | 1.24x / 2.37x / 0.48 cm | 0 | 1.04 cm (armpit) |
| menu lean | 1.18x / 2.04x / 0.34 cm | 0 | 0.36 cm |
| Settings sit (down, sit, up) | 1.54x / 3.74x / 0.91 cm | 29, 0.19 cm on `pelvis` | 1.09 cm |
| photo Idle / Walk / Run | 1.24x / 1.26x / 1.43x; grew 0.32 / 0.37 / 0.70 | 0 | 1.04 / 1.01 / 0.87 cm |
| photo Bookend | 1.22x / 2.08x / 0.50 cm | 7, 0.46 cm on `hand_l` | 0.65 cm |
| photo Crown | 1.31x / 3.23x / 0.81 cm | 26, 0.46 cm on `hand_l` | 0.46 cm |
| photo FeetUp | 1.38x / 3.25x / 0.81 cm | 0 | 1.52 cm |
| photo Glance | 1.54x / 3.60x / 0.85 cm | 13, 0.08 cm on `pelvis` | 1.52 cm |
| photo Ledge | 2.07x / 4.86x / 1.24 cm | 124, 0.25 cm on `pelvis` | 0.91 cm |
| photo LegLift | 1.54x / 3.96x / 0.99 cm | 105, 0.22 cm on `pelvis` | 1.52 cm |
| photo Mermaid / Peace | 1.42x / 3.38x / 0.78 cm | 0 / 1 (0.03 cm) | 1.29 / 1.33 cm |
| proud idle, height 145 / 185 | 1.19x / 2.09x / 0.31 cm | 0 | 1.05 cm |

TOTAL garment: pass (2.07x, no edge over 3x, grew 1.24 cm). **TOTAL poke: FAIL**, on two hems: the
hip cutout's edge where it crosses the hip crease in the sits and deep leg lifts (0.17-0.25 cm
under, up to 124 vertices), and the left wrist cuff with the wrist bent hard in Bookend and Crown
(0.46 cm, 7-26 vertices). Narrowing the cover margin from 1.0 to 0.5 cm did not change them: they
are within 0.8 cm of the edge itself. The armpit numbers are the split triangles (see Traps), under
clipped skin in every pose, the standing ones included.

**Seen in the 4K frames** (`Saved/QA/s8_*.png`, `g3_*.png`; game view, grade on):
- **Menu lean**: reads as today's Silversuit from the same camera (`stage3_suit_lean.png` against
  `stage3_suit_lean_old.png`): scoop neck, keyhole, hip cutout, belt and buckle, cuffs. Clean.
- **Walk, mid-stride (from behind)**: back and hip openings hold their shape, belt level, ankle
  cuffs follow. Nothing slides. The chrome's reflections smear into dark patches as on today's suit.
- **Settings sit**: clean where it shows (her near foot hides the hips from that camera).
- **LegLift** (the deepest bend): the suit folds with her, no skin through a panel, no spikes.
- **Ledge**: clean; at 4K the belt's lower edge and the back cutout's edge show a thin dark second
  line beside the binding: a hem detail, not a hole. Its cause was not checked.
- **Mermaid**: clean; the side cutout's edge has a few small teeth on her right flank (cause not
  checked; the drawn shape passes from the front view to the side view about there).
- **FeetUp**: clean, back and hip openings smooth. Her chest sinks into the sand as before (pose).
- Not seen: the wrists in Crown and Bookend are outside the photo camera's frame, so the cuff's
  0.46 cm is a number only.

**Captures** (`Docs/Research/metahuman/`, 3840x2160, Lvl_Dust, game view): `stage3_suit_lean.png`,
`stage3_suit_walk.png`, `stage3_suit_bend.png` (LegLift), all cvar on; `stage3_suit_lean_old.png`
(cvar off, same camera); `stage3_suit_walk_lean.mp4` (23 s, 720p: walk in, lean, Body page and back,
cvar on; shot before the head's straps were painted out, so they show at the neckline there).

**Assets.** All new, under `/Game/Character/MH/Outfits`: `SKM_Silversuit_MH`, `T_Cover_Silversuit`,
`MI_Body_Silversuit`, `M_Body_Under`, `MI_Body_Under`, `T_Body_{BC,N,SRMF}_Bare`,
`T_Head_{BC,N,SRMF}_Bare`, `MI_Head_Bare`. No existing `.uasset` was modified; Silversuit's material
instances are referenced, not changed.

**The remaining outfits: recipe and estimate.** Per outfit, automatic (about 5 minutes of machine
time): `blender_outfit.py --body=mh --only=<Name>` (cut, weights, cover mask, 2 minutes),
`mh_outfit.py` with `OUTFIT='<Name>'` (import, materials, body material; add the outfit's old mesh
path to `OLD_MESH`), then the spike tour with the outfit saved as worn (3 minutes, garment and poke
lines). Re-export the skin (`mh_skin_export.py`) and re-run `mh_bare_skin.py` after any re-assembly.
What needs a person:
- **Catsuit**: the same builder as Silversuit without the kit; expect 20 minutes, mostly looking.
- **Opaque sets (Mirror, Harness, Cocoa)**: 30-60 minutes each. `cups()`, `bra()`, `band(TORSO)` and
  the strap rays have not run on this body: the bust is larger with a real cleavage, the cup extents
  come from the 9 cm stand-in, and `TORSO` now reads the MetaHuman's helper joints. Check each piece.
- **Sheer, lace, net, nylon (Bodystocking, Sheer, Noir, Garter, Fishnet)**: 1-2 hours each. The cover
  mask only clips under opaque fabric, so under a translucent panel the skin stays and only the
  0.1-0.25 cm offset keeps it in. Expect to raise offsets or add a clearance pass per pose. They
  also show the skin itself: the filled-in underwear area has no anatomy (see above), so these need
  the top switched off at assembly, and a decision on the briefs area.
- **Kit strokes (KitTartan) and Tartan's wrapped straps**: `sketch.json` holds 3D lines on the old
  skin; they need the same landmark map as the shapes (`mh_kit_tpose` gives it per vertex, not yet
  per point). Half a day of tool work, once.
- **The skirt (cloth)**: a new Chaos cloth asset and capsules on the MetaHuman skeleton. Half a day.
- **Body sliders and fit morphs**: none on this body yet (stage 5). Outfits are cut on her default
  shape only.

**Traps.**
- The body's UVs sit in tile 1 (u 1..2): a bake that assumes 0..1 writes an empty map.
- `FString::Printf` needs a literal format: build asset names by concatenation.
- The Blender preview camera is off-axis: on this bust the keyhole looks lopsided there and is not.
- Measuring poke-through against the nearest skin vertex's plane reads centimetres deep in every
  crease. Compare each fabric vertex with the skin triangle it was cut over (barycentric), and match
  by bone so the other side of a fold is not picked.
- Splitting the skin's triangles has a cost: a new fabric vertex blends its two corners' weights, and
  where those differ a lot (armpit, hip crease, a bent wrist) it leaves the skin's surface by up to
  1.5 cm. Under clipped skin that is unseen; at a hem it is the poke numbers above.
- A `-game` tour appends to `Saved/QA/report.txt`: read the last block, not the first TOTAL lines.
- The cvar-off gaps tour's walk frames did not share a camera with the cvar-on run this time
  (`g0_walk2` looks down on her): only the lean pair is same-camera. Not looked into.

**Left open.**
- The two hems the poke check fails (hip crease, bent wrist). Options: cut the fabric from the
  body's own vertices near hems instead of the split ones, or lift hems that cross a crease.
- With the cvar on, the outfit's cut is on her default shape only; Height scales it with her.
- The figure is held at LOD 0 under a garment (face and grooms too): cost not measured.
- `/Game/Character/MH` is still not cooked, so none of this is in a packaged build.
- The filled-in skin has no anatomy: fine under this suit, not for outfits that bare chest or hips.

### 2026-11-14 · Stage 4a · her hair on the MetaHuman head (captures `stage4_hair_*`)

With the cvar on, the MetaHuman head wears the style chosen on the Hair tab (Long, Elf, Short) and the
preset's coil groom is hidden (brows, lashes and peach fuzz are grooms too and stay). Cvar off:
unchanged. No hair asset was made or modified; the head is a placeholder until the face step.

**How it follows.** `UVisexMHHairComponent` (a skeletal mesh component, `Source/visex/VisexMHBody.*`)
is created on the figure, attached to the MetaHuman **Body**, with the style's own mesh and physics
AnimBP. The AnimBP's Copy Pose From Mesh reads its attached parent, so head, neck and spine (the
Kawaii colliders' driving bones) are the MetaHuman's own, with its bone lengths and after RigLogic:
the hair no longer knows the driver exists. Kawaii Physics runs as before (the locks swing in the walk
frames). The attach parent ticks first, so nothing lags a frame.

**The fit: one scale and one offset per style, in one place.** The cards were grown on the old
skull. Each frame, before its own pose, the component turns the style's fit (in the head bone's
space) into its relative transform: a card vertex `p` lands at `Head * (s * p + t)`. A uniform scale
commutes with the head's rotation, so this is exact in any head pose.
- The numbers live in **`Config/DefaultGame.ini`, `[VisexMHHairFit]`**, a line per style:
  `Long=0.952,-0.46,1.13,0` is scale, then cm up, to her front, to the side. No rebuild.
- **To retune on a new head (about 2 minutes):** re-export the head
  (`MESH=<FaceMesh>; OUT=.../SourceArt/Character/MH/mh_head.fbx` with `Tools/mh_skin_export.py`), then
  `Blender -b --factory-startup --python Tools/mh_hair_fit.py -- <project> --save`. It fits the scalp
  cap and 2500 near-scalp card vertices by least squares (a vertex should clear the new scalp by `s`
  times what it cleared the old one) and rewrites the section.
- **By hand, live in Play:** `visex.Body.MetaHuman.HairFit "0.95,-0.5,1.1,0"` overrides the worn
  style; `visex.Body.MetaHuman.HairStyle Elf` wears a style whatever the Hair tab says.

| style | scale | up | front | fit residual rms / 95% | cap vertices under the skin (deepest) |
|---|---|---|---|---|---|
| Long | 0.952 | -0.46 | 1.13 | 0.26 / 0.43 cm | 2083 of 4103 (0.41 cm) |
| Elf | 0.958 | -0.53 | 1.17 | 0.22 / 0.39 cm | 1802 of 4103 (0.38 cm) |
| Short | 0.943 | -0.31 | 1.23 | 0.39 / 0.54 cm | 2991 of 4913 (0.50 cm) |

The scale is against the hair asset's own cm (today's head draws it at 0.935), so on this skull the
hair is 1-3% larger than on hers and sits 1.1-1.2 cm further forward on the joint.

**The two heads** (cm from the head joint, at the size each is drawn; `mh_hair_fit.py` prints them):

| | her current head (x0.935) | MetaHuman placeholder |
|---|---|---|
| crown over the joint | 15.9 | 15.6 |
| chin's underside | -5.2 | -5.0 |
| head height, chin to crown | 21.1 | 20.6 |
| skull width (top 6 cm, ears left out) | 13.2 | 14.1 |
| skull depth, brow to back | 17.2 (9.4 front, 7.8 back) | 17.3 (10.4 front, 6.8 back) |
| ear, bottom to top | 1.6 to 9.4 (7.8 long, pointed) | 1.3 to 6.9 (5.6 long) |
| ear, front to back | 1.2 in front to 5.2 behind | 2.1 in front to 1.5 behind |
| ear, furthest out from the middle | 9.1 (the tip: 9.4 up, 5.1 behind) | 8.1 |

So the placeholder skull is 0.9 cm wider, the same depth but 1 cm further forward on the neck, and
its ear is 2.2 cm shorter, lower at the top by 2.5 cm and does not sweep back.

**What a uniform fit cannot do (not built, by decision: the head is re-assembled next).** The scalp
cap rides 0.05 cm over the old scalp, and one scale leaves +-0.4 cm of shape difference, so about
half the cap ends up just under the new skin. Where cards are dense nothing shows; where they part,
skin shows instead of cap. `mh_hair_fit.py --write` plus `Tools/mh_hair.py` is the refit for the
final head: every cap vertex goes 0.08 cm over the new skin and the cards take the correction of
the cap under them, fading out over 6 cm; same bones, so the same AnimBPs, as new
`/Game/Character/MH/Hair/SKM_Hair*_MH` meshes that the game prefers when they exist. Tried on this
skull (cap under the skin 2083/1802/2991 -> 0/0/2; Short's crown closes) and then set aside: no
`_MH` asset is committed.

**Seen** (4K, Lvl_Dust; `-visexclose` scene captures lit and unlit, where skin reads white on hair):
- **Long**: menu camera reads as her (`stage4_hair_lean.png` against `stage4_lean_old.png`). From
  above and behind no scalp. The fringe sits at the brows as on her head. In the walk the lengths
  lie down the back over the suit and swing; no clipping into shoulders or suit seen.
- **Elf**: hairline sits on the forehead, no gap (a soft contact shadow, stepped by the card ends).
  From above and behind no scalp. The face locks hang in front of the shoulders, clear of the suit.
- **Short**: sits; from above, a 1 cm dot of scalp shows at the crown whorl, where the cards part
  and the cap is under the skin (the gap of the uniform fit; the refit closes it). Nape and neck clean.
- **Ears.** Her styles are cut around a pointed ear that rises 9.4 cm and sweeps back. Long: the
  opening is there and a round human ear sits low in it, crossed by the strands that used to pass
  behind the tip, with a patch of temple skin above it. Elf: the ear is bare, and above it a
  wedge of skin where the pointed ear used to cover the swept-back side. Short: the ear is under the
  hair, nothing to see. Options: (1) the face step gives the MetaHuman her ear (decided), then rerun
  the fit and, if wanted, the refit; (2) a separate ear mesh on the head bone (seam at the root);
  (3) regrow the styles on this skull with `blender_hair_cards.py` (HAIR_SKIN = the head FBX).

**Tools added to the tour.** `-visexclose=<What>:<bone>:<x,y,z>:<fov>[u][:<tx,ty,tz>][+...]`: with the 4K
frame of `<What>` (or its ending, `Crown`), a 4K scene capture looking at a bone of the body on
screen from x forward, y right, z up of it; `u` is unlit (base colours: skin against hair or fabric
in any light). `-visexcvar=a=1+b=2` sets several.

**Captures**: `stage4_hair_lean.png` (menu camera, Long = the Hair tab's), `stage4_hair_long/elf/short.png`
(menu lean per style), `stage4_hair_walk_back.png` (mid-stride from behind), `stage4_hair_top.png` (from
above, scene capture, no grade), `stage4_lean_old.png` (cvar off, same camera), and
`stage4_hair_views_<style>.jpg` (top, back, side unlit; ear, face, back lit).

**Traps.**
- `FParse::Value` stops at a comma: pass `false` to read a value with commas from the command line.
- The Blender round trip of a hair FBX keeps bones, weights and bounds if imported with
  `colors_type="LINEAR"` and exported as `blender_hair_cards.py` does (bounds matched to 0.1 cm).
- An unlit scene capture draws the MetaHuman skin white: handy for "does skin show through".

### 2026-11-14 · Stage 4b · the two failing Silversuit hems (captures `stage4_cuff*`, `stage4_hip*`)

**The cause was not the split.** Stage 3 blamed the skin being split in four (a fabric vertex between
two body vertices blends their weights and leaves the body triangle's plane). Checked offline: the
deepest vertices sit over the body's **own** vertices, with that vertex's exact weights (wrist: body
vertex 5171, 62% on the hand, fabric 0.54 cm over it, 0.49 cm under in the bend). What happens is a
fold: fabric rides the skin vertex under it and its offset turns with that vertex's blend of bones,
while the body triangle on the far side of the crease turns with its own. Bend the wrist 80 degrees
or sit, and that triangle comes over the hem. The higher the hem stands, the further under it ends up
(depth = height x how far the neighbour has turned past the fabric's offset): lifting a hem that
crosses a crease makes it worse, and the cuff stood at 0.5 cm in the wrist crease itself (its
hand-side hem is where the hand's weight is 0.62: on the joint). A real cuff there is pinched by the
skin too. The split adds its part where weights differ along an edge, but it is second order here.

**The fix, at the cut** (`Tools/mh_outfit_body.py`, `blender_outfit.py region()`; `--body=mh` only, the
old body's Silversuit builds to the same size, differing only in the 6 KB that differ between any two runs):
- `mh_safe_height()`: per skin vertex, the height up to which fabric over it stays within 0.02 cm of
  every neighbouring body triangle's plane in a set of test poses (each wrist bent 100 degrees in
  eight directions; each hip flexed 100, 125 and 140 degrees, straight, knee out and knee across;
  linear skinning from the skin's own weights, one joint at a time, the split vertices' own error
  included).
- `region()`: within 0.9 cm of a cut edge a panel sits down to that height (never under 0.04 cm),
  easing back to its own offset by 1.8 cm in; the rim wall and the binding follow.
  Only joints whose hems the tour fails are in the pose list (a hem sat down where the game never
  folds it loses its standing edge for nothing): add one when a poke line names it.
- `mh_poke_sim()`: the tour's poke line without the game, printed at every build (20 s instead of a
  9 minute import-and-tour loop). It reads 0.46-0.50 cm at the wrist before the fix, the tour's 0.46.
  On this cut: 1754 vertices of the suit's hems sat down by up to 0.21 cm (hip cutouts where they
  cross the crease), 370 of the belt's by up to 0.54, 1122 of the wrist cuffs' by up to 0.46 (their
  hand-side hem goes from 0.5 cm to 0.04); the ankle cuffs not at all.

**The full check, before and after** (`-visexqa -visexspikes`, worst frame of each pose; fabric
within 0.8 cm of a cut edge that is under the skin: vertices, deepest; the tour fails past 0.10 cm):

| pose | stage 3 | first cut (80 / 125 degrees, 0.04) | this cut |
|---|---|---|---|
| Settings sit (down, sit, up) | 29, 0.19 cm | 13, 0.08 | 27, 0.06 |
| photo Bookend (left wrist) | 7, 0.46 cm | 5, 0.08 | 5, 0.07 |
| photo Crown (left wrist) | 26, 0.46 cm | 16, 0.19 | 13, 0.07 |
| photo Glance | 13, 0.08 cm | 13, 0.05 | 15, 0.05 |
| photo Ledge | 124, 0.25 cm | 100, 0.11 | 92, 0.08 |
| photo LegLift | 105, 0.22 cm | 89, 0.10 | 95, 0.08 |
| photo Run / Peace | 0 / 1, 0.03 cm | 1, 0.03 | 1, 0.03 |
| every other pose | 0 | 0 | 0 |
| **TOTAL poke** | **FAIL** | FAIL (Crown, Ledge) | **pass**, 0.08 cm on `pelvis` |

Run three times on this cut with the same poke numbers. TOTAL body, face and garment pass as before
(garment 1.85-2.07x, creases 4.5-5.0x, grew 1.13-1.21 cm; under clipped skin still 1.52 cm in the
armpit, unseen). The first cut shows why the test poses are harder than the game's: at 80 degrees
the offline wrist read clean and Crown still folded 0.19 cm.

**Seen in 4K** (`-visexclose` scene captures framed on `hand_l` and the thighs, lit and unlit; in the
unlit ones fabric is blue and skin white or pink, so skin through fabric cannot hide in the chrome):
- **Wrist cuff, Bookend** (`stage4_cuff.png`, six sides in `stage4_cuff_bookend_views.jpg`): a clean band.
  Its arm-side edge keeps its standing lip; its hand-side edge now lies on the skin, and on the
  inside of the bend the cuff folds into the wrist crease with the skin (a sharp V in the chrome's
  reflections). No skin through the fabric from any of the six sides.
- **Wrist cuff, Crown**: not visible. Her left hand is in her hair and the wrist sits inside the
  hair's volume, in Short as in Long: 15 camera positions round the wrist, none shows the cuff's
  edge (`stage4_cuff_crown_views.jpg`). It stays a number (0.07 cm), and nobody will see it either.
- **Hip crease, Ledge** (`stage4_hip.png`, both hips in `stage4_hip_views.jpg`): the cutout's edge runs
  through the crease as one line and the belt stays level over it. Where the edge crosses the fold
  its binding kinks in two or three short facets. No skin through the fabric (unlit: unbroken blue).

**The "thin dark second line" beside the belt and the back cutout's binding (Ledge): shading, not a
hole.** A lit and an unlit capture from the same camera: unlit, belt meets skin in one clean line;
lit, under the bright binding there is a black band 0.25-0.3 cm tall (the rim wall: the panel's edge
down to the skin, in shade) and then the belt's own shadow on the skin. In scene captures that shadow
is stair-stepped (coarser shadow pages than the game view), which made it look like a mask edge. The
cover mask is not involved: the skin is drawn right up to the rim. Not changed. To thin it: a lower
belt (0.3 cm now, plus 0.3 where the suit runs under it).

**Mermaid's "small teeth" on the right-flank cutout: facets, logged, not fixed.** A row of four or
five dark triangles on the fabric just inside the edge, lit only (unlit: nothing, so no skin and no
gap). They lie along the waist's side fold in that bend: the body's quads are triangulated one way
(`bmesh.ops.triangulate` in `mh_outfit_body.py`), the fold runs across the diagonals, and chrome
shows each triangle of the strip as its own mirror. Skin has the same facets and hides them. A fix
is a triangulation that follows the fold (shortest diagonal in the bent pose), or a softer normal
there: an afternoon with looking, not cheap.

**Captures** (`Docs/Research/metahuman/`, 3840x2160 scene captures, Lvl_Dust, cvar on, no post grade):
`stage4_cuff.png` (Bookend, left wrist), `stage4_hip.png` (Ledge, right hip crease), and the sheets
`stage4_cuff_bookend_views.jpg`, `stage4_cuff_crown_views.jpg`, `stage4_hip_views.jpg`.

**Assets.** Re-cut and re-imported, all stage 3's own under `/Game/Character/MH/Outfits`:
`SKM_Silversuit_MH`, `T_Cover_Silversuit`, `MI_Body_Silversuit` (re-saved by `mh_outfit.py`), and
`SourceArt/Character/MH/Outfits/outfit_silversuit.fbx`. Nothing else.

**Traps.**
- A hem that crosses a crease: lower, not higher. More offset puts it further under the fold.
- zsh reads `$p:hand_l` as `$p` with the `:h` modifier. Write `${p}:hand_l`.
- Unlit, the MetaHuman skin is white where its baked maps are the originals and pink where the
  underwear was painted out (`T_Body_*_Bare`): the border between them is the old briefs' line, not
  a hole. Lit, the two do not differ.
- A scene capture's shadows are stair-stepped next to an edge. Judge shadow lines in the game view.

**Left open.**
- The pose list in `_mh_test_poses()` is wrists and hips only. Another outfit's failing hem on
  another joint (a neckline at the shoulder, a stocking top behind the knee) needs its joint added.
- A sat-down hem stands 0.04 cm over the skin. Fine in every frame looked at; if it shimmers at a
  distance on a sheer outfit, raise `MH_HEM_MIN`.
- The cuff in Crown cannot be looked at while her hand is in her hair.
