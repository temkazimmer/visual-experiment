#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VisexBodyEditor.generated.h"

class UCineCameraComponent;
class UWidgetComponent;
class UVisexBodyWidget;
class UVisexBodyComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVisexBodyEditorClosed);

/**
 * The body page. Open() moves the view to a closer full-body camera in front of the
 * player character, who turns slowly on the spot (turntable), with the slider panel
 * on the left. Up/down picks a slider, left/right shapes her live, triangle resets
 * the slider, circle saves and closes (OnClosed fires; the menu shows itself again).
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
	void TryOpen();

	bool bOpen = false;
	bool bKeysBound = false;
	int32 Selected = 0;
	FRotator StartRotation;
	TWeakObjectPtr<AActor> PreviousView;
	FTimerHandle OpenTimer;
};
