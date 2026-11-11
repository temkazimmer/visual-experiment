#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VisexBodyProfile.h"
#include "VisexBodyEditor.generated.h"

class UCineCameraComponent;
class UWidgetComponent;
class UVisexBodyWidget;
class UVisexBodyComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVisexBodyEditorClosed);

/**
 * The Character page. Open() moves the view to a camera in front of the player character,
 * with the panel on the left. Picking a body row eases the camera to a close-up of that part
 * (longer lens, shallow focus, framed from her bones so it follows the sliders); rows about
 * her back turn her round. The right stick (or a mouse drag) turns her on the spot.
 * Two pages. The Body page (Open) has BODY, FACE and HAIR tabs; L1/R1 (Q/E, Tab) switch
 * them, up/down picks a row, left/right shapes her. The Wardrobe (OpenWardrobe) lists the
 * costumes: moving onto one tries it on, CROSS wears it; a locked costume only opens an
 * "under construction" window. Every change shows on her at once; CROSS (Enter, Space)
 * on APPLY applies that tab and saves it.
 * TRIANGLE resets a slider. CIRCLE goes back: what was not applied is undone, OnClosed
 * fires and the menu shows itself again.
 *
 * The menu director spawns one of these (no level edit needed) and calls Open().
 */
UCLASS(Blueprintable)
class VISEX_API AVisexBodyEditor : public AActor
{
	GENERATED_BODY()

public:
	AVisexBodyEditor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Body")
	TObjectPtr<UCineCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Body")
	TObjectPtr<UWidgetComponent> Panel;

	/** Camera distance from her, height of the look-at point above her feet, focal length. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Camera") float Distance = 420.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Camera") float LookHeight = 84.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Camera") float CameraHeight = 100.f;
	/** 28 mm on the default 23.76 mm filmback frames her head to toe at this distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Camera") float FocalLength = 28.f;
	/** Shift the camera so she stands right of centre and the panel has the left. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Camera") float SideShift = 70.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Camera") float BlendTime = 1.2f;
	/** Seconds the camera takes to ease from one row's framing to the next. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Camera") float CloseUpTime = 0.6f;
	/** f-stop of the full-body shot and of the close-ups. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Camera") float WideAperture = 2.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Camera") float CloseAperture = 4.f;

	/** Idle turntable, degrees per second (0 = she holds still until turned). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Turn") float TurnSpeed = 0.f;
	/** Right stick fully over, degrees per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Turn") float StickTurnSpeed = 150.f;
	/** How quickly the turn picks up and settles (higher = less inertia). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Turn") float TurnResponse = 6.f;
	/** Degrees per unit of mouse travel while the left button is held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Turn") float MouseTurn = 0.4f;
	/** How far she is turned from the row's own side, degrees. Kept from row to row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Turn") float Turn = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Panel") FVector PanelOffset = FVector(100.f, -25.f, 3.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Panel") float PanelYaw = 18.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Panel") float PanelScale = 0.031f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Panel") float PanelBrightness = 5.f;

	/** For testing without the menu: open as soon as the player exists. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body") bool bOpenOnPlay = false;

	UPROPERTY(BlueprintAssignable, Category = "Body")
	FVisexBodyEditorClosed OnClosed;

	UFUNCTION(BlueprintCallable, Category = "Body") void Open() { OpenPage(0); }
	UFUNCTION(BlueprintCallable, Category = "Body") void OpenWardrobe() { OpenPage(1); }
	/** 0 the Body page, 1 the Wardrobe. */
	UFUNCTION(BlueprintCallable, Category = "Body") void OpenPage(int32 InPage);
	UFUNCTION(BlueprintPure, Category = "Body") int32 GetPage() const { return Page; }
	UFUNCTION(BlueprintCallable, Category = "Body") void Close();
	UFUNCTION(BlueprintPure, Category = "Body") bool IsOpen() const { return bOpen; }

	/** Scriptable control (also what the keys call). */
	UFUNCTION(BlueprintCallable, Category = "Body") void Select(int32 Delta);
	/** Jump straight to a row of the current tab. */
	UFUNCTION(BlueprintCallable, Category = "Body") void SelectRow(int32 Row) { Select(Row - Selected); }
	/** Turn her back to the side the row shows (front, or her back for glutes and calves). */
	UFUNCTION(BlueprintCallable, Category = "Body") void FaceCamera();
	UFUNCTION(BlueprintCallable, Category = "Body") void Adjust(float Steps);
	UFUNCTION(BlueprintCallable, Category = "Body") void ResetSelected();
	UFUNCTION(BlueprintCallable, Category = "Body") void SwitchTab(int32 Delta);
	/** Apply the current tab: what she shows now becomes what she keeps (and is saved). */
	UFUNCTION(BlueprintCallable, Category = "Body") void ApplyTab();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	UVisexBodyComponent* BodyOf() const;
	UVisexBodyWidget* Widget() const;
	void Refresh();
	void BindKeys();
	void Up() { Select(-1); }
	void Down() { Select(1); }
	void Left() { Adjust(-1.f); }
	void Right() { Adjust(1.f); }
	void PrevTab() { SwitchTab(-1); }
	void NextTab() { SwitchTab(1); }
	void TryOpen();

	/** One framing: the point looked at, how far the camera stands, the lens. */
	struct FShot
	{
		FVector Look = FVector::ZeroVector;
		float Distance = 420.f;
		float Focal = 28.f;
		float Aperture = 2.8f;
	};
	FShot ShotNow(bool& bOutBack) const;
	void Reframe(bool bSnap);
	void PlaceCamera(const FShot& Shot);

	bool bOpen = false;
	bool bKeysBound = false;
	void StickNavigate(float DeltaSeconds);
	void StickPress(int32 Dir);
	int32 StickDir = 0;       // left stick held: 0 none, 1 up, 2 right, 3 down, 4 left
	float StickHeld = 0.f;
	float StickNext = 0.f;
	int32 Page = 0;            // 0 Body, 1 Wardrobe
	int32 Tab = 0;             // Body page tab: 0 body, 1 face, 2 hair
	int32 Selected = 0;
	int32 TabRow[3] = {0, 0, 0};
	bool bUnderConstruction = false;   // the locked-costume window is up
	/** The slider the selected row shapes, or INDEX_NONE (hair, costumes, APPLY). */
	int32 SliderHere() const;
	FVisexBodyProfile Applied;
	FRotator StartRotation;
	FShot From;
	FShot Current;
	float ShotTime = 1.f;
	FName ShotRow = NAME_None;
	FVector SmoothLook = FVector::ZeroVector;
	bool bBack = false;
	bool bRecentre = false;
	float SideYaw = 0.f;
	float TurnVelocity = 0.f;
	TWeakObjectPtr<AActor> PreviousView;
	FTimerHandle OpenTimer;
};
