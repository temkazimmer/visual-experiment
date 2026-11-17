# Idle jitter on uneven ground

Tema, 2026-11-16: "when model stays on uneven surface and start animation of idle staing (when her
weight goes from one leg to another) her legs start slightly glitching." Branch `idle-jitter`,
cut from `main` at 7444479 (after the foot pin and its fix).

## How to reproduce and measure

`-visexjitter` is a mode of the QA tour (`Source/visex/VisexQATour.cpp`, `VisexJitterMeter.*`).
After Start it stands her in the idle on six spots of `Lvl_Dust`, 40 s each (one idle loop, 8 to
10 weight shifts), and reads her legs frame by frame from the anim proxy (`FVisexFootDebug`: every
track comes from the one evaluation, so they line up).

```
UnrealEditor visex.uproject -game -windowed -ResX=1920 -ResY=1080 -visexqa -visexjitter
    [-visexjitterhitch]                        an uneven frame time (every 7th frame +30 ms, every 3rd +8 ms)
    [-visexjitterspots=name:x:y:z:yaw;...]     other spots (z is a hint; her capsule is swept onto the ground)
    [-visexjittersecs=40]
    [-visexjitterfilm=<spot> -visexjitterfilmfrom=<s> -visexjitterfilmsecs=<s>]   4K frames of her legs, Saved/QA/jitterfilm
    [-visexjitterscan]                         lists the ground around her instead (Saved/QA/jitter_scan.csv)
    [-visexcvars=name=value,name=value]
```

It prints a table to `Saved/QA/report.txt` and the log, writes every frame to
`Saved/QA/jitter_<spot>.csv`, and exits 3 when a spot is over the limit (0 otherwise).
`python3 Docs/Research/idle_jitter/analyse.py Saved/QA/jitter_<spot>.csv --knees` says at which
step of the frame the jitter enters.

The spots (defaults): `start` where the Start walk-off leaves her (a 3 degree slope); `flat` a
step tread; `dune13x` and `dune25x` dune slopes of 13 and 25 degrees across her stance (one foot
higher); `dune25up` the same slope facing up it; `stepedge` astride a 20 cm step edge.

