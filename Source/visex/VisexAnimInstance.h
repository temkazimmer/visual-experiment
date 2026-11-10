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

	FVisexBodyProfile Profile;
	bool bHasProfile = false;

	// catwalk layer: tuning read from the visex.Catwalk.* console variables each update
	struct FCatwalkTune
	{
		float HipRoll = 0.f, HipYaw = 0.f, HipShift = 0.f, Line = 0.f, Shoulders = 0.f, Head = 0.f;
		float Posture = 0.f, ChestLag = 0.f, ArmSwing = 1.f, HeadLine = 0.f;
	} Tune;
	float GaitTarget = 0.f; // 0 standing or airborne, 1 walking, less when sprinting
	float PhotoGait = -1.f; // copied from the instance: the gait forced by photo mode, or -1
	float Gait = 0.f;       // GaitTarget, eased
	float Step = 0.f;       // -1..1: which foot is ahead (+ left)
	float Lift = 0.f;       // -1..1: which foot is in the air (+ left)
	float ChestStep = 0.f, ChestLift = 0.f; // Step and Lift as the chest feels them, a few frames late
	float HeadSide = 0.f;   // the head's slow average sideways position, its line
	float Dt = 0.f;
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

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FVisexAnimProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete static_cast<FVisexAnimProxy*>(InProxy); }
	friend struct FVisexAnimProxy;
};
