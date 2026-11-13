#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "VisexBodyProfile.h"
#include "VisexAnimInstance.generated.h"

/**
 * Runs the Anim Blueprint's graph as usual, then adds the catwalk layer to the walk
 * (hips sway and turn with each step, the chest counters them, the head stays level,
 * the feet land near one line), then reshapes the body from the profile:
 * each body region scales its bone (width, depth or length, worked out from the
 * reference pose) and counter-scales the next segment, so only that region changes.
 * Being the last step before skinning, it works for every animation, montage and
 * photo-mode pose, and the shell (Leader Pose) follows for free.
 */
struct FVisexAnimProxy : public FAnimInstanceProxy
{
	FVisexAnimProxy() = default;
	explicit FVisexAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate_WithRoot(FPoseContext& Output, FAnimNode_Base* InRootNode) override;

private:
	void Reshape(FCompactPose& Pose) const;
	void Catwalk(FCompactPose& Pose);
	void Alive(FCompactPose& Pose);
	void Contact(FCompactPose& Pose);
	void Limits(FCompactPose& Pose);
	void SteadyHead(FCompactPose& Pose);
	/** Last of all: planted soles onto the visible sand (visex.Plant). */
	void Plant(FCompactPose& Pose);
	void PlantGround(UAnimInstance* Instance);   // game thread: the traces

public:
	virtual void PostUpdate(UAnimInstance* InAnimInstance) const override;

private:

	FVisexBodyProfile Profile;
	bool bHasProfile = false;

	// catwalk layer: tuning read from the visex.Catwalk.* console variables each update
	struct FCatwalkTune
	{
		float HipRoll = 0.f, HipYaw = 0.f, HipShift = 0.f, Line = 0.f, Shoulders = 0.f, Head = 0.f;
		float Posture = 0.f, ChestLag = 0.f, ArmSwing = 1.f, HeadLine = 0.f, ShoulderRoll = 0.f;
		float Clear = 0.f; // cm the thigh axes keep apart at the crossing (the swing knee turns out)
	} Tune;
	float GaitTarget = 0.f; // 0 standing or airborne, 1 walking, less when sprinting
	float PhotoGait = -1.f; // copied from the instance: the gait forced by photo mode, or -1
	float Gait = 0.f;       // GaitTarget, eased
	float Step = 0.f;       // -1..1: which foot is ahead (+ left)
	float Lift = 0.f;       // -1..1: which foot is in the air (+ left)
	float ChestStep = 0.f, ChestLift = 0.f; // Step and Lift as the chest feels them, a few frames late
	// the walk clip's own head, before any layer: the catwalk keeps the head on this line and aim
	FVector ClipHeadAt = FVector::ZeroVector;
	FQuat ClipHeadRot = FQuat::Identity;
	FVector CatwalkLeft = FVector::XAxisVector;
	bool bClipHead = false;
	float KneeOut[2] = {0.f, 0.f}; // degrees each thigh (L, R) is turned out to clear the other, eased
	float Dt = 0.f;

	// moving hold: a held pose breathes and drifts instead of freezing (visex.Hold.*)
	struct FHoldTune
	{
		float Breath = 0.f, BreathPeriod = 4.f, Collar = 0.f, Sway = 0.f, SwayPeriod = 8.f, Head = 0.f, Pin = 30.f, LyingBreath = 1.f;
	} HoldTune;
	float HoldTarget = 0.f; // copied from the instance: 1 while a pose is held
	float Hold = 0.f;       // HoldTarget, eased
	float HoldClock = 0.f;  // seconds of hold time; the breath and drift run on it

	// toes at push-off (visex.Toes.*): the clips bend only the ball, so the toes moved as one block
	void Toes(FCompactPose& Pose);
	struct FToesTune
	{
		float Big = 0.f, Lift = 0.f, Grip = 0.f, Plant = 0.f, Spread = 0.f, Full = 25.f;
	} ToesTune;
	float ToesTarget = 0.f;           // 1 while she walks or runs
	float ToesOn = 0.f;               // ToesTarget, eased
	float ToesPush[2] = {0.f, 0.f};   // each foot's ball bend past rest (deg), eased: quick in, slower out

