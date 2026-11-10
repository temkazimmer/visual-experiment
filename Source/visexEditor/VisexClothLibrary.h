#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "VisexClothLibrary.generated.h"

class UPhysicsAsset;
class USkeletalMesh;

/** Editor helpers for the cloth assets, called from Tools/char_skirt_cloth.py. */
UCLASS()
class UVisexClothLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Fills PhysicsAsset with one capsule per bone in Bones, fitted to Mesh's reference pose: each
	 * runs from its bone to the paired bone in Ends (same index; a bone of the same name means a
	 * sphere) and its radius is the Percentile distance of the vertices that bone mostly drives.
	 * Replaces any bodies already there. Returns a line per capsule.
	 */
	UFUNCTION(BlueprintCallable, Category = "Visex|Cloth")
	static FString FitCapsules(UPhysicsAsset* PhysicsAsset, USkeletalMesh* Mesh, const TArray<FName>& Bones,
		const TArray<FName>& Ends, float Percentile = 0.8f, float RadiusScale = 1.f);
};
