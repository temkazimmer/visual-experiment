#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VisexMenuDirector.generated.h"

class ACameraActor;
class UWidgetComponent;
class UVisexMenuWidget;
class AVisexBodyEditor;
class USpotLightComponent;
class URectLightComponent;
class ADirectionalLight;
class ASkyLight;
class APostProcessVolume;

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

	/** Seconds for the Start move: the menu camera circles low around her and lands
	 * exactly on the play camera behind her. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Start")
	float BlendToPlayTime = 3.5f;

	/** Which way round her the camera circles: 1 or -1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Start")
	int32 OrbitDirection = 1;

	/** Point the camera watches during the move, above her feet (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Start")
	float OrbitLookHeight = 120.f;

	/** Share of the move spent low by the ground before rising to the play camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Start", meta = (ClampMin = "0", ClampMax = "1"))
	float OrbitStayLow = 0.55f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Menu")
	TObjectPtr<UWidgetComponent> Panel;

	/** What the Body item opens; spawned on first use. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Body")
	TSubclassOf<AVisexBodyEditor> BodyEditorClass;

	/** Character page lighting: the desert goes nearly dark and a theatrical spot plus a
	 * rim light pick her out, so body shape, clothes and motion read clearly. The two lights
	 * use lighting channel 1 and only reach her (and whatever she wears). */
	/** Seconds to fade the studio look in or out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Studio")
	float StudioBlendTime = 1.2f;

	/** How much of the sun, the sky light and the vignette stay on the page. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Studio")
	float StudioSunScale = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Studio")
	float StudioSkyScale = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Studio")
	float StudioVignette = 0.8f;

	/** Spot placement around her, relative to the camera's side of her: yaw (deg), distance, height (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Studio")
	FVector SpotPlacement = FVector(-25.f, 140.f, 330.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Studio")
	float SpotIntensity = 140.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Studio")
	FVector RimPlacement = FVector(165.f, 200.f, 200.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Studio")
	float RimIntensity = 35.f;

	/** Hides the menu and opens the body page; the menu comes back when it closes. */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void OpenBody();

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

	/** Item 0 starts, 1 opens the body page, 2 is photo mode, 3 quits. Override in Blueprint to change. */
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
	void TickOrbit(float DeltaSeconds);
	UFUNCTION()
	void HandleBodyClosed();
	class UCineCameraComponent* GetLens() const;
	void TickStudio(float DeltaSeconds);
	void PlaceStudioLight(class ULocalLightComponent* Light, const FVector& Placement, const FVector& Feet, float CamYaw, float AimHeight) const;

	float StudioAlpha = 0.f;
	bool bStudio = false;
	bool bHaveStudioBase = false;
	float BaseSun = 0.f, BaseSky = 0.f, BaseVignette = 0.f;
	bool bBaseVignetteOverride = false;
	TWeakObjectPtr<ADirectionalLight> Sun;
	TWeakObjectPtr<ASkyLight> Sky;
	TWeakObjectPtr<APostProcessVolume> Post;
	// spawned on first use: light components on a level-placed director didn't render
	UPROPERTY(Transient)
	TObjectPtr<AActor> StudioSpot;
	UPROPERTY(Transient)
	TObjectPtr<AActor> StudioRim;
	void SpawnStudioLights();

	FTransform CameraBase;
	bool bHaveBase = false;
	bool bOrbiting = false;
	float OrbitT = 0.f;
	float OrbitA0 = 0.f, OrbitR0 = 0.f, OrbitZ0 = 0.f;
	float MenuFocal = 0.f, MenuFStop = 0.f;
	bool bHaveLens = false;
	bool bOpen = false;
	bool bInputBound = false;
	float Clock = 0.f;
	float FadeOut = -1.f;
	FTimerHandle SetupTimer;
	FTimerHandle HandBackTimer;
	FTimerHandle BodyTimer;
	UPROPERTY(Transient)
	TObjectPtr<AVisexBodyEditor> BodyEditor;
};
