#include "VisexAnimInstanceLibrary.h"

#include "Animation/BlendSpace.h"
#include "Animation/Skeleton.h"

FQuat UVisexAnimInstanceLibrary::RefPoseLocalRotation(USkeleton* Skeleton, FName Bone)
{
	if (!Skeleton)
	{
		return FQuat::Identity;
	}
	const FReferenceSkeleton& Ref = Skeleton->GetReferenceSkeleton();
	const int32 Index = Ref.FindBoneIndex(Bone);
	return Index == INDEX_NONE ? FQuat::Identity : Ref.GetRefBonePose()[Index].GetRotation();
}

bool UVisexAnimInstanceLibrary::ResampleBlendSpace(UBlendSpace* BlendSpace)
{
#if WITH_EDITOR
	if (!BlendSpace)
	{
		return false;
	}
	BlendSpace->Modify();
	BlendSpace->ValidateSampleData();
	BlendSpace->ResampleData();
	BlendSpace->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}
