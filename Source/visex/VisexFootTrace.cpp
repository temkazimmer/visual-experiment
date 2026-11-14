#include "VisexFootTrace.h"
#include "visex.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/UObjectIterator.h"

int32 UVisexFootTrace::SetUp(UWorld* World)
{
	// Her Foot Placement traces FootTrace (visex.h). Every visible surface with query collision
	// that blocks Visibility blocks it too: the dunes, the plaza slabs, the steps, the rubble,
	// the drum. The invisible walking ramp under the steps doesn't, so her soles meet the stone
	// she is seen on, not a surface 10 cm under it. Set at runtime, so the levels are unchanged.
	int32 Count = 0;
	for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
	{
		UStaticMeshComponent* C = *It;
		if (!World || C->GetWorld() != World || !C->IsVisible() || C->bHiddenInGame || (C->GetOwner() && C->GetOwner()->IsHidden())
			|| !C->IsQueryCollisionEnabled() || C->GetCollisionResponseToChannel(ECC_Visibility) != ECR_Block)
		{
			continue;
		}
		if (C->GetCollisionResponseToChannel(VisexFootTraceChannel) != ECR_Block)
		{
			C->SetCollisionResponseToChannel(VisexFootTraceChannel, ECR_Block);
			++Count;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("Foot trace: %d more visible surfaces block FootTrace"), Count);
	return Count;
}
