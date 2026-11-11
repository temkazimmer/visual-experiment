# Agent playbook

Lessons from running many parallel sessions and subagents on vis-ex (2026-11-11). Read
this before briefing or running a subagent for a complicated subtask.

## Surviving network drops

Long agent turns kept dying with `ECONNRESET` / "connection lost mid-response", most
often while writing a big file or reading a big one in one go. What worked:

- **Short replies.** Under ~15 lines per turn, no long plans or recaps.
- **Small file writes.** Write a file in chunks of ≤40–50 lines (write a first part,
  then append), never one large Write.
- **Small reads.** Never Read a whole large source file (`VisexAnimInstance.cpp`,
  `VisexMenuDirector.cpp`). `grep -n` for the function, then Read a ~60-line range.
- **Filter tool output** with `head`/`grep`/`tail`; no full directory listings or logs.
- **Commit each working piece** at once, so a drop loses minutes, not the task.
- **Fresh beats resumed.** After two drops with no progress, start a fresh agent with a
  tight brief that names what's already committed, rather than resuming (a resumed
  agent carries its whole history and keeps dropping).

## Briefing a subagent

- State the goal in Tema's words, then exact files, the worktree, what's already done
  (commit hashes) and what not to touch (files other agents are editing right now).
- **At most 4 UE editors on the machine at once** (memory fills). Count before launching;
  at 4, keep working on code and poll (bounded) until a slot frees; quit yours right after its check.
- One editor per project folder. Give each agent its own worktree and branch for
  editor work; never two writers on one worktree.
- New binary assets as new files; never modify an existing `.uasset`/`.umap` that
  another branch may also change.
- End with a short report format: hashes, shot paths, numbers, what's left.

## Traps that cost real time

- **`pgrep -f <pattern>` matches its own command line**, so a wait loop on it never
  ends. Use `pgrep -x <name>`, or `ps -axo pid,args | grep … | grep -v grep`.
- **Raw `UObject*` held across frames or captured in a timer** crashed Shipping twice
  (SIGSEGV 0x38 in `CreateSlotAnimationAsDynamicMontage`) when GC freed the clip. Hold
  clips in a `UPROPERTY` (`LoadedLoops`, `LoadedLean`). The QA tour forces a GC to catch it.
- **Spawned meshes come up BlockAll** and can block the player spawn (she becomes a
  spectator). Set the `NoCollision` profile after the mesh; run a Play spawn check
  before saving Lvl_Dust.
- **Her mesh bounds miss the trailing foot**, so a narrow SceneCapture culls her
  (empty frames). Set `bounds_scale` ≥2 on her skeletal meshes before captures.
- **A Play session you didn't start** may be Tema's. Never stop it; ask. The auto-mode
  check also refuses relayed approval, so Tema stops it himself.
- **Interchange FBX import** brought outfits in 100x too large; a replace-import reuses
  the old import settings. The outfit tools use the legacy importer and stamp settings first.
- **Bone names by substring**: `indextoe`/`middletoe`/`ringtoe` matched the hand bones
  `index`/`middle`/`ring`. Match whole names.
- **Game-side layers change poses.** The anim instance clamps joints and runs contact
  passes; bake poses with the kit's predictor (c031ab8) and key `ik_foot_l/r`, or the
  feet stay pinned at the idle stance.
- **Contact passes that read the last drawn pose** feed back a frame late and jitter at
  low fps. Keep them off where a baked pose already holds contact, or make them
  frame-rate independent.

## Merge, build, QA

- Merge into main only when no cook is running (`pgrep -x AutomationTool`) and no editor
  has main's project open, or ask before closing Tema's editor (check Play and dirty
  packages first).
- Before merging, list files changed on both sides since the merge base; binary files on
  both sides mean one side's work is lost.
- C++ change: build `visexEditor`, then relaunch the editor.
- QA: `Tools/make_dmg.sh` (DMG number = commit count), the `-visexqa` tour twice from the
  mounted DMG, back up and restore Tema's `VisexBody.sav`/`GameUserSettings.ini`/`Game.ini`,
  log in `Docs/QA.md`. A pass that Tema asks to release is copied to `Build/VIS-EX-<n>-stable.dmg`.
