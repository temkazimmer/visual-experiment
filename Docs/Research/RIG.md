# Our rig

Read before touching skeleton, retargeting or the anim layers. Sources: `Docs/CHARACTER_DIRECTION.md`, `Docs/AGENT_CHARACTER.md`, `Docs/CLOTHES.md`, `Source/visex/*`, `Tools/*.py`. Bone names below marked *(verify)* come from my memory of the UE5 mannequin, not from Epic's docs (I found no official bone list). Dump the skeleton in the editor once and trust that over this file.

## What the character is

- **One skeleton:** the UE5 Manny/Quinn skeleton. Quinn is scaled to 159 cm (`ReferenceHeight 177.5` in `VisexBodyComponent.h`).
- **Layers on that skeleton, all following the body mesh by Leader Pose:** the sculpted skin (`SKM_AndroidSkin`), the outfit, and the shell (`UVisexShellComponent`). Anything that drives the body drives all of them.
- **Body shape is done by animation, not by the mesh.** `UVisexAnimInstance` runs the Anim Blueprint, then adds the **catwalk layer**, then **reshapes** by scaling bones from the profile sliders. It is the last step before skinning, so it applies to every clip, montage and photo pose.
- **Soft tissue is morph-driven.** `UVisexBodyComponent` runs damped springs from bone velocity into `Bust/GluteBounce` and `BustSway` morphs.

## Bone hierarchy (UE5 mannequin)

```
root
 ├─ pelvis → spine_01..spine_05 → neck_01, neck_02 → head
 │            spine_05 → clavicle_l/r → upperarm → lowerarm → hand → fingers
 │            pelvis → thigh_l/r → calf → foot → ball
 ├─ ik_foot_root → ik_foot_l, ik_foot_r
 └─ ik_hand_root → ik_hand_gun → ik_hand_l, ik_hand_r
```

