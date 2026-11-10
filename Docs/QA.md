# QA log

Each packaged build gets the automated tour (`-visexqa`, `Source/visex/VisexQATour.cpp`), run from
the read-only mounted DMG. Shots go to `~/Library/Application Support/Epic/visex/Saved/QA/`, and the
log goes to `~/Library/Logs/visex/visex.log`. Frame times are ms at 1920×1080 windowed on the M5 Max.
They're only comparable when the machine is idle.

Shipping writes no UE log. Since 2e6efc4 the tour also writes `Saved/QA/report.txt`, which works in every config.

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
