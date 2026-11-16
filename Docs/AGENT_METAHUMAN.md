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
- **After every assembly run `Tools/mh_after_build.sh`** (with `shape` if the body's vertices moved): the
  assembly rewrites the body mesh without morph targets and with the bust as the Creator solves it, so
  her softened bust, the shape sliders, the jiggle and the flex layer are gone until they are put back,
  and the outfits and bare-skin maps are per vertex and per bake (see the stage 7b and stage 8 logs).
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
5. Body page: bone-scale sliders against the corrective joints (done with skin tone, see the stage 7
   log); morph sliders rebuilt
   from MetaHuman body variants (same topology, so a variant minus the base is a morph).
6. Remaining outfits, then hair refit to the new head.
7. Muscle flex layer (morphs by joint angle and load: done, see the stage 8 log), tension wrinkles
   (a recipe only, same log), face expressions for photo mode.

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

### 2026-11-14 · Stage 5 · MetaHuman hair (captures `stage5_hair_*`)

**Tema's decision:** "let's make hair as in metahuman. i don't like our existing". On the MetaHuman body
she wears MetaHuman's own hair (a groom), not our cards. Stage 4a's card hair stays in the code and
assets but is no longer the default. Then: "it's enough hairstyles", so one style, no catalogue.

**The style: `Hair_L_StraightBangs`, black** (Melanin 1, Redness 0, Whiteness 0), with the preset's
`Eyebrows_M_SlightArch` and `Eyelashes_L_SlightCurl` in the same black. It is the longest straight
style in the Creator (strands up to 48.5 cm, to the mid-back) and has her fringe; `Hair_L_Straight`
stops at the shoulders (`stage5_hair_pick.jpg`, both on the Creator's preview actor). The wardrobe is
what ships in the plugin (`/MetaHumanCharacter/Optional/Grooms/Bindings/<slot>`: 38 hair, 18 brows,
6 lashes); the cloud serves the rig and textures only, not grooms.

**Built** by one clean pass, `STEPS=['fresh','create','shape','look','hair','rig','textures','build']`
(2 min 16 s, sign-in cached; the new `hair` step of `Tools/mh_create.py`, `HAIR=` picks another style).
Same body targets and paths. `Tools/mh_vertex_order.py` before and after: body 30,455 and face 33,845
vertices, same triangles, **no vertex moved (0.000 cm)**, so the follower, the Silversuit cut, the
masks and the bare-skin maps hold. Spike tour after it (`-visexqa -visexspikes`, cvar on, Silversuit
worn): **body pass** (stretch 2.57x), **face pass** (1.35x), **garment pass** (2.07x), **poke pass**
(97 fabric vertices under drawn skin, deepest 0.08 cm). The old `Hair_S_Coil*` files under
`Built/MHC_Her/Grooms` (101 MB) are no longer referenced and can go.

**The switch: `visex.Body.MetaHuman.Hair`** (default 1). 1: the Blueprint's own groom is her hair.
0: the Hair tab's card style on the MetaHuman head (stage 4a, still works). 2: no hair, to time a
frame without it. Logged as `VisexMH: hair = ...`. The Hair tab does nothing at 1.

**How it behaves on this Mac** (M5 Max, Metal SM6, which is what strands need; the project targets it):
- **Strands, not cards.** 92,525 strands (24,642 + 67,883), 2.26 M vertices. The groom's own LODs are
  strands at 0-1, cards at 2-4, a helmet mesh at 5-7, and it follows the Blueprint's LOD Sync; under a
  garment the figure is held at LOD 0, so with an outfit on it is strands at every distance. The
  distances of the card LODs with no outfit on were not measured.
- **Physics runs** (Niagara `StableSpringsSystem`, angular springs, on at LOD 0-2). In the arrival walk
  the lengths lift off the back and trail behind the neck (film frames 241-261); in the lean they hang
  off the tilted head with the same outline in four frames over a second. Judged from frames, not
  measured, and the clip was not watched through for jitter. Switching the physics off
  (`r.HairStrands.Simulation 0`) changes the frame time by nothing measurable.
- **It clips into the suit at the shoulders.** The groom collides with nothing: locks that fall in
  front of the shoulder sink about a centimetre into the Silversuit (`stage5_hair_close.png`, both
  shoulders, idle and lean). From the menu camera and from behind in the walk it does not read.
- **Menu camera and backlight: holds.** The low camera shows the fringe and the lengths behind the
  shoulder with no scalp; backlit from behind it is a solid black shape with loose strands at the rim.
- **Frame time, 3840x2160, menu camera, lean, Silversuit on** (`Tools/mh_hair_cost.sh`: a `-game` run in
  a 1920x1080 window at `r.ScreenPercentage 200`, because the display is 3456x2234; vsync off, no frame
  cap, CSV profiler, 10 s and about 95 frames per mode after 30 dropped; GPU ms is the profiler's GPUTime):

  | hair | frame ms | GPU ms |
  |---|---|---|
  | MetaHuman groom | 82.4, then 83.7 on the repeat | 81.8, 83.2 |
  | none (`Hair 2`) | 79.4 | 78.9 |
  | our cards (`Hair 0`, Long) | 85.3 | 84.8 |
  | groom, physics off | 83.4 | 82.9 |

  So the groom costs about 3-4 ms of an 80 ms frame, the same or less than the cards, with 1.3 ms of
  drift between two runs of the same mode. The frame is GPU bound (game thread 6 ms) and the scene
  itself is 12 fps at this size: that is not the hair. The window's pixel size was taken from the log
  (system resolution 1920), not read back from the render target.

**Captures** (3840x2160, Lvl_Dust, cvar on, Silversuit): `stage5_hair_lean.png` (menu camera, game view
with the grade), `stage5_hair_walk_back.png` (Start walk, 4 s in, game view), `stage5_hair_close.png`
(idle, head and shoulders 3/4; a scene capture, so no grade, and its hair shadow on the skin is grainy
from four capture frames), `stage5_hair_walk_lean.mp4` (22 s, 720p, the film tour: walk in, lean, Body
page and back), `stage5_hair_pick.jpg` (the two long straight styles on the Creator's preview actor).

**Traps.**
- **A scene capture never computes a groom.** The renderer skips strand interpolation for captures
  (`AllowGPUParticleUpdate`), so a capture shows what the main view last drew. In a `-game` run that
  is fine. In a background editor the viewport does not draw ("Use Less CPU when in Background"), and
  every capture of the Creator's preview actor is bald. `Tools/mh_hair_sheet.py` switches that setting
  off in memory, aims the viewport at her, and reads the back of the head to tell a bald shot.
- `try_add_slot_selection` on a slot that has a selection wears both; use `set_single_slot_selection`.
  A groom's colour parameters exist only after `assemble_for_preview`.
- `-game` runs log to `~/Library/Logs/visex/`, the editor to `~/Library/Logs/Unreal Engine/visexEditor/`.
- **The editor restarted itself four minutes in**, and `Config/DefaultEngine.ini` gained
  `r.GPUSkin.UnlimitedBoneInfluences=True`: the MetaHuman plugin's "missing project settings" toast was
  accepted in the editor window (18:50:59 the setting, 18:51:09 restart; no bridge job did it, and who
  clicked is not known). The unexplained `QUIT_EDITOR` of the stage 3 log may be the same toast. The line is left in the working tree,
  **not committed**; this stage's editor and game runs passed
  `-ini:Engine:[/Script/Engine.RendererSettings]:r.GPUSkin.UnlimitedBoneInfluences=False`, so the
  assembly and every number here are on the committed config. Tema or main decides: keep it (then
  re-assemble and re-run the spikes with it on) or drop the line.
- Python has no PIL here and this ffmpeg has no drawtext: `Tools/sheet.swift` labels a sheet.

**Left open.**
- Hair into the shoulders: needs the groom to collide (a physics asset on the groom component, or
  the solver's collision against the body) or a style that stays behind the shoulders.
- Packaged build: the grooms are under `/Game/Character/MH`, still not cooked into the DMG.
- The Hair tab has no MetaHuman styles: each extra style is another groom in the assembly
  (107 MB of assets for this one) and was cut from this step.
- Brows and lashes are the preset's; the face step may want others.

### 2026-11-14 · Stage 6 · her face on the MetaHuman (captures `stage6_*`)

**Tema's decision:** "Match her current face": the MetaHuman head takes the proportions and features
of today's head (`SKM_AndroidSkin`), pointed ears included, on realistic MetaHuman skin. Commits
f1d149c, dae961c, 31fb25f, 4b1f5ba (tools, C++), a047c10 (assets).

**What makes the face hers** (`stage6_face_ref.jpg`: front, 3/4, profile, hair hidden, one light):
- Eyes: sealed. Two closed lids shaped like a flat almond (3.0 x 1.0 cm), 2.9 cm apart at the inner
  corners, almost level (outer corner 0.15 cm higher), in shallow sockets, no brows or lashes.
- Nose: small and short (3.0 cm wide at the wings, tip 2.1 cm over the joint), a low smooth bridge.
- Lips: a small closed mouth (4.0 cm), close under the nose, the lower lip the fuller one.
- Jaw and chin: a V. Widest at the cheekbones (13.4 cm), 9.8 cm at the mouth, a narrow chin that
  recedes a little in profile; flat cheeks with no hollow.
- Skull and ears: a tall round cranium (the face is its lower half), a long slim neck, and long
  blade ears rooted at nose height, tip 9.8 cm over the joint, swept 4 cm back and out to 9.5 cm.

**Her head is drawn at 0.935 x 1.04**, not 0.935: the Head slider's default (1.04) scales the head
bone. Stage 4a's table left that out, so her real skull is 13.6 cm wide and 21.8 chin to crown
(the preset's: 14.1 and 20.3). The fit uses her drawn size (`HEAD_SCALE` in `mh_face_fit.py`).

**The path: fit to given vertices, and it keeps everything.** `import_from_template` (and
`fit_state_to_target_vertices` under it) fits the face model to the target and then stores what the
model cannot reach as per-vertex deltas at full strength (`FitToTarget` + `UpdateVertexDeltas`, global
delta 1.00), so the head comes back as given: the round trip of the preset's own head is exact above
the neck (0.004 cm mean) and the Creator re-blends the neck itself (0.13 cm mean, 0.5 max). Landmark
sculpting (`translate_face_landmarks`) stays inside the human face model and was not needed.
- **Input**: four static meshes in the MetaHuman's own topology and UVs (head 24,416 vertices, each
  eye 802, teeth 4,613), matched to the DNA by UV (`match_vertices_by_u_vs`), alignment NONE: they
  are taken where they sit on this body, in the assembled mesh's cm frame. No DNA file, no skeletal
  mesh. Eyes and teeth are passed too, so the model does not guess them.
- **The ears survive the whole way**: the Creator's preview, the cloud auto-rig (joints only, 6-13 s)
  and the assembly. No morph on top. (Fit to the model alone, delta 0, would give a human ear.)

**The wrap** (`Tools/mh_face_fit.py`, Blender, about a minute; `landmarks.json` beside its output):
1. Her head is slid 1.32 cm forward first: the two skeletons put the head joint at different depths
   in the neck, and joint on joint would hang her face behind the MetaHuman's neck.
2. A landmark warp (145 pairs): nine midline points read off both profiles by the same rule (nose
   tip, under the nose, lips, chin by direction: her chin has no point that stands out), nose wings
   and mouth corners placed by eye on gridded pictures (`--views`), eight outline points per ear.
3. **Eyes as a whole.** Her lids are shut, so there is no eye to copy. Each MetaHuman eyeball moves
   under her closed lid (one lid's thickness, 0.12 cm, behind it), 4.6% larger, 0.3 cm nearer the
   middle; the skin lying on it keeps its distance from the eyeball's centre while the opening's
   outline goes to her lids' outline (2.82 x 1.03 -> 2.96 x 0.99 cm). The lids still close (blink shot).
4. Everything else settles on her surface (nearest point that faces the same way, smoothed over the
   mesh, seven rounds from stiff to loose: 13,400 of 24,000 vertices end on it). Hollows (mouth,
   nostrils) and their rims only follow. **Ears follow the landmark warp only**: settling a human
   ear on her blade ear crumpled its folds, so it is a MetaHuman ear stretched to her outline, with
   a rolled rim and a real concha where hers is a flat blade.
5. The neck fades to the body's over 8 cm (7 to 15 cm under the joint); creases under the jaw (her
   mesh has them where its head was joined on) and the step round the eyes are smoothed.

**How close** (`mh_face_fit.py --look <head.fbx> <name>` tapes any head; cm, the head joint is 0):

| | her | preset (Ada) | built MetaHuman |
|---|---|---|---|
| chin to crown | 21.8 | 20.3 | 21.8 |
| skull width / cheekbones / jaw at the mouth | 13.6 / 13.4 / 9.8 | 14.1 / 13.3 / 11.3 | 13.7 / 13.4 / 9.8 |
| nose tip forward, its height | 11.8, 2.1 | 11.7, 2.3 | 11.8, 2.1 |
| mouth line height / chin forward | -0.8 / 10.4 | -1.1 / 10.3 | -0.8 / 10.4 |
| ear: top / behind / out | 9.8 / 4.1 / 9.5 | 6.9 / 1.5 / 8.1 | 9.6 / 4.1 / 9.4 |

Read from `stage6_face_match.jpg` (her over the MetaHuman, same cameras and light): the outline is
hers from all three sides: cranium, V of the jaw, small nose, mouth height, chin, ears. What is
not hers is what she never had: open brown eyes under the preset's dark eye shadow, brows and
lashes, fuller coloured lips, an orbital rim and cheekbone that catch the light (the eye sits
deeper than her painted-on lid). It reads as a person with her head, not as her mask with skin.

**Skin.** Tone `SKIN_UV = (0.01, 0.40)` (u is how dark, 0 the lightest; v the undertone); stages 1-5
wore (0.12, 0.35). Chosen on the Creator's preview under the sheet's lights: forehead (R, G, B)
her 195/167/146, MetaHuman before 182/143/125, now 204/167/150. Her tone is the picker's lightest
column; it cannot go less pink than that. Face skin stays the preset's (index 16: the cleanest of
eight looked at), roughness 1.06, and **the preset's makeup is untouched** (eyes "SoftSmokey",
black, 0.85): her face has none, so taking it off is a one-line change if Tema wants it.
`show_top_underwear = False`: the chest is Epic's bare skin; `mh_bare_skin.py` now fills only what
is still grey cloth in the bake (7.7% of the map instead of 19.8%: the briefs).

**`r.GPUSkin.UnlimitedBoneInfluences`: reverted, the line is dropped.** Built once with it on: the
MetaHuman's own skinning has up to 12 influences (4,502 body vertices over 8; off, they are cut
to 8), and on it the body and face read better (body creases 4.29x against 5.80x, face grew 0.48
against 0.75 cm). But the Silversuit, cut from the 8-influence skin, then goes under the skin at the
hip crease: poke FAIL, 0.12 cm on `pelvis` (sit 0.11, Ledge and LegLift 0.12; the limit is 0.10,
stage 5 read 0.08). A re-cut from the 12-influence export did not cure it (0.14: the garment import
probably caps at 8; not chased). The 4K frames of Ledge and LegLift show nothing either way. So
the final build is on the committed config and every number below is from it. Worth another go
when outfits are next re-cut: it needs garment and skin on the same weights.

**Checks on the final build** (one clean pass; the Epic sign-in was cached, no prompt came up):
- **Vertex order** (`mh_vertex_order.py`): body 30,455 vertices, same triangles, moved 0.000 cm: the
  follower, the Silversuit cut and its mask hold as they are. Face: same counts and triangles,
  shape changed (4.88 cm at the ear tips), as meant.
- **Spike + poke tour** (cvar on, Silversuit worn): body pass (stretch 2.74x, creases 5.80x), face
  pass (1.38x, creases 2.37x on `FACIAL_R_12IPV_NeckA1`, grew 0.75 cm), garment pass (1.72x), poke
  pass (95 fabric vertices under drawn skin, deepest 0.08 cm on `pelvis`). The face line covers
  jaw and neck through the walk, the lean, the sit and every photo pose.
- **Feet** (fixed-step tour): idle drift 0.38 cm, sole gap L 0.23 R -0.11; Start walk drift median
  1.50, max 2.69. As stage 5.
- **Neck seam**: none to see under the menu light (`stage6_lean.png` at 4K) or in the close-up at
  the lean; neck and collarbones run through. Her neck is slimmer than the body's, so in profile
  the nape steps out a little where the fade ends (`stage6_face_match.jpg`, bottom right).
- **Eyes** drawn at LOD 0-3 (`stage6_eyes_lods.jpg`); the brows are missing in the LOD 2 shot.
- **Face rig**: neutral, a blink (both lids close fully on the moved eyeballs) and a smile
  (`stage6_expressions.jpg`), driven by `visex.Body.MetaHuman.Face "CTRL_expressions_eyeBlinkL=1,
  CTRL_expressions_eyeBlinkR=1"` (`off` gives the face back). The Blueprint gives the Face no anim
  class of its own; the rig is joints only (no blendshapes or wrinkle maps were asked of the cloud).

**Captures** (`Docs/Research/metahuman/`, 3840x2160): `stage6_face_ref.jpg` (her head: front, 3/4,
profile), `stage6_face_match.jpg` and `stage6_ears.jpg` (her over the MetaHuman, hair hidden, same
head-fixed cameras and lights, black ground, fixed exposure; scene captures, no grade),
`stage6_expressions.jpg`, `stage6_eyes_lods.jpg`, `stage6_portrait.png` (with hair, Lvl_Dust, the
idle, a tour close-up with the grade: `-visexclose=idle:head:120,60,0:34g:0,0,-6`), `stage6_lean.png`
(menu camera, game view). `Tools/mh_face_sheets.sh` reshoots the four sheets (4 minutes, editor on
Lvl_Studio); `Tools/mh_face_shots.py` is the camera (head bone's frame, `MODE='preview'` for the
Creator's preview before the cloud steps).

**To change the face**: edit `landmarks.json` or the numbers at the top of `mh_face_fit.py`, run it
(it writes `SourceArt/Character/MH/Face/face_*.fbx` and gridded pictures of the result), look at it
on a scratch character (`CHAR_NAME='MHC_Try'; STEPS=['create','shape','face']`, then
`mh_face_shots.py` with `MODE='preview'`), then the one clean pass.

**Traps.**
- **`SourceArt/Character/MH/mh_head.fbx` is the preset's head and must stay it**: the fit wraps that
  mesh and `landmarks.json` is placed on it. The built head is exported as `mh_head_fit.fbx`.
- Exporting a Creator preview texture (`T_Face_Editable_*`, transient, no source) with an
  `AssetExportTask` asserts in `UTextureExporterGeneric` and takes the editor down.
- A scene capture of `SCS_BASE_COLOR` comes out white in this project.
- In a background editor a light moved by `set_actor_location` is still at its old place for a
  capture in the same job; setting a property on it as an edit re-registers it.
- The preview actor spawns at the origin, inside Lvl_Studio's set: lift it clear before shooting.
- Past LOD 1 the MetaHuman body's `head` socket no longer follows the pose; aim by the driver's.
- The console cannot set a string variable to empty ("name" alone prints it): give it a word.
- Brows and lashes show in a capture about half a minute after the body is switched on, not before.
- zsh does not split `$pair` on spaces in a `for`: use `${pair%%:*}` forms.

**Left open (Tema's calls first).**
- **Makeup and brows**: the preset's smoky eye shadow and its brows are still on; her own face has
  neither. Eye colour is the preset's brown.
- **Head size**: her head is 4% larger than a MetaHuman's on this body (the Head slider's 1.04), and
  it is kept. `HEAD_SCALE` in `mh_face_fit.py` is the one number if it reads large with real skin.
- **Lips and lids** are the preset's flesh on her outline: fuller than her mask's. The landmark file
  can pull them in.
- **Ear**: a rolled rim and concha, not her flat blade; from behind it stands off the head a little
  more than hers (`stage6_ears.jpg`, middle).
- The card hairstyles (`visex.Body.MetaHuman.Hair 0`) were fitted to the preset's skull: rerun
  `mh_hair_fit.py` against `mh_head_fit.fbx` if they are used again. The groom was re-bound by the
  assembly; the ear tip comes through it at the side (`stage6_portrait.png`).
- Unlimited bone influences (above); `/Game/Character/MH` is still not cooked into the DMG.

### 2026-11-16 · Stage 7 · Body page sliders on the MetaHuman body (captures `stage7_*`)

With `visex.Body.MetaHuman 1` every bone-scale slider of the Body page and Skin tone now reach the
MetaHuman: Head, Neck, Shoulders, Ribcage, Waist, Hips, Leg length, Thighs, Calves, Arm length, Arms,
Hands, Feet (Height already did). The seven morph sliders (Bust, Bust height, Bust spacing, Glutes,
Abs, Body fat, Thigh gap) still do nothing on it: next step, recipe at the end. C++ only, no asset
was added or changed. `visex.Body.MetaHuman.Reshape 0` gives the body as built, to compare.
Commits 15521a3 (the follower), e3c076c (the sliders tour and its tools), then this log and the sheets.

**Each slider over its default.** The MetaHuman was shaped to her at the default sliders (stage 1,
3, 6: the head's 1.04 is in the face fit, the thigh's 1.208 in the tape), so the follower takes
`value / default` per slider (`UVisexMHBodyComponent::ApplyShape`, `FVisexMHShape`): 1 everywhere at
the defaults, and the same relative range as the driver's. Torso depth terms keep Reshape's
weights (ribcage 0.6, waist 0.7, hips 0.5 of the width's change).

**Where it goes** (`FVisexMHBodyProxy::Build` and `Evaluate`, `Source/visex/VisexMHBody.cpp`). The
driver's Reshape on the same bones: spine_04/05 (Ribcage), spine_01/02 (Waist), pelvis (Hips), each
local axis taking the term of the body axis it is closest to; thigh and calf (Leg length along X,
Thighs / Calves around it); upperarm and lowerarm (Arm length, Arms); head, hand and foot whole. A
segment's scale sits on its bone and the next *followed* bone gets the inverse, so the change stays in
the segment and reaches the joints below as an offset. It is put on in the same root-down pass that
builds the pose (local transform times the parent's component space), before the foot pin: the stage
2 bug class (layers composed child-first under non-uniform scale) has no second composition to
disagree with. Two sliders are done differently from the driver, on purpose:
- **Neck: an offset, no scale.** neck_02 and head move along the neck by the factor. The Face mesh
  hangs 58 neck joints of its own on neck_01 / neck_02 with their axes turned a quarter (X is not the
  neck's length), and scale passes down by axis *index*: a length scale would have thickened the
  Face's neck sideways (up to 1.44x) while the Body's neck grew long, and opened the seam.
- **Shoulders: the collarbone longer along X**, upper arm compensated. The upper arm lands where the
  driver's offset puts it (its offset is 13.0, 0.0, -0.1), and `clavicle_out` / `clavicle_scap`
  spread with the collarbone instead of the whole stretch landing at the shoulder joint.

**The 172 corrective, twist and volume joints: they inherit, and four are corrected after.** RigLogic
writes each driven joint's whole local transform every frame, translation, rotation and scale
(`FAnimNode_RigLogic::UpdateJoints`: neutral + delta, in the parent's space), and reads only the
driver joints' rotations. So a scale or a compensation set on a driven joint is lost at LOD 0-1 and
kept past LOD 1 (where RigLogic stops): the one behaviour that is the same at every LOD is to leave
them alone. They then sit and scale with the segment they hang on, offsets included, which is the
rigid-lump answer a bone scale means. The catch is that FTransform passes scale down per axis index,
not per direction. Checked on the reference pose (`Saved/MH/skel_dump.json`, dumped through the
bridge): of the joints that inherit a non-uniform scale, all on the limbs have the segment's axes or
Y and Z swapped (harmless: girth is the same on both), except:
- `clavicle_pec_l/r` and `spine_04_latissimus_l/r` under spine_05: X and Z swapped. The bust hangs
  on the pec joints, and the Ribcage's width reached it as *height* (the keyhole grew taller at
  max and shrank at min instead of widening: seen in the first frames). Fixed after the post-process
  AnimBP: `UVisexMHBodyComponent::FixScales`, on the Body's bones-finalized delegate, moves the terms
  to the axes they belong on in the finished component-space transforms (a scale only: the four have
  no children), before the garment's Leader Pose and the renderer read them. The list is built from
  the reference pose, not by name (`UVisexMHBodyAnimInstance::ScaleFixes`).
- `upperarm_in_l/r` (armpit): 42 degrees off every axis, so neither answer is right. Left inheriting.
  Under Arms and Arm length its patch of skin takes length where it should take some girth; nothing
  shows in the frames.

**Soles, pelvis, capsule.** The foot pin's sole offset and the pelvis lift are rebuilt when a slider
moves: the MetaHuman's ankle and ball heights over the sole take the Feet factor, and the leg length
that sets the pelvis is thigh plus calf under their scales. The pin still reads no ground: it
repeats the driver's answer on a leg and foot of the new size. Capsule, mesh placement and camera are
the driver's (`UVisexBodyComponent`, untouched), so they are the current body's by construction; the
sliders tour logs the capsule per setup (half height 72.8 at 145 cm, 92.9 at 185, 79.9 / 88.9 at Leg
length 0.90 / 1.12, radius unchanged by the legs) and reads the same numbers with the cvar off.

**The check: `Tools/mh_sliders.sh <name> [cvar]`** (`-visexqa -visexsliders`, `AVisexQATour::
BuildSliders`). It puts 35 setups on her one after another (the default body; each of the 13
bone-scale sliders and Height at minimum and maximum; skin tone 0 / 0.5 / 0.75 / 1; two combined
bodies), each in the menu lean, the photo LegLift and the photo Walk: 1.6 s to settle, a second
measured under its own phase (spike, poke, the soles' vertex gap, how far the MetaHuman's hand, foot,
head and pelvis joints sit from the driver's, the capsule), then a 1280x2160 portrait from one fixed
camera per pose and a head-and-shoulders frame. The profile is changed in memory and loaded back at
the end; nothing is saved (`VisexBody.sav` was backed up first and is byte-identical after).
`Tools/mh_slider_table.py` turns the report into the table, `Tools/mh_slider_sheets.sh` the frames
into the sheets. `-visexbody=Legs=1.12+Feet=1.15` puts values on her for any other tour.
The combined bodies: *small and slight* Height 150, Head 1.0, Neck 0.84, Shoulders 0.82, Ribcage 0.90,
Waist 0.74, Hips 0.88, Leg length 0.92, Thighs 0.90, Calves 0.90, Arm length 0.93, Arms 0.84, Hands
0.85, Feet 0.87; *tall and strong* 183, 1.08, 1.10, 1.15, 1.15, 1.05, 1.15, 1.10, 1.35, 1.22, 1.06,
1.22, 1.10, 1.10.

Numbers (fixed 30 fps step, cvar on, Silversuit worn; body stretch and creases as worst frame in
lean / LegLift / walk; the hem column is fabric vertices under drawn skin and the deepest, LegLift;
sole gap is the lowest sole vertex over the sand in the lean; hand is the MetaHuman's hand joint
from the driver's in the lean, cm):

| setup | body stretch | creases | hem poke | sole gap L / R | hand L / R |
|---|---|---|---|---|---|
| default | 1.48 / 2.14 / 1.61 | 1.83 / 4.31 / 2.71 | 0 | 0.33 / 0.28 | 1.34 / 1.23 |
| Head 0.85 / 1.15 | 1.48, 1.49 / 2.14 / 1.61 | 1.86, 1.79 / 4.31 / 2.71 | 10, 0.07 / 0 | 0.33 / 0.26 | 1.35 / 1.24 |
| Neck 0.80 / 1.30 | 1.49 / 2.14 / 1.61 | 2.01, 1.97 / 4.31 / 2.71 | 3, 0.02 / 8, 0.04 | 0.33 / 0.26 | 1.36 / 1.23 |
| Shoulders 0.80 / 1.20 | 1.49 / 2.14 / 1.61 | 2.00, 1.96 / 4.31 / 2.71 | 0 / 10, 0.06 | 0.34 / 0.26 | 1.34, 1.44 / 1.22, 1.30 |
| Ribcage 0.85 / 1.20 | 1.50, 1.49 / 2.14 / 1.61 | 1.81 / 4.31 / 2.71 | 0 / 8, 0.05 | 0.33 / 0.28 | 1.42, 1.26 / 1.29, 1.16 |
| Waist 0.60 / 1.20 | 1.48, 1.50 / 2.14 / 1.61 | 1.86 / 4.31 / 2.71 | 0 / 60, 0.10 | 0.33 / 0.26 | 1.35 / 1.24 |
| Hips 0.85 / 1.50 | 1.48, 1.64 / 2.12, 2.23 / 1.61, 1.79 | 1.85, 2.94 / 4.05, 5.86 / 2.58, 3.50 | 14, 0.05 / 0 | 0.34 / 0.29 | 1.36 / 1.26 |
| Leg length 0.90 / 1.12 | 1.42, 1.61 / 2.03, 2.31 / 1.54, 1.91 | 1.79, 2.03 / 4.07, 4.70 / 2.61, 3.02 | 0 / 14, 0.07 | 0.33 / 0.26, 0.34 | 1.28, 1.42 / 1.21, 1.30 |
| Thighs 0.80 / 1.40 | 1.38, 1.55 / 1.99, 2.20 / 1.48, 1.66 | 2.02, 1.97 / 4.73, 4.17 / 2.96, 2.76 | 0 / 13, 0.09 | 0.33 / 0.28 | 1.34 / 1.24 |
| Calves 0.80 / 1.25 | 1.48, 1.50 / 2.14 / 1.61, 1.63 | 1.86 / 4.31 / 2.71 | 0 | 0.33 / 0.27 | 1.35 / 1.24 |
| Arm length 0.92 / 1.08 | 1.50 / 2.14 / 1.63 | 1.81, 2.01 / 4.31 / 2.71 | 10, 0.07 / 0 | 0.33 / 0.28, 0.12 | 1.25, 1.51 / 1.13, 1.42 |
| Arms 0.80 / 1.25 | 1.48, 1.50 / 2.14 / 1.63 | 1.84, 2.08 / 4.31 / 2.71 | 8, 0.05 / 0 | 0.20, 0.31 / 0.33, 0.26 | 1.35 / 1.28 |
| Hands 0.85 / 1.15 | 1.50, 1.48 / 2.14 / 1.61 | 1.81, 2.34 / 4.31 / 2.71 | 0 | 0.33 / 0.28 | 1.34 / 1.24 |
| Feet 0.85 / 1.15 | 1.48, 1.49 / 2.14 / 1.57, 1.74 | 1.86, 1.80 / 4.31 / 2.74, 2.61 | 0 | 0.32, 0.35 / 0.26, 0.30 | 1.39, 1.28 / 1.28, 1.15 |
| Height 145 / 185 | 1.49 / 2.14 / 1.65, 1.60 | 2.01, 1.97 / 4.31 / 2.72 | 10, 0.06 / 0 | 0.29, 0.37 / 0.23, 0.29 | 1.19, 1.52 / 1.08, 1.37 |
| small and slight | 1.32 / 1.93 / 1.36 | 1.73 / 4.23 / 2.57 | 0 | 0.29 / 0.22 | 1.17 / 1.09 |
| tall and strong | 1.68 / 2.37 / 2.01 | 2.24 / 5.02 / 3.09 | **56, 0.14 FAIL** | 0.39 / 0.29 | 1.68 / 1.52 |

Worst over every setup and pose: body stretch 2.37x (limit 3) and grew 1.73 cm (limit 3), both
LegLift on the tall body; creases 5.86x (LegLift, Hips 1.50: the groin's short edges; the standard tour's
Ledge reads 5.8-6.6 on the default body); face stretch 1.73x (the neck at 1.30, which is the neck getting 44%
longer); garment stretch 1.51x. **Poke: one FAIL**, the tall body in LegLift, 56 fabric vertices up to
0.14 cm under the skin at the hip cutout's edge where it crosses the hip crease (limit 0.10; Waist
1.20 alone sits on the limit, 60 at 0.10). It is stage 4b's hem. That hem reads 0.00-0.09 cm from
second to second on the default body in this pose (the Head and Height rows above show it: those
sliders do not touch the hip), so single sliders stay inside the limit and the big body goes past it.
Soles in the lean: 0.12-0.39 cm over the sand at every setup (target 0.5). Hands in the lean: 1.1-1.7
cm from the driver's hand joint at every setup, 1.34 / 1.23 on the default body: the gap stage 2
left (a longer arm), scaled with her, not opened by any slider. In LegLift the left hand is 2.2-3.0
cm off (2.6 on the default body).

**The real walk and the idle** (gaps tour, fixed 60 fps step, `-visexbody`), MetaHuman / current body:

| cm | Start walk drift, median | Start walk sole gap, median | lean sole gap L, R | idle sole gap L, R |
|---|---|---|---|---|
| default | 1.50 / 2.17 | 0.58 / 0.76 | 0.33, 0.26 / 0.19, 0.19 | 0.23, -0.11 / 0.19, 0.18 |
| Leg length 1.12 | 8.38 / 8.32 | -0.42 / -0.23 | 0.33, 0.40 / 0.19, 0.34 | 0.23, -0.11 / 0.20, 0.19 |
| Leg length 0.90 | 0.90 / 1.10 | 2.80 / 2.80 | 0.33, 0.28 / 0.19, 0.19 | **6.43, 4.50 / 6.40, 4.80** |
| small and slight | 2.80 / 2.78 | 6.63 / 6.77 | 0.29, 0.24 / 0.17, 0.17 | **3.80, 7.03 / 3.87, 7.11** |
| tall and strong | 4.98 / 5.65 | -0.93 / -0.52 | 0.39, 0.31 / 0.21, 0.21 | 0.26, 0.25 / 0.22, 0.21 |

The MetaHuman stays within 0.3 cm of the current body in every cell: the pin holds under the leg and
foot sliders. What the table also shows is **the current body's own**, with the MetaHuman repeating
it: with short legs she floats 4-7 cm over the sand after Start and in the photo Walk (3.9-5.0 cm
with the cvar off and on), and with long legs her Start walk slides 8 cm a step. Those are the
driver's (the capsule and KeepFeet outside the lean, the walk's play rate against a longer stride),
not the follow, and were not touched. The default rows are stage 6's and stage 2's numbers to the
digit, so the default body and the cvar-off game are unchanged.

**Seen in the frames** (the 1280x2160 portraits of the lean at full size, min and max beside the
default, cropped to the part the slider moves; LegLift and walk on their sheets and in crops):

| slider | min and max | verdict |
|---|---|---|
| Height | the whole figure, suit and hair with it; soles on the sand at both ends | works |
| Head | head, face, ears and hair scale as one; nothing detaches, no step at the neck | works |
| Neck | shorter, and 4.5 cm longer at 1.30: a long even neck, no seam between the Face and Body meshes, the scoop neckline stays put | works |
| Shoulders | narrower; at 1.20 broad, the collarbones and the neckline widen with them, no pinch at the armpit | works |
| Ribcage | the ribcage and the bust narrow and widen; the keyhole changes width, not height (after the fix above) | works |
| Waist | a wasp waist at 0.60, a thick one at 1.20 where the belt stands a little proud at the sides (garment grew 1.47 cm); hip cutouts stay on the skin | works; LegLift hem at the 0.10 cm limit at 1.20 |
| Hips | at 1.50 a very wide pelvis with the thighs set apart and a web of suit across the gap at the crotch. The current body does the same | works (the slider's own look at 1.50) |
| Leg length | knees and cuffs move, the feet do not: same place on the sand in all three frames | works in the lean; see the driver's float and slide above |
| Thighs | thin thighs under her hips at 0.80 with a gap at the crotch (same on the current body), heavy at 1.40; knees clean | works |
| Calves | thin and thick calves, the ankle cuff follows | works |
| Arm length | wrists and cuffs move; the hand stays on the ledge in the lean and on the calf in LegLift | works |
| Arms | thinner and thicker, elbows clean, nothing seen at the armpit joint that is left inheriting | works |
| Hands | at 1.15 large hands (the MetaHuman's are already 13% longer than hers), the cuff stays on the forearm. **In LegLift the fingertips come through the lifted calf**; the current body's hand at 1.15 rests on it | works with a flaw |
| Feet | smaller and larger feet flat on the sand | works |
| small and slight | reads as one slim figure in the lean and the walk | works |
| tall and strong | reads as one big figure; in LegLift the hand sinks into the calf (the current body's does too) and the hip hem fails the poke check | works with a flaw |

Not the sliders: with the legs at 0.90 and on the small body her hands cannot be seen from the front
in the lean (the arms end at the cuffs) on both bodies; in the lean one foot now and then rests on
its toes (her idle), on both bodies. Neither was looked into.


**Skin tone.** One material draws all of her skin: the body, every face LOD (slots 0, 9, 11-14) and
the copies worn under outfits (`MI_Body_<Outfit>` on stage 3's `M_Body_Under`, `MI_Head_Bare`) are
instances of `M_skin_unified_baked` or its masked copy, and all of them have "Use Global Adjustments
Post-Bake" on and "Basecolor Global Multiply Post-Bake" at white. `UVisexMHBodyComponent::ApplySkin`
puts a dynamic instance on each skin slot (found by that parameter, so an outfit's body material is
picked up when it goes on) and sets the multiplier to the slider's colour over the default's colour
(`FVisexBodyProfile::SkinColor`, linear, per channel). At the default 0.25 that is exactly 1: the skin
is stage 6's, untouched. Multipliers: 0 -> (1.02, 1.20, 1.18), 0.5 -> (0.70, 0.60, 0.44), 0.75 ->
(0.38, 0.27, 0.17), 1 -> (0.12, 0.08, 0.06). It is the android skin's own ramp taken relative to its
default, so both bodies cover the same range. Nothing in the plugin's or the assembled materials is
edited, and no new asset is needed (the brief expected a copy to extend: the parameter was already
there). Seen (`stage7_tone.jpg`): face, neck, shoulders and the bare skin in the suit's keyhole move
together at every step; at 1 it is a deep brown that keeps its highlights; lips, the preset's eye
shadow and the ear darken with the rest. What a multiplier cannot do: the palms, soles and lips of a
dark skin stay as dark as the rest (real ones are lighter), and the subsurface profile is one for
every tone. Brows, lashes and the groom are not touched.

**Ground for the morph sliders (nothing built).**
- The assembled body mesh has no morph targets (0 on body and face, 4 LODs, checked in the editor), and the Creator's Python API has no morph step
  (`MetaHumanCharacterSkelMeshUtils.h` lists morph targets as a TODO). A re-assembly rewrites
  `SKM_MHC_Her_BodyMesh`, so anything added to it is lost each time and must be re-applied.
- What can add them from a script: Geometry Script's `GeometryScript_AssetUtils.
  copy_morph_target_to_skeletal_mesh(dynamic_mesh, skeletal_mesh, name, options, lod)` (the plugin
  is on: MetaHumanCharacter depends on it, and Python sees the function). It writes the morph into the LOD's mesh description
  and rebuilds, so it is real asset data (cooked, kept through a DDC rebuild), and it asks only for
  the same vertex count as the LOD. Not run here: the first thing to check is that the rebuild
  leaves the skin weights, the DNA user data and the vertex order as assembled.
- Recommended recipe: (1) deltas per body LOD 0 vertex in a file under `SourceArt/Character/MH/Morphs`,
  in the vertex order `mh_vertex_order.py` already guards. (2) Where the Creator has a control, take
  the delta from a body variant on a scratch character (same topology: variant minus base): Chest at
  a held Underbust for Bust, Bust Span for Bust spacing, Shoulder to Apex for Bust height, Hip at a
  held High Hip for Glutes, the Fat axis for Body fat, Thigh for Thigh gap. (3) For Abs (lines and
  pack), which no control gives, carry the old body's shape keys over with stage 3's landmark map:
  `mh_kit_tpose()` already gives every MetaHuman vertex its place on the old body's T-pose; take the
  closest point on the old skin, read the shape key's delta there (barycentric), and scale it back by
  the band's width, depth and height factors. The same route can stand in for any of (2) that reads
  wrong. (4) `Tools/mh_morphs.py`, run after every assembly like `mh_bare_skin.py`: copy LOD 0 to a
  dynamic mesh, add each delta, `copy_morph_target_to_skeletal_mesh`, save, then `mh_vertex_order.py`
  and the spike tour. (5) Runtime: `ApplyShape` sets each weight as slider minus default on the Body
  (the body is built at Bust 0.80, so less bust is a negative weight). (6) Outfits: export the skin
  with its morphs and `blender_outfit.py --body=mh` carries them to the fabric as it does for the old
  body; the cover mask does not change. LOD 0 only at first: a worn garment holds her at LOD 0.
- Risks: a variant also moves joints (the skeleton is fitted to each body), a morph does not, so
  large deltas (a much bigger bust) will sit a little off their corrective joints; the Face mesh
  carries a strip of chest, so a morph that reaches the collarbones (Body fat) needs the same delta
  on that strip or the seam opens.

**Captures** (`Docs/Research/metahuman/`, 3840 px wide, cells are the tour's 1280x2160 scene captures
with the level's grade, Silversuit worn, MetaHuman hair): `stage7_sliders_sheet_1.jpg` ... `_4.jpg`
(one cell per slider, min | default | max, the lean, one camera), `stage7_profiles.jpg` (small |
default | tall, the lean) and `stage7_walk_profiles.jpg` (the same three in the photo Walk),
`stage7_tone.jpg` (skin tone 0 | 0.25 | 0.5 | 0.75 | 1, head and shoulders, the camera turned to her
face each time). Reshoot: `Tools/mh_sliders.sh s7 1 -benchmark -fps=30`, then
`Tools/mh_slider_sheets.sh s7 lean stage7`. The frames come from a scene capture: lens flares and a
ghost of the hair hang in some cells, and the light is the lean's back light.

**Traps.**
- `ps -axo args | grep "MacOS/UnrealEditor "` came back empty twice while a `-game` run was alive (a
  build then ran under a finishing tour). Wait on `pgrep -x UnrealEditor`.
- zsh reads `$geo[a]` as an array subscript: write `${geo}[a]` in ffmpeg filter strings.
- A camera anchored to the driver's mesh origin is not a fixed camera: in the lean the origin moves
  with Leg length. The tour keeps the first setup's camera for the whole pose.
- The hip hem's poke number moves 0.00-0.09 cm within one pose from second to second: compare
  setups on the same fixed-step frame, not across runs.
- `FTransform` has no shear: scale goes down the chain per axis index. A joint whose axes are
  turned against its parent takes the parent's terms on the wrong axes, and RigLogic's local scale
  cannot carry a correction.

**Left open.**
- The seven morph sliders (recipe above). Until then Bust, Glutes, Abs, Body fat and Thigh gap do
  nothing on the MetaHuman.
- Hands 1.15 in LegLift (fingers through the calf); the tall body's hip hem (0.14 cm). An arm pin
  toward the driver's contact, like the foot pin, would close the first and the lean's 1.3 cm.
- `upperarm_in` takes Arms and Arm length on the wrong axes (42 degrees off, left alone).
- Not checked: the Settings sit and the other photo poses at the extremes; LOD 2 and further (the
  joints inherit there too, the four-joint fix still runs; not looked at); whether the Face's copies
  of the four fixed joints read the corrected scale (its chest strip showed nothing at Ribcage 0.85 /
  1.20, face stretch 1.11x); the Body page's own close-up cameras on the MetaHuman.
- The driver's float at short legs and slide at long legs (both bodies), for the movement branch.

### 2026-11-16 · Stage 7b · no eye shadow; the shape sliders and the jiggle on the MetaHuman body (captures `stage7b_*`)

Two steps. Commits 9fa8953 (the rebuild), b0811cb (tools, runtime, checks), then the assets, sheets and this log.

**Step 1: rebuilt without the eye shadow** (Tema: "Yes, remove it"; the preset's SoftSmokey read as
bruising in Lvl_Dust's light). `Tools/mh_create.py`'s `look` step now sets the eye makeup's type to
None (`EYE_MAKEUP = False`; eyes only: foundation, blush and lips are the preset's as before), then the
one clean pass `fresh, create, shape, face, look, hair, rig, textures, build`. The brief's step list left
`face` out; without it she would have come back with Ada's face, so it ran as in stage 6. Same TARGETS,
face template, ears, `SKIN_UV`, hair, brows, lashes, bare top. Sign-in was cached, no prompt.
- **Vertex order** (`mh_vertex_order.py`): body 30,455 and face 33,845 vertices, same triangles, moved
  0.000 cm on both. The Silversuit cut, the cover mask and the follower hold; the bare-skin maps were
  remade (`mh_bare_skin.py`: the head's changed with the bake, the body's did not).
- **`r.GPUSkin.Support16BitBoneIndex=True`** still in `Config/DefaultEngine.ini`; no line was added by the
  plugin's toast this time.
- **Spike and poke tour** (cvar on, Silversuit), the run before the rebuild in brackets: body pass,
  stretch 2.77x (2.70), creases 6.11x (5.42; this number moves 5.4-6.6 from run to run, see stages 3 and 6), grew 2.12 cm (2.03);
  face pass 1.38x (1.38); garment pass 1.73x (1.82); poke pass, 95 fabric vertices under drawn skin,
  deepest 0.08 cm on `pelvis` (129, 0.08).
- **Stage 7's bone-scale sliders**, a subset (`-visexsetups=Chest+Hips+Legs+Neck+Feet`, both ends, three
  poses): every number equals stage 7's table to the digit (default 1.48 / 2.14 / 1.61, Hips 1.50 creases
  5.86, Leg length capsule 79.9 / 88.9, hem poke 0.07 cm at most).
- **Eyes**: `stage7b_eyes_before_after.jpg` (3840 wide: the stage 6 portrait framing, both eyes in the
  photo Idle, one eye in the idle's light; before | after, same cameras, same fixed-step frame). The dark
  band over the lid and the smudge under the eye are gone; the lid is skin. What is left under the eye is
  the skin texture's own redness.
- **Brows at face LOD 2: not fixed.** Looked at for the 20 minutes allowed. The brows groom is strands
  at LOD 0-1 and cards at LOD 2-4 (visible), and the assembly bakes the brows into the LOD 2 and LOD 5-7
  skin maps as well (`T_PreBakedGroom_*`: pipeline intermediates, "can now be deleted" in
  `MetaHumanDefaultEditorPipelineBase.cpp`; they stay dirty in memory after a build, cannot be saved and
  are not the cause). A LOD 2 close-up after the rebuild (`visex.Body.MetaHuman.LOD=2`) shows the hair as
  cards, no lashes, and the fringe over the brow line from every camera tried, so it could not be told
  whether the brow cards draw. No setting was found. In the game it does not show: she is held at LOD 0
  whenever a garment is worn, and now whenever the morphs are live (below).

**Step 2: the shape sliders.** With the cvar on, Bust, Bust height, Bust spacing, Glutes, Abs, Body fat
and Thigh gap now reshape the MetaHuman body, the Silversuit follows them, and the driver's jiggle
springs move her bust and seat. `visex.Body.MetaHuman.Morphs 0` gives the body as built. Cvar off:
`UVisexBodyComponent` and the driver are untouched (the only shared code that changed is the QA tour).

**The recipe's first question: does adding a morph keep the mesh? Yes.** Geometry Script's
`copy_morph_target_to_skeletal_mesh` on the assembled body (LOD 0's source model), tried with one test
morph before anything was built on it: 0.6 s per morph; the skin weights in the mesh description are
the same before and after (a hash over every fifth vertex's weights, up to 16 influences each); the
`DNAAsset` user data and the post-process AnimBP are still on the mesh; `mh_vertex_order.py` reads 0.000
cm moved, order kept, with all 24 morphs on; and the tours pass with the same crease numbers as before,
so RigLogic still drives the correctives. The morphs are asset data on `SKM_MHC_Her_BodyMesh` and are
lost at every assembly: `Tools/mh_morphs.py` (`STEP='apply'`, 15 s) puts them back from
`SourceArt/Character/MH/Morphs/mh_morphs.json`, and stops if the body's vertices are not the ones the
deltas were made on.

**Where the deltas come from** (`Tools/mh_morph_build.py`, run through `blender_outfit.py --body=mh
--mh-morphs`, 40 s):
- **Creator variants.** `mh_morphs.py STEP='variants'` copies the preset to a scratch character, shapes
  it once like the build (TARGETS plus one control moved), reads the preview body and deletes the
  scratch. The `base` variant comes back as the assembled body exactly: 30,455 vertices, the same
  order, 0.000 cm. So a variant less another is a delta in the mesh's own vertex order, with nothing to
  match. Each is kept inside the old slider's footprint (where today's shape key moves the skin,
  faded out round it), because a control moves more than its name says (Chest -9 cm also lifts the bust
  4 cm and shortens the front; Hip +7 moves every vertex 1.5 cm on average).
- **The old shape keys, by landmark**, for what no control gives (abs lines and pack, the 16 jiggle
  keys): stage 3's map (`mh_kit_tpose`) puts each MetaHuman vertex on the old body's T-pose; the key's
  delta is read at the closest point of the old skin on the same side of her (barycentric; the body sits
  0.80 cm from the old skin at the median, 2.64 at the 95th percentile, torso only) and scaled back by
  the band's width, depth and height. Arms are left out (the T-pose turns them).
- **Size.** Each Creator delta is sized against the old key carried over the same way, so a slider moves
  her about as far as today: by root mean square over the footprint (Bust, Glutes, Thigh gap), by the
  peak (Body fat), by the travel of the bust's tips (Bust height, Bust spacing). A least-squares fit
  shrinks a delta by the cosine between the two shapes and was dropped.

| morph (slider) | source | how like the old key (cosine) | at most per unit | at the slider's ends |
|---|---|---|---|---|
| `Bust` | Creator: Chest 93.8 against 84.8, Underbust, Bust Span and Waist held | 0.83 | 6.3 cm (old key 8.0) | 0: tips 4.5 cm back; 1: 1.1 cm forward |
| `BustLift` (Bust height) | Creator: Shoulder to Apex +3 cm | 0.86 | 2.5 cm (3.0); tips 2.0 cm up | -1: tips 3.0 cm lower; 1: 1.0 higher |
| `BustApart` (Bust spacing) | Creator: Bust Span +3 cm, resized (below) | 0.79 | 1.2 cm (2.5); tips 0.8 cm apart | -1: each tip 2.0 cm in; 1: 0.2 out |
| `Glutes` | Creator: Hip +7 cm, High Hip held | 0.70 | 4.8 cm (5.1) | 0: 2.4 cm in; 1: 2.4 out |
| `Fat` (Body fat) | Creator: the Fat axis +1.2 with the girths let go | 0.52 | 1.6 cm (1.6) | 0: 0.6 cm in; 1: 1.0 out |
| `ThighGap` | Creator: Thigh -6 cm | 0.54 | 1.7 cm (1.6) | 1: 1.7 cm off the inner thigh |
| `AbsLines`, `AbsPack` | old keys | (themselves) | 0.71, 0.32 cm | Abs 0: lines -0.73, pack -0.12; Abs 1: pack +0.61 |
| `BustBounce{L,R}_{Up,Down}`, `BustSway{L,R}_{Out,In,Fwd,Back}`, `GluteBounce{L,R}_{Up,Down}` | old keys | (themselves) | 2.2, 1.3-1.9, 1.8 cm | the springs' |

**Bust spacing is a third of today's reach, on purpose (Tema's call if he wants more).** Today's key
takes each bust tip 5.6 cm toward the other at -1 and carves a cleavage on the way. Sized by root mean
square the Creator's shape took each tip 4.4 cm in, and two real breasts moved that far run through
each other and come out as one cone (seen on the first frames, bare and in the suit). It is sized so the minimum brings each tip 2.0 cm in, where they meet
(`SPAN_IN` in `mh_morph_build.py`). The slider's range is the same; its maximum moves almost nothing,
as on the current body (the default is 0.8 of a range that ends at 1).

**Runtime** (`UVisexMHBodyComponent::ApplyMorphs`, every frame after `ApplyShape`). Each weight is the
slider less its default, since she was built at the defaults: Bust 0 is -0.80, Glutes 0..1 is -0.5..0.5.
Bust spacing keeps the driver's factor (`1.2 x min(Bust / 0.7, 1.2)`: less bust, less to move). Abs is
`FVisexBodyProfile::AbsWeights` now less at the defaults (lines 0.734, pack 0.122), so Body fat still
blurs the abs. The jiggle is not a second set of springs: `UVisexBodyComponent::TickJiggle` already
writes its weights on the driver's mesh every frame, and they are read from there. Its bust bounce
carries the standing bust height (`BustLift x BustLiftRange`), which is the `BustLift` morph here, so
that part is taken out and the swing kept. Negative weights are plain extrapolation; nothing clamps.
The morphs exist on body LOD 0 only, so while they are live she is held at LOD 0, as she already was
under a garment (face and grooms with her; cost not measured).

**The neck seam cannot open.** The Body's only open edge is the neck opening it shares with the Face
mesh (92 vertices, 131-142 cm up: it comes down to the top of the chest). Every delta is faded to
nothing from 7 cm to 2 cm off that edge. Read back from the file: no morph moves a vertex nearer than
2.6 cm to it, and within 4 cm none moves more than 0.10 cm per unit (Bust, Bust spacing). The Face mesh
gets no morphs and needs none. Seen: the collarbones and the top of the chest run through at every
slider end on the bare sheets.

**The Silversuit follows.** `mh_outfit_body.py` reads the same file onto the split skin and `finalize`
hands the deltas to the fabric like the old body's shape keys (each split vertex takes the mean of
the two body vertices it sits between); `mh_outfit.py` imports morph targets now. In the game the
garment is a Leader Pose follower and takes the Body's morph weights by name. The cover mask and the
hem logic did not change. The build prints each morph's worst edge stretch on the fabric at weight
-1 and +1 (Bust 2.36x, Glutes 1.94x, the rest under 1.9x): a torn edge shows there, not in the game.

**After an assembly: `Tools/mh_after_build.sh`** (editor open, right after `mh_create.py`'s pass) runs
what hangs on the assembled meshes in order: bare-skin maps, skin export, the morphs back on the body
mesh, the Silversuit re-cut and import. With `shape` (new TARGETS: the body's vertices moved) it first
makes the deltas again (base, seven variants, `mh_morph_build.py`). Not run end to end yet: each of its
steps was run by hand this stage, in that order.

**The check** (`Tools/mh_sliders.sh m7 1 -benchmark -fps=30 -visexsetups=shape ...`, see its header). The
spike and poke check skinned the reference shape only (`ComputeSkinnedPositions` knows no morphs), so it
would have passed anything: it now adds the worn morphs to body and garment, each delta turned by its
vertex's own blend of bone matrices (`AddMorphs` in `VisexMHSpikes.cpp`), with the weights read from
the Body. Limits as before: stretch 3x, growth 3 cm, fabric 0.10 cm under drawn skin. Each of the seven
sliders at its minimum and maximum, in the menu lean, the photo LegLift and the photo Walk, fixed 30 fps
step, Silversuit worn (body stretch, creases and growth as lean / LegLift / walk; hem poke is fabric
vertices under drawn skin and the deepest, same order):

| setup | body stretch | creases | grew cm | garment stretch | hem poke: vertices, cm |
|---|---|---|---|---|---|
| default | 1.48 / 2.14 / 1.65 | 1.83 / 4.31 / 2.71 | 0.58 / 1.36 / 1.04 | 1.18 / 1.35 / 1.36 | 0 / 0 / 0 |
| Bust 0 / 1 | 1.48, 1.50 / 2.14 / 1.65 | 1.86, 1.81 / 4.31 / 2.71 | 0.58, 0.60 / 1.36 / 1.04 | 1.18 / 1.35 / 1.36 | 0 / 10, 0.07 at 0 / 0 |
| Bust height -1 / 1 | 1.64, 1.49 / 2.14 / 1.71, 1.65 | 1.81, 1.87 / 4.31 / 2.71 | 0.99, 0.59 / 1.36 / 1.23, 1.04 | 1.42, 1.18 / 1.49, 1.35 / 1.53, 1.36 | 0 / 3, 0.02 and 8, 0.04 / 0 |
| Bust spacing -1 / 1 | 1.48, 1.50 / 2.14 / 1.65 | 2.02, 1.92 / 4.31 / 2.71 | 0.59 / 1.36 / 1.04 | 1.23, 1.18 / 1.35 / 1.36 | 0 / 10, 0.06 at 1 / 0 |
| Glutes 0 / 1 | 1.41, 1.58 / 2.09, 2.19 / 1.54, 1.82 | 1.86, 1.81 / 4.31 / 2.71 | 0.49, 0.76 / 1.31, 1.43 / 0.66, 1.31 | 1.13, 1.29 / 1.35, 1.38 / 1.27, 1.54 | 0 / 10, 0.07 at 0 / 0 |
| Abs 0 / 1 | 1.48, 1.50 / 2.14 / 1.65 | 1.86, 1.82 / 4.31 / 2.71, 2.60 | 0.57, 0.60 / 1.36 / 1.04 | 1.18 / 1.35 / 1.36 | 0 / 0 / 0 |
| Body fat 0 / 1 | 1.49 / 2.14 / 1.65 | 1.79 / 4.31 / 2.60 | 0.59 / 1.36 / 1.04 | 1.18 / 1.35 / 1.36 | 0 / 16, 0.06 at 1 / 0 |
| Thigh gap 0 / 1 | 1.50, 1.49 / 2.14, 2.11 / 1.65, 1.67 | 1.81, 1.77 / 4.31, 4.38 / 2.71, 2.83 | 0.60 / 1.36, 1.35 / 1.04, 1.06 | 1.18, 1.20 / 1.35 / 1.36, 1.38 | 0 / 10, 0.06 at 1 / 0 |

**Every slider end passes in all three poses**, and on bare skin too (the same body numbers; no garment
lines). Worst over the 15 setups: body stretch 2.19x and growth 1.43 cm (LegLift, Glutes 1), creases
4.38x (LegLift, Thigh gap 1), garment stretch 1.54x (walk, Glutes 1), hem poke 0.07 cm (LegLift, Bust 0
and Glutes 0; 16 vertices at most, Body fat 1), face stretch 1.22x. The hem numbers are stage 4b's hip hem, which
reads 0.00-0.09 cm from second to second on the default body in LegLift: no slider end adds to it.
The walk's default row grew from stage 7's 1.61x / 0.74 cm to 1.65x / 1.04 cm: that is the jiggle,
which the check now sees. Soles in the lean 0.20-0.34 cm over the sand at every setup. These are the
numbers of the deltas as committed (the Glutes row from a re-run after its delta was smoothed, the rest
from the run before that, which the smoothing does not touch); the sizing before it (larger Bust
spacing, smaller Body fat) was toured the same way and passed as well.

**Traps.**
- `-visexclose=idle:...` also fires on `photo_00_Idle` (the match ignores case), and the stage 6
  portrait's `idle` frame is the gaps / feet tour's, not the spike tour's.
- **`MH_BASE_VERTS` is not "the body's own vertices come first".** bmesh gives the slots of the
  welded-away duplicates to new vertices: about 1,500 split vertices carry a number under it and as
  many of the body's own (soles and toes) sit above it, and `MH_PARENTS` makes those split vertices
  their own parent. Harmless for stage 4b's hems; for morphs it left 1,500 fabric vertices behind and
  the first garment tore (stretch 5.0x, creases 13x at Bust 0). `mh_morph_deltas` matches every vertex by
  place and reads a split vertex's two corners from the mesh's edges.
- The fabric's morphs are checked at build time (edge stretch per morph at -1 and +1). Import and
  tour to find a torn garment costs ten minutes.
- A least-squares fit of one shape to another returns the cosine times the size: Body fat came out at
  a third of today's depth. And matching the size of two different shapes over a region is not the same
  as matching what the slider is for (Bust spacing).
- The Creator's girth controls pull on each other. Body fat with the girths pinned has nowhere to
  go (stage 3 saw the same): both ends of that variant are solved with the girths let go.
- Importing a garment with morph targets leaves `metahuman_base_skel` dirty. It was not saved and the
  garment follows the morphs without it (weights go by name), so a re-assembly's fresh skeleton is fine.
- The photo Idle is not a still pose (she shifts and turns) and the walk never shows two setups in
  the same step: shapes are compared in the reference pose (`visex.Body.MetaHuman.TestPose ref`, the
  tour's "stand" pass). The torso and side framings aim at standing height and miss her in LegLift.
- The system `python3` has no numpy; Blender's has. The tour's PNGs make this ffmpeg print "Invalid
  PNG signature" and decode anyway.
- `Tools/ue.py` gives up after `UE_WAIT`, but its job stays in `Tools/.bridge/in` and runs when the
  editor gets to it: a first launch after a C++ build took over four minutes.
- A killed editor writes `Saved/Autosaves/PackageRestoreData.json` every time: move it aside before the
  next launch (stage 2's trap), not once.

**The standard tour on the committed assets** (`-visexqa -visexspikes`, cvar on, Silversuit, default
sliders, the morphs live and the jiggle running): body pass, stretch 2.67x, creases 5.39x, grew 2.00 cm;
face pass 1.38x; garment pass 1.85x; poke pass, 95 fabric vertices under drawn skin, deepest 0.08 cm on
`pelvis`. Vertex order after the last apply: 0.000 cm, kept. **Cvar off**: the same tour with
`visex.Body.MetaHuman=0` runs through with no MetaHuman spawned or logged and nothing to measure; the
source diff of this stage touches `VisexMHBody.*`, `VisexMHSpikes.cpp` and the QA tour only, and
`ApplyMorphs` runs only while the MetaHuman is shown. That is a reading of the code and one clean run,
not a frame-by-frame comparison with the old build.

**Seen** (every sheet at full size, and min | default | max crops of the tour's 1280x2160 frames):

| slider | what it does on her | verdict |
|---|---|---|
| Bust | 0 is a small natural bust, not a flat chest (the tips 4.5 cm back; the old key's 0 is 5.4); 1 a little fuller than the default. Smooth on skin and under the suit, the keyhole opens and closes with it, no step where the delta fades out on the upper chest | works |
| Bust height | -1 hangs 3 cm lower and heavier, the keyhole's lower end closes; 1 sits 1 cm higher. The tips travel two thirds of today's (2.0 against 3.0 cm per unit) | works |
| Bust spacing | -1 brings the breasts together until they meet, 1 is hardly different from the default | works, at a third of today's reach (above) |
| Glutes | 0 a flat seat, 1 a round one, in profile and from behind; it also narrows and widens the hips a little (the Creator's Hip is a girth). In the first version the top of the cleft (94-98 cm up) came out 0.1 cm proud of the cheeks at 0; the delta is now spread across the cleft and the groove reads 0.0-0.2 cm there. In the suit the cleft still shows as a dark slit at 0: that is the body's own groove (2.3 cm deep lower down) between cheeks flat enough to mirror the sky | works |
| Abs | 0 a smooth belly, 1 lines down the middle and under the ribs. On skin it reads; under the suit it barely shows. The lines are the old key's on a mesh 0.7 cm coarse: soft, not faceted | works, subtle |
| Body fat | 0 a slimmer waist and belly, 1 a fuller one (1.0 cm at most). Spread over belly, flanks and hips like a real body rather than today's belly | works, subtle |
| Thigh gap | 1 opens a gap up to the crotch. It thins the upper thigh all round, not only the inside (the Creator's Thigh is a girth; cosine 0.54 with the old key). In the suit the inner thigh's highlight breaks into two or three flat patches at 1 | works with a flaw |

On bare skin (sheets 3 and 4) nothing is dented or stepped at any end. There are no nipples or navel
detail beyond the texture's: Epic's bare skin.

**The jiggle** (`stage7b_jiggle.mp4`, 3840x2160, 30 fps, 10 s: the photo Walk in the Silversuit, then on
bare skin; a scene capture with the grade from her right, travelling with her capsule; `-visexclip`).
Bust and seat swing with the driver's springs, each breast on its own, and the suit goes with the skin.
With the morphs live the walk's default row in the check grew from stage 7's 1.61x / 0.74 cm to
1.65x / 1.04 cm: the bounce and sway deltas (up to 2.2 cm per unit) are on the mesh and moving. Two
things about the clip are not the jiggle's: the photo Walk lifts and drops her whole torso about 9 cm
twice a second and turns it with each step (the driver's catwalk walk; the feet meter reads
hip bobs of 6-7 cm on other walks too), so the tissue is thrown hard; and the hair's ends flick in
front of the shoulder. Judged from frames laid side by side and the encoded clip's frame count, not
watched in motion.

**Captures** (`Docs/Research/metahuman/`, 3840 px wide; cells are the tour's 1280x2160 scene captures
with the level's grade, MetaHuman hair): `stage7b_eyes_before_after.jpg`; `stage7b_shape_sheet_1.jpg`
(Silversuit, menu lean, from her front right) and `_2` (Silversuit, standing in the reference pose:
profile, front right, behind, thighs); `_3` and `_4` the same two on bare skin (no outfit: Epic's bare
chest, the baked briefs); `_5` Bust, Glutes, Body fat and Thigh gap in the photo LegLift and Walk, full
figure; `stage7b_jiggle.mp4`. One cell per slider: min | default | max. Reshoot: the two
`Tools/mh_sliders.sh` lines in that script's header, then `Tools/mh_shape_sheets.sh <suit run> <bare
run>`. The lean frames differ a little from cell to cell because she looks about in the lean; the
standing ones do not move at all.

**Left open.**
- **Tema's calls**: Bust spacing's reach (a third of today's); whether Bust 0 should go flatter (a second,
  smaller Creator variant would give it a real shape instead of more extrapolation); Thigh gap as a
  whole-thigh thinning against today's inside-only cut (`--mh-old=ThighGap` on the build line is meant to
  take the old key instead, for any slider named there; the option was written and not run).
- A morph moves skin, not joints: the corrective and twist joints stay where the built body has them.
  Nothing showed at these sizes; a much larger bust or seat would sit off its correctives.
- The morphs are on body LOD 0 only and hold her there while they are on (the garment already did).
  Cost at a distance, face and grooms included, not measured. LOD 1-3 would need the deltas carried to
  their vertices.
- Only the Silversuit exists on this body. Any other outfit cut with `--body=mh` carries the morphs the
  same way; none was built. There are no fit morphs (`Fit_*`) here: fabric follows the skin exactly.
- The driver's ArmUp / ThighUp helper morphs have no counterpart (RigLogic's correctives do that job).
- Not checked: two or more shape sliders pushed together, or with the bone-scale sliders at their ends
  (the tall body's hip hem was already over the limit in stage 7); the Settings sit and the other photo
  poses at slider ends; the Body page's own close-up cameras and live dragging of a slider (the tour
  sets values); a packaged build (`/Game/Character/MH` is still not cooked); `Tools/mh_after_build.sh` end
  to end; the hips-and-thighs TARGETS change that was asked about (no new TARGETS arrived, so they are
  as in stage 3).
- Brows at face LOD 2 (step 1 above).

### 2026-11-16 · Stage 8 · her default bust softened; the flex layer (captures `stage8_*`)

Two steps, in this order because outfits and every delta are cut on the base. Commits c62d29f (the flex
builder, work in progress), 70a1021 (the bust), then the flex layer, the captures and this log.

**Step A: her default bust** (Tema, on `stage7b_shape_sheet_3.jpg`: "Yes, soften it"; hips, thighs and
everything else "Keep as is"). At the default it read as two high round balls starting at the
collarbone. Now a teardrop: the tip 2.9 cm lower and 0.5 cm less forward (17.9 cm forward, 124.1 up ->
17.4, 121.2), a longer upper slope, fuller below. Still a full bust: a shape fix, not a size cut.
`stage8_bust_before_after.jpg` (bare skin and Silversuit, front three-quarter and profile, old | new, the
reference pose, one camera and light).

**Route: a correction baked into the base, not new TARGETS.** The Creator's controls were tried first, on
throwaway variants (`mh_morphs.py STEP='variants'` with other VARIANTS; her build was not touched):
Shoulder to Apex +6 and +9 cm, with Chest -2 / -3, Underbust +1.5, Front Interscye Length +1.5. They do
lower the bust, but only 1.2-1.6 cm, with *more* projection (+0.8 cm) and a steeper upper chest; and the
same solve moves the rest of her: legs and hips 0.4 cm on average and 1.0 at most, the arms 1.0 (the
shoulder joints go 1 cm out), the back up to 1.7, the head 0.6 up (Shoulder to Apex +6; +9 is half as
much again). "Everything else stays" rules that out, and a re-solve would also have needed a new face fit.
So `Tools/mh_base_fix.py` makes the shape from the bust morphs stage 7b already has (the same Creator
variants, inside the bust's footprint, faded at the neck opening):

    fix = -1.0 x BustLift + (-0.28 - 0.15 x above-the-tip) x Bust        (852 vertices, 1.87 cm at most)

BustLift is the Creator's Shoulder to Apex direction, Bust its Chest direction; the last term takes a
little more off the upper pole. Chosen from four candidates rendered in Blender beside the base (profile,
three-quarter, front), then looked at in the game. `Tools/mh_morphs.py STEP='apply'` writes it into body
LOD 0's own vertices before the morphs go on (Geometry Script: positions set, normals recomputed where
the skin moved, `copy_mesh_to_skeletal_mesh` with `use_original_vertex_order`). Tried on a copy of the mesh
first: 30,455 vertices and 60,816 triangles in the same order, skin weights identical on every third
vertex (10,152 compared, up to 16 influences, worst difference 0.00000), `DNAAsset` user data and the
post-process AnimBP still there, 4 LODs, one section, 32,334 render vertices as before. The skin itself:
no edge stretched past 1.36x, two short edges in the fold under the bust at 0.49x.
- `mh_morphs.json` now holds `built` (as assembled), `base` (built + fix: what the mesh holds, and what the
  outfit cut and the flex builder match the skin FBX to) and `fix`. `STEP='unfix'` takes it off again.
- **Not done: body LOD 1-3 still have the old bust.** She is held at LOD 0 whenever the morphs are live or
  a garment is worn, so it does not show in the game; with `visex.Body.MetaHuman.Morphs 0` and no outfit
  it would pop at a distance. Carrying the delta to the other LODs by nearest vertex is the fix.
- Untried third route: `conform_body` (the Creator fitted to given body vertices, like the face). It would
  make the shape native at every LOD, at the price of a fixed body and an unknown fit at the neck.

**The sliders round the new default.** Both ends of Bust, Bust height and Bust spacing again, lean / LegLift
/ walk / reference pose, Silversuit (`stage8_bust_sliders.jpg`):
- **Bust height resized to 0.30 of its reach.** Unchanged (and at 0.67) its minimum, now 4 cm under the old
  default, put the lower pole over the keyhole's hem: poke FAIL, 0.17-0.20 cm on `spine_04` in all three
  poses. At 0.40 still 0.12 in LegLift. At 0.30 the slider ends 2.9 cm under the old default, where its old
  minimum was (0.9 cm down and 0.3 up from the new one): 0.07 cm. Tema's call if he wants more range; it
  needs that hem lowered where it meets the fold under the bust.
- **Bust resized to 0.65**, so 0 is the small bust it was and not that less the correction again (which
  dented the inner breast). Bust spacing untouched: at -1 the breasts still just meet.
- Every end passes: body stretch 2.14x, garment 1.36x, hem poke 0.07 cm at most (LegLift, Bust 0 and Bust
  height -1); the default body reads 0 fabric vertices under drawn skin in all four poses.

**Checked after the bake.** Vertex order (`mh_vertex_order.py`): body count and triangles the same, 788
vertices moved, 1.873 cm at most (the bust, as meant); face 0.000 cm, order kept.
`r.GPUSkin.Support16BitBoneIndex=True` in the committed config and 1 in the editor. Standard spike and
poke tour (`-visexqa -visexspikes`, cvar on, Silversuit, the flex drivers live): body pass, stretch 2.83x,
creases 6.49x, grew 2.19 cm; face pass 1.38x; garment pass 2.08x; **poke pass**, 95 fabric vertices under
drawn skin, deepest 0.08 cm on `pelvis` (stage 7b: 2.67x, 5.39x, 2.00 cm; 1.38x; 1.85x; 95, 0.08).

**`Tools/mh_after_build.sh shape` ran end to end** for the first time (about 4 minutes, editor open). Nothing
in it failed; its order had to change, because the correction comes before the skin export and the flex
deltas after it: bare-skin maps; the body back as built (`unfix`), its skin FBX, base and ten Creator
variants (the eight of stage 7b plus `musc0` / `musc1`), `mh_morph_build.py`, `mh_base_fix.py`; correction
and morphs on the mesh; skin export again (the corrected base); `mh_flex_build.py` and the morphs again;
the Silversuit. The remade deltas equal the committed ones (base variant 0.000 cm from the body). The
bare-skin textures came back byte-different with the same PNGs and were not committed.
`Tools/mh_recut.sh` is the short loop after a change to `mh_morphs.json` alone (morphs, suit: 3 minutes).

**Step 1: the flex layer.** With the cvar on, 21 muscle bellies tense with what she does, on skin and under
the Silversuit. `visex.Body.MetaHuman.Flex 0` switches it off (at once, so one frame can be compared),
`visex.Body.MetaHuman.FlexScale` is its strength (1), `visex.Body.MetaHuman.FlexTest` holds weights for
checks ("1": every morph; "Quad=1,BicepsL=0.5"; "log": the drivers once a second). With
`visex.Body.MetaHuman 0` none of it runs: `ApplyFlex` is called only while the MetaHuman is shown.

**The morphs** (`Tools/mh_flex_build.py`, Blender, 10 s; in `mh_morphs.json` beside the shape morphs, so
`mh_morphs.py apply`, the Silversuit cut and the spike check take them as they are). The Creator's
Muscularity axis (the body solved at -1.0 and +0.5 with the girths let go: `musc0`, `musc1`) says where a
real body puts muscle; per limb each cross section's own shift is taken out (the solve moves joints too),
the rest is read along the skin's normal, and a soft window per muscle (along the bone, round it, gated
by skin weight so an arm's window cannot reach the ribs) cuts one belly out. Half of each shape is that,
half a plain bump of the window, sized to a peak height. The variant alone is nearly an even
thickening (upper arm 1.0-1.3 cm front and back, 0.6 at the sides), so the windows do the shaping.

| morph (left and right) | where | peak at weight 1 | on the fabric at +-1 |
|---|---|---|---|
| `FlexBiceps` | front of the upper arm, 20-86% down it | 0.85 cm | 1.26x |
| `FlexTriceps` | back of the upper arm | 0.80 | 1.22x |
| `FlexForearm` | under the elbow, outside and front; nothing in the last third | 0.45 | 1.19x |
| `FlexDeltoid` | the shoulder's cap | 0.65 | 1.15x |
| `FlexQuad` | front of the thigh, with the teardrop over the inside of the knee | 0.80 | 1.12x |
| `FlexHam` | back of the thigh | 0.55 | 1.07x |
| `FlexCalf` | the gastrocnemius; the Achilles and ankle stay | 0.65 | 1.12x |
| `FlexGlute` | round the cheek's apex, off the cleft, with a little lift | 0.70 | 1.10x |
| `FlexOblique` | between ribs and hip bone | 0.35 | 1.07x |
| `FlexLat` | under the armpit, on the back's side | 0.50 | 1.17x |
| `FlexAbs` (one) | the sliders' own lines and pack (0.6 and 1.0), stopped under the bust | 0.40 | 1.18x |

**Dropped: the neck cords.** The Body mesh ends at the top of the chest (its neck opening is 131-142 cm up);
the sternocleidomastoid is on the Face mesh, which carries no morphs, and a delta there would sit on the
seam the brief forbids. It needs a morph on the Face mesh's neck (same Geometry Script call, 33,845
vertices, 8 LODs) and its own seam check. Read back from the file: no flex delta comes nearer than 3.8 cm
to the neck opening, and no hand or foot vertex moves (0.000 cm; wrists and ankles are faded out).

**The drivers** (`UVisexMHBodyComponent::ApplyFlex`, `Source/visex/VisexMHBody.cpp`, every frame after
`ApplyMorphs`). Read from the MetaHuman body's own last pose in component space (so a TestPose flexes
too) and from the driver's foot plant state; it moves no bone and does not touch the foot pipeline. Each
weight eases toward its target (7 a second up, 4 down), so nothing pops. Angles in degrees:

| muscle | by angle | by load or speed |
|---|---|---|
| biceps | elbow bend 40 -> 125 | |
| triceps | | the propping arm (below) |
| forearm | 0.55 x elbow bend 50 -> 125; 0.6 x wrist bend 30 -> 65 | 0.8 x prop |
| deltoid | 0.9 x upper arm raised from the chest's down 45 -> 110 | 0.7 x prop |
| lat | 0.7 x arm raised 115 -> 165 | 0.8 x prop |
| quad | | the leg's tension x (0.75 + 0.25 x knee bend 15 -> 60) |
| hamstring | 0.8 x knee bend 45 -> 120; 0.35 x hip extension 5 -> 30 | + 0.35 x push-off |
| calf | plantarflexion past the reference ankle 8 -> 35 (tiptoe) | 0.6 x tension, + 0.4 x push-off |
| glute | 0.8 x hip extension 6 -> 30 | 0.6 x tension, + 0.3 x push-off |
| oblique | 0.8 x chest turned away from that side 8 -> 35; 0.7 x bent to that side 6 -> 25 | |
| abs | 0.85 x bent forward 8 -> 35; 0.5 x chest turned 10 -> 40 | |

- **A leg's tension** is its share of her weight, 0.3 -> 0.8. The share: a foot is down when the driver's
  plant pass says so (`UVisexAnimInstance::PlantedL/R`, read only) or its ball is as low as the other's;
  the weight goes to the foot the pelvis stands over (its place along the line between the ankles, 0.2 ->
  0.8); a pelvis under 0.72 of the leg's length over the feet (sitting, lying) puts none on the legs.
  Standing evenly gives both legs 0.5, a tension of 0.26. In the photo Idle the split wanders 1.00 / 0.00
  to 0.51 / 0.49 from second to second (she shifts her weight), which is why the ramp is that wide.
- **The propping arm**: the hand bent back 30 -> 55, or the chest leaning back 4 -> 10 (x 0.85), times an
  arm not folded (elbow under 60 -> 100), a hand 10 -> 25 cm below its shoulder, and not walking. The menu
  lean measures chest 10-12 back, elbows 35-50, wrists 20-29, hands 0-7 cm behind the shoulders: so it
  is the lean of the chest that tells it, not the wrist. Both arms tense at 0.85 there.
- **Push-off**: speed 30 -> 110 cm/s, times the foot 4 -> 22 cm behind the pelvis along her heading, while
  it is down.

**Not doubling the correctives.** RigLogic's `upperarm_bicep` / `upperarm_tricep` and knee joints fill the
joint; the flex morphs are windows on the bellies between joints (the biceps window is 0 within a fifth
of the arm's length of either joint). In the bent-arm and bent-knee pairs the elbow and knee look the
same off and on; what changes is the upper arm's and the thigh's fullness.

**Checks at full flex** (`FlexTest=1`: all 21 morphs at 1 whatever the pose; Silversuit; fixed 30 fps step;
limits stretch 3x, growth 3 cm, fabric 0.10 cm under drawn skin):
- The standard tour, every pose she has (arrival, lean, Settings sit, the eleven photo motions with their
  bent arms and lifted legs, Start walk, idle, height 145 / 185): **body pass**, stretch 2.84x (Ledge; 2.83
  with the drivers), creases 6.49x, grew 2.20 cm; face pass 1.38x; **garment pass** 2.12x; **poke pass**, 97
  fabric vertices under drawn skin, deepest 0.08 cm on `pelvis`. No pose line fails. Full flex adds 0.01x
  and 0.01 cm to the worst body numbers and 2 hem vertices: it does not load the folds that lead them.
- Full flex with the shape sliders it overlaps at both ends (Glutes, Abs, Thigh gap; lean / LegLift / walk,
  `Tools/mh_sliders.sh sf8 "1+visex.Body.MetaHuman.FlexTest=1" -benchmark -fps=30 -visexsetups=Glutes+Abs+ThighGap`):
  all 21 lines pass. Worst: body stretch 2.18x and growth 1.47 cm (LegLift, Glutes 1), creases 4.38x
  (LegLift, Thigh gap 1), garment 1.60x (walk, Glutes 1), hem poke 0.04 cm. One "settle" stretch between
  two setups read 0.10 cm on `pelvis` while the Glutes slider was jumping; no measured second did.
- On the fabric each flex morph stretches an edge 1.26x at most (the build's own check, weight +-1).
- The same build line reads Bust 5.19x and BustLift 2.09x at +-1 on the new base: that is past the
  sliders' reach (Bust runs -0.52 .. +0.13 of its morph now), and in the game their ends pass (above).

**Seen** (`stage8_flex_onoff_bare.jpg`, `stage8_flex_onoff_suit.jpg`: six poses, off | on in each cell, the
clips held while a pair is shot; 4K scene captures with the grade, cropped to their middle; looked at
at full size):

| group | what drives it there | verdict |
|---|---|---|
| quad | the leg that carries her (lean, photo Idle) | reads: the thigh fills and the teardrop over the knee stands out, the free leg stays soft. The clearest of all at game distance |
| calf | tiptoe; the carrying leg | reads: a belly high on the shin, on skin and as a wider highlight on the suit |
| biceps, forearm | elbow bent 95 | reads in profile: the upper arm's front fills. From in front it is slight |
| abs, obliques | chest turned 34 degrees | reads on skin (the middle line and the flank's edge); under the suit only the highlight's edge moves |
| triceps, deltoid | the menu lean's arms | weak: the upper arm is a little fuller behind. Seen in the pair, not something the menu camera shows |
| hamstring, glute, lat | knee bend, hip extension, load; an arm overhead or propping | on the mesh and in the checks; not looked at in a pair of their own |
| neck cords | | dropped (above) |

Nothing reads as lumps at these sizes. **Chrome**: no flex morph adds facets; the suit's knee highlight
breaks into four or five small patches in the lean, the same with flex off and on (the knee's own
geometry under mirror fabric, not this stage's). FlexScale 1 is what is shot; it was sized up once
(biceps 0.55 -> 0.85, triceps 0.40 -> 0.80, quad 0.65 -> 0.80) after the first pairs were too faint.

**The walk** (`stage8_flex_walk.mp4`: 3840x2160, 30 fps, 10 s, the photo Walk on bare legs, flex on, a camera
62 degrees round from her front, 310 cm away, level with her knees, travelling with her capsule; a warm
200 candela spot travels with her from 140 degrees round, so it rakes across the thigh and calf.
`stage8_flex_walk_onoff.mp4` is the same frames with flex off | on, each half the middle of the picture;
both from `Tools/mh_flex_clip.sh`, fixed 30 fps step, so the two runs walk the same steps). In stills
taken from both the stance leg's quad and the teardrop over the knee are fuller with flex on and the
swing leg is not; the difference per frame is small, the size of the idle pair's. **Not watched in
motion**: whether the firing reads at speed, and whether the load's hand-over from leg to leg is smooth
at every step, is judged from frames only.

**Step 2: skin creases, not started; the recipe.** What the body material exposes: the body draws with
`M_skin_unified_baked` (`MI_Body_Baked_VT`), the same material as the face. It has the rig's wrinkle
inputs (three animated normal maps `Normal Animated wm1..3` with their delta maps, three colour maps, and
83 scalars), but every one of those scalars is a *head* mask (`head_wm1_normal_head_wm1_blink_L`,
`..._neckStretch_L` ...), and on the body's instance `Use Animated Maps` is off and the wm textures are
flat placeholders (`T_Flat_N`). So the body has no wrinkle maps and no mask to drive one: it needs new
textures, which is past the two hours allowed. To build it:
1. Textures, in the body's UV layout (tile 1, u 1..2; `mh_bare_skin.py` already reads and writes it):
   one tangent-space normal map of fold lines (inner elbow, back of the knee, the waist's flank; the wrist
   if wanted) and one mask map with a channel per region. Bake them in Blender from the skin FBX: for
   each joint, the crease line is where the skin's two sides meet in a bent test pose
   (`_mh_test_poses()` in `mh_outfit_body.py` already bends wrists and hips; add elbow and knee), drawn
   as two or three parallel grooves a few millimetres wide, fading out along the line.
2. Material: `M_Body_Under` is already our copy of the skin material (stage 3) and `ApplySkin` already puts
   dynamic instances on every skin slot. Add to the copy: the crease normal blended over the baked normal
   (BlendAngleCorrectedNormals), its strength the mask channel times a scalar per region
   (`CreaseElbowL` ...). `MaterialEditingLibrary` can add and wire those nodes from a script. The bare body
   draws with the assembled original, so it has to wear the copy too (unmasked) for creases to show
   without an outfit.
3. Drivers: the angles `ApplyFlex` already has (elbow bend 60 -> 130, knee bend 60 -> 130, chest turn and side
   bend for the waist), set on the dynamic instances beside the tone.
4. Shoot off | on with `Tools/mh_flex.sh` (an elbow, a knee, the twist), and check the neck seam: the head
   uses the same material and must not get the body's maps.

**Cvar off.** A sliders tour with `visex.Body.MetaHuman=0` runs through with "no MetaHuman body" on every
line and not one `VisexMH:` or `VisexFlex:` line in the game's log; `ApplyFlex` sits behind the same
`bShown` as every other MetaHuman step. One short run and a reading of the code, not a frame comparison.

**Traps.**
- **The plugin's "missing project settings" toast was accepted again** (5 minutes into the first editor
  session; by whom is not known): `r.GPUSkin.UnlimitedBoneInfluences=True` was written into
  `Config/DefaultEngine.ini`, the dirty packages were **saved** (two scratch meshes of the base-write test
  and `Lvl_Studio`, dirty from the variants' preview actors) and the editor restarted itself with its
  old arguments. Undone: scratch files deleted, the level checked out again. For the rest of the stage
  the line stayed in the working tree so the toast had nothing to ask, and every editor and `-game` run
  got `-ini:Engine:[/Script/Engine.RendererSettings]:r.GPUSkin.UnlimitedBoneInfluences=False` (stage 5's
  way; the editor read 0). The line is **reverted and not committed**, so the next editor launch will show
  the toast again: pass the override, or expect a restart that saves whatever is dirty.
- A base check on every 97th vertex cannot tell the built body from the corrected one (none of the
  sampled vertices is on the bust): `mh_morphs.py` compares every vertex.
- A hash over the weights' list order changes when a mesh is written back although no weight did: sort
  each vertex's influences before comparing.
- zsh: `"$S:suit:..."` applies the `:s` modifier to `$S` and eats the text. `"${S}:suit"`.
- The photo Idle shifts her weight within a second: two shots 2 s apart are two poses. The flex tour
  holds her clips (`GlobalAnimRateScale 0`) while a pair is shot; the flex layer still eases on real time.
- In the menu lean her wrists are bent only 20-29 degrees and her hands are 0-7 cm behind the
  shoulders: neither tells a propping arm. The chest's 10-12 degrees of lean back does.
- A light added to Lvl_Dust for a capture: 6000 candela burns her white, 200 is a raking key.
- The clip tour and the flex tour leave their old frames in `Saved/QA`: a frame count includes them.
- This session's shell check refused `rm -f Tools/.bridge/in/*` (a glob removal). The inbox was empty;
  name the files to delete.

**Left open.**
- **Tema's calls**: the new default bust itself (`FIX_*` in `mh_base_fix.py`: one run of `mh_base_fix.py`,
  `mh_recut.sh` and the bust tour per try); the Bust height slider's short reach; FlexScale (1) and
  whether the lean's arms should read from the menu camera at all.
- Body LOD 1-3 carry the old bust (above). Neck cords (a Face-mesh morph). Skin creases (the recipe).
- Flex is on body LOD 0 only, like every morph; cost not measured.
- Not looked at in a pair of their own: hamstring, glute, lat, the deltoid with a raised arm. Not
  checked: flex with bone-scale sliders at their ends (a thicker or longer limb scales the belly with
  it, by skinning; no number), the Settings sit and the photo poses with the *drivers* on beyond the
  standard tour's pass, live dragging of a slider, a packaged build (`/Game/Character/MH` is still not
  cooked), any outfit but the Silversuit.
- The walk clip was not watched in motion (above). The propping test is a posture, not a contact: a
  pose that leans her chest back with straight arms hanging free would tense them too.

### 2026-11-16 · Stage 9a · the wardrobe on the MetaHuman body: the checklist only (stopped)

Stopped before the first cut (Tema: "let's skip outfits for now"). No outfit was built, no tool or asset
changed, no editor was opened. What is here is the checklist, read from `FVisexBodyProfile::Outfits()`
(`Source/visex/VisexBodyProfile.cpp`), the builders in `Tools/blender_outfit.py` and `Docs/CLOTHES.md`.

| outfit | in the game | what the builder really cuts | type | on the MetaHuman |
|---|---|---|---|---|
| Silversuit | wearable | `catsuit_set("Silversuit")`, kit openings | opaque tight | done (stages 3, 4b, 7b, 8) |
| Catsuit | wearable | `catsuit_set()`: same builder, no kit; adds a collar, zip tape and pull | opaque tight | next, the simplest |
| Harness | wearable | satin cups, collar, bib, thong; `Binding` tapes laid by rays (`line`, `taut`, `tube(flat=)`), rings; `Jersey` chap stockings | opaque set with straps | next batch |
| Mirror | wearable | a **sheer** mesh bodysuit and sheer opera gloves; satin collar, bandeau and brief (thong back); boning seams | sheer with opaque pieces | next batch, with the sheer ones |
| Cocoa | **locked** | satin bra, thong, belt, thigh bands, choker, two garter straps | opaque set with straps | after it is unlocked |
| Noir | wearable | nylon bodysuit, lace cups, satin cincher, nylon thigh-highs | sheer / lace | not started |
| Bodystocking | wearable, her default | one piece of sheer nylon, string tanga, chain, bangles | sheer | not started |
| BodystockingSkirt | wearable | the bodystocking plus `CA_Skirt` | sheer + cloth | not started |
| Tartan | wearable | tartan panels, straps traced by `wrap`, sheer thigh-highs with lace tops | straps + sheer | not started |
| KitTartan | wearable | Figma shapes and `sketch.json` strokes | kit strokes | not started |
| Garter, Fishnet | locked | lace set with stockings; net tights | lace / net | not started |
| Sheer | built, unlisted | sheer bodysuit, latex bands, gloves | sheer | not started |

**What the next outfits agent should know before cutting.**
- **Stage 3's "opaque sets (Mirror, Harness, Cocoa)" is wrong for two of the three.** Mirror is a sheer
  bodysuit: the cover mask clips nothing under `Sheer`, so it belongs with the 1-2 hour sheer outfits and
  shows the painted-out briefs area through the mesh over hips and seat. Cocoa is a locked row
  (`SetOutfit` refuses it), so it cannot be worn on either body in the game's normal way, and there is
  no current-body frame to compare with until its refresh.
- **`OPAQUE = ("Satin", "Latex", "Trim", "Tartan")`** (`blender_outfit.py`). Harness's chap stockings are
  `Jersey` and its tapes `Binding`: neither is in the list, so the skin under the opaque jersey would not
  be clipped. Add `Jersey` (and check it against the old body's build, which reads the same tuple).
- Harness's straps are not the kit-stroke or `wrap` tooling: they are rays on whatever skin is loaded
  (`TORSO_BVH`, `LEG_BVH`, `leg_axis`, `Bn["thigh_l"]`, `Bn["foot_l"]`, `ANCHOR`, `CUP`). None of that has
  run on this body; `CUP` comes from the 9 cm bust stand-in and predates the softened bust.
- `mh_outfit.py` needs each outfit's old mesh in `OLD_MESH`: Catsuit's is under `/Game/Character/Outfits`,
  the others under `/Game/Character/Clothes`. Wearing needs no C++: `ApplyOutfit` loads `SKM_<Outfit>_MH`
  and `MI_Body_<Outfit>` by name, and logs "no cut on the MetaHuman body yet" until they exist.
- `mh_after_build.sh` and `mh_recut.sh` name the Silversuit alone in their last step: make that a loop
  over a list when the second outfit lands.
- `_mh_test_poses()` bends wrists and hips only. Catsuit's collar, Mirror's glove tops and any stocking
  top sit on joints it does not test (stage 4b's open point).

**Estimates, unchanged from stage 3 except where the table moved an outfit:** Catsuit 20-30 minutes;
Harness 1-1.5 hours (strap rays, `Jersey`, the thong baring the hips); Mirror, Noir, Bodystocking, Sheer
1-2 hours each plus one decision on the briefs area under sheer fabric; Tartan and KitTartan half a day
of tool work once; the skirt half a day; Cocoa, Garter, Fishnet after their refresh on the current body.
