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
