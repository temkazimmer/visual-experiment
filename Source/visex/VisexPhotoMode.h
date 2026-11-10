#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Animation/AnimEnums.h"
#include "VisexPhotoMode.generated.h"

class ACineCameraActor;
class UAnimSequence;
class SWidget;
class STextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVisexPhotoModeClosed);

/**
 * Photo mode: she plays a motion on a loop (idle, walk, run, or any pose from the wheel)
 * while a free camera orbits her.
 *
 *  L1 / R1 (Q / E)          previous / next motion
 *  Right stick (← →)        orbit the camera round her
 *  Left stick ↕ (W / S)     camera height
 *  Left stick ↔ (A / D)     turn her
 *  Triggers (↑ / ↓, wheel)  camera distance
 *  CROSS (Space)            freeze / play
 *  SQUARE (P)               save a 2x shot to Saved/Screenshots
 *  CIRCLE (Esc)             back to the menu
 */
UCLASS(Blueprintable)
class VISEX_API AVisexPhotoMode : public AActor
{
	GENERATED_BODY()

public:
	AVisexPhotoMode();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Photo|Camera")
	float StartDistance = 340.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Photo|Camera")
	float MinDistance = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Photo|Camera")
	float MaxDistance = 800.f;

	/** Camera height above her feet (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Photo|Camera")
	float StartHeight = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Photo|Camera")
	float MinHeight = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Photo|Camera")
	float MaxHeight = 320.f;

	/** The point the camera watches, above her feet (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Photo|Camera")
	float LookHeight = 95.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Photo|Camera")
	float FocalLength = 32.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Photo|Camera")
	float Aperture = 2.8f;

	/** Degrees per second at full stick. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Photo|Speed")
	float OrbitSpeed = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Photo|Speed")
	float TurnSpeed = 120.f;

	/** Centimetres per second at full stick or trigger. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Photo|Speed")
	float HeightSpeed = 140.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Photo|Speed")
	float ZoomSpeed = 320.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Photo")
	float BlendTime = 0.8f;

	/** Walk and run loops, played in place. The wheel's poses follow them in the list. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Photo|Motion")
	TSoftObjectPtr<UAnimSequence> WalkLoop;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Photo|Motion")
	TSoftObjectPtr<UAnimSequence> RunLoop;

	UFUNCTION(BlueprintCallable, Category = "Photo")
	void Open();

	UFUNCTION(BlueprintCallable, Category = "Photo")
	void Close();

	UFUNCTION(BlueprintPure, Category = "Photo")
	bool IsOpen() const { return bOpen; }

	/** Jump to a motion: 0 idle, 1 walk, 2 run, then the poses. */
	UFUNCTION(BlueprintCallable, Category = "Photo")
	void SetMotion(int32 Index);

	UFUNCTION(BlueprintPure, Category = "Photo")
	int32 GetMotion() const { return Motion; }

	UFUNCTION(BlueprintCallable, Category = "Photo")
	void TogglePause();

	UFUNCTION(BlueprintCallable, Category = "Photo")
	void TakeShot();

	UPROPERTY(BlueprintAssignable, Category = "Photo")
	FVisexPhotoModeClosed OnClosed;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	struct FMotion
	{
		FString Name;
		UAnimSequence* Loop = nullptr;   // played on DefaultSlot
		int32 Pose = INDEX_NONE;         // or handed to the pose wheel
		float Gait = -1.f;               // catwalk strength for an in-place loop
	};
	void BuildMotions();
	void Next() { SetMotion(Motion + 1); }
	void Prev() { SetMotion(Motion - 1); }
	void PlaceCamera();
	void ShowHud(bool bShow);
	void RefreshHud();
	void ClearMotion();

	TArray<FMotion> Motions;
	int32 Motion = 0;
	int32 PlayingPose = INDEX_NONE;
	float Yaw = 0.f;
	float Distance = 0.f;
	float Height = 0.f;
	float LookZ = -1.f;
	float LookGoal = -1.f;
	bool bOpen = false;
	bool bPaused = false;
	bool bKeysBound = false;
	TEnumAsByte<ERootMotionMode::Type> SavedRootMotion = ERootMotionMode::RootMotionFromMontagesOnly;
	TWeakObjectPtr<AActor> PreviousView;
	UPROPERTY(Transient)
	TObjectPtr<ACineCameraActor> Camera;
	TSharedPtr<SWidget> Hud;
	TSharedPtr<STextBlock> Title;
};
