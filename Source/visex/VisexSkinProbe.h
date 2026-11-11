#pragma once

#include "CoreMinimal.h"

class USkeletalMesh;
class USkeletalMeshComponent;

/**
 * Where her skin actually is. Bones sit a few centimetres inside the body, and how far
 * depends on her shape, so contacts are judged on skinned vertices: a few hundred per region
 * (the palms, buttocks and the backs of the thighs, and the whole body but the hands),
 * skinned on the CPU from the component's current bone transforms (body reshape included,
 * morph targets not). Built once per mesh; cheap enough to run every frame.
 */
struct FVisexSkinProbe
{
	enum ERegion : uint8 { HandL, HandR, Seat, Body, Num };

	/** Read the mesh's LOD 0 and pick the region vertices. False if the CPU copy is missing. */
	bool Build(const USkeletalMeshComponent* Component);
	bool IsBuiltFor(const USkeletalMeshComponent* Component) const;

	/** The region's vertex with the lowest Score (e.g. world Z), in world space. */
	bool Lowest(const USkeletalMeshComponent* Component, ERegion Region, TFunctionRef<double(const FVector&)> Score, FVector& Out) const;

	int32 Count(ERegion Region) const { return Regions[Region].Num(); }

	/** The skin's radius around a limb bone (thigh_*, calf_*, upperarm_*, lowerarm_*, head), from the
	 *  reference mesh, at a point Along01 of the way from the bone to its child (0..1), in the direction
	 *  OutWorld (world, from the bone's axis toward the point asked about). 0 if unknown. */
	float Radius(const USkeletalMeshComponent* Component, FName Bone, float Along01, const FVector& OutWorld) const;

private:
	struct FVert
	{
		FVector3f Ref;
		uint16 Bone[4] = {0, 0, 0, 0};
		float Weight[4] = {0.f, 0.f, 0.f, 0.f};
		uint8 Num = 0;
	};
	TArray<FVert> Regions[Num];
	TWeakObjectPtr<const USkeletalMesh> Mesh;
	TMap<FName, TArray<float>> Radii;  // per bone, RadiusBins samples along it x 8 sectors around it
	TMap<FName, FMatrix> Frames;       // per bone, the (along, U, V) frame the sectors were counted in
	TMap<FName, FQuat> RefRots;        // per bone, its reference rotation in component space
	bool bBuilt = false;
};
