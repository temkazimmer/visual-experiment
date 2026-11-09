#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "VisexBodyProfile.h"
#include "VisexAnimInstance.generated.h"

/**
 * Runs the Anim Blueprint's graph as usual, then reshapes the body from the profile:
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

	FVisexBodyProfile Profile;
	bool bHasProfile = false;
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

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FVisexAnimProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete static_cast<FVisexAnimProxy*>(InProxy); }
	friend struct FVisexAnimProxy;
};
