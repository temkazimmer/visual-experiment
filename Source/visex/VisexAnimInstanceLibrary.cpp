#include "VisexAnimInstanceLibrary.h"

#include "Animation/BlendSpace.h"

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
