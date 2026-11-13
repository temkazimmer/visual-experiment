#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VisexFootprints.generated.h"

class UDecalComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Footprints in the sand: each time one of the player's feet comes down on a surface
 * tagged "Sand", a shallow imprint (a normal-only decal, M_Footprint) is pressed where
 * it landed. Only the last Keep prints exist: each new step makes the older ones
 * shallower, so the trail fades out over ~8 steps.
 *
 * Where a sole really touches the sand, a low rim of sand (SM_SandMound, the dune
 * material) forms once around it and stays there, settling flat over a few seconds.
 * Place one in the level.
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

	/** cm/s: a low foot counts as planted once it moves slower than this (a planted
	 *  foot is still), and as lifted again above PlantSpeed x 2. Height alone missed
	 *  steps in a slow walk, where the ankle barely rises. */
	UPROPERTY(EditAnywhere, Category = "Footprints")
	float PlantSpeed = 45.f;

	/** cm: half length and half width of the decal box. */
	UPROPERTY(EditAnywhere, Category = "Footprints")
	FVector2D HalfSize = FVector2D(18.f, 10.f);

	/** cm above and below the sand the decal reaches: enough for a sloped dune under an 18 cm
	 *  half-foot, short of her ankles and the menu's stone drum (25 put dark blotches on both). */
	UPROPERTY(EditAnywhere, Category = "Footprints")
	float HalfDepth = 8.f;

	/** cm in front of the ankle bone where the sole's middle is (heel ~6 cm behind it, toes ~20 in front). */
	UPROPERTY(EditAnywhere, Category = "Footprints")
	float SoleOffset = 6.5f;

	/** Fade speed toward each print's target depth (per second). */
	UPROPERTY(EditAnywhere, Category = "Footprints")
	float FadeRate = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Footprints")
	bool bDebug = false;

	/** Sand mounds: on real contact (the plant pass has the foot down, its sole within
	 *  MoundGap of the sand) a low rim of sand forms once around the sole, on the sand,
	 *  and stays put there, settling flat over MoundLife seconds. */
	UPROPERTY(EditAnywhere, Category = "Mounds")
	bool bMounds = true;
	/** The rim at its real size (cm), its pivot on the sand under the sole's middle. */
	UPROPERTY(EditAnywhere, Category = "Mounds")
	TObjectPtr<UStaticMesh> MoundMesh;
	UPROPERTY(EditAnywhere, Category = "Mounds")
	TObjectPtr<UMaterialInterface> MoundMaterial;
	/** cm: contact is a planted sole at most this far over the sand... */
	UPROPERTY(EditAnywhere, Category = "Mounds")
	float MoundGap = 0.5f;
	/** ...and the foot counts as lifted again (a new contact may form) above this. */
	UPROPERTY(EditAnywhere, Category = "Mounds")
	float MoundLift = 2.f;
	/** Plant weight (0..1) that counts as planted. */
	UPROPERTY(EditAnywhere, Category = "Mounds")
	float MoundPlanted = 0.5f;
	/** Seconds the sand takes to push out, and a mound's whole life. */
	UPROPERTY(EditAnywhere, Category = "Mounds")
	float MoundGrow = 0.25f;
	UPROPERTY(EditAnywhere, Category = "Mounds")
	float MoundLife = 8.f;
	/** Mounds alive at once; the oldest goes first. */
	UPROPERTY(EditAnywhere, Category = "Mounds")
	int32 MoundMax = 12;
	/** cm the rim sits sunk, so its feathered edge meets the sand below the surface. */
	UPROPERTY(EditAnywhere, Category = "Mounds")
	float MoundSink = 0.3f;

protected:
	virtual void BeginPlay() override;

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
	FVector LastAnkle[2] = { FVector::ZeroVector, FVector::ZeroVector };
	bool bHaveLast[2] = { false, false };

	/** The print under each foot while it's down: it follows the foot until it lifts,
	 *  so it ends up under the flat foot, not where the heel first touched. */
	TWeakObjectPtr<UDecalComponent> Active[2];

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Mounds;   // oldest first
	TArray<float> MoundAge;
	bool bMounded[2] = { false, false };   // this contact already left its mound
	TWeakObjectPtr<UStaticMeshComponent> Held[2];   // the mound a still-planted foot stands in: it holds until she steps off
	void Mound(const FVector& Where, const FVector& Up, const FVector& Forward);
	void Feet(class ACharacter* Pawn, class USkeletalMeshComponent* Mesh);
	UDecalComponent* Stamp(const FVector& Where, const FVector& Forward, bool bLeft);
	FRotator PrintRotation(const FVector& Forward) const;
};
