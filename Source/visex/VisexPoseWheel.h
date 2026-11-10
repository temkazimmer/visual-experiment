#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "VisexPoseWheel.generated.h"

class UAnimMontage;
class UAnimSequence;
class UInputAction;
class UInputComponent;
class SVisexPoseWheel;
struct FInputActionValue;

/** What the wheel widget draws; written by the component each frame. */
struct FVisexPoseWheelView
{
	TArray<FString> Labels;
	int32 Selected = INDEX_NONE;
	float Fade = 0.f;                       // 0 hidden, 1 fully open
	FVector2D Pointer = FVector2D::ZeroVector; // stick or mouse, x right, y up, length 0..1
};

/**
 * The pose wheel. Hold IA_PoseWheel (E, gamepad right shoulder): a ring of poses fades in,
 * either stick or the mouse points at one, releasing strikes it. The poses are every
 * AnimSequence in /Game/Characters/Poses, plus "Stand", which lets go of the pose.
 * A pose is held on the Anim Blueprint's DefaultSlot until she moves, jumps or dashes.
 * A pose may bring A_Pose_X_In and A_Pose_X_Out clips (sitting down, getting up): then she
 * plays In, stays on its last frame, and moving plays Out before she can walk again.
 * Bound to the pawn's own input, so it is silent while the menu or Body page has it disabled.
 */
UCLASS(ClassGroup = (Visex), meta = (BlueprintSpawnableComponent))
class VISEX_API UVisexPoseWheelComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVisexPoseWheelComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Strike pose Index (order of the wheel, clockwise from the top). */
	UFUNCTION(BlueprintCallable, Category = "Pose")
	void PlayPose(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "Pose")
	void StopPose();

	UPROPERTY(EditAnywhere, Category = "Pose")
	float BlendTime = 0.4f;

private:
	void OnWheelPressed(const FInputActionValue& Value);
	void OnWheelReleased(const FInputActionValue& Value);
	void SetOpen(bool bInOpen);
	void BindInput();
	void StartPose(int32 Index);
	void BeginExit();
	void StartFlow(int32 Index);
	UAnimSequence* FlowTo(int32 From, int32 To) const;
	void FinishPose();
	void UpdateFootIK(float DeltaTime);
	void UpdateGround(float DeltaTime);
	class UAnimInstance* GetAnim() const;

	UPROPERTY()
	TObjectPtr<UInputAction> WheelAction;

	UPROPERTY()
	TArray<TObjectPtr<UAnimSequence>> Poses;

	/** Same order as Poses; null where a pose has no way in or out of its own. */
	UPROPERTY()
	TArray<TObjectPtr<UAnimSequence>> PoseIns;

	UPROPERTY()
	TArray<TObjectPtr<UAnimSequence>> PoseOuts;

	/** Floor to floor, Poses.Num() squared, [From * Num + To]: null where there is none. */
	UPROPERTY()
	TArray<TObjectPtr<UAnimSequence>> Flows;

	UPROPERTY()
	TObjectPtr<UAnimMontage> PoseMontage;

	enum class EPhase : uint8 { None, Enter, Hold, Exit };
	EPhase Phase = EPhase::None;
	float PhaseTime = 0.f;        // seconds since she began to sit down
	float EnterLength = 0.f;      // the clip taking her into the pose: its _In, or a flow from the last one
	bool bFlowing = false;        // moving from one floor pose to another, already down
	float FootIKAlpha = 1.f;      // the Anim Blueprint's foot IK, off while she is on the floor
	float GroundAlpha = 0.f;      // how far the body is tilted onto the slope under her
	FVector MeshBaseLocation = FVector::ZeroVector;   // the body's own placement, put back when she is up
	FQuat MeshBaseRotation = FQuat::Identity;
	int32 Current = INDEX_NONE;
	int32 Pending = INDEX_NONE;   // the pose picked while she was still getting up
	bool bExitQueued = false;     // asked to get up before she had finished sitting down
	bool bMovementHeld = false;   // walking is switched off while she is on the ground

	TWeakObjectPtr<UInputComponent> BoundInput;
	TSharedPtr<FVisexPoseWheelView> View;
	TSharedPtr<SVisexPoseWheel> Widget;
	FVector2D Cursor = FVector2D::ZeroVector; // the mouse's virtual pointer while the wheel is open
	bool bOpen = false;
};

/** Gives the local player's character a pose wheel, so no Blueprint has to carry the component. */
UCLASS()
class VISEX_API UVisexPoseWheelSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UVisexPoseWheelSubsystem, STATGROUP_Tickables); }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}
};
