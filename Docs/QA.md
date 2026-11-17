# QA log

Each packaged build gets the automated tour (`-visexqa`, `Source/visex/VisexQATour.cpp`), run from
the read-only mounted DMG. Shots go to `~/Library/Application Support/Epic/visex/Saved/QA/`, and the
log goes to `~/Library/Logs/visex/visex.log`. Frame times are ms at 1920×1080 windowed on the M5 Max.
They're only comparable when the machine is idle.

Shipping writes no UE log. Since 2e6efc4 the tour also writes `Saved/QA/report.txt`, which works in every config.

## 2026-11-14 14:20 · VIS-EX-0627 Shipping (f130e81) · PASS: the pin-fix cures 0622's broken skeleton, she walks out at Start, film mirrored with a clean hand-off

- **Cook:** `Success - 0 error(s), 0 warning(s)`, 926 packages ordered, Shipping binary rebuilt from HEAD. DMG md5 `daf95421…` (1.6 GB). Not copied to `-stable`.
- **Tour:** two runs from the mounted DMG, 1440×1080 windowed. Both `VisexQA: done`, exit 0 (310 s, 297 s), no game `.ips`.
  On ground at every report step (only photo motions 5/6/7/9 in mode 0, as 0612), `body page open`, `wardrobe open`, photo `moved 0.0 cm`, `layout save PASS`,
  menu feet IN FRAME at 16:9 and 4:3. Back on the lean spot (9500 −900 152) after Body, Wardrobe and Photo.
  Run 1: Tema's save (Elf hair). Run 2: the 0612 copy with Short hair. Load average 3–6, mostly idle (a visex-metahuman editor started between runs and settled first).
- **Pose: fixed** (`sheets/pose_vs0612_0622_run1.jpg`, `pose_vs0612_0622_run2.jpg`: rows 0612 / 0622 / 0627; play, Body tab, photo Idle, photo Bookend).
  0627 matches 0612 in every shot of both runs: no inverted body, no leg in the air. Menu feet check at y 0.76–0.78 for both feet (0622: 0.24–0.31), same as 0612.
  `run1_all.jpg` / `run2_all.jpg`: outfits, Body tabs, Wardrobe, all 11 photo motions and the turnarounds look normal.
- **Play step: she walks out.** At `play` (4 s after Start) she's at 9387 −1322 on `A_PhotoSeq_Walk` (rm mode 1), 440 cm out (0612/0622: still at the column).
  At `walked` she's 1000 cm out (9236 −1885, down the dune). Same in both runs. `sheets/play_3fps.jpg`: Start orbit, then a steady walk away into the dunes.
- **Film** (`run{1,2}/rec/arrival.mp4`, `sheets/arrival_2fps.jpg`, `run2_arrival_2fps.jpg`): plays mirrored, stride going left to right, ~6.5 s.
  Then ~1 s black and the plaza fades up. No desktop or gap, both runs.
- **Arrival** (`sheets/handoff_arrival_6fps.jpg`): she walks in from the left, turns and sinks into the lean in one move. No snap, and **no leg kick** in the walk-in.
- **Menu moves** (`run2/rec/settings_body_wardrobe_photo.mp4`, `photo_back.mp4`, `sheets/moves_1fps.jpg`, `photo_back_2fps.jpg`): Settings (onto the drum and back down),
  Body, Wardrobe and Photo all go out and come back to the lean. Run 1's recording of these was covered by another session's Development game window and was deleted.
  - Minor, not new: on the Settings Graphics → Visuals tab switch she starts to stand up on the drum for ~0.2 s and drops back to sitting (`sheets/settings_flash_30fps.jpg`).
    0622's recording shows the same blip at that moment, so it predates the pin-fix.
- **Feet:** pass (`sheets/settings_standup_feet_4fps.jpg`, `feet_mounds_zoom.jpg`, `feet_play_walked_settings_turn.jpg`). The Settings stand-up lands on the sand,
  the Body turn has both soles on the sand with contact shadows, play has the planted foot on the sand. Not measured in cm (no meter in Shipping).
- **Mounds:** ring mounds and toe prints around her feet in the Body turn, a heel mound in play. At `walked` she's in the dune's shadow, so prints don't read there.
- **Short hair:** `Hair: SKM_HairShort` all through run 2, the layered shag shows on the Body tabs, turnaround and photo.
- **Perf** (load 3–6, comparable-ish): menu 18.6 / 16.2 ms (0558: 17.6), quality 4 32 / 38 ms (0622: 90 / 70, a busier machine).
- **Saves:** backed up `VisexBody.sav` (`7ac129aa…`) and `GameUserSettings.ini` (`06e783cf…`). Restored after each run, both md5s match. No `Game.ini` before or after.
  Raw screen recordings deleted after cutting game-only clips. Shots in `Saved/QA_0627ship_run{1,2}/` and `Tools/.bridge/shots/qa0627/run{1,2}/`, sheets in `sheets/`.

## 2026-11-14 13:05 · VIS-EX-0622 Shipping (b54e246) · FAIL: her legs are thrown upward, upside down in run 1, after the foot pin merge; film mirrored and hand-off clean

- **Cook:** `Success - 0 error(s), 0 warning(s)`, 926 packages ordered. DMG md5 `4ee14590…` (1.6 GB). Not copied to `-stable`.
- **Tour:** two runs from the mounted DMG, 1440×1080 windowed (the 1920×1080 request clamps to this screen, same as 0612). Both `VisexQA: done`, exit 0 (310 s, 300 s), no `.ips`.
  On ground at every report step, `body page open`, `wardrobe open`, photo `moved 0.0 cm`, `layout save PASS`, menu feet IN FRAME at 16:9 and 4:3, back on the lean spot (9500 −900 152) after Body, Wardrobe and Photo.
  Run 1: Tema's save (Elf hair). Run 2: the 0612 copy with Short hair. Load average 5–7 both runs.
- **The fail: her pose breaks** (`sheets/inverted_vs0612.jpg`, `menu_play_vs0612.jpg`, `run1_all.jpg`, `run2_all.jpg`). The capsule is fine; the skeleton is not.
  - Run 1: in the menu she's folded up on the drum. At `play` and `walked`, on the Body page and in most photo motions she hangs **upside down**, legs in the air.
    The menu feet check puts both feet at y 0.24–0.31 of the frame (0612: 0.76).
  - Run 2: one leg is thrown up. She walks with a leg pointing at the sky (`sheets/play_walk_3fps.jpg`), a foot sits beside her head on the Body face/hair tabs,
    and photo Bookend/Crown/Idle show a leg straight up. The right foot is at y 0.30 in the menu feet check, the left one on the sand at 0.76.
  - The outfit poses (12–21) and some photo motions look normal. The two runs differ, so this looks like uninitialised or garbage state, not a bad clip.
  - Since 0612 the only runtime change is the pin pass in `VisexAnimInstance.cpp` (the film is just a movie). Suspect: `PinAnchor`, `PinLastHeel/Ball` and `PinGraphStart`
    are `FVector`s with no initialiser (garbage in Shipping), and `PinAnchor` is lerped before it's first set. Not confirmed: `-dpcvars` is compiled out of Shipping,
    so `visex.Pin 0` couldn't be tested in the packaged build. Next: check `visex.Pin 0` vs 2 in a Development or Shipping build.
- **Film** (run 2 screen recording, `run2/rec/arrival.mp4`, `sheets/film_strip.jpg`, `film_and_lean.jpg`): it plays mirrored, toes and stride going left to right, ~6.5 s.
  Then ~1 s of black and the plaza fades up (`sheets/handoff_arrival_4fps.jpg`). **No desktop showed through and no gap** beyond that black, in windowed mode.
  The 0612 desktop flash didn't repeat. Music not checked (no audio in the capture).
- **Arrival:** she walks in from the left and sinks into the lean with no snap, but a leg kicks up during the walk-in (the pose bug). The lean itself isn't clean either (one leg missing from view).
- **Menu moves** (`run2/rec/settings_body_wardrobe.mp4`, `sheets/moves_1fps.jpg`): Settings (onto the drum and back), Body and Wardrobe go out and come back to the lean.
  Photo goes out (walk in the dunes) and comes back (`58_after_photo`), but the window was covered during Photo, so that motion wasn't seen.
- **Play step:** still at the column 4 s after Start (9494 −923, root-motion montage on), as in 0612, on an idle machine this time. So it isn't the load:
  the arrival montage holds her there. At `walked` (+3.5 s) she's 500 cm out (9365 −1406).
- **Feet:** fail. With the pose broken, feet aren't on the sand in the Settings stand-up, the Body turn or play. The pin itself can't be judged until the pose is fixed.
- **Mounds:** prints and shallow mounds show around her in `04_walked`.
- **Short hair:** the report has `Hair: SKM_HairShort` all through run 2, and the Body hair tab shows the layered shag (`sheets/walked_settings_hair_run2.jpg`).
- **Perf:** frame times noisy (menu 48 / 26 ms, quality 4 90 / 70 ms). Not compared.
- **Saves:** backed up `VisexBody.sav` (`7ac129aa…`) and `GameUserSettings.ini` (`06e783cf…`). Restored after each run, both md5s match. No `Game.ini` before or after.
  Run 1's screen recording was lost (screencapture killed before it wrote). Run 2's was cut to game-only clips and the raw file deleted.
  Shots in `Saved/QA_0622ship_run{1,2}/` and `Tools/.bridge/shots/qa0622/run{1,2}/`, sheets in `sheets/`.

## 2026-11-14 10:09 · VIS-EX-0612 Shipping (8992d06) · PASS, root-motion arrival turns and sinks into the lean with no snap, Short hair wears and is listed WORN; perf not measurable (machine busy)

- **Cook:** `Success - 0 error(s), 0 warning(s)`, 946 packages (926 cooked, 20 skipped by platform). The uncommitted MetaHuman plugin entries in `visex.uproject` didn't break it:
  their content is rejected as NeverCook. DMG md5 `94db06bb…` (1.6 GB). Not copied to `-stable`.
