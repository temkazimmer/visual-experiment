# Animation guidelines for vis-ex

The short version, for any agent making animation. Detail lives in [ANIMATION.md](ANIMATION.md), [RIG.md](RIG.md) and [ANATOMY.md](ANATOMY.md).

**Aim (Tema):** raise animation quality by *learning* from professional work and building that knowledge into our own model. Not by pasting assets in. Proper anatomy. A calm, playful, proud protagonist.

## The character in one breath

Upright, unhurried, head level. Calm base with small playful accents. Weighty and deliberate, never floaty or arcade. Android: clean stops, head-first turns, a tiny settle.

## Ten rules

1. **Principle before clip.** Write the rule you are applying ("weight dips on contact", "head settles last") before you touch keys.
2. **Measure, don't guess.** Pull numbers from reference clips with Python/Sequence Editor: timing, pelvis height, foot plant frames, rotation ranges.
3. **Silhouette first.** Check every key pose as a black shape from three angles.
4. **Weight over the support foot** in every still. Stills are the product.
5. **Contact has weight.** Dip on landing, settle on stops, counter-rotation on turns.
6. **Overlap.** Pelvis leads; spine, chest, head, hands and hair arrive later. Keep it small and clean.
7. **Calm base, playful accent.** One small surprise per movement, starting late and ending clean.
8. **Proper anatomy.** Respect joint ranges, check armpit, crotch, knee-back and waist in deep poses; add a morph rather than bending a bone past what the skin handles.
9. **Content line:** pin-up and fashion yes; sexual positions no.
10. **4K captures to review**, at most two editors, kill what you start (project rules).

## Workflow: learn → write down → implement → compare

| Step | What | Output |
|---|---|---|
| 1. Question | One thing you want to learn ("how does a good stop settle?") | A sentence |
| 2. Study | Look at 2 to 3 reference clips (see [BUY_LIST.md](BUY_LIST.md), GASP, your own earlier work). Extract numbers. | A short table in your notes |
| 3. Rule | State it in our terms: frames, degrees, cm | One line, e.g. "stop: hips settle 0.25 s, head 4 frames after feet" |
| 4. Implement | Our own clip, an additive layer, a Control Rig, a `visex.Catwalk.*` change, or a blend-space edit | Committed change |
| 5. Compare | Source vs ours, same camera, 4K | Capture pair |
| 6. Log | Add a line to the log below | Updated file |

**Do not ship a bought clip unchanged as a signature move.** Use it as study material, or as filler where nobody will notice.

## Where each kind of work goes

| Work | Where | Notes |
|---|---|---|
| New walk/run/turn rule | Blend space, `calm_walk.py`, `visex.Catwalk.*` | Re-measure the real speed. Catwalk layer stacks on sway already in the clip. |
| Idle and micro-animations | Random variations in the AnimBP and additive layers | Breath, head tracking, weight shifts, calibration tics |
| Poses (photo mode) | `Tools/char_pose_*.py` building `A_Pose_*` assets, plus `_In` and `_Out` | Joint ranges, foot IK, floor slope |
| Soft tissue | `UVisexBodyComponent` springs | Keep small for the calm read |
| Shape correction | Blender morphs (`blender_correctives.py`) | Not bone tweaks |
| Clothes follow | Outfit uses all weights and shape keys | New morph on skin also goes on outfit |

## Review checklist (before telling Tema it is done)

- [ ] Key poses read as silhouettes from front, 3/4 and low.
- [ ] Feet planted at the intended speed (no skate).
- [ ] Pelvis dip on contact, settle on stop, head last.
- [ ] Proud posture (chin level, chest open), calm tempo, one accent.
- [ ] Armpit, crotch, knee-back and waist are clean at the extremes.
- [ ] Soft tissue calm.
- [ ] Catwalk layer not doubled (check the CVars).
- [ ] 4K captures attached; source vs ours when relevant.
- [ ] Rule written down in the log.

## Learning log (append as you go)

