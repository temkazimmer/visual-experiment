# QA log

Each packaged build gets the automated tour (`-visexqa`, `Source/visex/VisexQATour.cpp`), run from
the read-only mounted DMG. Shots go to `~/Library/Application Support/Epic/visex/Saved/QA/`, and the
log goes to `~/Library/Logs/visex/visex.log`. Frame times are ms at 1920×1080 windowed on the M5 Max.
They're only comparable when the machine is idle.

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