- **Tour:** two runs from the mounted DMG. Both `VisexQA: done`, exit 0 (343 s, 358 s), no `.ips`, on ground at every step (only photo motions 5/6/7/9 in mode 0, as 0558).
  `body page open`, `wardrobe open`, 11 motions, photo `moved 0.0 cm`, `layout save PASS`, menu feet IN FRAME at 16:9 and 4:3. After Body, Wardrobe and Photo she is back on the lean spot (9500 −900 152).
  - Run 1: Tema's save (Elf hair), 1920×1080 windowed. Run 2: a copy of his save with `Hair` set to Short, `-fullscreen`.
  - Run 2 in fullscreen: shots are 1728×1084, not 4K. Shots 01–05 were requested but never written, and the 4:3 step stayed at 1728×1117. Use windowed for the tour.
- **Arrival** (`run1/rec/arrival.mp4`, `sheets/arrival_strip.jpg`, `arrival_lean_30fps.jpg`): she walks in from the left, curves into the walk-turn and sinks into the lean in one continuous move. No snap frame by frame at 30 fps.
  The film → game hand-off showed ~1.7 s of desktop through the window (windowed run). Unconfirmed, because Tema was using the Mac during the run; recheck in fullscreen.
- **Menu moves:**
  - Settings in and out (jump onto the drum, slide back) and Body out (lean-up into the first step, walk, page) are smooth on video (`sheets/body_out_strip.jpg`).
  - Wardrobe and Photo out/back: only the stills (page open, back on the lean). The game window was covered by other apps for the rest of the run, so their motion wasn't seen.
  - At the tour's `play` step (4 s after Start) she was still at the column (9495 −921, root-motion montage on). In 0558 she was 400 cm out. Probably the load slowing game time; recheck on an idle machine.
- **Feet / mounds** (`sheets/feet_lean_zoom.jpg`, `feet_walk_mounds.jpg`): on the lean the soles sit on the sand with a contact shadow; the front foot rests on its heel, toes slightly up. Mounds and prints show around the feet in `04_walked`.
  No mound is distinct at the lean from the menu camera.
- **Short hair** (`sheets/short_hair_run2.jpg`, `short_hair_lean_zoom.jpg`, `hair_tab_list.jpg`): the report says `Hair: SKM_HairShort` all run. The Hair tab lists Long / Elven / **Short, layered — WORN**.
  - The turnaround reads as a layered shag to the jaw with the copper top. Ears are covered at every angle, and the nape is clean.
  - From the low menu camera the fringe hides both eyes. Correct for the style, but her face reads less in the lean: Tema's call.
  - Hair-tab text shows a faint double image (low frame rate during the fade).
- **Perf: not comparable.** Load average was 20 during run 1 and up to 113 during run 2: a second editor (visex-ground) launched mid-run and Tema was working. Menu 46.5 ms (0558: 17.6), quality 4 80.5 ms; later steps hit the 400 ms clamp.
- **Saves:** backed up `VisexBody.sav` (`7ac129aa…`) and `GameUserSettings.ini` (`06e783cf…`); the tour rewrote both. Restored after each run, and both md5s match. No `Game.ini` before or after.
  Screen recordings were trimmed to game-only footage. Shots in `Saved/QA_0612ship_run{1,2}/` and `Tools/.bridge/shots/qa0612/run{1,2}/`, sheets in `sheets/`.

## 2026-11-13 12:25 · VIS-EX-0558 Shipping (49c8994) · STABLE PASS, every page opens in ~3.1 s at real speed in one walk; exits bring the menu back at 3.0 s but she's on the lean only at ~5.5 s

- **Cook:** `Success - 0 error(s), 0 warning(s)`, 854 packages. DMG md5 `63046835…` (1.7 GB), copied to `VIS-EX-0558-stable.dmg`. Editor target built first (`Result: Succeeded`).
- **Real-speed transitions** (packaged, no `-visexqa`, 1920×1080 fullscreen, menu ~50 fps; keys sent by `osascript`, `screencapture -x` every 0.2 s from 0.7 s before the key;
  end = her motion goes flat in a crop around her, ±0.2 s). Frames and `sheet.jpg` per transition in `qa0558/real/<name>/` (8 per row = 1.6 s, first frame −0.7 s).

  | | in (page up, she's settled) | out: menu back | out: settled on the lean |
  |---|---|---|---|
  | Body | 3.2 s | 3.1 s | 5.5 s |
  | Wardrobe | 3.2 s | 3.1 s | 5.6 s |
  | Photo mode | 3.1 s | 3.1 s | 5.7 s |
  | Settings | 3.1 s | (panel swaps at once) | 3.0 s |

  - **Ins hit the target.** Body, Wardrobe and Photo use the same walk: she pushes off the lean (~0.5 s), long athletic strides with the camera following, then the page.
    No different walk for Body. Photo cuts to the dunes at 1.7 s and opens washed-out bright, exposure settles by ~2.9 s.
  - **Outs miss it by ~2.5 s.** She walks back in ~2.5 s and the menu returns at ~3.0 s (what the dev log times), then she turns ~180° in place facing the lens (~1 s)
    and sinks into the lean (~1.2 s), settled ~5.5 s (`body_out/lean_zoom.jpg`). The 45 fps clip agrees: key → flat at 5.6 s (`real/video/body_out.mov`).
  - **Settings jump** (`settings_in/jump_zoom.jpg`): slow crouch 0.5–1.3 s, both feet off the sand at 1.5 s, seated on the drum at 1.7 s, dolly settles by 3.1 s. Smooth, not sudden.
    Back: she slides down into the lean and is still at 3.0 s.
  - No backward steps, no foot sliding visible at 0.2 s. **Two small pops in the Body exit** (45 fps clip, `video/spike1.jpg`, `spike2.jpg`): a one-frame camera
    jump mid-walk (~1.5 s after the key) and a one-frame motion-blur smear as the menu returns (~3.0 s).
- **Launch** (`launch/launch_contact_sheet.jpg`, 0.5 s per cell from 4 s): film ~4–10 s, full frame, black ≤0.5 s, then the plaza fades up empty; she walks in, the panel fades in
  during the walk, she sinks into the lean, settled ~19 s after launch. No pop after: frame diff 1–2.6 for 15 s.
- **Tour:** two runs from the mounted DMG, `-visexqa` 1920×1080 windowed. Both `VisexQA: done`, 58 shots, exit 0 (303 s, 294 s). Launch cover held 0.01 s, `settled`.
  On ground at every step (only photo motions 5/6/7/9 in mode 0, as 0550), no spectator, no `.ips`. **Wardrobe opens in the tour now** (`wardrobe open` both runs,
  the shorter exits closed the timing gap). Shots in `Saved/QA_0558ship_run{1,2}/` and `Tools/.bridge/shots/qa0558/run{1,2}/`, sheets in `sheets/`.
- **Menu, outfits** (`sheets/flow_sheet.jpg`, `outfits_sheet.jpg`): Unopened Gate logo and the one-line menu as 0550. All ten outfits wear on the lean. Wardrobe: list with
  Silversuit WORN, a locked costume shows UNDER CONSTRUCTION. In the tour's `menu_after_wardrobe` (4 s after Close) she's still turning, consistent with the 5.5 s exit.
- **Regressions:** none. Start move 4:3 `fills the window`, feet IN FRAME at 16:9 and 4:3, forced GC fine, `layout save PASS, on disk` in both runs, photo `moved 0.0 cm`.
- **Frame times** (machine idle): run 1 quality 0/3/4: 8.3 / 15.3 / 31.3 ms; menu 17.6, settings 39.1, after body 38.9, after wardrobe 38.6, after photo 39.6.
  Run 2: 8.3 / 15.7 / 31.5; menu 18.7, settings 40.0, after body 38.8, after photo 39.9. About 20 % faster than 0550.
- **Saves:** backed up `VisexBody.sav` (`7ac129aa…`, changed since 0550) and `GameUserSettings.ini` (`06e783cf…`), restored after every launch, both md5s match. No `Game.ini` before or after.

## 2026-11-12 21:10 · VIS-EX-0550 Shipping (d799e54) · STABLE PASS, the intro film plays in-game, any key skips it, she walks on and leans with no pop; cook clean again

- **Cook:** `Success - 0 error(s), 0 warning(s)`, 854 packages; dca6395 keeps `/MovieRenderPipeline` out. DMG md5 `3f69708c…` (1.6 GB), copied to `VIS-EX-0550-stable.dmg`.
- **Launch** (no `-visexqa`, 1920×1080 fullscreen, `screencapture -x` every 0.25 s; `qa0550/launch/launch_contact_sheet.jpg`, from frame 36 = 9 s, 8 per row = 2 s):
  - Cold first launch from the DMG: desktop 9 s, then 3.25 s black before the film. A warm relaunch shows the film ~4.5 s after launch.
  - Film plays full (~6.5 s visible): her feet on rippled sand, low sun, black at both ends. Pillarboxed (16:10 in 16:9), as in 0534.
  - Film → game: ≤0.5 s black, then up from black at once (~1.2 s fade). No wait, no still, no ghosting (the 0534 still→live cross-fade is gone).
  - The plaza fades up empty; she walks in from off frame left, the panel fades in during the walk, she turns and sinks into the lean (settled ~16 s after the film starts).
  - **No pop** in the 4 s after she settles: frame-to-frame difference stays flat (1–3), against the 0534 snap. At 4 fps a sub-0.25 s pop could still slip through.
  - **Skip** (`qa0550/skip/skip_sheet.jpg`): space 2.5 s into the film cuts it to black within 0.3 s, then the game fades up over ~1.2 s with no hold.
  - Music: no audio track in captures. coreaudiod held audio-out from launch through the menu, so sound runs from the start; not heard.
- **Tour:** two runs from the mounted DMG, `-visexqa` 1920×1080 windowed. Both `VisexQA: done`, 58 shots, exit 0, 297 s each. Launch cover held 0.02 s, `settled`.
  On ground at every step (only photo motions 5/6/7/9 in mode 0, as 0505), no spectator, no `.ips`. Shots in `Saved/QA_0550ship_run{1,2}/` and `Tools/.bridge/shots/qa0550/run{1,2}/`, sheets in `sheets/`.
- **Settings jump** (run 2, 60 fps `screencapture -V` clip; `sheets/settings_jump_zoom.jpg`): she pushes off the lean, tucks both legs and rises onto the drum in ~0.5 s,
  then sits with one leg hooked. The recording is ~20 fps real, and the Dock covers the bottom of the windowed frame, so **both feet off the sand is likely but not proven**.
- **Menu, outfits, poses** (`sheets/flow_sheet.jpg`, `outfits_sheet.jpg`, `photo_sheet.jpg`): Unopened Gate logo and the one-line menu as 0505. All ten outfits wear on the lean,
  incl. Silversuit and Tartan (kit). Photo mode labels the motions by name (Idle, Walk, Run, Bookend, Crown, Feetup, Glance, Ledge, Leglift, Mermaid, Peace).
- **Body:** page opens (Body/Face/Hair), she walks back and settles after Close. **Photo:** opens walking in place, `moved 0.0 cm`.
- **Wardrobe:** the tour still logs `DID NOT OPEN` (timing gap, as 0464–0534). **By hand it opens** (`qa0550/wardrobe/wardrobe_sheet.jpg`): Down ×2, Enter,
  list with Tartan WORN, Tartan (kit) and three locked costumes.
- **Regressions:** none. Start move 4:3 `fills the window`, feet IN FRAME at 16:9 and 4:3, forced GC fine, `layout save PASS, on disk` in both runs.
- **Frame times** (machine idle, no other editor): run 1 quality 0/3/4: 16.7 / 16.7 / 30.5 ms; menu 20.5, settings 46.7, after body 50.6, after wardrobe 48.0, after photo 57.2.
  Run 2: 16.7 / 16.7 / 36.3; settings 49.6, after body 51.9, after photo 57.9. The 0534 run-2 434.8 ms "after body page" doesn't repeat.
- **Saves:** backed up `VisexBody.sav` (`bddfbe76…`) and `GameUserSettings.ini` (`06e783cf…`), restored after every launch (the tour rewrites both), both md5s match. No `Game.ini` before or after.

## 2026-11-12 18:44 · VIS-EX-0534 Shipping (e1585c4) · FAIL, cook fails on MovieRenderPipeline content; in a workaround build the film plays, but the still→live cross-fade ghosts and she pops off the lean once

- **Cook FAILS as committed:** `Failure - 1 error(s)`, `Failed to cook /MovieRenderPipeline/Blueprints/Graph/UI_MovieGraphImagePreview for platform Mac`
  (imports `MovieRenderPipelineCore.MovieGraphImagePreviewData`, editor-only). The MovieRenderPipeline plugin enabled in `visex.uproject` (Editor-only allow list)
  still gets its plugin content cooked. No DMG. Log: `Tools/.bridge/shots/qa0534/make_dmg_fail.log`. **b1c3936 doesn't touch this, so it fails the same way.**
- **Workaround build (QA only, not committed):** `+DirectoriesToNeverCook=(Path="/MovieRenderPipeline")` under `DirectoriesToAlwaysCook` in `DefaultGame.ini`, then reverted.
  `0 error(s), 0 warning(s)`, 854 packages. `Build/VIS-EX-0534.dmg` md5 `1ff3579b…`. That line (or disabling the plugin outside intro work) is the likely fix.
- **Film staged:** `Content/Movies/VisexIntro.mp4` (h264 3840×2400, 24 fps, 7.0 s, md5 matches source) and `VisexIntro_last.png` are in the app as loose files.
- **Launch recording** (no `-visexqa`, 1920×1080 windowed, 60 fps `screencapture -V`; `launch/launch.mov`, sheet `launch/launch_contact_sheet.jpg`, 4 fps from launch):
  - 0–0.75 s black (window opening), then the film fades in from black and plays the full 7 s. No black frames anywhere after that.
  - **Film → still pops** (`launch/film_to_still.jpg`): the film plays pillarboxed (black side bars, 16:10 in 16:9); the cover still fills the window,
    so the frame jumps ~11 % larger and brighter (mean luma 98 → 108) in one frame.
  - The still holds 3.0 s, frozen (8.0–11.0 s), then cross-fades over ~0.5 s.
  - **Still → live doesn't match** (`still_vs_live_run1.jpg`, `launch/still_to_live.jpg`, `launch/handoff_sheet.jpg`): the cross-fade shows two women.
    The still has her in the Silversuit, mid-stride at left of frame; live she's in the saved outfit (Tema's save: red Tartan/KitTartan), about half a body width
    further right and turned more toward the lens. The drum is framed slightly differently too. With a non-Silversuit save, the outfit can never match the film.
  - Live: she walks on, turns her back to the column and sinks into the lean (settled ~16 s). The panel fades in during the walk (~12.3–13 s). Logo and menu read well.
  - **Pose pop on the lean at ~17.3 s** (`launch/lean_glitch_60fps.jpg`, `launch/lean_settle.jpg`): about 1 s after she settles, for ~5 screen frames
    she snaps to standing off the lean with her arms up at her hair, then smears back. Also visible in the tour-start capture (`tourlaunch/sheet.jpg`).
  - Music not checked: the screen recording has no audio track.
