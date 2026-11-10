#include "VisexSkinProbeLibrary.h"

#include "Engine/SkeletalMesh.h"

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
