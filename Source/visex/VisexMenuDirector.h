#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CineCameraSettings.h"
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

	/** Every scalability group at Cinematic (4) when Play starts: max shadows, lighting, GI, reflections. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	bool bMaxQualityOnPlay = true;

	/** Untick to skip straight to play while testing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	bool bShowMenuOnPlay = true;

	/** Panel position in camera space: forward, right, up (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Panel")
	FVector PanelOffset = FVector(100.f, -34.f, 6.f);

	/** How far the panel turns (deg): negative sends its right end away, so the menu recedes toward her. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Panel")
	float PanelYaw = -55.f;

	/** Tilts the panel in the frame (deg): positive lifts its right end, so the menu rises toward her
	 * on a diagonal (Persona-style perspective). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Panel")
	float PanelRoll = 4.f;

	/** World cm per panel pixel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Panel")
	float PanelScale = 0.048f;

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

	/** Back from Character or Photo mode: a dip to black while she returns to the column (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean")
	float ReturnFadeTime = 0.3f;

	/** Share of the stand-up after which the player can walk; the camera keeps circling meanwhile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Start", meta = (ClampMin = "0", ClampMax = "1"))
	float StandUpHandOver = 0.6f;

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

	/** What the Photo mode item opens; spawned on first use. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Photo")
	TSubclassOf<class AVisexPhotoMode> PhotoModeClass;

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

	/** Share of the studio look photo mode uses (the Character page uses all of it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Studio", meta = (ClampMin = "0", ClampMax = "1"))
	float PhotoStudioStrength = 0.5f;

	/** How much of the menu camera's distance to her is kept: 0.5 frames her twice as large.
	 * The camera moves along the line to CameraAimHeight on her, so she stays where she is in frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Camera", meta = (ClampMin = "0.1", ClampMax = "1"))
	float CameraPullIn = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Camera")
	float CameraAimHeight = 115.f;

	/** Menu shot from the sand by her feet, looking up at her, instead of the level camera's
	 * spot (which only sets which side of her it sits on). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Camera")
	bool bFloorCamera = true;

	/** Lens height above the sand (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Camera")
	float FloorCameraHeight = 14.f;

	/** From her feet to the camera, along the ground (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Camera")
	float FloorCameraDistance = 160.f;

	/** Swings the floor camera round her from the level camera's side (deg). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Camera")
	float FloorCameraYaw = -60.f;

	/** Turns the floor camera off her (deg): negative puts her right of centre, leaving the left for the menu. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Camera")
	float FloorCameraPan = -16.f;

	/** Where the floor camera points, above her feet (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Camera")
	float FloorCameraLookHeight = 80.f;

	/** Menu lens focal length (mm): wide, low by her feet; 0 keeps the camera's own. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Camera")
	float MenuFocalLength = 15.f;

	/** Menu depth of field: the lens is simulated on a sensor this many times larger, with the
	 * focal length scaled to keep the framing, so the background blurs like a big-format camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Camera", meta = (ClampMin = "1"))
	float MenuSensorScale = 3.f;

	/** Menu aperture (f-stop) on that sensor; 0 keeps the camera's own. f/8 keeps all of her sharp
	 * (about ±40 cm around the focus at menu distance) while the far haze still goes soft. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Camera")
	float MenuAperture = 8.f;

	/** Panel material override (e.g. MI_MenuDifference, which draws the text as the difference
	 * from the scene behind it). Unset = the stock widget material: plain white over a soft shade. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Panel")
	TSoftObjectPtr<class UMaterialInterface> PanelMaterial = TSoftObjectPtr<class UMaterialInterface>(FSoftObjectPath(TEXT("/Game/VisEx/UI/M_MenuPanel.M_MenuPanel")));

	/** Post-process on the menu camera: a soft darkening from the screen's top-left corner so the
	 * white menu reads on bright sky. Fades out during the Start move. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Panel")
	TSoftObjectPtr<class UMaterialInterface> CornerShade = TSoftObjectPtr<class UMaterialInterface>(FSoftObjectPath(TEXT("/Game/VisEx/UI/M_MenuCornerShade.M_MenuCornerShade")));

	/** Re-frame the menu shot from the current settings (for tuning while it is open). */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void ReframeMenu();

	/** Options / O in play: back to the menu; in the menu: back to play. */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void ToggleMenu();

	/** The Settings page (VisexSettings.h): open, close (saves), step the selected row, switch tab. */
	UFUNCTION(BlueprintCallable, Category = "Menu|Settings") void OpenSettings();
	UFUNCTION(BlueprintCallable, Category = "Menu|Settings") void CloseSettings();
	UFUNCTION(BlueprintCallable, Category = "Menu|Settings") void SettingsStep(int32 Dir);
	UFUNCTION(BlueprintCallable, Category = "Menu|Settings") void SettingsTab(int32 Dir);

	/** On the menu she leans on a fallen column, spawned behind her at Play (no level edit). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean")
	bool bLeanOnColumn = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean")
	TSoftObjectPtr<class UStaticMesh> LeanColumnMesh = TSoftObjectPtr<class UStaticMesh>(FSoftObjectPath(TEXT("/Game/VisEx/Geo/SM_Drum_0.SM_Drum_0")));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean")
	float LeanColumnRadius = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean")
	float LeanColumnLength = 320.f;

	/** From her feet back to the column's near surface (cm); set to match the lean clip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean")
	float LeanColumnGap = 24.f;

	/** How far along the column she sits, from its middle toward its left end (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean")
	float LeanColumnShift = 60.f;

	/** The stone's measured surface radius at Play (cm): the collision capsule's radius, and the
	 * pivot sits this far above the drum's axis. The spawned actor carries the tag LeanColumn. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Menu|Lean")
	float LeanColumnSurfaceRadius = 0.f;

	UFUNCTION(BlueprintPure, Category = "Menu|Lean")
	AActor* GetLeanColumn() const { return LeanColumn; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean")
	TSoftObjectPtr<class UAnimSequence> LeanIdle = TSoftObjectPtr<class UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/Menu/A_Menu_Lean.A_Menu_Lean")));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean")
	TSoftObjectPtr<class UAnimSequence> LeanUp = TSoftObjectPtr<class UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/Menu/A_Menu_Lean_Up.A_Menu_Lean_Up")));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean")
	TSoftObjectPtr<class UAnimSequence> LeanDown = TSoftObjectPtr<class UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/Menu/A_Menu_Lean_Down.A_Menu_Lean_Down")));

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

	/** Fired when photo mode opens. */
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

	// the Settings page: same panel, other rows. Page 0 is the main menu, 1 Graphics, 2 Visuals.
	int32 Page = 0;
	/** The panel redraws its (large) render target only when it changes: frames left to redraw. */
	int32 PanelDirty = 0;
	void MarkPanel(int32 Frames = 2) { PanelDirty = FMath::Max(PanelDirty, Frames); }
	bool bStickXHeld = false;
	void ShowSettingsPage(int32 Select);
	void Left() { SettingsStep(-1); }
	void Right() { SettingsStep(1); }
	void PrevTab() { SettingsTab(-1); }
	void NextTab() { SettingsTab(1); }
	void Back() { if (bOpen && Page > 0) { CloseSettings(); } }
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
	FTransform LevelCamera;
	void SetCornerShade(float Weight);
	bool bHaveLevelCamera = false;
	bool bHaveBase = false;
	bool bOrbiting = false;
	float OrbitT = 0.f;
	float OrbitA0 = 0.f, OrbitR0 = 0.f, OrbitZ0 = 0.f;
	float MenuFocal = 0.f, MenuFStop = 0.f;
	FCameraFilmbackSettings MenuFilmback;
	bool bHaveLens = false;
	bool bOpen = false;
	bool bInputBound = false;
	bool bStickHeld = false;
	bool bPlaying = false;   // handed back to the player after Start
	bool bWalkOffArmed = false;   // player has her mid stand-up: first stick push ends the clip
	FVector MenuAim = FVector::ZeroVector;

	// leaning on the column
	void SpawnLeanColumn();
	void Lean(bool bFromStanding);
	/** Plays the push-off; returns its length in seconds (0 if she wasn't leaning). */
	float StandUp();
	void SetColumnBlocksPawn(bool bBlock);
	class UAnimInstance* GetPawnAnim() const;
	bool bLeaning = false;
	bool bHaveLeaned = false;
	void OpenBodyNow();

	// photo mode
	void OpenPhoto();
	void OpenPhotoNow();
	UFUNCTION()
	void HandlePhotoClosed();

	void BackToMenu(float BlendTime, int32 Select);
	UPROPERTY(Transient)
	TObjectPtr<class AVisexPhotoMode> PhotoMode;
	void SetSceneStripped(bool bStrip);
	TArray<TWeakObjectPtr<AActor>> Stripped;
	float StudioStrength = 1.f;
	UPROPERTY(Transient)
	TObjectPtr<AActor> LeanColumn;
	FTimerHandle StandTimer;
	float Clock = 0.f;
	float FadeOut = -1.f;
	FTimerHandle SetupTimer;
	FTimerHandle HandBackTimer;
	FTimerHandle BodyTimer;
	UPROPERTY(Transient)
	TObjectPtr<AVisexBodyEditor> BodyEditor;
};
