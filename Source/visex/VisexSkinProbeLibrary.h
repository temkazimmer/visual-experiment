#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VisexSkinProbeLibrary.generated.h"

class USkeletalMesh;
class USkeletalMeshComponent;

/** Editor helpers for the contact probe. Python can't reach a skeletal mesh's per-LOD
 *  settings in 5.7, so this sets the one the probe needs in packaged builds. */
UCLASS()
class VISEX_API UVisexSkinProbeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Keep a CPU copy of this LOD's vertices in cooked builds (the probe reads them). Marks the
	 *  mesh dirty; save it after. False if the LOD doesn't exist. */
	UFUNCTION(BlueprintCallable, Category = "Visex|Contact")
	static bool SetLODAllowCPUAccess(USkeletalMesh* Mesh, int32 LODIndex, bool bAllow);

	UFUNCTION(BlueprintPure, Category = "Visex|Contact")
	static bool GetLODAllowCPUAccess(const USkeletalMesh* Mesh, int32 LODIndex);

	/** How far a palm is from her body's skin (cm), skinned as drawn now: the closest pair of a palm
	 *  vertex and a body vertex that isn't the arm's own (for checking a hand on the hip). -1 if the
	 *  mesh can't be probed. */
	UFUNCTION(BlueprintCallable, Category = "Visex|Contact")
	static float HandBodyGap(USkeletalMeshComponent* Mesh, bool bLeft);
};
