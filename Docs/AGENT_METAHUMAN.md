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
  a line per pose to `Saved/QA/report.txt` (add `-visexshots=<name>` for a 4K frame of each). Both TOTAL
  lines must read `pass`, and look at any pose whose crease count jumps. `visex.MH.Spikes` checks the
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
