# visex

UE 5.7 project `visex` (repo `visual-experiment`), macOS / Apple silicon. Read this file
before doing anything, then the brief for your branch.

## What the game is

A posing / photo-mode playground: 3D characters in different poses and outfits, in
cinematic environments, with a photo mode and stackable photo filters (CRT, 80s anime,
low poly). The first scene, `Content/VisEx/Lvl_Dust`, is an orange dust-fog plaza: a
giant ruined colonnade climbing into the haze, wide steps, wind-shaped dunes. The bar is
**maximal cinematic look**. Movement should feel weighty and deliberate, like a figure in
a Villeneuve shot, not floaty or arcade.

## Branches and who owns what

Two agents work in parallel, often on two laptops. Binary `.uasset`/`.umap` files can't
be merged, so each folder has one owner; edit the other side's files only through a
merge.

| Branch | Scope | Owns | Brief |
|---|---|---|---|
| `main` | Look and atmosphere, the scene, the menu, music, the Start camera move | `Content/VisEx/` (levels, materials, stone, dunes, menu, audio), the scene-building `Tools/` scripts, `Source/visex/VisexMenu*` | this file |
| `character-animation` | The character's body, skin, body page and movement | `Content/Character/`, `Content/Characters/`, `Content/ThirdPerson/`, `Content/Input/`, `Tools/char_*.py`, `Source/visex/VisexBody*`, `VisexShell*`, `VisexAnimInstance*` | [Docs/AGENT_CHARACTER.md](Docs/AGENT_CHARACTER.md) |

Character direction: [Docs/CHARACTER_DIRECTION.md](Docs/CHARACTER_DIRECTION.md). If
something needs a change on the other branch, write it down for that agent (or message
its session) rather than doing it.

## The game flow (C++ in `Source/visex`)

- `AVisexMenuDirector` (Blueprint child `BP_MenuDirector` in Lvl_Dust): on Play the view
  holds `MenuCam`, she stands where `MenuModel` stands, and the menu is a 3D panel
  (`UVisexMenuWidget`, Blueprint `WBP_Menu`) turned toward her.
- Items: Start · Body · Photo mode · Quit. Start circles the camera low around her and lands
  exactly on the third-person camera, lens and field of view included. Body opens
  `AVisexBodyEditor`; closing it brings the menu back.
- There's no live coding on Mac. After a C++ change, build with
  `Engine/Build/BatchFiles/Mac/Build.sh visexEditor Mac Development -Project=... -WaitMutex`,
  then fully quit and relaunch the editor. A restart is only real if the editor process
  started after the dylib was built.

## Tooling: drive the editor live

Tema wants to **watch you work in the open editor**. A folder bridge makes that possible:
- `Content/Python/init_unreal.py` auto-loads in the editor and runs any `.py` dropped in
  `Tools/.bridge/in/`. Output appears in `Tools/.bridge/out/`.
- Client: `python3 Tools/ue.py -c "import unreal; ..."` or `python3 Tools/ue.py some_script.py`.
  It exits 1 on a Python error. For long jobs, raise the wait with `UE_WAIT=6000` (tenths of a second).
- The bridge is per project folder. A second editor opened from another worktree has its
  own; both windows are titled "visex", so check which folder you're talking to.
- Screenshots: `Tools/capture.py` renders through a SceneCapture2D to
  `Tools/.bridge/capture.png`. Set a global `VIEW = ((x,y,z), (tx,ty,tz), fov)` to shoot
  from anywhere, or `CAM = "MenuCam"` to shoot through a level camera.
- In Play, `HighResShot` can grab the editor viewport instead of the game. Render through
  a SceneCapture2D placed in the level before Play, moved to the player camera instead.
  It skips the post grade, so use it to check framing, not the look.
- Python Remote Execution (UDP multicast) never worked on these Macs. Don't retry it.

## Gotchas learned the hard way

- **Play mode:** check `unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()`
  first, and **never stop a Play session you didn't start**: it's probably Tema playing.
  Abort the whole run, not just the step. While he's playing, actor queries return nothing
  and saving is unsafe.
- Save the level with `LevelEditorSubsystem.save_current_level()`, not "save all dirty":
  that also saves whatever Tema has open.
- `unreal.Rotator(a, b, c)` is **(roll, pitch, yaw)**. Always use keywords.
- **Blueprint defaults:** use `unreal.get_default_object(bp.generated_class())`, then
  `BlueprintEditorLibrary.compile_blueprint` and `EditorAssetLibrary.save_loaded_asset`.
- Freeze a pose on a SkeletalMeshComponent with `override_animation_data`, not by setting
  `animation_data` directly.
- `unreal.CustomInput()` takes no constructor kwargs. Use `set_editor_property`.
- In Custom-node HLSL, `?:` evaluates both sides, so use if/else when there are out-params.
- Don't run a headless commandlet against a level the open editor has loaded.
- Exponential height fog only gets thicker going down, and local fog volumes don't render
  here. The haze that swallows the columns' tops is in the stone material
  (`Tools/add_sand.py`), and its colour must equal the fog's inscattering colour
  (`Tools/cinematic_pass.py`), or a seam shows.

## Git

- Binary assets are in **Git LFS**: run `git lfs install` once per machine.
- Fab/Megascans sand textures are gitignored (licensed). Download them as described in
  `README.md`, or the sand material won't compile. That only matters when you open Lvl_Dust.
- Commit small and often on your branch. Push only when Tema asks.
- Moving work across: merge the other branch in, check no binary file changed on both
  sides, rebuild, play-test Lvl_Dust.
- Commit messages: short imperative summary line, body explains why.

## Working with Tema

- He's a design leader, and this is a hobby project. Optimise for how it looks and feels,
  and for visible output. Skip plumbing that doesn't produce something.
- Replies and plans about **30% shorter than default**, not minimal.
- Discuss decisions before making them. Raise disagreement plainly, then go with his call.
- Ask before anything destructive: deleting assets, overwriting his hand edits, force-pushing.
- Show results: a capture or a short clip beats a description.