- **Spine has five bones** (`spine_01` to `spine_05`), the neck two (`neck_01`, `neck_02`). The catwalk and reshape code use these names, so a retargeted source with three spine bones needs a chain mapping that distributes onto five.
- **Twist bones** *(verify)*: `upperarm_twist_01/02`, `lowerarm_twist_01/02`, `thigh_twist_01/02`, `calf_twist_01/02`. They exist to share axial rotation along a limb. Our `Reshape` deliberately skips twist bones when counter-scaling children (`VisexAnimInstance.cpp`, the `Contains("twist")` check).
- **Corrective bones** *(verify)*: `*_correctiveRoot` and in/out/front/back helpers on shoulder, elbow, hip and knee. Epic drives them with a pose driver in the mannequin's Control Rig / AnimBP. **Our skin does not use them.** It uses shape-key correctives (`ArmUp_l/r`, see `Tools/blender_correctives.py`). So a shoulder or hip problem on our skin needs a morph, not a bone.
- **IK bones:** `ik_foot_root`, `ik_foot_l/r`, `ik_hand_root`, `ik_hand_gun`, `ik_hand_l/r`. They are children of `root`, not of the pelvis. By default they do nothing until something drives them ([forum explanation](https://forums.unrealengine.com/t/explanation-of-the-mannequin-ik-bones/45166)). Our foot IK is a Control Rig node in the Anim Blueprint after the pose slot (`UVisexPoseWheelComponent::UpdateFootIK` drives its alpha).
- **Root motion:** Epic's stock clips keep `root` still and move the pelvis, or carry root motion on `root` in the Game Animation Sample set (e.g. the `MF_Unarmed_*` clips and `MM_Dash`). Rules:
  - Gait loops: in place, speed from the blend space and Character Movement.
  - Dash: root motion (`MM_Dash`).
  - Pose in/out clips: see the Pose wheel notes below.

## Retargeting into our skeleton

Epic's [IK Rig retargeting docs](https://dev.epicgames.com/documentation/unreal-engine/ik-rig-animation-retargeting-in-unreal-engine) cover the tool. The parts that matter for us:

| Source | Plan |
|---|---|
| **Gamma Studio, XM3, lumiflo** (bought, native Manny/Quinn) | No retarget. Import and check the root: some keep motion on `root`, some on `pelvis`. |
| **Park Life / MoCap Online** | Retargeters are supplied, and the listing says the stock UE5 retargeters need their custom ones. Start from theirs. |
| **MoCap Central / Digital Kinetics** | Standard UE5 mannequin or "any humanoid". Test one clip, then batch. |
| **UE4 skeleton packs** (Mobility Pro) | UE4 to UE5 IK Retargeter, with their retarget project. Expect foot slide until stride is trimmed. |
| **Mixamo** | T-pose, 22 or so bones, **no `root`**, hips carry travel. Use a Mixamo IK Rig and map to `IK_Mannequin`, set the **Retarget Pose** on both ends to match, and reuse an A-pose fix for the arms ([forum thread on Mixamo X Bot to Manny](https://forums.unrealengine.com/t/ik-retarget-issues-with-upper-body-mixamo-x-bot-ue5-manny/655488)). Export "In Place" and let Character Movement travel. |

**Chain mapping (Mixamo or generic humanoid to ours):**
- `Hips`→`pelvis`; `Spine`, `Spine1`, `Spine2` → `spine_01..spine_05` (map the whole chain; the retargeter distributes);
- `Neck`, `Head` → `neck_01..neck_02`, `head`;
- `LeftShoulder` → `clavicle_l`, then arm, forearm, hand chains;
- legs map to `thigh_l`, `calf_l`, `foot_l`, `ball_l`.
- Fingers as chains; twist bones are not driven by retargets, so skin twist comes from the mesh weights.
- Set Pelvis as the retarget root and turn on translation scaling for the pelvis, so a 159 cm body with a shorter leg keeps ground contact. Re-check after changing leg-length slider values.

**Retarget quality checks:**
1. Arms at rest do not clip the thighs (the A-pose vs T-pose fix).
2. Feet stay planted in a walk at the intended speed. Otherwise note the clip's real speed (see below).
3. Head level through a cycle.
4. Compare the retargeted result to the source in a split view before using it.

## Pitfalls found in our code and tooling

1. **Catwalk layer stacks on any walk.** `FVisexAnimProxy::Catwalk` adds hip roll/yaw/shift, chest counter-rotation and a level head to *every* walk, tuned by `visex.Catwalk.*` console variables. A bought clip that already has hip sway will be doubled. Lower the CVars while testing a new walk, or exclude that clip.
2. **PhotoGait must match the clip.** Photo mode plays gait clips in place and sets `PhotoGait` by hand (1 for `MF_Unarmed_Walk_Fwd`, 0.35 for `MF_Unarmed_Jog_Fwd`). Each new walk or run clip needs its own value, or the catwalk layer runs at the wrong strength.
3. **Walk speed is tuned in three places:** `Tools/tune_movement.py` (190 cm/s), `Tools/calm_walk.py` (the blend space with `WALK_RATE 0.75`, clip speed 300 cm/s at rate 1) and the Shift-release node. A new walk clip changes the real cm/s per cycle, so re-measure and re-tune all three. Stride Warping trims to about 85%. Trim harder and the knees bend and she looks "sneaky".
4. **Bone scaling happens after all animation,** and the IK bones sit under `root`, not under the scaled limbs. A big leg-length or thigh change can leave the foot IK target and the foot out of step. Test with foot IK on and slider extremes.
5. **Floor poses turn foot IK off** and lay onto the slope (`UpdateGround`). A new floor clip is held on the `DefaultSlot` and ends on its last frame. In and Out clips (`A_Pose_X_In`, `_Out`) and floor-to-floor "flows" are looked up by name. Missing a name means no way in or out.
6. **Lying poses run along the character's own Y axis** (head +Y, legs -Y, belly down; see `Tools/char_pose_prone.py`). A bought lying clip may lie along another axis. Fix on import.
7. **Poses must respect real joint range** (commit `74c753c`: ankles kept inside a real joint's range). The same applies to retargeted clips, which can push mannequin joints into ranges the skin cannot deform.
8. **Python in UE 5.7 cannot add Blueprint graph nodes.** Node changes go through `.t3d` paste (`Tools/*.t3d`), by hand.
9. **Never save during Play.** Many tools raise if PIE is running; follow `CLAUDE.md`.
10. **Soft tissue reads its bone's velocity.** A very bouncy or snappy clip will fire the bust and glute springs. A calm character needs low `BustStrength` and `BustSwayStrength`. Check stops and landings.
11. **Outfit and shell follow by Leader Pose.** A new pose that is not on the same skeleton asset will not reach them.
12. **Menu poses:** the menu director owns the camera and input, so a new idle or lean must work with input disabled.

## Typical new-clip workflow (our tools)

1. Import to `/Game/Characters/...` on the Manny/Quinn skeleton.
2. Check root motion mode, length, loop points, and the real speed.
3. Add to the blend space or the pose folder. Tools: `Tools/char_pose_*.py` build pose assets, `Tools/char_photo_loops.py` builds photo loops.
4. Shoot 4K captures from the bridge (`Tools/capture.py`) at three points in the cycle.
5. Check the catwalk layer, foot IK and tissue springs.

## Useful docs

- [Motion Matching](https://dev.epicgames.com/documentation/unreal-engine/motion-matching-in-unreal-engine) and [UE 5.7 release notes](https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-7-release-notes) (Pose Search in Choosers is experimental).
- [Game Animation Sample 5.7 post](https://www.unrealengine.com/tech-blog/explore-the-updates-to-the-game-animation-sample-project-in-ue-5-7): a foot-placement Control Rig to read.
