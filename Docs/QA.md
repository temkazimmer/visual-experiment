# QA log

Each packaged build gets the automated tour (`-visexqa`, `Source/visex/VisexQATour.cpp`), run from
the read-only mounted DMG. Shots go to `~/Library/Application Support/Epic/visex/Saved/QA/`, and the
log goes to `~/Library/Logs/visex/visex.log`. Frame times are ms at 1920×1080 windowed on the M5 Max.
They're only comparable when the machine is idle.

Shipping writes no UE log. Since 2e6efc4 the tour also writes `Saved/QA/report.txt`, which works in every config.

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