	FVector HandFix[2] = {FVector::ZeroVector, FVector::ZeroVector}; // copied from the instance

	// joint limits (visex.Limits): the worst excess this frame, reported to the instance
	bool bLimits = true;
	float LimitWorst = 0.f;
	FName LimitWorstBone;
	FString LimitReportBuf;
	// plant pass (visex.Plant): ground under each foot (component space), traced on the game thread
	float PlantTarget = 0.f;          // the instance's PlantAlpha times the cvar
	float PlantMoving = 0.f;          // 0 standing, 1 walking (by speed)
	bool bGround[2] = {false, false};
	float GroundZ[2] = {0.f, 0.f};
	FVector SoleAt[2] = {FVector::ZeroVector, FVector::ZeroVector}; // each foot's lowest sole point last frame
	float PlantFix[2] = {0.f, 0.f};   // cm each ankle moves up, eased
	float PelvisFix = 0.f;            // cm the pelvis drops so a leg can reach, eased
	// each foot's lowest skinned sole vertex last frame, under its foot bone (component space, cm)
	bool bSole[2] = {false, false};
	float SoleUnder[2] = {0.f, 0.f};
	void MeasureSole(const USkeletalMeshComponent* Mesh);
	// what the plant pass did this frame, for the footprints' mounds (copied to the instance in PostUpdate)
	float OutGap[2] = {1e4f, 1e4f};   // cm between the planted sole and the sand, component space
	float OutPlant[2] = {0.f, 0.f};   // how planted each foot is, 0..1
};

UCLASS(Transient, Blueprintable)
class VISEX_API UVisexAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Set by UVisexBodyComponent; read by the proxy each update. */
	UPROPERTY(BlueprintReadWrite, Category = "Body")
	FVisexBodyProfile BodyProfile;

	UPROPERTY(BlueprintReadWrite, Category = "Body")
	bool bReshapeBody = true;

	/** The catwalk layer normally follows her speed. Set to 0 or more to drive it directly
	 *  while a walk or run clip plays in place (photo mode): 1 for MF_Unarmed_Walk_Fwd,
	 *  0.35 for MF_Unarmed_Jog_Fwd. -1 hands it back to her speed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Catwalk")
	float PhotoGait = -1.f;

	/** 1 while a still pose is held (set by the pose wheel): the moving-hold layer breathes
	 *  and drifts it, so a photo pose reads as a held pose, not a frozen frame. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Hold")
	float HoldAlpha = 0.f;

	/** Where each hand has to move (component space, cm) so its skin meets the surface under
	 *  it, set each frame by the pose wheel's contact probe. Applied last, after the reshape. */
	UPROPERTY(BlueprintReadWrite, Category = "Contact")
	FVector HandFixL = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Contact")
	FVector HandFixR = FVector::ZeroVector;

	/** Joint limits: how many degrees the worst joint asked to go past human range of motion
	 *  this frame (before it was held back), and which joint. For auditing poses. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Limits")
	float LimitExcess = 0.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Limits")
	FName LimitBone;

	/** Every limited joint this frame: "bone bend/side/twist" in degrees, for the audit scripts. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Limits")
	FString LimitReport;

	/** 1 plants her soles on the sand after every other layer; the menu sets it to 0 while she
	 *  sits for Settings, photo mode while its motions key their own feet. */
	UPROPERTY(BlueprintReadWrite, Category = "Plant")
	float PlantAlpha = 1.f;

	/** Set by the plant pass each frame: cm between each foot's lowest sole vertex and the sand
	 *  (component space; 1e4 when unknown), and how planted it is (0..1). AVisexFootprints
	 *  forms a sand mound on contact from these. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Plant")
	float SoleGapL = 1e4f;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Plant")
	float SoleGapR = 1e4f;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Plant")
	float PlantedL = 0.f;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Plant")
	float PlantedR = 0.f;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FVisexAnimProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete static_cast<FVisexAnimProxy*>(InProxy); }
	friend struct FVisexAnimProxy;
};