- **Tour (unchanged):** two runs from the mounted workaround DMG. Both reached `VisexQA: done` with 59 shots and exit 0 (327 s, 296 s). The tour also plays the film.
  Launch cover held 6.08 s (run 1, machine loaded) and 1.43 s (run 2), released `settled`. On ground at every step (only photo motions 5/6/7/9 in mode 0, as 0505),
  no spectator, no `.ips`. Shots in `Saved/QA_0534ship_run{1,2}/` and `Tools/.bridge/shots/qa0534/run{1,2}/`, run 2 screen frames (~4 fps) in `rec2/`.
- **Settings jump** (`settings_jump_sheet.jpg`, `settings_jump_zoom.jpg`): she pushes off the lean, crouches, and is up on the drum in under 0.5 s. At ~4 fps
  no frame catches the flight, so **"both feet off the sand" is not verified**. She sits with one leg hooked; Back returns her to the lean.
- **Outfits** (`outfits_sheet.jpg`): all ten wear on the lean (None … KitTartan). **Body:** she walks clear, page opens (Body/Face/Hair), after Close she walks back
  and settles on the lean (`body_sheet.jpg`). **Photo:** opens walking in place, `moved 0.0 cm`. **Wardrobe NOT exercised** (`DID NOT OPEN`, as 0505).
- **Regressions:** none. Start move 4:3 `fills the window`, feet IN FRAME at 16:9 and 4:3, forced GC fine, `layout save PASS, on disk` in both runs.
- **Frame times not comparable:** another worktree's editor (visex-intro) was rendering a Movie Render Queue job during run 1. Run 1 quality 0/3/4: 18.3 / 47.6 / 90.2 ms;
  run 2: 11.7 / 17.5 / 54.2, menu 25.7, settings 59.8, after photo 60.1. **Run 2 "after body page" averaged 434.8 ms** (run 1: 110.5, 0505: 59); worth a recheck on an idle machine.
- **Saves:** backed up `VisexBody.sav` (`bddfbe76…`) and `GameUserSettings.ini` (`06e783cf…`), restored after each run (the launch run rewrote the ini), both md5s match. No `Game.ini` before or after.
- No `-stable` copy (FAIL; main is rebuilding with the matched film from 3ef295f).

## 2026-11-12 17:46 · VIS-EX-0505 Shipping (cc18718) · STABLE PASS, Tartan (kit) and the re-imported Silversuit wearable, everything from 0485 holds

- **Tour (b7ded30, unchanged):** two runs from the mounted DMG, clean cook. Both reached `VisexQA: done` with 58 shots (one more outfit) and exit 0
  (295 s, 293 s). On ground at every step (only photo motions 5/6/7/9 in mode 0, as 0485), no spectator, no new `.ips`.
  Shots in `Saved/QA_0505ship_run{1,2}/` and `Tools/.bridge/shots/qa0505/` (run 2 in `run2/`, sheets in `crops/`, 5 fps recording in `rec/`).
- **Cook:** `0 error(s), 0 warning(s)`, 851 packages cooked (+6 kit assets). DMG md5 `36d1e432…`, copied to `VIS-EX-0505-stable.dmg`.
- **Outfit kit (`crops/outfits_silver0485_silver0505_kittartan_tartan.jpg`, `crops/silver_kittartan_torso.jpg`):** `Tartan (kit)` (`21_outfit_KitTartan`)
  wears at her size on the lean: collar, red cups, straps, red brief, as the kit's `build_front.png`. The re-imported Silversuit now has the
  drawn chest keyhole and side/hip cutouts with clean edges, matching the kit. No skin through cloth on either at tour resolution (1080p, menu distance).
- **Silversuit default:** no fresh profile in the tour; the reset after the outfit loop puts her in the new Silversuit (`22_body_tab_body`, `58_after_photo`).
- **Menu, Settings, Body, Photo:** as 0485. Logo white, one-line menu, no text shadows, build number still barely readable. Settings: same-height dolly,
  climb onto the drum, seated with one leg hooked and one hanging, Back slides her into the lean. Proud idle on the Body page, walk back and turn after Close.
  Photo opens walking in place (`moved 0.0 cm`). 11 motions.
- **Wardrobe NOT exercised:** `wardrobe DID NOT OPEN` in both runs (tour timing gap, as 0464/0485).
- **Regressions:** none. First menu column lit, Start move `fills the window` at 4:3, feet IN FRAME, forced GC fine, `layout save PASS, on disk` in both runs.
- **Frame times:** run 1 quality 0/3/4: 16.7 / 16.6 / 34.1 ms; menu 22.0, settings 51, after body 59, after photo 65 (0485: 48 / 54 / 59).
  Run 2 slower (18.3 / 42.8 at quality 3/4, after body 89): two other editors were running, not comparable.
