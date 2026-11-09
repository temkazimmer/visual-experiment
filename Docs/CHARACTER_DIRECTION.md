# Character direction

> **Draft for discussion.** Sections marked *Proposed* are suggestions, not decisions.
> Read this before picking up character work on any machine. Rules for the
> `character-animation` branch also live in `CLAUDE.md` on that branch.

## First task: the aerogel shell prototype

Approved by Tema 2026-11-09. Before animation polish, prove the look in `Lvl_Dust`.
Spec: §1, "Direction" and "Body".

- **Build:**
  - Scale Quinn to 159 cm.
  - Add a second skeletal mesh (the shell) on `BP_ThirdPersonCharacter`, following the
    body via Leader Pose.
  - Make `M_AerogelShell`, the translucent shell material: thickness-driven milkiness,
    Fresnel rim, blue scatter, optional inner glow.
  - First pass uses the mannequin as the inner machine. The sculpted shell body (bust,
    contour) comes after the material works.
- **Files:**
  - new `Content/Character/Android/`
  - `BP_ThirdPersonCharacter`
  - new `Tools/char_shell.py`, idempotent and run through the bridge
  - Nothing in `Content/VisEx/`.
- **Done when:** Tema approves three captures (`Tools/capture.py`):
  1. **The menu shot from `MenuCam`.** The rim reads at small size.
  2. **A backlit close shot.** The machine shows faintly through thin skin.
  3. **A shot deep in the fog.** The figure doesn't glow or vanish.

  Plus a play-test: walk, run, jump and dash, with the shell following and no z-fighting.
- **Then:** fix steps, dash and Stride Warping for the 159 cm scale, and only then
  move on to the idle micro-animations (§3).

## 1. The figure

vis-ex is a posing / photo-mode game. The character is the subject of every photo, so
it is what the game is about, not an avatar for getting around.

In `Lvl_Dust` the figure is the only human-scale thing in the frame: 159 cm against
columns that rise out of sight into an orange dust storm. The figure gives the scale.
Everything below follows from that.

**Today:** the UE template mannequin (Manny/Quinn), grey and untextured. It is a
stand-in, and it has no identity yet.

### Constraints any design must meet

- **Reads at 40–80 px.** That's the figure's height in the menu shot and wide shots,
  backlit by dust. It has to work as a flat dark shape first: head, shoulders, legs,
  and one signature element.
- **Vertical, like the columns.** Slightly elongated proportions (about 8.5 heads, long
  legs) rhyme with the colonnade and make the figure feel deliberate rather than heroic.
- **One value step from the scene.** Darker than lit sand, lighter than the stone's
  cool shadow side, so the figure separates in both.
- **Matte, dry materials.** Gloss looks pasted onto the dust. One accent that catches
  the low sun is enough.
- **Not orange.** The grade already turns everything amber, so the figure should carry
  the complement. The aerogel's blue scatter does this (see below).
- **Moves in the wind.** The storm is the scene's motion. Something on the figure (cloth
  edge, hair, a strap) should answer it, so stills show wind direction.

### Direction: an android under aerogel skin *(Tema, 2026-11-09)*

> **Update (Tema, same day):** the skin moved away from aerogel. It is now opaque and
> human-like: a matte, mannequin-white skin that hides the machine completely, with no
> joints or panel lines (one smooth sculpted mesh, `SKM_AndroidSkin`). The aerogel text
> below is kept as history.

A humanoid android. A smooth pseudo-skin shell covers a mechanical body, and the shell
is translucent like aerogel: milky and soft, so the machine inside shows only as a
blurred shape and a faint glow. There is no face. At this distance it would blur anyway.

**Body** *(Tema)*
- A female android, **159 cm**. At this height the scale contrast with the colonnade is
  stronger than with the default 1.8 m.
- Display-mannequin anatomy: a medium bust with no nipples, and a slight, smooth pubic
  contour. Sculpted and abstract, like a store mannequin, never detailed.
- Base: Quinn (the UE5 female mannequin), scaled uniformly to 159 cm. Measure Quinn
  first. Scaling changes the capsule, step reach and dash distance, so re-check the
  plaza steps, Stride Warping and the menu stand-in (`PlacePawn` reads the capsule
  automatically). The shell (below) gets the sculpted bust and contour. The inner
  machine stays mechanical.

