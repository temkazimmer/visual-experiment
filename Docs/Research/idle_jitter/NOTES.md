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
right knee snaps at 2.0, 6.5, 7.2, 7.4, 9.2 and 10.1 s into the clip.
