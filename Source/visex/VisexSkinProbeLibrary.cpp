#include "VisexSkinProbeLibrary.h"

#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "VisexSkinProbe.h"

bool UVisexSkinProbeLibrary::SetLODAllowCPUAccess(USkeletalMesh* Mesh, int32 LODIndex, bool bAllow)
{
	FSkeletalMeshLODInfo* Info = Mesh ? Mesh->GetLODInfo(LODIndex) : nullptr;
	if (!Info)
	{
		return false;
	}
	if (Info->bAllowCPUAccess != bAllow)
	{
		Mesh->Modify();
		Info->bAllowCPUAccess = bAllow;
#if WITH_EDITOR
		Mesh->PostEditChange();
#endif
		Mesh->MarkPackageDirty();
	}
	return true;
}

bool UVisexSkinProbeLibrary::GetLODAllowCPUAccess(const USkeletalMesh* Mesh, int32 LODIndex)
{
	const FSkeletalMeshLODInfo* Info = Mesh ? Mesh->GetLODInfo(LODIndex) : nullptr;
	return Info && Info->bAllowCPUAccess;
}

float UVisexSkinProbeLibrary::HandBodyGap(USkeletalMeshComponent* Mesh, bool bLeft)
{
	static FVisexSkinProbe Probe;
	if (!Mesh || (!Probe.IsBuiltFor(Mesh) && !Probe.Build(Mesh, true)))
	{
		return -1.f;
	}
	TArray<FVector> Palm, Body;
	TArray<FName> PalmBone, BodyBone;
	Probe.Skinned(Mesh, bLeft ? FVisexSkinProbe::HandL : FVisexSkinProbe::HandR, Palm, PalmBone);
	Probe.Skinned(Mesh, FVisexSkinProbe::Body, Body, BodyBone);
	double Best = TNumericLimits<double>::Max();
	for (int32 b = 0; b < Body.Num(); ++b)
	{
		const FString Name = BodyBone[b].ToString();
		if (Name.Contains(TEXT("arm")) || Name.Contains(TEXT("clavicle")) || Name.Contains(TEXT("hand")))
		{
			continue; // the arm's own skin (either arm) isn't what the hand rests on
		}
		for (const FVector& P : Palm)
		{
			Best = FMath::Min(Best, FVector::DistSquared(P, Body[b]));
		}
	}
	return Palm.Num() && Best < TNumericLimits<double>::Max() ? float(FMath::Sqrt(Best)) : -1.f;
}