**Build: two layers on the standard UE5 skeleton**
- **Inner: the machine.** The UE5 mannequin already has panel lines and joints, so it
  works as the endoskeleton for a first pass. Later, any android mesh rigged to the UE5
  skeleton.
- **Outer: the skin.** The same skeletal mesh drawn a second time, inflated 1–2 cm
  along its normals (World Position Offset), with a translucent material. Both layers
  share one skeleton, so every animation, pose and outfit works on both for free.
  Later, a smoother sculpted body for the shell.
- **The aerogel read**, built in the shell material:
  - **Thickness:** the gap between the skin and the machine behind it (scene depth
    minus pixel depth). Thin areas, like the face of a forearm, show the parts. Thick
    areas and edges turn milky.
  - **Fresnel:** edges go nearly opaque and glow when backlit. That's the rim that makes
    the figure read at 40–80 px in the dust.
  - **Color:** real aerogel scatters blue and passes warm light. Blue scatter against
    the orange grade is the complement we wanted, and the accent color comes from the
    material itself.
  - **Inner glow:** a faint emissive pulse from the machine, softened by the skin. It
    gives stills a heartbeat.
- **Risks to test early:**
  - Translucency inside heavy volumetric fog. It needs the right fogging settings, or
    the figure glows or vanishes.
  - True blur of what's behind the skin isn't free in UE. The thickness trick fakes most
    of it. Substrate's rough refraction may do the rest; that needs checking in 5.7.

**Why not MetaHuman:** most of what MetaHuman offers is the face, hair and realistic skin
shading, and we need none of it. Its body mesh could become a smoother shell later. That
is optional.

### How look and motion connect

- **Posture is identity.** Upright, unhurried, head level. The calm walk (§2) already
  aims for this, and the design should never push it into a hunch or a swagger.
- **Secondary motion carries the mood.** Cloth or hair trailing downwind, a slight lean
  into the gusts, eyes squinted against the dust. These do more for "storm" than speed.
- **Weight shows on contact.** Landings sink and sand puffs at the feet. A light figure
  with heavy contacts reads as deliberate. A heavy figure with floaty jumps reads as
  arcade.
- **Stills are the product.** Every move should have a frame worth photographing: the
  apex of the jump, the end of a stop, the turn of the head.

## 2. Movement feel

The target is weighty and deliberate, like a figure in a Villeneuve shot. Not floaty,
not arcade. Small steps against colossal scale.

| Move | Current value | Input (KB / PS pad) | Source |
|---|---|---|---|
| Walk (default) | 190 cm/s, walk clips at 0.75 rate, Stride Warping trims to ~85% | stick / WASD | `tune_movement.py`, `calm_walk.py` |
| Run (hold) | 540 cm/s | Shift / **L2** | `setup_input.py`, `sprint_nodes.t3d` |
| Accel / stop | 800 accel (~0.7 s to speed), 900 braking (~0.6 s glide), friction 4, turn 300°/s | | `tune_movement.py` |
| Jump | JumpZ 550, air control 0.5 | Space / ✕ | `add_double_jump.py` |
| Double jump | 2nd jump launches to 2× height, replays `MM_Jump` | Space / ✕ in air | `jump_dash_nodes.t3d` |
| Dash (ground + air) | `MM_Dash` root motion, ~8.75 m in 0.6 s; in air: Z velocity 0, gravity 0.2; 1.2 s cooldown | Q / **L1** | `add_dash.py`, `jump_dash_nodes.t3d` |

Note: walk was 150 in `9415d00`. `c0c3fae` moved it to 190 with a slower step rhythm,
because heavier stride trimming bent the knees and made the walk look sneaky.

**Open on feel:** do the double jump and air dash fit "weighty"? They're the most arcade
moves in the set (see Questions).

## 3. Animation priorities *(Proposed order)*

