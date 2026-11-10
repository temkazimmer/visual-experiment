#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VisexAnimInstanceLibrary.generated.h"

class UBlendSpace;

/** Editor helpers for the animation setup scripts (Tools/calm_walk.py). */
UCLASS()
class VISEX_API UVisexAnimInstanceLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Rebuild a blend space's runtime triangulation from its samples. Setting sample_data from
	 *  Python moves the samples but leaves the cached triangles where they were, so the blend
	 *  at runtime still uses the old positions. Marks the asset dirty; save it after. */
	UFUNCTION(BlueprintCallable, Category = "Visex|Animation")
	static bool ResampleBlendSpace(UBlendSpace* BlendSpace);
};
