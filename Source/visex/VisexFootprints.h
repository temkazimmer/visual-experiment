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
 * Standing still, each foot also sinks into a small sand mound (SM_SandMound, the dune
 * material) that rises just above the sole: it hides any gap between foot and sand and
 * gives her weight. The mounds settle back when she walks off. Place one in the level.
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

	/** Sand mounds under standing feet: they hide the small gap between sole and sand. */
	UPROPERTY(EditAnywhere, Category = "Mounds")
	bool bMounds = true;

	/** A unit mound: 100 cm across, 100 cm high, zero slope at its rim. */
	UPROPERTY(EditAnywhere, Category = "Mounds")
	TObjectPtr<UStaticMesh> MoundMesh;

	UPROPERTY(EditAnywhere, Category = "Mounds")
	TObjectPtr<UMaterialInterface> MoundMaterial;

	/** cm: the ankle bone's height over the sole when the foot is flat. */
	UPROPERTY(EditAnywhere, Category = "Mounds")
	float AnkleRest = 7.0f;

	/** cm the mound rises above the sole, and its height limits. */
	UPROPERTY(EditAnywhere, Category = "Mounds")
	float MoundCover = 2.0f;
	UPROPERTY(EditAnywhere, Category = "Mounds")
	FVector2D MoundHeightRange = FVector2D(1.5f, 6.f);

	/** cm: mound length (along the foot) and width. */
	UPROPERTY(EditAnywhere, Category = "Mounds")
	FVector2D MoundSize = FVector2D(42.f, 28.f);

	/** cm/s: she counts as standing below this speed. */
	UPROPERTY(EditAnywhere, Category = "Mounds")
	float StandSpeed = 15.f;

	/** cm/s the mounds grow and settle. */
	UPROPERTY(EditAnywhere, Category = "Mounds")
	float MoundRate = 8.f;

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
	TObjectPtr<UStaticMeshComponent> Mounds[2];
	float MoundH[2] = { 0.f, 0.f };
	float MoundTarget[2] = { 0.f, 0.f };

	void Feet(class ACharacter* Pawn, class USkeletalMeshComponent* Mesh);
	UDecalComponent* Stamp(const FVector& Where, const FVector& Forward, bool bLeft);
	FRotator PrintRotation(const FVector& Forward) const;
};