**Jitter as a number.** Per track (each foot, ball, knee, the pelvis; world space): how far each
frame's point sits off the straight line between the frames before and after it, time-weighted so
an uneven frame time is not read as jitter. A smooth move leaves hundredths of a millimetre; a
pop of P mm in one frame reads P/2. The table gives the worst frame and the RMS in mm, how often
the motion reverses per second, the plant/pin state flips, and how much a thigh or calf changed
length (bones don't).

## Before (main, 7444479): it reproduces everywhere, worst on slopes

Worst frame off the line, mm, at the real frame rate (92 to 103 fps) / with the uneven frame time (84 fps):

| spot | knee | foot | pelvis | bone length change | plant/pin flips |
|---|---|---|---|---|---|
| start (3 deg) | 70.6 / 108.3 | 0.70 / 2.04 | 0.24 / 0.84 | | 8 / 8 |
| flat tread | 54.8 / 75.6 | 0.72 / 1.11 | 0.28 / 0.95 | | 2 / 2 |
| dune 13 deg across | 101.2 / 133.5 | 1.62 / 1.71 | 0.33 / 0.87 | | 24 / 24 |
| dune 25 deg across | 137.9 / 184.0 | 3.46 / 5.60 | 0.47 / 2.15 | 26.7 % (fixed 60 Hz run) | 16 / 18 |
| dune 25 deg, facing up | 121.1 / 163.7 | 2.91 / 3.26 | 0.25 / 0.87 | | 4 / 4 |
| step edge | 144.4 / 230.6 | 1.68 / 2.29 | 0.24 / 1.09 | | 12 / 18 |

The feet, hips and pelvis are steady (under a millimetre on the flat). The knee is not: it jumps
5 to 14 cm in a single frame, several times per idle loop, and on a slope the uphill leg's thigh
and calf shorten by up to a quarter for seconds at a time. `before.mp4` is 20 s of the 25 degree
cross slope (seconds 11 to 31 of the spot, fixed 60 Hz, 4K, a camera fixed in the world): her
right knee snaps at 2.0, 6.5, 7.2, 7.4, 9.2 and 10.1 s into the clip. (Re-shot on the fixed build
with the fix switched off, so it shares its camera with `after.mp4`; that run read the same
135.84 mm and 26.7 % as main did.)

## The cause

**The idle locks the standing knee dead straight, and every two-bone leg solve after it took its
bend direction from the incoming knee, which a locked knee does not have.**

Evidence, from the per-frame log:

- With the graph's foot IK off (`visex.Feet.Off 1`), the clip's own knee sits exactly on the
  hip-ankle line (0.000 cm off it) for 13 to 30 % of the idle loop, in stretches of up to 5 s,
  always the leg that carries her. Its motion is smooth (worst frame 1.5 mm): the clip is fine.
- The knee is already wrong as the graph hands the pose over, before any C++ layer
  (`analyse.py --knees`: on the 25 degree slope the graph's knee jumps 135 mm; catwalk, alive,
  toes, reshape, contact, limits and the steady head add nothing). In those same stretches the
  graph's thigh shrinks from 42.8 to 33.5 cm and the calf from 39.0 to 28.5 cm, with the knee on
  the hip-ankle line. That is `FAnimNode_LegIK`'s two-bone solve with a zero bend direction: the
  knee goes on the line, at the law-of-cosines distance. The frame the clip unlocks the knee, the
  solve gets a direction and the knee snaps out to where a bent leg has it. The node had
  `HingeRotationAxis = None`, the setting that gives it a direction to fall back on.
- On the flat the leg hardly bends, so the graph's snap is 1 to 1.5 cm. The pin and the plant
  pass then reach again (the plant drops the pelvis 2.5 to 3.5 cm on a step tread, so the leg
  must bend) with the bend plane of that noisy knee: 15 mm in the graph became 25 after the pin
  and 54 after the plant. Their `Reach` also could not bend a straight leg at all.

So it shows on uneven ground because there the uphill leg has to bend a lot, and a missing or
flipped bend direction then moves the knee by 13 to 15 cm instead of 1.

A second, smaller cause, for the feet: the plant pass traced the ground under the lowest sole
vertex. As the foot rocks, that vertex hops between heel and toe (7 to 9 cm apart); on a slope the
ground height it read jumped by 1 to 2 cm, the plant fix chased it (feet 2.5 to 3.5 mm in a frame)
and the pin's heel/ball "planted" state flipped with it (up to 24 flips per loop).

Ruled out:

- **The pin flipping at its speed or bleed thresholds.** `pinw` never flipped on any spot. The
  heel/ball flips that did happen came with the ground height jumps above, not with speed
  (heel and ball speeds stayed under 12 cm/s against the 20 cm/s threshold).
- **The pelvis hunting between the feet.** The pelvis and the plant's pelvis drop are the
  steadiest tracks there are: worst frame 0.24 to 0.47 mm on every spot.
- **The pin and Foot Placement correcting each other.** The graph's foot, the foot after the pin
  and the final foot stay within a millimetre of smooth wherever the knee is not popping; the foot
  jumps that remained all trace to the ground height above.
- **Full extension by itself.** The leg is near full extension (0.999) on every spot, flat ones
  included, and that does make the solve touchy (0.1 mm of ankle distance is 5 mm of knee), but
  the pops are the missing bend direction: they happen exactly at the clip's lock and unlock.

## The fix

Foot Placement stays the one owner of where the feet go; nothing new solves for the ground. The
fix gives each existing leg solve a bend direction it can rely on, and the plant pass a ground
reading that does not hop. `visex.Feet.Steady 0` with `visex.Feet.Hinge 0` turns all of it off,
for comparing.

1. **Leg IK gets a hinge axis** (`visex.Feet.Hinge`, thigh Z; set from code next to the Foot
   Placement tuning in `VisexPoseWheel.cpp`, so `ABP_Unarmed` is unchanged). With it the node
   remembers the last bend direction it saw and carries it with the thigh while the knee is
   locked: no knee on the line, no snap at the unlock. Z, not Y: Y left the thigh up to 15 % short.
2. **`FShaper::ReachLeg`** replaces `Reach` for the pin and the plant pass. The bend plane comes
   from the thigh's own frame (the calf's reference Z, the axis the knee flexes about on this
   skeleton) while the knee is straighter than about 4 degrees, from the incoming knee past about
   12, blended between. A straight leg can bend, and a nearly straight one no longer turns
   graph noise into centimetres.
