#include "VisexShellComponent.h"

UVisexShellComponent::UVisexShellComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// the body casts the shadow; a translucent skin shouldn't double it
	SetCastShadow(false);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	// her skin and clothes take no decals: the sand's footprint decal blotched her ankles
	bReceivesDecals = false;
	bUseBoundsFromLeaderPoseComponent = true;
}

void UVisexShellComponent::OnRegister()
{
	Super::OnRegister();
	if (bFollowParentPose)
	{
		if (USkinnedMeshComponent* Body = Cast<USkinnedMeshComponent>(GetAttachParent()))
		{
			SetLeaderPoseComponent(Body, true);
		}
	}
}
