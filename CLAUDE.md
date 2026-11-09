# visex — character & movement animation branch

You're the agent on the **`character-animation`** branch of `visual-experiment` (UE 5.7
project `visex`, macOS / Apple silicon). Tema works on this branch from a second laptop.
Another agent works on `main` in parallel. Read this file before doing anything.

## What the game is

A posing / photo-mode playground: 3D characters in different poses and outfits, in
cinematic environments, with a photo mode and stackable photo filters (CRT, 80s anime,
low poly). The first scene, `Content/VisEx/Lvl_Dust`, is an orange dust-fog plaza: a
giant ruined colonnade, wide steps, wind-shaped dunes. The bar is **maximal cinematic
look**. Movement should feel weighty and deliberate, like a figure in a Villeneuve shot,
not floaty or arcade.

## Your scope on this branch

The playable character and its movement animation: locomotion (idle, walk, run, starts,
stops, turns), jump and double jump, landing, slopes and steps, camera feel while
moving. Poses for photo mode count too.

**Files this branch owns** (edit freely):
- `Content/ThirdPerson/`: `BP_ThirdPersonCharacter`, `BP_ThirdPersonGameMode`,
  `BP_ThirdPersonPlayerController`
- `Content/Characters/`: mannequins, `ABP_Unarmed`, `BS_Idle_Walk_Run`, anims
- `Content/Input/`: input mapping and actions
- any new folder for this work, e.g. `Content/Character/` and `Tools/char_*.py`

**Don't touch** (main owns them; binary `.uasset`/`.umap` files can't be merged, so
editing them on both branches loses one side's changes):
- `Content/VisEx/`: levels, materials, stone, dunes
- `Tools/` scripts that build the scene (`build_dust.py`, `add_*.py`, `bake_stone.py`,
  `stone_geo.py`, `apply_geo.py`, `import_sand_tex.py`)

If you need a test map, make your own under `Content/Character/`. You can play-test in
`Lvl_Dust`, but don't save it. If something here truly needs a change on main, write
it down in `HANDOFF.md` on this branch rather than doing it.

## Current state of the character

- Lvl_Dust has no game-mode override, so it uses the project default `BP_ThirdPersonGameMode`,
  which spawns `BP_ThirdPersonCharacter` (UE Third Person template, Manny/Quinn mannequin).
- Double jump (`Tools/add_double_jump.py`, idempotent): `JumpMaxCount 2`,
  `JumpZVelocity 550` (template default 500), `AirControl 0.5` (default 0.35).
- Second-jump boost: **not applied yet**. Python in 5.7 can't add nodes to a Blueprint
  graph, so the nodes are in `Tools/double_jump_boost.t3d`. Copy its text, open
  `BP_ThirdPersonCharacter` → Event Graph, paste, compile, save. It adds an `OnJumped`
  event that pushes extra Z velocity on the air jump. Tema decides whether he wants it.
- The level's floor is uneven on purpose (chipped slabs, worn step nosings, dunes).
  Collision is the full-res mesh, so foot IK / step-up tuning has something real to hit.
  `Lvl_Dust` steps are being resized to the mannequin (lower risers, 220 cm treads) on main.

## Tooling: drive the editor live

Tema wants to **watch you work in the open editor**. A folder bridge makes that possible:
- `Content/Python/init_unreal.py` auto-loads in the editor and runs any `.py` dropped in
  `Tools/.bridge/in/`. Output appears in `Tools/.bridge/out/`.
- Client: `python3 Tools/ue.py -c "import unreal; ..."` or `python3 Tools/ue.py some_script.py`.
  It exits 1 on a Python error. For long jobs, raise the wait with `UE_WAIT=6000` (tenths of a second).
- Screenshots: `Tools/capture.py` renders through a SceneCapture2D to
  `Tools/.bridge/capture.png`. Set a global `VIEW = ((x,y,z), (tx,ty,tz), fov)` to shoot
  from anywhere:
  `( echo 'VIEW = (...)'; cat Tools/capture.py ) > Tools/.bridge/v.py && python3 Tools/ue.py Tools/.bridge/v.py`
- Python Remote Execution (UDP multicast) never worked on these Macs. Don't retry it.

## Gotchas learned the hard way

- **Play mode:** check `unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()`.
  While Tema is playing, actor queries return nothing and saving is unsafe. Ask him to stop.
- `unreal.Rotator(a, b, c)` is **(roll, pitch, yaw)**. Always use keywords.
- **Blueprint defaults:** use `unreal.get_default_object(bp.generated_class())`, then
  `BlueprintEditorLibrary.compile_blueprint` and `EditorAssetLibrary.save_loaded_asset`.
- Freeze a pose on a SkeletalMeshComponent with `override_animation_data`, not by setting
  `animation_data` directly.
- `unreal.CustomInput()` takes no constructor kwargs. Use `set_editor_property`.
- In Custom-node HLSL, `?:` evaluates both sides, so use if/else when there are out-params.
- Don't run a headless commandlet against a level the open editor has loaded.

## Git

- Binary assets are in **Git LFS**: run `git lfs install` once per machine.
- Fab/Megascans sand textures are gitignored (licensed). Download them as described in
  `README.md`, or the sand material won't compile. That only matters when you open Lvl_Dust.
- Commit small and often on `character-animation`. Push when Tema asks.
- Before merging: rebase or merge `main` into this branch, check the Content conflicts list
  is empty, play-test Lvl_Dust, then open a PR to `main`.
- Commit messages: short imperative summary line, body explains why.

## Working with Tema

- He's a design leader, and this is a hobby project. Optimise for how it looks and feels,
  and for visible output. Skip plumbing that doesn't produce something.
- Replies and plans about **30% shorter than default**, not minimal.
- Discuss decisions before making them. Raise disagreement plainly, then go with his call.
- Ask before anything destructive: deleting assets, overwriting his hand edits, force-pushing.
- Show results: a capture or a short clip beats a description.
