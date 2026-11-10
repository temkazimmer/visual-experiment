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