1. **Idle and micro-animations.** The menu opens on the figure standing still, and that
   frame is the first thing anyone sees. Every animation should be distinct, not the
   template's. The android's stillness is its character, and small events break it:
   - **Base:** near-perfect stillness, with slow "breath" as a cooling cycle (a small
     chest and shoulder rise, synced to the inner glow).
   - **Layers on top (additive, procedural):**
     - The head tracks the camera or points of interest. The head leads, and the body
       follows late.
     - Wind pushes the figure slightly, and it corrects.
     - Servo settles: a tiny overshoot and correction after each stop.
   - **Events after 5–20 s, randomized:**
     - A calibration tic: finger flex, wrist roll, neck tilt.
     - Scanning the columns upward.
     - Brushing dust off the skin.
     - A weight shift.
   - **Built with:** random idle variations in `ABP_Unarmed`, plus additive layers via
     Control Rig (look-at, noise, wind). No new mocap is needed for the layers.
2. **Locomotion.** Walk and run with starts, stops and turns. Stride Warping is in place
   for walk. To make it distinct, consider Epic's free Game Animation Sample (hundreds
   of mocap clips plus Motion Matching on the UE5 skeleton). Check its 5.7 build. The
   android flavor is "slightly too precise": clean stops with a settle, head-first turns.
3. **Steps and ground.** The plaza steps have a hidden collision ramp
   (`add_step_ramp.py`, 20 cm rise, 220 cm treads). Foot IK so the feet land on the
   real treads and chipped slabs.
4. **Jumps and dash.** Landing weight (dip and recover), a distinct air-jump animation,
   a dash that reads as effort.
5. **Poses for photo mode.** See §4.

## 4. Poses and outfits roadmap *(Proposed)*

| Step | What | Done when |
|---|---|---|
| P1 | Freeze any frame of an anim as a pose (`override_animation_data`) | Photo mode can hold the figure in a chosen pose |
| P2 | Pose library: 10–15 poses, Mixamo retargets or Control Rig Pose Assets | Cycle poses with a key / pad button |
| P3 | Pose tweaks in photo mode: head/gaze toward camera, hand adjust | One slider or stick controls gaze |
| O1 | Aerogel shell prototype: mannequin inside, inflated translucent shell | A menu-shot capture where the rim reads, the parts show faintly, and the fog behaves |
| O2 | Modular outfits on Leader Pose Component, 2–3 sets | Swap outfits live, poses still work |

Photo mode itself (free camera, filters) is a separate workstream. The character side
exposes "hold pose N" and "current outfit".

## 5. Where the character touches the scene and menu

- **Menu:** `AVisexMenuDirector` (C++, on main) teleports the player to the `MenuModel`
  spot, disables input and holds the `MenuCam` shot. On Start, the camera blends onto
  the character's follow camera over 2.5 s. Any change to capsule height, mesh offset
  (currently −90° yaw) or the follow camera affects that hand-back, so tell main.
- **Look:** low sun at −24° pitch, cool sky fill, fixed exposure 2.3. Judge character
  materials and silhouette in `Lvl_Dust`, not in a neutral test map.

## 6. Done vs open

**Done** (all committed on `main`)
- Double jump with 2× air jump (`e4b1dbf`, `9cba31b`)
- Step comfort: 20 cm risers, collision ramp (`a07d85f`, `f9a2887`)
- Calm walk at 190 with Stride Warping, run on Shift/L2 (`429e9ba` → `c0c3fae`)
- Dash on Q/L1 with root motion and cooldown (`b98120c`, `c0c3fae`)

- Shell prototype v1 (`character-animation`): Quinn at 159 cm, graphite core
  (`MI_AndroidCore`), aerogel skin (`M_AerogelShell`) on `UVisexShellComponent`, which
  follows the body by Leader Pose. Build: `Tools/char_shell.py`. Shots:
  `Tools/char_capture.py`. Menu, backlit and fog shots render; awaiting Tema's verdict.
- Skin v3 (`character-animation`): one smooth opaque skin mesh sculpted in Blender
  (`Tools/blender_skin.py` → `Tools/char_skin.py`), matte mannequin white, no joints.
  Bust and glute morph targets.