- **Saves:** backed up `VisexBody.sav` (`bddfbe76…`) and `GameUserSettings.ini` (`06e783cf…`), restored after each run, both md5s match. No `Game.ini` before or after.

## 2026-11-12 17:33 · VIS-EX-0485 Shipping (a1835b0) · PASS, Unopened Gate logo and one-line menu read well, she climbs onto the drum in Settings, build number nearly invisible

- **Tour (b7ded30, unchanged):** two runs from the mounted DMG, clean cook. Both reached `VisexQA: done` with 57 shots and exit 0
  (290 s, 283 s). On ground at every step (only photo motions 5/6/7/9 in mode 0, as 0464), no spectator, no new `.ips`.
  Shots in `Saved/QA_0485ship_run{1,2}/` and `Tools/.bridge/shots/qa0485/` (run 2 in `run2/`, sheets in `crops/`), plus a 5 fps screen recording in `rec/`.
- **Cook:** `0 error(s), 0 warning(s)`, 845 packages cooked. DMG md5 `f551ed2f…`. No `-stable` copy (main is rebuilding with outfit-kit).
- **Menu (`01_menu`, `crops/menu_0464_vs_0485.jpg`):** the logo is pure white, cracks see-through to the haze. Logo, build number and
  items start on one vertical line (left edges within ~3 px at 1080p; 0464's drifted right). No text shadows. No small dark patch;
  the shade just evens out the yellow corner behind the text. **The build number `0485` is barely readable**: thin, low-contrast on bright orange (`crops/menu_buildnum.png`).
- **Settings:** the camera dollies in at the same height (view z 68 → 67, x 9303 → 9430/9455). She pushes up off the lean with both hands
  and lands crouched on the drum in ~0.8 s (`crops/settings_jump_mid_run1_f565.jpg`, `..._top_run1_f566.jpg`, `crops/rec1_jump_563.jpg`); reads more as a hoist than a jump at 5 fps.
  Seated at 3.5 s: one leg hooked over the drum with the sole to the lens, the other hanging down the front (`11_settings_visuals`; run 2 the same).
  The 1.5 s shot still catches her settling. The panel keeps the main page's on-screen shape and skew. Back: she slides down into the lean, menu returns (`crops/rec1_settings_560.jpg`).
- **Silversuit default:** the tour doesn't run a fresh profile; after the outfit loop it resets to `FVisexBodyProfile().Outfit` and she's in Silversuit (`21`, `57`).
- **Body:** walks clear, page opens standing in the proud idle (hand on hip, chest up). After Close she walks back, turns and settles on the lean; menu back ~6 s after Close, as 0464 (`crops/rec1_body_700.jpg`).
- **Wardrobe NOT exercised again:** `wardrobe DID NOT OPEN` in both runs (same tour timing gap as 0464).
- **Outfits:** all nine wearable on the lean. **Poses:** 11 motions as 0464. **Photo:** opens walking in place, `moved 0.0 cm in 1 s (PASS)`.
- **Regressions:** none. First menu column lit. Start move 4:3 menu/mid/landing/play `fills the window`, fov 90, feet IN FRAME at 16:9 and 4:3. Forced GC fine, `layout save PASS, on disk` in both runs.
- **Frame times:** run 1 quality 0/3/4: 16.7 / 16.6 / 33.8 ms; menu 21.9, settings 48, after body page 54, after photo 59 (0464: 52 / 59 / –).
  Run 2: 16.7 / 16.7 / 34.5; settings 50, after body 58, after photo 64.
- **Saves:** backed up `VisexBody.sav` (`bddfbe76…`) and `GameUserSettings.ini` (`06e783cf…`), restored after each run, both md5s match. No `Game.ini` before or after.

## 2026-11-12 14:59 · VIS-EX-0464 Shipping (29353f7) · PASS, she walks back and turns onto the lean, proud idle reads, tour's Wardrobe step now fires too early

- **Tour (b7ded30, unchanged):** two runs from the mounted DMG, clean cook. Both reached `VisexQA: done` with 57 shots and exit 0
  (291 s, 293 s). On ground at every step (only photo motions 5/6/7/9 in mode 0, as 0451), no spectator, no new `.ips`.
  Shots in `Saved/QA_0464ship_run{1,2}/` and `Tools/.bridge/shots/qa0464/` (run 2 in `run2/`, sheets in `crops/`).
  New: a screen recording at ~3 fps during both runs, to see the walk back between tour shots.
- **Cook:** `0 error(s), 0 warning(s)`, 851 packages cooked, 843 in the IoStore. DMG md5 `2aef5675…`, copied to `VIS-EX-0464-stable.dmg`.
- **Return after Body:** she pushes off and walks clear of the stone, the page opens with her standing (run 2 frames, `crops/run2_body_stepout_and_return.jpg`).
  After Close she walks forward toward the column, turns through ~180° in small steps facing the lens, and settles on the lean
  (`crops/return_turn_after_body.jpg`). No backward steps. Feet slip couldn't be judged at 3 fps. The 1.4 m gap is a UE_LOG line, not in Shipping.
- **Menu back takes ~6 s now:** the menu only fades in once she's on the lean, ~6 s after Close (0451: under 4 s).
- **Wardrobe NOT exercised:** the tour calls `OpenWardrobe` 4.5 s after closing Body, while she's still walking back (`bOpen` false),
  so both runs log `wardrobe DID NOT OPEN` and shots 24–26 show the menu (`crops/body_wardrobe_shots.jpg`). A tour timing gap, not a
  game failure: the page code is unchanged and Body opens through the same path. The tour needs ~3 s more after the Body close (main owns VisexQATour).
- **Return after photo:** she's already at the lean after Close, menu back within ~1 s, hand on hip (`57_after_photo`, `crops/run1_photo_return.jpg`).
- **Proud idle:** on the Body page she stands in contrapposto, weight on one leg, right hand on the hip, chest up, no slouch (`21_body_tab_body`).
  The look-around is visible as a head turn between frames; not judged on stills.
- **Settings sit (c6ac475):** at 3.5 s the raised left leg is stretched out straight and level toward the panel, sole to the lens,
  right foot on the sand, hand on the stone (`11_settings_visuals`). Same in run 2.
- **Outfits:** all nine wearable on the menu lean. **Poses:** as 0451.
- **Start move:** 4:3 menu, mid-move, landing and play all `fills the window`, landing at fov 90. Feet IN FRAME at 16:9 and 4:3.
- **Regressions:** none. First menu column lit. GC and `layout save PASS, on disk` in both runs. Photo mode opens walking in place: `moved 0.0 cm in 1 s (PASS)`.
- **Frame times:** run 1 quality 0/3/4: 16.7 / 16.7 / 34.5 ms; menu after quality 4 a little below 0451 (settings 52 vs 56, after body page 59 vs 68).
  Run 2 was much slower (34 / 76 ms at quality 3/4, menu 79–97): the machine was busy with other sessions, not comparable.
- **Saves:** backed up `VisexBody.sav` (`bddfbe76…`) and `GameUserSettings.ini` (`06e783cf…`), restored after each run, both
  md5s match. No `Game.ini` before or after.

## 2026-11-12 12:35 · VIS-EX-0451 Shipping (65be362) · PASS, first menu column lit, Settings sit and Body step-out work, menu frame times a little up

- **Tour (b7ded30, unchanged):** two runs from the mounted DMG, clean cook. Both reached `VisexQA: done` with 57 shots and exit 0
  (292 s, 282 s). On ground at every step, no spectator, no new `.ips`. Shots in `Saved/QA_0451ship{,_run2}/` and
  `Tools/.bridge/shots/qa0451/` (run 2 in `run2/`, sheets in `crops/`).
- **Cook:** `0 error(s), 0 warning(s)`, 845 packages cooked, 837 in the IoStore. DMG md5 `6c893ee8…`, copied to `VIS-EX-0451-stable.dmg`.
- **First menu (fixed):** the drum is lit on the first menu now, flutes and pitting read as on `05_menu_again`
  (`crops/first_menu_0441_vs_0451.png`: 0441 black, 0451 first menu, 0451 menu again). Same in run 2.
- **Settings sit:** at 1.5 s she's mid-climb, hip on the drum's front edge (`10_settings`). At 3.5 s (after the 2.15 s perch
  key) she's perched: right foot on the sand, left leg raised with the sole to the lens, hand on the stone behind her
  (`11_settings_visuals`, `crops/settings_sit.png`). The left knee reads bent rather than stretched long. Camera frames
  her head and both feet in both shots. After Back she's on the lean again (`12_outfit_None`). Same in run 2.
- **Body page:** opens at 4 s, frames her as on 0441, head to thighs (`crops/body_page.png`). The step-out walk and
  "no foot lift" aren't visible: the tour shoots only after the page opens, feet out of frame. Back at the lean after
  closing (`after body page` at 9500 -900, on ground).
- **Outfits:** all nine wearable on the menu lean (`crops/outfits.png`). Wardrobe order as on 0441, `wardrobe open`.
- **Poses (11 motions):** as 0441 (`crops/photo_poses.png`). Crown and Peace still cut her head, Ledge end pose still not caught.
- **Start move:** 4:3 menu, mid-move, landing and play all `fills the window`, landing at fov 90. Feet IN FRAME at 16:9 and 4:3.
- **Regressions:** none. GC and `layout save PASS, on disk` in both runs. Photo mode opens walking in place: `moved 0.0 cm in 1 s (PASS)`.
  The dark patch with specks under her feet on the menu was there on 0441 too.
- **Frame times:** quality 0/3/4: 16.7 / 16.7 / 34.8 ms (run 2: 16.7 / 19.5 / 43.5). Menu after quality 4 runs 2–9 ms
  above 0441 (settings 56 vs 53, after body page 68 vs 59): cause unknown, could be noise, worth a look.
- **Saves:** backed up `VisexBody.sav` (`bddfbe76…`) and `GameUserSettings.ini` (`06e783cf…`), restored after each run, both
  md5s match. No `Game.ini` before or after.

## 2026-11-11 23:25 · VIS-EX-0441 Shipping (e9f0adf) · STABLE PASS, new fluted menu column reads well once lit, Mirror and Silversuit wearable, Ledge end pose still not caught

- **Tour (b7ded30, unchanged):** two runs from the mounted DMG, clean cook. Both reached `VisexQA: done` with 57 shots and exit 0
  (290 s, 282 s). On ground at every step, no spectator, no new visex `.ips`. Shots in `Saved/QA_0441ship{,_run2}/` and
  `Tools/.bridge/shots/qa0441/` (run 2 in `run2/`, sheets in `crops/`).
- **Cook:** `0 error(s), 0 warning(s)`, 834 packages in the IoStore. DMG md5 `a4af5d9a…`, copied to `VIS-EX-0441-stable.dmg`.
- **GC and layout:** both forced GCs pass in both runs. `layout save PASS, on disk`.
- **Menu column (SM_LeanColumn_HD, FitLeanColumn):** crisp. Flutes with sharp arrises, chipped edges and pitting all read at
  menu distance (`crops/05_menu_again_column.png`, `crops/outfits.png`). She leans on it, hip against the flutes, hand on
  the top edge: no gap, nothing sinks. `01_menu` and `02_menu_feet` (1 s apart): same pose, small even sway, no jitter.
  **But on the first menu after launch the drum is a black mass** (`crops/01_menu_column.png`): it sits in shadow, the
  flutes don't show. From `05_menu_again` on it's lit. 0401's first menu had the same shadow (`crops/menu_0401_vs_0441.png`),
  so it's the first-menu light, not the mesh. Worth a look: it's the first thing Tema sees.
- **Outfits:** Bodystocking, + skirt, Catsuit, Silversuit, Noir, Mirror, Harness and Tartan all wearable at her size
  (`crops/outfits.png`, run 2 `crops/run2_outfits.png`). Silversuit: full-body metallic gold-silver, belt with a hip cutout, ankle
  cuffs. Mirror: glossy black cups and brief over a sheer bodice, choker, arm cuff (`crops/silversuit_mirror_torso.png`).
  No skin through cloth at menu distance. Not checked in deep bends: photo mode runs in the default Bodystocking.
- **Wardrobe order:** Nothing, Sheer bodystocking, + skirt, Latex catsuit, Silversuit, Noir lace, Mirror, Harness, Tartan, then
  Garter set, Cocoa satin, Fishnet locked (`24_wardrobe`). Noir, Mirror, Harness, Tartan as asked.
- **Poses (11 motions):** Idle, Walk, Run, Bookend, Crown, FeetUp, Glance, Ledge, LegLift, Mermaid, Peace. Crown is in, labelled
  CROWN (`34_photo_04`), but the close camera frames hips to thighs, head cut. Mermaid: seated end pose, legs swept, peace hand by
  her face (`39_photo_09`). **Ledge: end pose not caught again**: she's moved to (8884, -354) on the stone (trace z 101), but at
  +4.5 s still stands upright, stone out of frame (`37_photo_07`). LegLift right after is still in its kneeling flow (`38_photo_08`).
  Sheets `crops/poses_a.png`, `crops/poses_b.png`.
- **Growing from the soles:** not checked, the tour doesn't move body sliders.
- **Start move:** 4:3 menu, mid-move, landing and play all `fills the window`, landing at fov 90. Feet IN FRAME at 16:9 and 4:3.
- **Regressions:** none. Body and Wardrobe `open` at 4 s. Photo mode opens walking in place: `moved 0.0 cm in 1 s (PASS)` both runs.
- **4K column close-up:** not taken. The editor's `take_high_res_screenshot` with `camera=MenuCam` shot the editor viewport instead, so it was dropped.
- **Frame times:** quality 0/3/4: 16.7 / 16.7 / 35.2 ms (run 2: 16.7 / 17.3 / 36.3). Load around 3.5.
- **Saves:** backed up `VisexBody.sav` (`37c1a469…`) and `GameUserSettings.ini` (`0f8144b4…`), restored after each run, both md5s
  match. The tour rewrites both. No `Game.ini` before or after.

## 2026-11-11 20:23 · VIS-EX-0401 Shipping (4d4be67) · PASS, classic photo mode walks in place, Harness and new Tartan wearable, Ledge end pose not caught

- **Tour (b7ded30):** two runs from the mounted DMG, clean cook. Both reached `VisexQA: done` with 54 shots and exit 0
  (285 s, 275 s). On ground at every step, body, shell, hair and outfit present, no spectator. No new visex `.ips`
  (the only one is 13:25). Shots in `Saved/QA_0401ship{,_run2}/` and `Tools/.bridge/shots/qa0401/` (run 2 in `run2/`,
  sheets in `crops/`).
- **Cook:** `0 error(s), 0 warning(s)`, 825 packages in the IoStore. DMG md5 `f2da54d1…`.
- **GC and layout:** both forced GCs pass in both runs (`menu again` reports after the lean-down GC). `layout save PASS, on disk`.
- **Classic photo mode (b7ded30):** opens on the PHOTO MODE / WALK HUD and walks in place: `moved 0.0 cm in 1 s (PASS)`
  in both runs (`26_photo_open_walk`, `27_photo_open_walk_1s`). The un-lean itself isn't shot: the camera takes over
  between +0.8 and +1.5 s, she stays at the menu spot. The default photo camera is close: in Idle, Walk, Run, Bookend
  and Peace the top of her head and her feet leave the frame (`crops/photo_poses.png`).
- **Menu:** `01_menu` and `02_menu_feet` are 1 s apart (the tour has no 0.5 s pair). Same pose, a small even sway, no
  arm or torso jump; her hand rests on the drum's edge in both (`crops/01_menu_torso.png`, `02_menu_feet_torso.png`).
  Feet IN FRAME at 16:9 and 4:3 in both runs (toes at 85–87 % and 76–77 %).
- **Outfits:** Bodystocking, Bodystocking + skirt, Catsuit, Noir, Tartan and Harness all wearable at her size
  (`crops/outfits.png`). Harness: cups with a ring between, bib collar, strap V, matte black chaps. Tartan (rebuilt):
  smooth strap tapes, hip strings, small dark-red cups, lace thigh-high; the weave doesn't read at menu distance
  (`crops/tartan_chest.png`). Noir and Catsuit unchanged against 0370 (`crops/tartan_harness_noir_torso.png`).
- **Deep bends:** FeetUp, Glance, Mermaid and the LegLift kneel hold the bodystocking, no skin through (`crops/deep_bends.png`).
- **Poses:** 10 motions: Idle, Walk, Run, then Bookend, FeetUp, Glance, Ledge, LegLift, Mermaid, Peace. Bookend is on the wheel.
- **Ledge (fe20bd7, 417ef8e):** on the wheel. Choosing it moves her from the menu spot to (8884, -354), with the stone's
  top traced under her (z 101 vs sand 66), movement off, in both runs. **Not caught: the end pose.** At +4.5 s she
  still stands upright, and the shot frames her hips to head, so the stone and the kneel on her forearms aren't in it
  (`34_photo_06`). LegLift right after it is still in its kneeling flow (no `Ledge_To_LegLift`), so its end pose
  isn't shot either. Needs a longer wait in the tour or a look by hand.
- **Start move:** 4:3 menu, mid-move, landing and play all `fills the window`, landing at fov 90 (`crops/start_move.png`).
- **Regressions:** none. Body and Wardrobe report `open` at the 4 s shot. No fall-through.
- **Frame times:** quality 0/3/4: 16.7 / 16.6 / 35.6 ms (run 2: 16.7 / 22.5 / 49.4). Load around 4.
- **Saves:** backed up `VisexBody.sav` (`37c1a469…`) and `GameUserSettings.ini` (`0f8144b4…`), restored after each
  run, both md5s match. The tour rewrites `VisexBody.sav` (to `7ac129aa…`), hence the restore. No `Game.ini` before or after.

## 2026-11-11 17:49 · VIS-EX-0370 Shipping (ce9b744) · STABLE PASS, the photo sequence runs end to end, Noir wearable, jump framing cuts her head

- **Tour (updated in 43f3455):** two runs from the mounted DMG, clean cook. Both reached `VisexQA: done` with 55 shots
  and exit 0. She plays as the character, on ground at every step, body, shell, hair and outfit present
  throughout. No new visex `.ips`. Shots are in `Saved/QA_0370ship{,_run2}/` and `Tools/.bridge/shots/qa0370/`
  (run 2 in `run2/`, sheets in `crops/`). A dry run on VIS-EX-0365 (43f3455, before Noir) is in `qa0365/` and
  serves as the outfit baseline.
- **Cook:** `0 error(s), 0 warning(s)`, 817 packages in the IoStore.
- **GC and layout:** both forced GCs pass in both runs. `layout save PASS, on disk`.
- **Photo sequence (7048985):** opens seated against the stone (`25_photo_sit_open`). From choosing to +4 s one
  sit-down montage plays, so the sit doesn't restart. Before it, she pushes off the menu lean and is put 11 cm away
  at the sit spot. Slow walk, walk, run and dash keep her whole body in frame (`crops/photo_seq.png`). The return
  walks her back, she stops and sits (`38_photo_return_sit`), in both runs.
- **Feet on the move:** the tour now measures the planted foot over 0.1 s. Walk and slow walk are planted in both
  runs (0.1 to 2.3 cm while she moves 7 to 15 cm). Run and dash read mixed, but at those speeds both feet leave
  the ground, so a still frame can't separate slide from flight. That needs a clip.
- **Jump: the camera cuts her head at the apex** in both runs and in the dry run (`30_photo_jump_apex`). The tour
  shoots as her vertical speed turns, about 1.4 m up. It lands clean, on ground.
- **Poses:** shot 5 s after each switch, all six now show the end pose, not the flow (`crops/photo_poses.png`):
  Bookend's kick, FeetUp, Glance, LegLift with head in hand, Mermaid, Peace. No limb through the sand.
- **Menu feet (620fd62):** heels, balls and toe tips project inside the frame at 16:9 and 4:3 in both runs
  (`menu 16:9 feet IN FRAME`, toes at 87 % and 77 % of the height). Visible in `02_menu_feet` and `06_menu_4x3`.
- **Noir (0723040, 7d8adee):** wearable ("Noir lace"), fitted at her size: lace cups, cincher with boning, glossy
  bindings, dark stockings (`crops/tartan_noir_torso.png`).
- **Outfit materials after the shared bases were re-saved:** no regression. Bodystocking, Bodystocking + skirt,
  Catsuit and Tartan look the same as on 0365 (`crops/outfits_0365_vs_0370.png`).
- **Tartan (c77501d):** wearable, fitted: red tartan cups with black bound edges, collar and tie, lace thigh-high top.
  At menu distance the weave reads as plain dark red.
- **FeetUp skirt hem (b9878e4): not checked.** Photo mode runs in Tema's saved Bodystocking, with no skirt.
- **Regressions:** none. Start move good: 4:3 menu, mid-move, landing and play all `fills the window`, landing at
  fov 90. Body and Wardrobe report `open` at the 4 s shot. Catsuit is unchanged. No HUD in the turnaround.
- **Frame times:** quality 0/3/4: 16.7 / 24.1 / 52.9 ms (run 2: 16.7 / 23.3 / 50.9). Load was around 7, not idle.
- **Saves:** backed up `VisexBody.sav` (`7ac129aa…`) and `GameUserSettings.ini` (`0f8144b4…`) and restored them
  after each run. Both md5s match. No `[VisexMenuLayout]` before, and no `Game.ini` before or after.

## 2026-11-11 16:50 · VIS-EX-0351 Shipping (81546fe) · PASS, she steps off on her own, Body and Wardrobe now inside 4 s

- **Tour:** two runs from the mounted DMG, clean cook. Both reached `VisexQA: done` with 46 shots (the Catsuit adds
  one) and exit 0. She plays as the character, not a spectator, on ground at every step. Body, shell, hair and
  outfit are present throughout. No new visex `.ips` (one `zen` report only). Shots are in
  `Saved/QA_0351ship{,_run2}/` and `Tools/.bridge/shots/qa0351/` (run 2 in `run2/`, crops in `crops/`).
- **Cook:** `0 error(s), 0 warning(s)`, 801 packages in the IoStore.
- **GC and layout:** both forced GCs pass in both runs. `layout save PASS, on disk`.
- **Start move (abd10ac, eb286e6):** good. 4 s after Start, before the tour pushes any input, she's already 4.4 m
  from the menu spot and mid-stride (`02_play`), so she steps off on her own. No frozen stand. The 4:3 mid-move
  frame shows her walking away, sharp (`06_start_4x3_mid`); the landing reports the third-person camera at fov 90
  (`07_start_4x3_landing`). Both runs match. **The 0.6 s step-off time itself isn't measurable**: the first
  sample is at the orbit's half-way point.
- **Stand-up 1.0 s:** Body and Wardrobe now report `open` with her standing in the page at the 4 s shot
  (`15_body_tab_body`, `18_wardrobe`). In 0320/0333 they arrived after it. The exact delay needs a clip.
- **Menu lean:** she leans on the drum, her right hand on its front and in view (`01_menu`, `14_outfit_Catsuit`).
- **Photo poses:** FeetUp, Glance, LegLift, Mermaid, Peace all run. **The tour shoots 2.5 s after each switch,
  which catches the floor-to-floor flows in transit** (same as 0333, `crops/photo_grid_0333_0351.png`), so
  LegLift's head-in-hand end pose isn't seen. Glance has both hands planted on the sand (`26_photo_04`). LegLift's
  head stays above the sand in both runs. No hand through a limb in any frame. Mermaid shows a ragged
  bodystocking edge at her right hip (`28_photo_06`).
- **Catsuit (577b9a3, 2fe615f):** wearable from the outfit row ("Latex catsuit", not locked), glossy black, fitted
  at her size (`14_outfit_Catsuit`). A thin skin-coloured strip may show at the back of the waist above the belt
  (`crops/cat_waist.png`), worth a closer look.
- **Toes (2f5d00a, 4735035):** no breakage in any shot. The menu camera now crops her feet, and in the other
  shots the toes sink into the sand, so the 0333 pale sliver can't be confirmed gone (`crops/toes_zoom.png`).
- **Bounds ×2 (1fcba70):** she's drawn in all 8 head-and-shoulders turnaround frames (`crops/turn_grid.png`). No
  HUD in the turnaround.
- **Frame times:** quality 0/3/4: 16.7 / 23.0 / 51.0 ms (run 2: 14.3 / 42.6 / 82.2), with load around 20 from
  a C++ build in another worktree, so not comparable.
- **Saves:** backed up `VisexBody.sav` (`7ac129aa…`, changed since 0333) and `GameUserSettings.ini`
  (`0f8144b4…`) and restored them after each run; both md5s match. No `[VisexMenuLayout]` before, no `Game.ini`
  before or after.

## 2026-11-11 15:43 · VIS-EX-0333 Shipping (31f4903) · STABLE PASS, 4:3 menu fixed, toe tips still show a pale sliver

- **Tour:** two runs from the mounted DMG, clean cook. Both reached `VisexQA: done` with 45 shots and exit 0,
  and the two runs' shots match. She spawns and plays as the character, not a spectator: `play` and `walked`
  report the third-person view, on ground, and the walk moves her about 5 m. Body (`SKM_Quinn_Simple`), shell
  (`SKM_AndroidSkin`), hair and outfit are present at every step. No new visex `.ips`; the only new reports are
  `zen` ones from before the build. Shots are in `Saved/QA_0333ship{,_run2}/` and `Tools/.bridge/shots/qa0333/`
  (run 2 in `run2/`), with comparisons against 0320 in `crops/`.
- **Cook:** 809 packages, `0 error(s), 0 warning(s)`. Nothing mentions either mesh, and there's no
  Payload, corrupt or bulk-data warning. The skeletal meshes came from the DDC, so this doesn't re-derive them.
- **GC and layout:** both forced GCs pass in both runs (lean-down and photo mode). `layout save PASS, on disk`.
- **Menu at 4:3 (c359518):** fixed. The full text shows: VIS/EX-0333, Start, Body, Wardrobe, Photo mode,
  Settings, Quit (`05_menu_4x3`). All four 4:3 checks report `fills the window`. **21:9 (31016d2) isn't
  checked**: the tour has no 21:9 step.
- **Menu lean and idle beats (895aa7d, a092f91):** she leans on the drum in both runs, before and after the GC,
  on the ground. Her feet now stand apart in the lean stance, not crossed as in 0320 (`crops/cmp_04_menu_again.png`).
  A beat itself isn't visible in stills. In photo poses her feet follow each pose (`crops/photo_grid_0333.png`).
- **Toes (696182d, e55d62a):** much better. 0320's big pale patch on the big toe is gone
  (`crops/toes_zoom_0320_0333.png`, `crops/cmp_feet_turn.png`). **But each toe tip still shows a thin pale sliver
  with a hard dark rim**, most visible in the menu close-up (`crops/toes_menu_both.png`). There are no catsuit
  pieces on her toes.
- **Skirt (7c33f3b):** clean in the menu lean, no seat hole or thigh through it (`crops/cmp_skirt_menu.png`).
  **Photo poses with the skirt aren't checked**: photo mode runs in Tema's saved Bodystocking.
- **Walk head (a771a5a):** not checkable from stills. `03_walked` is taken after she stops, and the WALK photo
  crops her head. Side-to-side sway needs a clip.
- **Against 0320:** no regression. She's clothed at the right size in every outfit (`crops/outfits.png`), and
  there's no HUD in the turnaround. Body and Wardrobe still arrive after the 4 s shot, as in 0320. Photo
  motions 3–7 report `mode 0`, as before. Quality 0/3/4: 16.7 / 23.3 / 59.2 ms (run 2: 19.5 / 25.3 / 61.6),
  with load around 5 and two other editors open, so not comparable.
- **Saves:** I backed up `VisexBody.sav` and `GameUserSettings.ini` and restored them after each run. Both md5s
  match the originals, `29ae709f…` and `0f8144b4…`. GameUserSettings had no `[VisexMenuLayout]` before the run.
  There's no `Game.ini`, before or after. The release copy is `Build/VIS-EX-0333-stable.dmg`.

## 2026-11-11 14:15 · VIS-EX-0320 Shipping (8a90666) · PASS, but her big toe pokes through the stocking and 4:3 clips the menu

- **Tour:** one run from the mounted DMG. No crash: it reached `VisexQA: done`, took 45 shots and exited 0.
  She stays on the ground throughout, with body, shell, hair and outfit present. No new visex `.ips`;
  the only one is 0298's, from 13:25. Shots are in `Saved/QA_0320ship/` and `Tools/.bridge/shots/qa0320/`,
  with crops and comparisons against 0307 in `crops/`.
- **GC (fede9ad):** both forced GCs pass: one during the lean-down (`04_menu_again`, right pose), and one in photo mode.
- **Layout SAVE AS DEFAULT (bd53e33):** `layout save PASS, on disk` in GameUserSettings.ini. The tour
  removes the key again afterwards. **Surviving a relaunch isn't checked**, because the tour doesn't test it.
- **Menu fills the window (491932e):** every 4:3 check reports `fills the window`: menu, mid-move, landing
  and play. With no bars at any of those four points, there are none to snap off. Back at 16:9, it fills too.
  At 4:3 her full height still fits, head to toes (`05_menu_4x3`). **But the 4:3 view crops the sides, so the menu
  panel is cut off on the left:** it reads "tart", "ody" and "IS/EX". Her raised arm leaves the frame on the right.
- **Toes (merge):** **regression.** In the bodystocking, her big toe pokes through the stocking foot. It shows as
  a pale skin patch with a hard dark rim (`crops/cmp_menu_foot.png`: 0307 above, 0320 below;
  `crops/toes_front_zoom.png`, `crops/feet_turn1.png`). The other toes and her footprints look right. The walk
  (`03_walked`) is too far away to see toes.
- **Outfits (merge):** both wearable outfits fit her at the right size, and she's clothed in both
  (`crops/outfits.png`: None, Bodystocking, BodystockingSkirt). The locked one in the wardrobe (`18`) fits too.
- **Knit moiré (merge):** fixed. The chevron hatching on the knit at her right shoulder, which 0305 and 0307 showed
  in the back view, is gone (`crops/cmp_shoulder_patch.png`). The back body view is clean too (`crops/cmp_back_knit.png`).
  The hair's stair-stepped edge next to it is still there.
- **Drum lean (merge):** her arm now reaches back to the drum, instead of a folded wrist at her hip
  (`crops/cmp_hand.png`). From the low menu camera, the hand itself is hidden behind the drum's edge, so
  "resting on it" isn't directly visible. Only one hand is in view.
- **Body and Wardrobe arrive late:** 4 s after opening, `14_body_tab_body` and `17_wardrobe` still show a
  motion-blurred move away from the column, with no UI. In 0307, the page was up by then. Face and hair (`15`, `16`)
  are fine, so the page does open; it just takes longer than 4 s now. This is probably the slower leave-the-column
  walk (70fd14b). Either the tour's wait or the move needs a look.
- **Against 0307:** otherwise no regression. There's no HUD in the turnaround shots. Photo motions 3–7 report `mode 0`, as before.
  Quality 0/3/4: 16.7 / 16.7 / 34.7 ms, with load 3.6–5.8 and the character editor open.
- **Saves:** I backed up `VisexBody.sav` and `GameUserSettings.ini` and restored them after the run. Both md5s
  match the originals, `29ae709f…` and `0f8144b4…`. Without the restore, GameUserSettings.ini would have stayed
  changed (`2eb25b4b…`). There's no `Game.ini`, before or after.

## 2026-11-11 13:56 · VIS-EX-0307 Shipping (0dd50c9) · PASS, the 0298 lean crash is fixed

- **Crash fix:** the tour now forces a full GC 0.3 s after ToggleMenu, while the lean-down is
  still playing. I ran it twice from the mounted DMG. Both runs got through it and reached
  `VisexQA: done`, with 41 shots each and exit 0. No new visex `.ips` in DiagnosticReports.
  The only new reports are three `zen` ones from 13:44, which came from another worktree's
  editor starting up, not from the game.
- **Menu again** (`Tools/.bridge/shots/qa0307/04_menu_again.png`, `run2/04_menu_again.png`): after
  the GC she leans on the column in the right pose, on the ground, holding the idle montage. It
  matches 0305's shot.
- **Against 0305:** no regression. Every screen and pose matches. In run 1 the turnaround shots
  were a bit darker (body 5: luma 102 against 116). Run 2 matched 0305 exactly, so that's run-to-run
  exposure, not a change. Only code changed between the two builds. Photo motion 3–7 report
  `mode 0`, as in 0305. Quality 0/3/4: 16.7 / 21.1 / 44.8 ms. Load was about 4 with two other
  editors open.
- **Shots:** in `Saved/QA_0307ship{,_run2}/` and `Tools/.bridge/shots/qa0307/` (run 2 is in `run2/`).
- **Saves:** I backed up `VisexBody.sav` and `GameUserSettings.ini` and restored them after each run.
  Both md5s match the originals: `29ae709f…` and `0f8144b4…`. There's no `Game.ini`, before or after.

## 2026-11-11 13:41 · VIS-EX-0305 Shipping (5964443) · PASS, menu bokeh in, only the elf hair checked

- **Tour:** no crash. She stays on the ground, with body, shell, hair and outfit present, and all 41 shots
  were taken. Shots are now 1920×1080; 0291's were 1600×900. They're in `Saved/QA_0305ship/` and
  `Tools/.bridge/shots/qa0305/`, with crops in `crops/`.
- **Menu (f/2.8):** she's sharp: face, hands and the front foot (`crops/menu_head.png`,
  `crops/menu_feet.png`). The colonnade behind her is clearly softer than in 0291's `01_menu`,
  where the brickwork was crisp. Here it's a smooth orange wash. Start move: the tour takes no frames
  between Menu and Play, so a focus pop at landing isn't checked.
