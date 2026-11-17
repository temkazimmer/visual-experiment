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
 * where she stands, and the camera either orbits her or flies free.
 *
 *  L1 / R1 (Q / E)          previous / next motion
 *  TRIANGLE (Tab)           ORBIT <-> FREE camera
 *  ORBIT
 *    Right stick (← →)      orbit the camera round her
 *    Left stick ↕ (W / S)   camera height
 *    Left stick ↔ (A / D)   turn her
 *    Triggers (↑ / ↓, wheel) camera distance
 *  FREE
 *    Left stick (WASD)      fly: forward / back, sideways
 *    Right stick (mouse)    aim
 *    Triggers (R / F)       rise / sink
 *  D-pad ↕ (Z / X)          lens 12..135 mm
 *  D-pad ↔ (C / V)          tilt (Dutch angle); L3 levels it
 *  Focus follows the part of her the camera aims at.
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

	/** Free camera: cm per second at full stick, close to her and far from her. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Photo|Free")
	float FlySpeedNear = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Photo|Free")
	float FlySpeedFar = 450.f;

	/** Degrees per second of aim at full stick. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Photo|Free")
	float LookSpeed = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Photo|Lens")
	float MinFocal = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Photo|Lens")
	float MaxFocal = 135.f;

	/** Degrees per second of tilt, and the most it tilts either way. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Photo|Lens")
	float RollSpeed = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Photo|Lens")
	float MaxRoll = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Photo")
	float BlendTime = 0.8f;

	/** Walk and run loops, played in place. The wheel's poses follow them in the list. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Photo|Motion")
	TSoftObjectPtr<UAnimSequence> WalkLoop;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Photo|Motion")
	TSoftObjectPtr<UAnimSequence> RunLoop;

	/** Photo mode opens on the walk at this rate: once she is off the column she walks slowly
	 * in place (same walk as the game), until Next/Prev picks another motion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Photo|Motion", meta = (ClampMin = "0.1", ClampMax = "1"))
	float OpenWalkRate = 0.7f;

	/** A motion whose name contains "Ledge" puts her on the actor with this tag (Tools/add_ledge.py):
	 * on its top, facing along its long axis, shot from her side. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Photo|Ledge")
	FName LedgeTag = TEXT("PhotoLedge");

	/** Where her root goes along the ledge from its centre (cm, + = the way she faces): she kneels
	 * at the root and her forearms reach forward, so the root sits back of centre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Photo|Ledge")
	float LedgeAlong = -35.f;

	/** The ledge camera: angle round her from her facing (deg; 90 = her right side, more = a
	 * little behind), distance and height above the stone (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Photo|Ledge")
	float LedgeCameraAngle = 115.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Photo|Ledge")
	float LedgeCameraDistance = 260.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Photo|Ledge")
	float LedgeCameraHeight = 70.f;

	UFUNCTION(BlueprintPure, Category = "Photo|Ledge")
	bool IsOnLedge() const { return bOnLedge; }

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

	/** A motion's name (Idle, Walk, Run, then the poses by asset name), or empty past the last. */
	FString GetMotionName(int32 Index) const { return Motions.IsValidIndex(Index) ? Motions[Index].Name : FString(); }

	UFUNCTION(BlueprintCallable, Category = "Photo")
	void TogglePause();

	/** Put the camera somewhere directly: orbit angle (deg, world), distance and height (cm). */
	UFUNCTION(BlueprintCallable, Category = "Photo")
	void SetView(float InYaw, float InDistance, float InHeight);

	UFUNCTION(BlueprintCallable, Category = "Photo")
	void TakeShot();

	/** Switch between the orbit camera and the free one (keeps the current view). */
	UFUNCTION(BlueprintCallable, Category = "Photo")
	void ToggleFree();

	UFUNCTION(BlueprintPure, Category = "Photo")
	bool IsFree() const { return bFree; }

	/** Put the free camera somewhere directly (for scripts): location, aim, tilt, lens. */
	UFUNCTION(BlueprintCallable, Category = "Photo")
	void SetFreeView(FVector Location, FRotator Aim, float InRoll, float InFocal);

	/** Show or hide the help overlay (scripts hide it for clean captures). */
	UFUNCTION(BlueprintCallable, Category = "Photo")
	void SetHudVisible(bool bShow) { if (bOpen) { ShowHud(bShow); } }

	UPROPERTY(BlueprintAssignable, Category = "Photo")
	FVisexPhotoModeClosed OnClosed;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	struct FMotion
	{
		FString Name;
		TWeakObjectPtr<UAnimSequence> Loop;   // played on DefaultSlot; LoadedLoops keeps it alive
		int32 Pose = INDEX_NONE;         // or handed to the pose wheel
		float Gait = -1.f;               // catwalk strength for an in-place loop
	};
	void BuildMotions();
	void Next() { SetMotion(Motion + 1); }
	void Prev() { SetMotion(Motion - 1); }
	void PlaceCamera();
	void FlyCamera(float DeltaSeconds, float MoveX, float MoveY, float Rise, float AimX, float AimY);
	void ApplyLens(const FVector& Loc, const FRotator& Aim);
	void LevelRoll() { Roll = 0.f; }
	void ShowHud(bool bShow);
	void RefreshHud();
	void ClearMotion();
	static bool IsLedgeMotion(const FString& Name);
	/** Put her on the ledge top (true if there is one), or back where she stood. */
	bool SnapToLedge();
	void LeaveLedge();
	bool bOnLedge = false;
	FTransform LedgeSpot;            // her actor transform on the stone
	FTransform GroundSpot;           // where she stood before
	bool bLedgeWasHidden = false;    // the menu strips the scene for photo mode; put it back after
	FVector LookPoint = FVector::ZeroVector;   // eased hips the ledge camera watches
	UPROPERTY(Transient)
	TObjectPtr<AActor> Ledge;

	TArray<FMotion> Motions;
	/** Keeps the walk/run clips alive: FMotion only holds them weakly (it isn't seen by GC), and
	 * a collected clip behind a raw pointer crashed SetMotion (Shipping 0240). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAnimSequence>> LoadedLoops;
	int32 Motion = 0;
	bool bOpenWalk = false;   // the next SetMotion plays its loop at OpenWalkRate
	int32 PlayingPose = INDEX_NONE;
	float Yaw = 0.f;
	float Distance = 0.f;
	float Height = 0.f;
	float LookZ = -1.f;
	float LookGoal = -1.f;
	bool bFree = false;
	FVector FreeLoc = FVector::ZeroVector;
	FRotator FreeAim = FRotator::ZeroRotator;
	float Roll = 0.f;
	float Focal = 32.f;
	bool bOpen = false;
	bool bPaused = false;
	bool bKeysBound = false;
	TEnumAsByte<ERootMotionMode::Type> SavedRootMotion = ERootMotionMode::RootMotionFromMontagesOnly;
	TWeakObjectPtr<AActor> PreviousView;
	UPROPERTY(Transient)
	TObjectPtr<ACineCameraActor> Camera;
	TSharedPtr<SWidget> Hud;
	TSharedPtr<STextBlock> Title;
	TSharedPtr<STextBlock> Help;
};
