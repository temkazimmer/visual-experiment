# visex — character & movement animation branch

Brief for the agent on **`character-animation`**. Read the root `CLAUDE.md` first: it
holds the shared tooling, gotchas and git rules. Tema works on this branch from a second
laptop; another agent works on `main` in parallel.

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
