# Grounded feet: the standard UE5 pipeline

Research note (2026-11-13) for the floating feet, uneven ground and foot sliding Tema
flagged on the menu walks. It's a solved problem; this is how Epic's samples (Lyra, Game
Animation Sample) and the ALS community project do it.

## AnimGraph order

1. Locomotion source → slots → Local To Component.
2. **Orientation Warping**, then **Stride Warping** (both write the `ik_foot_*` bones).
3. Motion Matching only: **Offset Root Bone** (absorbs capsule vs animation mismatch in turns).
4. **Foot Placement** (production-ready in 5.7): ground traces, planting (foot lock) and the
   pelvis offset; writes `ik_foot`. Plant: Speed Threshold, Unalignment Speed Threshold,
   Distance To Ground, Lock Type. Pelvis: Max Offset, Height Mode = planted feet, interpolation on.
5. **Leg IK** last (turns `ik_foot` into the FK legs) → Component To Local.

Exactly one ground-IK owner: Foot Placement *or* a Control Rig foot-IK node, never both. A
later node re-solves from `ik_foot` and overwrites anything earlier (our hand-rolled C++
ground pass lost to the ABP's own IK for this reason).

## Constant float (3–6 cm)

- **Stale `ik_foot` bones** (most likely): retargeted or pack anims often don't key `ik_foot`
  to match `foot`, and Leg IK pins the feet to them. Snap `ik_foot` to `foot` (Copy Bone before
  the warps, or an anim modifier).
- **Mesh offset:** mesh relative Z vs −capsule half-height with IK off; drop 1–2 cm if needed.
- **Sole height:** the foot bone is the ankle; trace target = ground + normal × foot height.

## Moonwalk and sliding

- Walk: scale the play rate and stride-warp the rest (Lyra keeps both to ≤15–20% total).
- Starts and stops: Distance Matching. Turn-in-place: Root Yaw Offset with turn anims, or Offset Root Bone.
- Planted feet: Foot Placement planting, or ALS foot lock (store the foot in component space
  when the `FootLock` curve hits 1, counter-move it by capsule velocity × dt and yaw, blend out only).
- A slow, weighty walk can be root-motion driven, which removes the moonwalk at the cost of turn response.

## Pitfalls

- Leg IK after every warp.
- Trace channel: decorative meshes are NoCollision, so Visibility traces miss them. Use a
  dedicated trace channel and give steps, sand and the drum simple collision on it.
- Interpolation: feet 15–30, pelvis 10–15 (ALS); too slow lags on steps, too fast jitters.

## Sources

- https://dev.epicgames.com/documentation/en-us/unreal-engine/pose-warping-in-unreal-engine
- https://dev.epicgames.com/documentation/en-us/unreal-engine/game-animation-sample-project-in-unreal-engine
- https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-in-lyra-sample-game-in-unreal-engine
- https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/AnimationWarpingRuntime/FFootPlacementPelvisSettings
- https://github.com/dyanikoglu/ALS-Community (foot lock in ALSCharacterAnimInstance.cpp)

## Findings in this project (2026-11-13, intro film + feet agent)

- The ground-IK owner today is `ABP_Unarmed`'s AnimNode_ControlRig (`CR_Mannequin_FootIK`: Visibility traces,
  ik_foot/pelvis offsets, PBIK). Setting its Alpha once doesn't stick (a pin binding or the pose wheel's
  per-frame UpdateFootIK restores it); the film holds Alpha 0 every frame. Anim graph nodes can't be added
  from Python, so Foot Placement needs a hand edit of ABP_Unarmed.
- It plants on whatever Visibility hits: the dune collision. NoCollision visual surfaces above it (hero sand,
  decals, mounds) read as floating or sunk feet. Hence the dedicated trace channel.
- Base float: the mesh sits ~1.2–1.5 cm high in Play (mesh relative Z −84.07 vs capsule half-height 83.33,
  plus a ~2 cm floor gap); VisexBodyComponent offsets the mesh by LegDelta × 0.5 on body reshapes.
- `MF_Unarmed_Walk_Fwd` has stale ik_foot (1.5–2.5 cm off the foot); `A_PhotoSeq_Walk` is clean (0.2 cm).
- Bounce: the game walk is a 1.9 m/s stride played at 0.59×, which gives a 9 cm hip bob, 85 cm steps at
  80/min and 13 cm foot lift. A genuinely slow walk clip with root motion (`A_PhotoSeq_Walk`: 5.6 cm bob,
  planted feet within 1–1.5 cm) reads calmer and doesn't skate. Prefer it for slow menu walks over
  slowing a fast clip down.