- **Hair:** Tema's save wears **Elven**, so every shot is the elf style. **The long style isn't
  checked** (layered strands, darker roots).
  - Face locks have body from the side, with strand lines (`35_turn_head_2`, `36_turn_head_3`).
    From the front and in the menu they read as flat black ribbons with a pale rim.
  - Hairline (`crops/elf_hairline.png`): no bald parting. **No baby hairs visible**: the edge is hard,
    with a red-brown rim along the forehead. The turnaround head shots cut off her hairline.
  - No white strands against the haze, from the back or in the walk.
  - Back (`37_turn_head_4`): solid strands, but there's a smeared grey sheen at the crown, and a pale,
    see-through, chevron-striped patch at her right shoulder (`crops/head4_shoulder_patch.png`).
  - Side (`35_turn_head_2`): a hard stair-stepped edge on the hair above the ear
    (`crops/head2_top_step.png`).
  - Physics: nothing through the body in play, walk or the 8 poses. Popping needs motion and isn't
    checked.
- **Skirt rings:** not checked. The skirt only shows in `09_outfit_BodystockingSkirt` (menu camera,
  front); photo mode runs in the Bodystocking.
- **Sand:** prints behind her in `03_walked` and `29_turn_body_4`, feet set in the sand. There's no
  HUD in the turnaround shots.
