#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VisexFootprints.generated.h"

class UDecalComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * Footprints in the sand: each time one of the player's feet comes down on a surface
 * tagged "Sand", a shallow imprint (a normal-only decal, M_Footprint) is pressed where
 * it landed. Only the last Keep prints exist: each new step makes the older ones
 * shallower, so the trail fades out over ~8 steps. Place one in the level.
 */
UCLASS()
class VISEX_API AVisexFootprints : public AActor
{
	GENERATED_BODY()

public:
	AVisexFootprints();
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "Footprints")
	TObjectPtr<UMaterialInterface> Material;

	/** Prints alive at once; the oldest is gone when the next one lands. */
	UPROPERTY(EditAnywhere, Category = "Footprints")
	int32 Keep = 8;

	/** cm: the ankle bone counts as planted below this height over the ground... */
	UPROPERTY(EditAnywhere, Category = "Footprints")
	float PlantHeight = 12.5f;

	/** ...and as lifted again above PlantHeight + this. */
	UPROPERTY(EditAnywhere, Category = "Footprints")
	float LiftMargin = 3.5f;

	/** cm: half length and half width of the decal box. */
	UPROPERTY(EditAnywhere, Category = "Footprints")
	FVector2D HalfSize = FVector2D(16.f, 8.f);

	/** Fade speed toward each print's target depth (per second). */
	UPROPERTY(EditAnywhere, Category = "Footprints")
	float FadeRate = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Footprints")
	bool bDebug = false;

private:
	struct FPrint
	{
		TWeakObjectPtr<UDecalComponent> Decal;
		TWeakObjectPtr<UMaterialInstanceDynamic> MID;
		float Fade = 0.f;
		float Target = 1.f;
	};
	TArray<FPrint> Prints;      // oldest first
	bool bPlanted[2] = { true, true };

	void Stamp(const FVector& Where, const FVector& Forward, bool bLeft);
};
