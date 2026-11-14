#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VisexFootTrace.generated.h"

/** Every scene her feet stand in gets the FootTrace ground (visex.h) at BeginPlay, menu or
 *  not (the intro film, a photo set): the plant pass and Foot Placement trace it. */
UCLASS()
class VISEX_API UVisexFootTrace : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	/** The visible surfaces that block Visibility block FootTrace too. Idempotent: run again
	 *  after spawning walkable things (the menu's drum). Returns how many it changed. */
	static int32 SetUp(UWorld* World);

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}
	virtual void OnWorldBeginPlay(UWorld& InWorld) override { Super::OnWorldBeginPlay(InWorld); SetUp(&InWorld); }
};
