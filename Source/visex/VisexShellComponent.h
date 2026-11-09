#pragma once

#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "VisexShellComponent.generated.h"

/**
 * A second skin drawn over the character: the android's translucent aerogel shell.
 * Attach it under the body mesh; on register it follows that mesh's pose exactly
 * (Leader Pose), so every animation, montage and photo-mode pose drives both layers.
 * The shell's look lives in its material (M_AerogelShell, Tools/char_shell.py).
 */
UCLASS(ClassGroup = (Rendering), meta = (BlueprintSpawnableComponent))
class VISEX_API UVisexShellComponent : public USkeletalMeshComponent
{
	GENERATED_BODY()

public:
	UVisexShellComponent(const FObjectInitializer& ObjectInitializer);

	/** Follow the attach parent's pose. Untick to animate the shell on its own. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shell")
	bool bFollowParentPose = true;

protected:
	virtual void OnRegister() override;
};