- **Frame times:** these aren't comparable. Load was 4.5 at the start, with other editors and Blender
  running, and the hair is elf, not long. Quality 0/3/4: 8.3 / 16.3 / 37.2 ms. 0291 had 17.3 / 17.0 /
  27.5. Photo poses ran 52–63 ms against 0291's 44–61.
- **Saves:** both files were restored from backup and their md5s match. During the tour,
  `VisexBody.sav` was replaced by an older 11:17 copy without the elf hair, with its mtime kept. That
  looks like another session's restore, not the game. Tema's 13:24 save is back.

## 2026-11-11 · VIS-EX-0291 Shipping (e5a260b) · PASS

- **Tour:** no crash, on ground, all 41 shots. Orbit landing smooth (0.656 → 0.543 → 0.427 → 0.397 → 0.388).
- **Body page:** BODY / FACE / HAIR tabs, with E and Q stepping both ways. FACE shows Head plus its note,
  HAIR shows "Long, straight" WORN. Escape returns to the menu with Body selected.
- **Wardrobe:**
  - It opens on the worn costume. Down tries on the next one, so the skirt appears; nothing is skipped.
  - Locked rows leave her as she is, and Enter opens UNDER CONSTRUCTION. Enter or Escape closes it.
  - Escape returns to the menu with Wardrobe selected.
