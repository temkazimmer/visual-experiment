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
	float LastZ = 0.f;       // the bone's last position along the spring's axis
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
 *   lands or stops. Each breast also swings sideways and forward/back on two more springs
 *   (BustSway{L,R}_{Out,In,Fwd,Back}).
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Jiggle") float BustFrequency = 1.9f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Jiggle") float BustDamping = 0.1f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Jiggle") float BustTravel = 2.4f;
	/** Bust bounce strength on its own (the shared JiggleStrength is the glutes'). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Jiggle") float BustStrength = 0.65f;
	/** The bust also swings side to side and forward/back on the chest (BustSway morphs):
	 *  its own springs, looser than the bounce, fed by the chest's sideways and forward moves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Jiggle") float BustSwayFrequency = 1.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Jiggle") float BustSwayDamping = 0.12f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Jiggle") float BustSwayTravel = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Jiggle") float BustSwayStrength = 0.8f;
	/** Where the springs start to stiffen, in travels: past it the tissue hardens (no hard stop). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Jiggle") float SoftLimit = 1.5f;
	/** Damping added while she stands still (fades out as she walks), so idle tissue settles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Jiggle") float StandingDamping = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Jiggle") float GluteFrequency = 2.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Jiggle") float GluteDamping = 0.16f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Jiggle") float GluteTravel = 1.9f;
	/** Each glute half rides its own leg: how far down the thigh its spring hangs (0 = the hip
	 *  joint, which both sides share; 1 = the knee). The leg swing then drives each half alone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Jiggle") float GluteLegFollow = 0.6f;
	/** Each breast hangs between the chest and its own shoulder: 0 = both ride the chest
	 *  together, 1 = each rides its own shoulder (arm swing alternates them). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Jiggle") float BustSideFollow = 0.85f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Jiggle") float BustDetune = 0.1f;
	/** Left and right glute springs ring at slightly different rates, so they drift apart. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Jiggle") float GluteDetune = 0.15f;
	/** Glute bounce strength (the bust has its own, BustStrength). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Jiggle") float JiggleStrength = 1.4f;
	/** Arm-raise correctives: how far the upper arm has risen from hanging (degrees, in the
	 *  torso's frame) when they start to fade in, and where they are full (the sculpt pose). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body") float ArmUpStart = 80.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body") float ArmUpFull = 176.6f;
	/** Hip-flexion correctives (ThighUp_l/_r): the thigh's lift from hanging, degrees, where
	 *  they start to fade in and where they are full (the sculpt pose). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body") float ThighUpStart = 45.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body") float ThighUpFull = 94.f;
	/** Bust height at the slider's ends, in bounce-morph weights (1 = 2.4 cm at the tip). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body") float BustLiftRange = 1.3f;

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
	/** Moves one spring on: X is its bone's position along the spring's axis (cm). */
	static void StepSpring(FVisexJiggleState& J, float X, float DeltaTime, float Hz, float Zeta, float Limit);
	/** Fades in the arm-raise and hip-flexion correctives (ArmUp_l/_r, ThighUp_l/_r, Tools/blender_correctives.py). */
	void TickCorrectives();

	UPROPERTY(Transient) TObjectPtr<class UVisexShellComponent> OutfitMesh;
	/** The skin's material, made dynamic so the tone can change. */
	UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> SkinMaterial;
	void ApplySkin();
	FName WornOutfit = NAME_None;
	FVisexJiggleState Jiggle[4];   // bust L, bust R, glute L, glute R
	FVisexJiggleState Sway[4];     // bust L, bust R sideways (her left +); bust L, bust R forward
	FVector SwayLast[2] = {FVector::ZeroVector, FVector::ZeroVector};
	float Stillness = 1.f;
};