| Date | Pack / source | Question | Finding (numbers) | Where we used it |
|---|---|---|---|---|
| 2026-11-10 | Girl Walk&Run (Strut 1–4, Walk_Cool) | What does a strut add beyond our catwalk layer? | Struts: spine 4° back (ours was 6° forward); head sway 0.5–1 cm (ours 7.4); arm swing 10–41 cm (ours 64); chest yaw 26–34° with the chest 4–5 frames behind the hips (ours 17°, no lag); pelvis bob 4–6 cm (ours 9). | `visex.Catwalk.*`: Posture 11, Shoulders 1.7, ChestLag 7, ArmSwing 0.6, HeadLine 0.9. Live result: lean −3°, head sway 2.5 cm, arms 41 cm, chest 15°. |
| 2026-11-10 | same | Rule limits | The chest counter can't go past what the hips give it, and the layer runs after foot IK, so lowering the pelvis would sink the feet. | Bigger chest counter and less bob have to come from the base clip (our own strut, later), not the layer. |
| 2026-11-10 | Gamma Walk_F_End | How does she stop? | The hips decelerate over ~0.3 s and never pass the stop, then creep the last 2–3 cm over 1.5 s. They sit ~3 cm low at the last contact and rise within 0.4 s. The chest brakes back (head 1.6 cm behind the hips), and the head comes forward last, ~0.6 s after the hips settle. | `A_Menu_Lean_Up`: the walk's clock now brakes instead of overrunning, which removes a 10 cm overshoot and slide-back. A 0.85 s settle adds dip 3 cm, creep 2 cm and a 3° chest brake (`char_menu_lean.py` DIP/CREEP/BRAKE). |
| 2026-11-10 | Gamma Walk_F_Start | How does she start? | For ~1 s before the first lift, weight shifts 4 cm onto the stance leg, the hips drop 3 cm and the head leads 3 cm forward. Speed reaches 90 cm/s ~0.5 s after the lift. | Next: the first-step weight shift in pose `_In` clips. |
| 2026-11-10 | Gamma TurnIP 90/180 | Who leads a turn? | The head starts ~4 frames before the chest and hips and is halfway through 4–14 frames earlier. Chest and hips turn together; the feet follow last, ~17 frames behind the hips. | Rule for later turns: head first, then torso as one, then the feet. |
| 2026-11-10 | Female Everyday idles (Neutral A/B, BreathDeep, WeightShift) | How little does a good idle move? | Neutral_B: one breath per 4 s, chest ~3° and shoulders ~2 cm peak to peak, upper body drifting 3° side to side and 4° in twist. Neutral_A is stiller (~1°, head ~3°). Weight shifts move the pelvis 30–40 cm: those are events, not idle. | New moving-hold layer (`visex.Hold.*`) on every held pose: breath 2.4°/4 s, collarbones 4°, drift 1.2°, head 1.5°; measured live: shoulders 1.1–1.3 cm, chest 1.9°, drift 2.2°, head 2.6°. The pelvis is untouched, and hands below 30 cm stay pinned (the floor hand in Mermaid drifted 5.4 cm before the pin, 0.5 cm with it). |
| 2026-11-10 | Park Life Bench_Sit_Stand_Sit 01/02 | How does she get up and sit down? | Up: the trunk tips forward 0.8 s before the hips leave the seat (−20° to 60° on a bench), the head comes over the feet, the lean peaks 0.35 s after seat-off, and standing takes 1.3–1.6 s more. Down: 1.4–1.5 s, the lean peaks mid-way, and the last 20% runs at half speed (a controlled landing). | `A_Menu_Lean_Up`: a trunk-first PREP key (0.35 s, hips still on the stone) before the push, with the push leaning further (9°). The clip is now 3.1 s, still inside the 3.5 s Start camera move. Lean_Down already lands slowly. |
| 2026-11-10 | Park Life Stand_Lean_Relax 01–04 | How alive is a relaxed lean? | Hips 2–5 cm below standing, feet 5–19 cm ahead; the trunk leans back 10–21° and moves 6–13° over ~30 s; the hips drift 5–12 cm. One take crosses the ankles 30% of the time. | `A_Menu_Lean` goes from a 4 s loop to 12 s: 3 breaths, one slow sway (trunk 4°, hips 4.6 cm), and the right knee easing in and out. That's a calm third of Park Life, because this is a menu backdrop. |
| 2026-11-10 | Lie Down (5 clips; numbers only, nothing ships) | How does weight sit on the floor? | Side-lying: the whole underside forearm, thigh, calf and foot rest on the floor, pelvis 15 cm and chest 24 cm up, head propped at 34 cm. Dozing on the back: the chest breathes 3.7 cm, deeper than upright (1–2 cm). | LegLift matches the side-lying contacts, but its pelvis is 4 cm lower (11 cm): check whether the hip sinks. The moving hold breathes 1.6× deeper when the trunk is horizontal (`visex.Hold.LyingBreath`). |
| 2026-11-10 | Her own skin vs the drum and the sand (CPU skin probe) | Do hands and hips actually touch? | Bones sit cm inside her, so judged on skinned vertices: menu-lean palms were 1–1.8 cm inside the drum (the lean was built for a smaller column). Floor poses: FeetUp chest and face 27 cm in the sand, Glance/LegLift/Mermaid 6–15 cm in. | Live contact pass (`visex.Contact`): palms step onto the surface under them (two-bone reach, within 6 cm), the lowest skin of a floor pose lands on the ground, and a lying body tips up to 20° so head end and feet end touch alike. All poses now read 0.0 cm. The lean is refit to the real drum (r 50.6, top 94.1). |
| 2026-11-10 | Her live walk at 190 cm/s, measured per foot (scripted stick on the flat plaza) | Why does she limp? | Left steps 105 cm, right 89; the hip line turned 10° toward one side the whole time. Not the clip (symmetric), not stride warping, not IK, not the catwalk layer: BS_Idle_Walk_Run's cached triangulation still had the walk at 300 cm/s, so at 190 it blended a third of the idle in, and the idle's staggered stance twisted her hips. | Rebuild the triangulation whenever samples move (`VisexAnimInstanceLibrary.resample_blend_space` in calm_walk.py). Now 90/100 cm, hip line 1.6°. A slow copy of each walk clip at 70 cm/s (rate 0.5) gives the light stick short, slow steps (63 cm, 0.7 s) instead of idle mixed in. Catwalk defaults raised to strut levels: hip roll 30°, chest counter 27°, arms 27 cm, lean 4° back. Stick answers in 0.1–0.2 s, stops in 0.3 s (acceleration 900, braking 1300). |