- **Saves:** Tema's `VisexBody.sav` is unchanged (same md5). `GameUserSettings.ini` was restored from a
  backup after the tour.
- **Mounds:** her feet sit in soft sand with mounds shaded like the dunes, and prints show behind her.
  No pale pads or rims.
- **Turnaround:** body shots now frame her whole, with no HUD. Head 2 (side) is still mostly a hair
  silhouette against the sky. Shots are still 1600×900.
- **Low-camera wind:** no horizontal streaks in the play and walk shots. A real grazing-angle check
  needs a lower camera than the tour has.

## 2026-11-10 21:45 · VIS-EX-0286 Shipping (ab61ef3 + QA.md) · PASS, close-ups show hair-card edges

- **Automated turnaround (new):** 8 body and 8 head shots at 1600×900 (window clamped), no keys.
  - The body shots cut off the top of her head; the camera sits too low or too close for 300 cm.
  - The HUD shows in every turnaround shot.
  - Head shots 0/1/7 frame well. Side shot 2 is mostly a silhouette of hair against the sky.
- **Skin:** no red under the chin, at the collarbone or on her right shoulder (`26_turn_head_0`).
  Pores are smooth even at 95 cm.
- **Hair:**
  - At normal distance the front lock is dark; the back is solid and full (`22_turn_body_4`).
  - Close up, card edges show: a grey striped see-through card beside her face
    (`crops/head0_face_lock_card.png`), and pale rectangular card outlines along the side
    (`crops/head2_side_card_edges.png`, also the left edge of `33_turn_head_7`).
  - Pale ends at her left hip: not seen in the body shots.
- **Regression:** orbit landing smooth (0.636 → 0.509 → 0.419 → 0.394 → 0.388), on ground, no crash.

## 2026-11-10 21:15 · VIS-EX-0281 Shipping (e1537a4) · regression PASS, hair partly checked

- **Orbit landing:** smooth, 0.597 → 0.432 → 0.401 → 0.391 → 0.386. Photo mode on ground. No crash
  across the tour and 5 plain launches.
- **Hair from behind:** solid black and full, no horizontal lines, not duller.
  - The face lock seen from behind: it now reads as dense dark strands; the beige see-through card
    of 0278 is gone (`QA_0281ship/turn_backonly/`).
  - A few thin pale strand ends still poke out at her left hip.
- **Front, 3/4, side and the right-shoulder strands:** not checked. The Mac's keyboard layout is
  Russian, so the System Events keypresses for D and P don't reach the game as letters. P by key
  code works; D by key code doesn't turn her. Arrow keys only orbit a few degrees.
