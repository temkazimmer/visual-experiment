#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VisexBodyProfile.h"
#include "VisexBodyComponent.generated.h"

/** One soft-tissue spring: the tissue lags the bone it hangs on, then settles. */
USTRUCT()
struct FVisexJiggleState
{
	GENERATED_BODY()

	float Offset = 0.f;      // cm, tissue relative to its bone (vertical)
	float Velocity = 0.f;    // cm/s, tissue's own vertical speed
	float LastZ = 0.f;
	bool bStarted = false;
};

/**
 * The character's body and what she wears. Lives on the player character; the
 * Character page (AVisexBodyEditor) edits it.
 * - Height scales the whole figure (mesh and capsule); leg length lifts the mesh so her
 *   feet stay on the ground; bone sliders go to UVisexAnimInstance.
 * - Bust and Glutes are morph weights on BustUp / GlutesUp.
 * - Jiggle: four damped springs (each breast, each glute) follow the bones they hang on
 *   and drive the Bust/GluteBounce{L,R}_{Up,Down} morphs, so they bounce when she runs,
 *   lands or stops.
 * - Outfit: a skin-fitted mesh on the same skeleton, following the body's pose and morphs.
 * Morphs are set on the body mesh; the skin and the outfit follow it (Leader Pose).
 */
UCLASS(ClassGroup = (Visex), meta = (BlueprintSpawnableComponent))
class VISEX_API UVisexBodyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVisexBodyComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	FVisexBodyProfile Profile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	FString SaveSlot = TEXT("VisexBody");

	/** Load the saved body when play starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	bool bLoadOnBeginPlay = true;

	/** Quinn at mesh scale 1: height (top of head), capsule half-height and radius, mesh Z. */
	UPROPERTY(EditAnywhere, Category = "Body|Reference")
	float ReferenceHeight = 177.5f;
	UPROPERTY(EditAnywhere, Category = "Body|Reference")
	FVector2D ReferenceCapsule = FVector2D(90.f, 35.f);
	UPROPERTY(EditAnywhere, Category = "Body|Reference")
	float ReferenceMeshZ = -89.f;

	/** Soft-tissue bounce. Frequency in Hz, damping ratio (0 = rings forever, 1 = no
	 *  overshoot), and the travel (cm) one full bounce morph stands for. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Jiggle") bool bJiggle = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Jiggle") float BustFrequency = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Jiggle") float BustDamping = 0.18f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Jiggle") float BustTravel = 1.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Jiggle") float GluteFrequency = 2.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Jiggle") float GluteDamping = 0.24f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Jiggle") float GluteTravel = 1.1f;
	/** Overall bounce strength (1 = the sculpted travel at full size). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Jiggle") float JiggleStrength = 0.5f;

	UFUNCTION(BlueprintCallable, Category = "Body")
	void Apply();

	UFUNCTION(BlueprintCallable, Category = "Body")
	void SetValue(FName Name, float Value);

	UFUNCTION(BlueprintPure, Category = "Body")
	float GetValue(FName Name) const { return Profile.Get(Name); }

	UFUNCTION(BlueprintCallable, Category = "Body")
	void ResetValue(FName Name);

	UFUNCTION(BlueprintCallable, Category = "Body")
	void SetOutfit(FName Outfit);

	UFUNCTION(BlueprintCallable, Category = "Body")
	bool Save() const;

	/** Save this profile instead of the one she shows now (the page saves only what was applied). */
	bool SaveProfile(const FVisexBodyProfile& Saved) const;

	UFUNCTION(BlueprintCallable, Category = "Body")
	bool Load();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;

private:
	class USkeletalMeshComponent* Body() const;
	void PushToAnim() const;
	void ApplyOutfit();
	void TickJiggle(float DeltaTime);

	UPROPERTY(Transient) TObjectPtr<class UVisexShellComponent> OutfitMesh;
	/** The skin's material, made dynamic so the tone can change. */
	UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> SkinMaterial;
	void ApplySkin();
	FName WornOutfit = NAME_None;
	FVisexJiggleState Jiggle[4];   // bust L, bust R, glute L, glute R
};
