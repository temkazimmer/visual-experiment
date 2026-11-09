#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VisexMenuDirector.generated.h"

class ACameraActor;
class UWidgetComponent;
class UVisexMenuWidget;

/**
 * Main menu as a scene state, not a separate level. On play the view holds MenuCamera
 * (slow drift), the player's character stands where StandIn is, and the menu floats as
 * a real 3D panel in front of the camera, turned toward the figure. Start blends the
 * camera onto the character and hands control back.
 *
 * Place BP_MenuDirector (a Blueprint child) in the level; everything here is editable
 * there, and the events below can be implemented in its graph.
 */
UCLASS(Blueprintable)
class VISEX_API AVisexMenuDirector : public AActor
{
	GENERATED_BODY()

public:
	AVisexMenuDirector();

	/** The shot the game opens on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	TObjectPtr<ACameraActor> MenuCamera;

	/** Editor stand-in marking where the character stands; hidden in game. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	TObjectPtr<AActor> StandIn;

	/** Untick to skip straight to play while testing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	bool bShowMenuOnPlay = true;

	/** Panel position in camera space: forward, right, up (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Panel")
	FVector PanelOffset = FVector(100.f, -22.f, 3.f);

	/** How far the panel turns toward the figure (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Panel")
	float PanelYaw = 24.f;

	/** World cm per panel pixel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Panel")
	float PanelScale = 0.032f;

	/** Unlit panel brightness against the scene's exposure. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Panel")
	float PanelBrightness = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Camera")
	float DriftDegrees = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Camera")
	float DriftDistance = 4.f;

	/** Seconds per drift cycle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Camera")
	float DriftPeriod = 16.f;

	/** Where the play camera ends up after Start, relative to the character's facing:
	 * 0 = straight behind her. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Camera")
	float StartCameraYaw = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Camera")
	float StartCameraPitch = -12.f;

	/** Seconds for the camera to swing from the menu shot onto the character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Camera")
	float BlendToPlayTime = 2.5f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Menu")
	TObjectPtr<UWidgetComponent> Panel;

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void ShowMenu();

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void StartGame();

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void MoveSelection(int32 Delta);

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void Confirm();

	UFUNCTION(BlueprintPure, Category = "Menu")
	bool IsMenuOpen() const { return bOpen; }

	UFUNCTION(BlueprintPure, Category = "Menu")
	UVisexMenuWidget* GetMenuWidget() const;

	/** Item 0 starts, 1 is photo mode, 2 quits. Override in Blueprint to change. */
	UFUNCTION(BlueprintNativeEvent, Category = "Menu")
	void OnItemChosen(int32 Index);

	UFUNCTION(BlueprintImplementableEvent, Category = "Menu")
	void OnMenuShown();

	UFUNCTION(BlueprintImplementableEvent, Category = "Menu")
	void OnGameStarted();

	/** Photo mode isn't built yet: for now it starts the game, then fires this. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Menu")
	void OnPhotoMode();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	void TrySetup();
	void PlacePawn();
	void Up() { MoveSelection(-1); }
	void Down() { MoveSelection(1); }
	void HandBack();

	FTransform CameraBase;
	bool bOpen = false;
	bool bInputBound = false;
	float Clock = 0.f;
	float FadeOut = -1.f;
	FTimerHandle SetupTimer;
	FTimerHandle HandBackTimer;
};