- **Fix for next time:** have the layout set to ABC/US before the turnaround, or add a turnaround stop
  to the QA tour (in C++, no keys needed).
- **Rerun with the layout switched to British (21:25):** hair PASS.
  - The face lock at the 0276/0278 angle is solid dark strands, with no beige, no see-through and no
    card lines (`turn/crops/face_lock_0281.png`).
  - 3/4 and side views: clean, no ear spike, and the strands no longer look pale.
  - The red under the strands at her right shoulder is the skin's dark-red shadow under her chin,
    not a hair tint.
  - Still left: one thin pale strand at the far edge in the side views (`head_turn_06/07`), and a
    clump-cut lock in front of her left breast in 3/4 (`head_turn_03`).

## 2026-11-10 20:10 · VIS-EX-0278 Shipping (ab3ac2e) · regression PASS, hair backlog not fixed

- **Orbit landing:** smooth. Luma eases 0.608 → 0.467 → 0.411 → 0.393 → 0.387.
- **Photo mode:** on ground. No crash.
- **Pale lock by her face on the left (front view):** still beige and see-through, slightly wider than
  in 0276, and now showing hard horizontal lines like a flat card. Compare
  `QA_0278ship/turn/crops/face_lock_0276.png` with `face_lock_0278.png`.
- **Thin pale strands on her side** (turn_06/07 ≈ 0276 turn_8/9): unchanged.
- **Red tint on the strands over her right shoulder:** unchanged.
- **Back:** not duller; still solid black.
- **Process:** Tema's game was closed before any keypress. Only screenshots newer than the test were
  moved; his two 19:48 photos stayed in place.

## 2026-11-10 19:56 · VIS-EX-0276 Shipping (a3988b1) · PASS, hair much improved

- **First run invalid:** Tema was playing. The capture now binds to the game's PID, and keypresses
  only go out once no other VIS-EX is running.
- **Orbit landing:** smooth. Luma eases 0.716 → 0.598 → 0.462 → 0.429 → 0.418 → 0.412, with no step.
- **Hair turnaround** (8 P shots, D-turned), `QA_0276ship/turn/` with head crops in `crops/`:
  - fixed versus 0269: the back is fuller and solid black, the front strands are dark, the arms are
    clean with no smudges, and there's no ear spike in the 3/4 or side views;
  - left over: a few thin pale strands at the edge against the bright haze (front, by her face on the
    left, in turn_7; on her side in turn_8/9), and a red-tinted strand on her shoulder in turn_7.
- **Photo mode:** on ground. No crash in the tour or the plain launch.
- **Lesson:** the game's `Saved/Screenshots` is shared with Tema's own installed copy. Only move
  shots newer than the test's start (two of his 19:48 photos were swept up, then restored).

## 2026-11-10 19:35 · VIS-EX-0273 Shipping (0ee4182) · PASS, release

- **Orbit landing is smooth now.** The corner shade fades its own Strength (7a92c5a). Luma eases
  0.597 → 0.448 → 0.401 → 0.387 → 0.383 → 0.379 over about 1 s and settles where 0269 jumped to,
  with no step. Frames are in `QA_0273ship/orbit/`.
- **Photo mode:** on ground at z 145. Walk, Run and Feetup play, and P writes a 3840×2160 HighResShot.
- **Escape** returns to the lit menu.
- **No crash** in the tour or the plain launch.
- **Not rechecked in this fast pass:** hair front view, skin, and wind.

## 2026-11-10 19:30 · VIS-EX-0269 Shipping (b317694) · FAIL on one item: the orbit landing still pops

- **Orbit landing:** same pop as 0259, despite the 0.6 s blend (7c31bac). At 14.7 → 15.0 s, with
  identical framing, mean luma goes 0.240 → 0.381 and the sky 0.144 → 0.465. There's no intermediate
  frame at a ~0.3 s capture interval, and the values are flat afterwards. Frames are in `QA_0269ship/orbit/`.
- **Menu wind:** calm, with no visible grains. Frame-to-frame change is 0.006, including her idle. Whether
  full wind comes back after Start can't be told from stills; it needs someone watching.
- **Hair:** long and straight, reading fine from the back.
  - From the front (photo mode, turned with D, P → HighResShot): the strands on her left go pale and
    see-through against the haze, and there are dark hair smudges on her upper arm.
  - It doesn't read as clearly fuller from the back.
- **Skin:** no WorldGridMaterial grid anywhere (menu, play, photo, 4K front crop). There's a strong
  red-brown patch under her chin and at the collarbone.
- **Regressions:** none. She's lit and sharp on the menu, idle text is white and dimmed with its shadow,
  and photo mode stays on the ground (z 145). A footprint shows by her foot. No crash in 2 tours and
  2 plain launches.

## 2026-11-10 19:02 · VIS-EX-0259 Shipping (80fb80e) · FAIL on one item: the Start orbit's brightness pops on landing

- **Menu:** she's lit and sharp. Idle items are white and dimmed. The text shadow is subtle but present.
  The tour at 3840×2160 windowed got clamped to 1600×900, because the displays max out at 3456 wide.
- **Start orbit (window captures every ~0.15 s):** the key light fades smoothly. When the camera lands
  at 13.5 → 13.7 s, the whole frame jumps from mean luma 0.24 to 0.38, and the sky from 0.14 to 0.47.
  That happens within one capture interval while the framing stays identical, which looks like the
  menu's grade or vignette cutting off. There's no earlier capture to compare with.
  Frames are in `Saved/QA_0259ship/orbit/`.
- **Settings:** readable. The Visuals values are small and low-contrast on the bright orange.
- **Photo mode:** on ground, with the dunes visible and poses applied. No crash. A plain launch reaches the menu.
- **P in photo mode** (Shipping, sent as System Events keypresses): `HighResShot 2` works. It wrote a
  3840×2160 frame with no HUD, now moved to `QA_0259ship/P_key_HighResShot_3840x2160.png`.

## 2026-11-10 18:50 · VIS-EX-0253 Shipping (145ec09) · PASS, release candidate

- **No crash:** the tour reached `done`, there's no new `.ips`, and a plain launch reaches the menu.
- **Photo mode is fixed:**
  - She stays at z 145 with ground traced under her, and the dunes are visible in every photo shot.
  - Poses 3–7 report `mode 0, floor none` with z fixed and ground still traced: that's the pose mode, not a fall.
  - Poses now apply visibly.
- **Footprints:** the mounds are gone. `03_walked` shows one print but no clear trail, because the camera
  faced away from her path, so this check is inconclusive.
- **Hair:** reads right in play and photo. The front lock over her eye in 3/4 view is unchecked (no such
  shot, and the Body page was skipped). There's a possible stray dark card beside her head in `14_photo_04`.
- **Not a bug:** the drift during the quality steps was Tema steering her while the tour ran.
  Perf numbers and the "walked" camera angle from this run are unreliable for the same reason.
- **Tema:** wants more sun on her on the menu page. Passed to main.

## 2026-11-10 18:35 · VIS-EX-0244 Shipping (2e6efc4) · FAIL: falls through the world in photo mode

- **Crash:** none. The tour ran to `done` (151 s), and there's no new `.ips`. A plain launch without
  `-visexqa` reaches the menu and stays up.
- **The bug:** in every photo step she's in movement mode FALLING at x/y 9420/-1198, and z drops
  steadily from −7685 at motion 0 to −105383 by "photo after GC". The camera follows her down.
  The Development build at the same spot is on ground at z 146.
  - On screen: she hangs in a fall pose against plain fog, and every motion looks identical.
  - Already present in Tema's installed Shipping 0240 (`QA_0240ship_1826/`), so it's Shipping-only, not new.
- **Before photo mode:** play, walk, the menus and all outfits are fine. She's on ground, and the
  meshes and skirt cloth are present.
- **Footprints (39d5b8e):**
  - the sand mounds under standing feet read as smooth pale pads, like clogs: lighter than the sand,
    hard-rimmed and without ripples (`02_play`, `03_walked`);
  - there's no footprint trail in the foreground of the walked shot.
- **Earlier today:** Shipping 0242 (be7f068) showed the same photo-mode fall, with no crash.

## 2026-11-10 18:12 · VIS-EX-0240 Development (HEAD 7abac20 + uncommitted GC fix) · PASS

- **Purpose:** the fix for Tema's Shipping 0240 crash (SIGSEGV 0x38 in
  `UAnimMontage::CreateSlotAnimationAsDynamicMontage` ← `AVisexPhotoMode::SetMotion`, 18:05). The fix
  holds the walk/run clips in a `UPROPERTY LoadedLoops`.
- **Crash:** none. The tour reached `done` and exited 0, and there's no new `.ips`.
- **Forced GC:** `forced GC` → walk → run → `photo after GC` all passed: on ground, montage playing.
  Caveat: the old build never ran this step, so it's evidence, not proof, that the step reproduces the bug.
- **Body:** on ground at every step. Photo motions 3–7 report movement "other", same as the 17:53 run.
  Shell `SKM_AndroidSkin`, Hair `SKM_HairCards`, Outfit and `CA_Skirt` cloth are present.
  All 3 outfits (None / Bodystocking / BodystockingSkirt) and all 8 photo motions ran.
- **Shots:** they match the 17:53 run, with no visual regression. Known cosmetic issues:
  - the menu highlight stays on "Settings" after the Settings page closes;
  - at the menu camera she's a near-silhouette, so the outfit shots can barely tell the outfits apart.
- **Perf:** not comparable. Blender was rendering at ~1700% CPU (load avg 55) during the run. The
  avg frame was +5–6 ms per Epic+Ultra category, and +13 ms at quality 4 (47.6 vs 34.8). Quality 0/1
  and play were unchanged (17.3 ms). Rerun idle before reading anything into it.
- **Hitches:** the 400 ms worst-frame hitches from Settings onward also appear in the 17:53 run.
  That part of the tour runs at quality 4.
