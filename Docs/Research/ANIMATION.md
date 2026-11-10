# Animation craft

For her specific character: **calm, playful, proud**. Weighty and deliberate (Villeneuve), never floaty or arcade. This file is about *why* movement looks good and how to build that into our own clips, layers and rules.

Core references: Disney's twelve principles as taught in Richard Williams, *The Animator's Survival Kit* (book, not link-verified) and Epic's [Motion Matching docs](https://dev.epicgames.com/documentation/unreal-engine/motion-matching-in-unreal-engine).

## 1. The character in four lines

| Trait | Shows up as |
|---|---|
| **Calm** | Slow-in, slow-out. Few simultaneous moves. Stillness is allowed. |
| **Proud** | Long spine, chest open, chin level, shoulders down and back. She never hunches, never looks down at her feet. |
| **Playful** | A small, unexpected accent on top of the calm base: a head tilt, a tucked lock of hair, a half-spin, a hand flourish. It starts late and ends clean. |
| **Android** | "Slightly too precise": clean stops with a tiny settle, head-first turns, a breathing-like idle. |

Rule of thumb: **calm in the base, playful in the accent**. If a clip is playful everywhere, it reads as cute or nervous.

## 2. Weight

- **Weight is timing and spacing.** Heavy things start slowly, travel with momentum, and stop with a settle. Duration of anticipation, drag and landing carry more than amplitude.
- **Contact tells weight.** The frame a foot lands is where weight is seen: the pelvis dips a few cm, the knee absorbs, the opposite shoulder drops. A light figure with heavy contacts reads as deliberate (`CHARACTER_DIRECTION.md` §1).
- **Centre of mass over the support foot.** In every still frame she should be balanced. A pose that would fall over looks wrong in a photo.
- **Anticipation then action then follow-through.** Before a step, a tiny weight shift to the other leg. After a stop, the head settles last.
- **Her speed:** walk at 190 cm/s, slower than a typical 130 to 150 cm/s...  means each step has to carry more *intent*, because there is time to see every frame.

## 3. Overlap and follow-through