- Body page (`character-animation`): 16 sliders (height, head, neck, shoulders, ribcage,
  bust, waist, hips, glutes, leg length, thighs, calves, arm length, arms, hands, feet).
  Bones reshape after the anim graph (`UVisexAnimInstance`), bust/glutes are morphs,
  height scales mesh and capsule (`UVisexBodyComponent`, saved to slot `VisexBody`).
  `AVisexBodyEditor` is the page: turntable cam, slider panel. The menu "Body" item is
  main's to add (`Source/visex` menu code).
- Starter build and latex skin *(Tema's reference, 2026-11-09)*: the body page opens on
  an hourglass (waist 0.72, hips 1.22, glutes 0.8, thighs 1.12, shoulders 0.9, ribcage
  0.92) in glossy peach latex (`M_AerogelShell`, `MODE="latex"`: clear coat 1, coat
  roughness 0.05). Glute morph runs up to 2× its sculpt; past that it turns conical.
  A fuller hourglass than this needs a sculpted base mesh, not more slider range.

**Open**
- Play-test at the new scale: steps, dash distance, Stride Warping, the menu hand-back.
- PS pad: L2 run and L1 dash are mapped. Not yet verified on a real controller end to end.
- The figure itself (§1), idle, foot IK, landings, poses, outfits.
- Uncommitted change to `ABP_Unarmed.uasset` on main (this machine). It needs an owner
  before handoff (see §7).

## 7. Handoff rules

**Owned by `character-animation`:** `Content/Characters/`, `Content/ThirdPerson/`,
`Content/Input/`, new `Content/Character/`, and `Tools/` movement scripts
(`add_double_jump.py`, `add_dash.py`, `setup_input.py`, `tune_movement.py`,
`calm_walk.py`, `*.t3d`).

**Not owned:** `Content/VisEx/` (levels, materials, menu), `Source/visex/`, scene
scripts. Play-test in `Lvl_Dust`, but don't save it. Requests for main go in `HANDOFF.md`.

**One owner per binary file.** `.uasset`/`.umap` can't be merged; if both sides edit
one, a side's work is lost.

**Before handoff:** the movement commits above landed on **main**, and
`character-animation` is 5 commits behind. Suggested sequence:
1. Commit or discard the `ABP_Unarmed` change on main.
2. Merge `main` into `character-animation` and push.
3. From then on, main stops touching character files.

**Working**
- Drive the open editor through the bridge: `python3 Tools/ue.py <script>` (jobs in
  `Tools/.bridge/in/`). Python can't add Blueprint nodes, so graph logic ships as
  `.t3d` text to paste.
- Check `is_in_play_in_editor()` first. In Play mode, actor queries return nothing and
  saving is unsafe.
- Git LFS (`git lfs install`). Fab sand textures are gitignored, see `README.md`.
- UE 5.7 on Xcode 27: `Apple_SDK.json` MaxVersion raised to 27.9 (backup:
  `Apple_SDK.json.visex-backup`). If the Metal toolchain goes missing after an Xcode
  update, run `xcodebuild -downloadComponent MetalToolchain`.
- Tema: keep it concise, discuss decisions first, ask before deleting or overwriting.
  Show a capture rather than a description.

## 8. Questions for Tema

1. **Name:** does she have one?
2. **How much machine shows:** a ghost of the parts only in backlight, or clearly
   visible joints through thin skin?
3. **Inner glow:** yes or no, and what color (aerogel blue, or a warm core)?
4. **Double jump and air dash:** keep them, tone them down (lower, slower), or make them
   photo-mode-only traversal?
5. **Priority:** figure design first, or idle and locomotion polish on the mannequin?
6. **Pose source:** Mixamo retargets (fast), or authored Control Rig poses (slower, but
   it's your own look)?
7. **Handoff timing:** OK to merge `main` into `character-animation` now and freeze
   character files on main?

## Next details (Tema, 2026-11-09)

1. ~~Hands and feet~~: done, grafted from the MakeHuman base mesh (Tools/blender_extremities.py).
   Still to do: the small dent on the index knuckle; a relaxed hand resting on the thigh.
2. Navel.
3. Skin tone choice on the BODY tab: Asian, European, African (and in between).