3. **`LegLengths`**, first thing after the graph: Leg IK's remembered direction is not square to
   the leg, so the thigh came out up to 3.7 % off (and collapsed while nothing was remembered
   yet). The reference lengths go back and the leg reaches for the same ankle.
4. **The plant's ground is a plane**: one trace under the middle of the foot (heel to ball) gives
   a point and a slope. The sole point is as low as the lowest vertex and sits where the lowest
   vertices are (weights falling off over 1.5 mm), so it glides from heel to toe as the foot rocks.
   The pin reads each heel and ball over the plane under it. On flat ground this is what it did
   before. Where nothing is under the middle of the foot (a foot raised past a ledge's edge, feet
   in the air) it falls back to the old trace under the sole's lowest point.

Every new member is initialised, `ReachLeg` falls back to `Reach` on a missing axis or a NaN, and
`LegLengths` skips a leg with a NaN or a missing bone.


## After (c1d03b5): PASS on all six spots, both frame-time modes

Worst frame off the line, mm, before -> after. Real frame rate: before 92 to 103 fps, after 68 to
71 (the same Mac, a day apart; a smooth move reads more at the lower rate, so the small numbers
are not like for like). Uneven frame time: before 84 fps, after 50 to 70.

| spot | knee, real | knee, uneven | foot, real | foot, uneven | pelvis, real | bone length change | state flips | sole vertex gap L / R, cm |
|---|---|---|---|---|---|---|---|---|
| start (3 deg) | 70.6 -> 2.6 | 108.3 -> 3.3 | 0.70 -> 0.88 | 2.04 -> 1.23 | 0.24 -> 0.35 | 0.0 % | 8 -> 4 | 0.29 / 0.30 |
| flat tread | 54.8 -> 1.9 | 75.6 -> 4.4 | 0.72 -> 1.03 | 1.11 -> 0.91 | 0.28 -> 0.33 | 0.0 % | 2 -> 4 | 0.19 / 0.19 |
| dune 13 deg across | 101.2 -> 1.0 | 133.5 -> 2.8 | 1.62 -> 0.81 | 1.71 -> 1.85 | 0.33 -> 0.36 | 0.0 % | 24 -> 4 | 0.25 / 0.49 |
| dune 25 deg across | 137.9 -> 2.2 | 184.0 -> 4.7 | 3.46 -> 1.36 | 5.60 -> 1.84 | 0.47 -> 0.35 | 26.7 -> 0.0 % | 16 -> 6 | 0.50 / 0.02 |
| dune 25 deg, facing up | 121.1 -> 1.6 | 163.7 -> 2.1 | 2.91 -> 0.89 | 3.26 -> 1.57 | 0.25 -> 0.40 | 0.0 % | 4 -> 6 | 0.24 / 0.31 |
| step edge | 144.4 -> 1.0 | 230.6 -> 3.5 | 1.68 -> 1.02 | 2.29 -> 1.27 | 0.24 -> 0.38 | 0.0 % | 12 -> 6 | 0.19 / 0.21 |

The slopes now read what the flat spot reads. The tour's limit is 5 mm (8 with the uneven frame
time), from the flat spot: sound flat runs read 1.3 to 3.8 mm (4.4 uneven), the bug 55 (76).

**Sole gap** (the feet meter's own measure: the lowest skinned sole vertex over the ground under
it, mean over the 40 s): 0.02 to 0.50 cm. It was 0.19 on every spot before, because the plant
pass rested exactly that vertex; on a slope the sole point now sits a little off it.

**Foot drift** over the 40 s loop, ball of the foot: 0.8 to 1.9 cm across the spots, flat included.
On the flat it is the same as before (L 1.03 -> 1.10, R 1.45 -> 1.46). Across the two dune
slopes it was 0.6 to 0.9 before and is 1.4 to 1.9 now, the flat's level; the step edge 0.9 to
1.0 before, 1.1 to 1.6 now. That is over the 1 cm rule, on main
too: it is one small re-settle per weight shift (all of it happens inside a second) that the pin
lets through, not a slide, and not something this change set out to touch. Facing up the 25
degree slope the heel reads 6.9 cm (6.8 before) because the heel lifts; the ball 0.8 to 0.9.

`after.mp4`: the same spot, window and camera as `before.mp4` (knee 1.41 mm in that run).
`before_after.mp4`: both, before on the left.

## Regression runs