Different parts arrive at different times. This is what makes motion feel alive and what a plain retarget removes.
- **Order:** pelvis leads, spine follows 2 to 4 frames later, chest and shoulders next, head after that, then hair and hands last. In a stop the order reverses into a settle.
- **Drag:** hands and hair trail the body by 3 to 6 frames at 30 fps. Hair is a prime place to show wind direction (the scene's motion).
- **Android twist:** keep the overlap *small and clean*, not floppy. The head leads a turn and the body follows late.
- Our jiggle springs (`UVisexBodyComponent`) are overlap for soft tissue. Keep bust motion at the level of a breath, not a bounce.

## 4. Contrapposto and posture

- **Contrapposto:** weight on one leg, the pelvis tilts down on the free-leg side, and the shoulders tilt the opposite way. The spine makes a gentle S. It is the base of every standing pose and the thing that makes a still look alive. See the weight-shift clips in *Female Everyday Idles*.
- **Proud posture:** ears over shoulders over hips. Sternum lifted, ribs not flared. Shoulders back and *down*, not squared up.
- **Free leg:** knee soft, foot angled out about 20 to 30 degrees, the toe or ball touching, not planted.
- **Hands:** never relaxed fists or flat paddles. Keep a slight curve, with one finger leading.

## 5. Catwalk mechanics

Our `Catwalk` layer already adds hip sway and chest counter-rotation. Understand what it is imitating:
1. **Foot placement near one line.** Feet land close to the midline (not hip-width apart), so the pelvis has to swing.
2. **Hips swing with the step:** pelvis rotates (yaw) and tilts (roll) toward the planted leg. Magnitude is modest: a few degrees.
3. **Shoulders counter the hips.** The chest rotates the opposite way, which keeps the head still.
4. **Head level and forward.** The head stays on a smooth line, and the gaze is fixed. This is the "proud" read.
5. **Arm swing small and loose, hands soft.** Arms follow the counter-rotation rather than pumping.
6. **Heel strike to ball, with a long stride and a slow tempo.** For heels, the knee is straighter and the stride shorter.
7. **Stop with a pose.** Land the last step, hold the free foot behind, settle the hips, a small head turn.

Tuning route: `visex.Catwalk.*` console variables (HipRoll, HipYaw, HipShift, Line, Shoulders, Head).

## 6. Pose design and silhouette

- **Read the silhouette first.** Fill the figure black. Is the pose still clear? Separate the arms from the torso, show the waist, and leave daylight between the legs where it helps.
- **One line of action.** A single sweeping curve from head through spine to the weight-bearing foot.
- **Asymmetry.** Mirror-symmetric poses are static. Offset shoulders, hips, hands and head.
- **Angles.** Avoid *twins* (two limbs doing the same). Vary arm angles by at least 15 to 20 degrees.
- **Hands and feet finish the pose.** Fingers have a rhythm (not all alike). Feet point in a direction.
- **Camera first.** In a photo-mode game the camera is part of the pose. Check each pose from the front, three-quarter and low angle.
- **Content line:** pin-up and fashion yes, sexual positions no. If in doubt, use a pose a fashion shoot or a mannequin display would use.

## 7. Transitions

- **Ease by default:** slow-in and slow-out (Williams' "slow in and slow out"). Linear moves look robotic, which is right only when you want the android read.
- **Arcs.** Hands, head and feet travel on curves. Straight lines read as mechanical.
- **Holds with life.** A held pose should drift a few mm, never freeze (the *moving hold*). Our idle breath and weight shift cover it.
- **Floor transitions:** getting down and up are the hardest and the least available in packs. Break them into: shift weight, lower a hand or knee, the pelvis travels down on an arc, settle, then the free limbs arrange themselves. Getting up reverses it but is faster and uses momentum.
- **Blends:** cross-fade between poses over about 0.3 to 0.5 s (pose wheel `BlendTime 0.4`). Match the foot planting at the blend or the feet slide.
- **Stops:** the weight keeps moving after the feet stop. Head and arms drift forward and settle over about 0.2 to 0.4 s.

## 8. Motion matching vs state machines (UE 5.7)

| | **State machine + blend space** (ours today) | **Motion Matching** |
|---|---|---|
| How it picks | Hand-built states and transitions. | Searches a database for the clip pose that best matches the trajectory and current pose. |
| Strength | Predictable, easy to debug, great for a few signature moves. Cheap. | Natural starts, stops and turns from lots of mocap, and much less hand wiring. |
| Weakness | Each transition is hand work and can look stiff. | Needs a lot of consistent mocap data and is harder to art-direct. In 5.7 the Chooser integration is **experimental**. |
| Fit for us | Idles, poses, signature moves (dash, pose in/out). | Walk, run, start/stop/turn, if we buy or reuse a dense set (Gamma Movement, GASP). |

Recommendation: **hybrid**. Keep the state machine for everything character-defining (poses, idles, jump, dash), and *try* Motion Matching for walk and run only if a spike shows it keeps the calm 190 cm/s feel. Motion Matching optimises for realism, which can wash out stylisation. Tag the database to pick only calm clips, and test with our catwalk layer on top.
- Learn from GASP: [Epic's 5.7 post](https://www.unrealengine.com/tech-blog/explore-the-updates-to-the-game-animation-sample-project-in-ue-5-7) and the Rewind Debugger for seeing why a pose was chosen ([debugging docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/motion-matching-debugging-in-unreal-engine)).

## 9. How to study a bought pack

The goal is **knowledge in our model, not assets in the build.** The workflow:

1. **Pick a question,** not a clip. E.g. "how does she stop?", "what does the pelvis do in the first step?", "how long is a weight shift?"
2. **Extract numbers.** From each study clip, note:
   - **Timing:** total frames, anticipation, action, settle, in seconds at the clip's fps.
   - **Weight curves:** pelvis height over time, and where it dips on contact.
   - **Foot placement:** stride length, step width, which foot leads, plant frames.
   - **Rotation ranges:** pelvis yaw and roll, chest counter-rotation, head offset (degrees).
   - **Overlap offsets:** frames by which spine, chest, head and hands lag the pelvis.
   - Use the Sequence Editor curves and Python (`unreal.AnimationLibrary`) in the editor, so the numbers are measured rather than eyeballed.
3. **Write the rule down** in `Docs/Research/` notes or the branch notes: "in the source, the head settles about 4 frames after the feet; ours settles on 0".
4. **Implement it in ours,** by one of: re-timing our hand-built clips, changing a `visex.Catwalk.*` value, adding an additive layer in the AnimBP or Control Rig, or authoring a new clip from scratch with the extracted numbers.
5. **A/B check at 4K,** source next to ours, same camera.
6. **Record what you learned** (a line per pack in the GUIDELINES log), so the next session starts from it.

**Do not:**
- Ship a bought clip unchanged as a signature move. It is licensed for use, but it will not be distinctive, and other games use it.
- Copy a whole performance frame by frame and call it ours.
- Redistribute the source FBX or pack files (check each licence).
- Mix two performers' styles inside one cycle.

**Use bought clips directly** only for background filler (e.g. generic turn-in-place) where nobody will notice.

## 10. A practical checklist for any new clip

- [ ] Has a clear silhouette in the key poses?
- [ ] Weight is over the support foot in every still?
- [ ] Pelvis dip on contact, settle on stop?
- [ ] Overlap: head and hands lag?
- [ ] An accent that is playful but small?
- [ ] Posture is proud (chin level, chest open)?
- [ ] Loops cleanly, with foot planting matching speed?
- [ ] Tissue springs calm on starts, stops and landings?
- [ ] Reads in 4K stills from three angles?
