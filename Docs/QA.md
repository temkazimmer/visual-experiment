# QA log

Each packaged build gets the automated tour (`-visexqa`, `Source/visex/VisexQATour.cpp`), run from
the read-only mounted DMG. Shots go to `~/Library/Application Support/Epic/visex/Saved/QA/`, and the
log goes to `~/Library/Logs/visex/visex.log`. Frame times are ms at 1920×1080 windowed on the M5 Max.
They're only comparable when the machine is idle.

Shipping writes no UE log. Since 2e6efc4 the tour also writes `Saved/QA/report.txt`, which works in every config.

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
