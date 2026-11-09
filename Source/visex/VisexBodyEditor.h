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
 * The Character page. Open() moves the view to a closer full-body camera in front of the
 * player character, who turns slowly on the spot (turntable), with the panel on the left.
 * Two tabs, left to right: BODY (sliders) and DRESS (outfits). L1/R1 (Q/E, Tab) switch
 * tabs, up/down picks a row, left/right shapes her (body), picking an outfit tries it on.
 * Every change shows on her at once; CROSS (Enter, Space) applies that tab and saves it.
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
	/** Turntable speed, degrees per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Camera") float TurnSpeed = 14.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Panel") FVector PanelOffset = FVector(100.f, -25.f, 3.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Panel") float PanelYaw = 18.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Panel") float PanelScale = 0.031f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Panel") float PanelBrightness = 5.f;

	/** For testing without the menu: open as soon as the player exists. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body") bool bOpenOnPlay = false;

	UPROPERTY(BlueprintAssignable, Category = "Body")
	FVisexBodyEditorClosed OnClosed;

	UFUNCTION(BlueprintCallable, Category = "Body") void Open();
	UFUNCTION(BlueprintCallable, Category = "Body") void Close();
	UFUNCTION(BlueprintPure, Category = "Body") bool IsOpen() const { return bOpen; }

	/** Scriptable control (also what the keys call). */
	UFUNCTION(BlueprintCallable, Category = "Body") void Select(int32 Delta);
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

	bool bOpen = false;
	bool bKeysBound = false;
	int32 Tab = 0;
	int32 Selected = 0;
	int32 TabRow[2] = {0, 0};
	FVisexBodyProfile Applied;
	FRotator StartRotation;
	TWeakObjectPtr<AActor> PreviousView;
	FTimerHandle OpenTimer;
};