Both tours were run twice on one binary, with the fix off (`-dpcvars=visex.Feet.Steady=0,
visex.Feet.Hinge=0`) and on. Development, `-game`, on this Mac. Not Shipping.

**Pin tour** (`-visexpin -visexpinmodes=gate -benchmark -fps=60`: lean, the Settings sit and
stand-up, the Body page walk, turns and sliders, photo walk, run and five poses, Start, then
walk, jog, run, stop, reversals, zigzag, a spin and two jumps). The two runs take the same steps
at the same times. Per phase, off / on: the pin's largest hold and one-frame change are equal
or within 0.2 cm everywhere except the run (hold 1.09 / 2.06 cm, drift median 1.66 / 0.74) and
the lean (hold 1.14 / 0.45); frames clamped at the 6 cm limit are equal (stand-up 29 / 29) or
fewer (Crown 1 / 0). Drift medians: menu and third-person walks 0.5 to 2.1 / 0.5 to 2.0 cm.
Two things to know:

- The tour found a real regression, now fixed (c1d03b5): the Ledge pose's raised foot was held
  by the pin (6.00 cm, 26 frames clamped, against 3.93 and 0) because nothing was under the
  middle of that foot. With the fallback: 3.93 / 3.93 and 0 / 0; FeetUp 0.27 / 0.21.
- Three stances read longer with the fix on (Body walk-out R, stand-up R, the run-jump): the
  feet meter splits a stance when a foot sweeps over 40 cm/s for a frame, and with the fix it
  no longer does in those places, so two stances count as one and their drift adds up (Body
  walk-out: 2.54 and 1.23 cm became one of 5.07). The foot is not sliding further.
- Standing still on the Body page the sole vertex gap went from 0.08 / 0.19 to 0.29 / 0.33 cm.

**QA tour** (`-visexqa`, all 57 report steps): every step "on ground" in both runs; her position
equal or within 1 cm at every step (lean 9500 -900 152, play 9387 -1321, walked 9236 -1884);
menu feet in frame at 16:9 and 4:3, within 0.01 of the frame; body page, wardrobe, photo "moved"
and layout save lines identical. These are 0627's numbers (`Docs/QA.md`: 9500 -900 152, 9387
-1322, 9236 -1885, feet at y 0.76 to 0.78). Twelve shot pairs were compared by eye (play, walked,
the Settings sit, Body tab, photo Idle, Crown, FeetUp, Ledge, LegLift, Mermaid, Peace, the
turnaround): the same poses; only dust and light differ.

## Not verified

- **Shipping.** Nothing was cooked or packaged. Every new member has an initialiser (the debug
  frame, the leg pole, the ground point, slope and sole offset, the switch), `ReachLeg` falls
  back to `Reach` on a missing axis or a NaN, `LegLengths` skips a leg with a NaN or a missing
  bone and does nothing until the pole is set on the first frame, and the ground trace skips a
  NaN. That is by reading the code, not by running a Shipping build.
- The walk on the steps and dunes was only covered as far as the two tours go (the Start
  walk-off down the dune, the third-person drive on the sand). No walk across the step edges.
- The pin tour was run once per setting, so differences of a few millimetres between the two
  runs are not separated from run-to-run variation.
- The pose kit's Python predictor of the game-side layers (Tools, c031ab8) does not know
  `ReachLeg` or the ground plane; baked poses were not re-baked or re-checked beyond the shots.
- Outfits, body reshapes and other body meshes: only the default body and outfit were run.

## Still wrong

- A knee blip of 2 to 4 mm a few times per loop, flat ground included. It is Leg IK switching
  between the knee's own bend direction and the remembered one as the clip's leg crosses 8
  degrees off the hip-ankle line (`KneeBendDot < 0.99` in `SolveTwoBoneIK`); the two differ by a
  few degrees. If it shows: ease the knee's swivel about the hip-ankle line in `LegLengths`
  (a short spring on the angle between the incoming plane and the hinge plane).
- The idle clip itself locks the knee at exactly 0 (`MM_Idle`, baked by
  `Tools/char_idle_proud.py`). Other baked clips may do the same; the fix does not depend on it.
- Astride the step edge the trace under the middle of the foot can flip between the two treads
  if the foot creeps across the edge. It did not in these runs.
- Idle foot drift is over the 1 cm rule on every spot, as above.
