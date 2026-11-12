#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CineCameraSettings.h"
#include "VisexMenuLayout.h"
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

	/** Tips the panel back (+) or forward (-), degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Panel")
	float PanelPitch = 0.f;

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

	/** Share of the stand-up after which she steps off the column and walks on her own, and the
	 *  stick reaches her; the camera is already circling. Timed off the stand-up clip's length. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Start", meta = (ClampMin = "0", ClampMax = "1"))
	float StandUpHandOver = 0.35f;

	/** Longest wait before she steps off (s), whatever the stand-up clip's length. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Start", meta = (ClampMin = "0"))
	float WalkOffMaxDelay = 0.5f;

	/** Her own pace walking off the column after Start (cm/s): slow and weighty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Start", meta = (ClampMin = "0"))
	float WalkOffSpeed = 140.f;

	/** Seconds she takes to come up to that pace. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Start", meta = (ClampMin = "0"))
	float WalkOffRamp = 0.9f;

	/** Seconds she keeps walking on her own once the camera has landed, easing to a stop
	 *  unless the player takes her. The stick takes over at any point without a pop. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Start", meta = (ClampMin = "0"))
	float WalkOffCoast = 1.5f;

	/** Bones the menu lens keeps in focus, first one found on her mesh wins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Camera")
	TArray<FName> FocusBones = {FName(TEXT("head")), FName(TEXT("spine_05"))};

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

	/** Main menu: a warm, soft sun-coloured key on her (lighting channel 1, so only she
	 *  gets it), so she isn't a black silhouette against the low sun behind her. It fades
	 *  in with the menu and out during Start's camera move. Placement as SpotPlacement:
	 *  yaw from the camera's side of her (deg), distance, height (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Key")
	FVector MenuKeyPlacement = FVector(-65.f, 260.f, 260.f);

	/** cd */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Key")
	float MenuKeyIntensity = 50.f;

	/** K: low-sun warm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Key")
	float MenuKeyTemperature = 3400.f;

	/** s to fade in or out */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Key")
	float MenuKeyBlendTime = 1.2f;

	/** Main menu: the blowing sand (Dunes_Blow) calms to a faint, slow drift, so only
	 *  fine grains slide across the ground. Play gets the full wind back during Start.
	 *  Share of the wind speed on the menu. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Wind", meta = (ClampMin = "0", ClampMax = "1"))
	float MenuWindSpeed = 0.12f;

	/** Blowing-sand opacity on the menu (Play uses the material's own, 0.45). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Wind", meta = (ClampMin = "0", ClampMax = "1"))
	float MenuWindOpacity = 0.1f;

	/** s from calm to full wind and back */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Wind")
	float MenuWindBlendTime = 3.f;

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

	/** From her feet to the camera, along the ground (cm). At 160 the 15 mm lens couldn't hold
	 *  her from toes to head: her feet fell under the frame's bottom edge. 200 keeps her whole,
	 *  toes included, with about a tenth of the frame to spare above and below. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Camera")
	float FloorCameraDistance = 200.f;

	/** Swings the floor camera round her from the level camera's side (deg). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Camera")
	float FloorCameraYaw = -60.f;

	/** Turns the floor camera off her (deg): negative puts her right of centre, leaving the left for the menu. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Camera")
	float FloorCameraPan = -16.f;

	/** Where the floor camera points, above her feet (cm): 70 centres her, feet to head, at 16:9.
	 *  Wider windows keep the same height of view and narrower ones show more, so the feet stay in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Camera")
	float FloorCameraLookHeight = 70.f;

	/** Menu lens focal length (mm): wide, low by her feet; 0 keeps the camera's own. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Camera")
	float MenuFocalLength = 15.f;

	/** Menu depth of field: the lens is simulated on a sensor this many times larger, with the
	 * focal length scaled to keep the framing, so the background blurs like a big-format camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Camera", meta = (ClampMin = "1"))
	float MenuSensorScale = 3.f;

	/** The menu fills the window like the play camera, with no letterbox, so the Start move lands
	 * without the bars snapping off. A narrower window than the shot shortens the lens to keep the
	 * shot's full width, panel included, and shows more above and below; a wider one widens the
	 * lens to keep her whole height and shows more at the sides. Off: the
	 * cine camera's own 16:9 frame, letterboxed on any other window. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Camera")
	bool bMenuFillsWindow = true;

	/** Menu aperture (f-stop) on that sensor; 0 keeps the camera's own. f/2.8 holds about ±14 cm
	 * around the focus at menu distance, so she stays crisp while the steps and colonnade behind
	 * her fall soft (f/8, about ±40 cm, kept the background too sharp). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Camera")
	float MenuAperture = 2.8f;

	/** Seconds the Start move blends into the play camera when it lands (0 = a cut). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Camera", meta = (ClampMin = "0"))
	float LandingBlendTime = 0.6f;

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

	/** While Settings is open she climbs onto the drum and perches on it, one foot on the sand, the
	 *  other leg stretched toward the lens, sole to the camera (Tools/char_menu_sit.py, Tema's sketch
	 *  2026-11-12), and the menu camera eases back a little so her raised foot fits the frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Settings")
	bool bSitForSettings = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Settings")
	TSoftObjectPtr<class UAnimSequence> SettingsSit = TSoftObjectPtr<class UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/Menu/A_Menu_Sit_Settings.A_Menu_Sit_Settings")));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Settings")
	TSoftObjectPtr<class UAnimSequence> SettingsSitDown = TSoftObjectPtr<class UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/Menu/A_Menu_Sit_Settings_Down.A_Menu_Sit_Settings_Down")));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Settings")
	TSoftObjectPtr<class UAnimSequence> SettingsSitUp = TSoftObjectPtr<class UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/Menu/A_Menu_Sit_Settings_Up.A_Menu_Sit_Settings_Up")));
	/** The Settings shot (Tema 2026-11-12): the menu camera dollies in along the ground toward her,
	 *  at its own low height, by SettingsCamOffset (cm, in the menu camera's heading: X ahead,
	 *  Y right, Z up; Z 0 keeps the height), turns onto SettingsCamAim (her mesh space: X her
	 *  left, Y forward, Z up) plus a look-up and a pan (deg), and its lens opens to
	 *  SettingsFocalLength on the way. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Settings")
	FVector SettingsCamOffset = FVector(110.f, 0.f, 0.f);
	/** The Settings lens (mm), on the same scale as MenuFocalLength (the camera's own Super 35
	 *  size filmback; MenuSensorScale multiplies both alike): 6 mm is a ~126 deg horizontal view,
	 *  about a 9 mm on full frame. The panel moves in with it so it keeps its size on screen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Settings", meta = (ClampMin = "0"))
	float SettingsFocalLength = 6.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Settings")
	FVector SettingsCamAim = FVector(0.f, 10.f, 75.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Settings")
	float SettingsCamLookUp = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Settings")
	float SettingsCamPan = -16.f;
	/** Focus slides this far (0..1) from her toward the raised foot's ball (SettingsFocusBone). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Settings")
	FName SettingsFocusBone = TEXT("ball_l");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Settings", meta = (ClampMin = "0", ClampMax = "1"))
	float SettingsFocusMix = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Settings", meta = (ClampMin = "0.1"))
	float SettingsCamTime = 2.2f;
	/** 0 the menu framing, 1 the Settings framing; eased in Tick. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Menu|Settings")
	float SettingsCamAlpha = 0.f;

	/** On the menu she leans on a fallen column, spawned behind her at Play (no level edit). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean")
	bool bLeanOnColumn = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean")
	TSoftObjectPtr<class UStaticMesh> LeanColumnMesh = TSoftObjectPtr<class UStaticMesh>(FSoftObjectPath(TEXT("/Game/VisEx/Geo/SM_LeanColumn_HD.SM_LeanColumn_HD")));

	/** SM_LeanColumn_HD's outer radius at scale 1 (cm): where the worn arrises are, the stone she
	 * actually touches. The drum is scaled so this lands on the radius FitLeanColumn solves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean")
	float LeanColumnMeshRadius = 46.4f;

	/** The seat the lean clip (Tools/char_menu_lean.py) was posed on, in her mesh space (cm, at
	 * her authored body scale): pelvis bone height above her feet, how far in front of her root
	 * (negative: behind), how far the pelvis bone sits off the stone, and the angle from the
	 * drum's top toward her where it sits. FitLeanColumn keeps this seat for any body. Leg length
	 * doesn't move it: in the lean her pelvis and hands hold still whatever the legs (measured). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean")
	FVector LeanSeat = FVector(85.22f, -31.51f, 8.31f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean")
	float LeanSeatAngle = 46.f;

	/** Her mesh scale when the lean clip was posed (Height 166). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean")
	float LeanAuthoredScale = 0.935f;

	/** How far the drum's underside sinks into the sand (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean")
	float LeanColumnSettle = 5.2f;

	/** Re-place the drum for her current body (Height scales her seat): her seat meets the stone at
	 * the angle the lean clip was posed for, the drum stays settled in the sand. Called on every
	 * ShowMenu (so after the Body editor closes and after a profile loads); callable any time. */
	UFUNCTION(BlueprintCallable, Category = "Menu|Lean")
	void FitLeanColumn();

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

	/** The drum's outer radius as placed (cm), measured by traces over its top: the pivot sits
	 * this far above the drum's axis. The spawned actor carries the tag LeanColumn. */
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

	/** Small things she does while she leans and nobody has picked anything (Tools/char_menu_beats.py):
	 *  a glance, a weight shift, playing with her toes. Each starts and ends in the lean. One plays
	 *  after a rest of IdleBeatRest seconds (min, max), never the same one twice running. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean")
	TArray<TSoftObjectPtr<class UAnimSequence>> IdleBeats = {
		TSoftObjectPtr<class UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/Menu/A_Menu_Beat_Glance.A_Menu_Beat_Glance"))),
		TSoftObjectPtr<class UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/Menu/A_Menu_Beat_Shift.A_Menu_Beat_Shift"))),
		TSoftObjectPtr<class UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/Menu/A_Menu_Beat_Toes.A_Menu_Beat_Toes")))};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean")
	FVector2D IdleBeatRest = FVector2D(8.f, 20.f);

	/** Plays one idle beat now if she is leaning: Index into IdleBeats, or -1 for a random one. */
	UFUNCTION(BlueprintCallable, Category = "Menu|Lean")
	void PlayIdleBeat(int32 Index = -1);

	/** The lean clips once loaded, held so GC can't free one while a timer waits to play it
	 * (a raw pointer caught in the lean-down timer crashed Shipping 0298). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UAnimSequence>> LoadedLean;

	/** Body/Wardrobe: after the push-off she walks clear of the column, far enough that a full
	 *  turn on the page keeps every part of her this far off the stone (cm), even with the
	 *  sliders at their ends. Measured from her bones when she sets off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean", meta = (ClampMin = "0"))
	float BodyClearance = 30.f;

	/** Flesh, fingertips and outfit past her outermost bones (cm, at her scale). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean", meta = (ClampMin = "0"))
	float BodyReachMargin = 10.f;

	/** How much of her reach the width sliders (Shoulders, Hips) widen, up to their ends. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean", meta = (ClampMin = "0", ClampMax = "1"))
	float BodyReachWidthShare = 0.8f;

	/** Her pace on those steps, out and back (cm/s): slow and weighty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean", meta = (ClampMin = "10"))
	float BodyStepSpeed = 100.f;

	/** Her pace backing onto the column after the page closes (cm/s): a few slow steps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean", meta = (ClampMin = "10"))
	float BodyStepBackSpeed = 70.f;

	/** Her farthest part from her own axis at the sliders' ends (cm), and the gap from her
	 *  axis to the stone she stopped at: last measured on the way to the Body page. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Menu|Lean")
	float BodyReach = 0.f;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Menu|Lean")
	float BodyStepTarget = 0.f;

	/** From a point on the ground plane to the drum's surface, horizontally (cm; <0 inside). */
	UFUNCTION(BlueprintPure, Category = "Menu|Lean")
	float ColumnGapAt(FVector Point) const;

	/** Her farthest bone from her axis, plus BodyReachMargin (cm), as she stands now. */
	UFUNCTION(BlueprintPure, Category = "Menu|Lean")
	float MeasureReach() const;

	/** Hides the menu and opens the body page; the menu comes back when it closes. */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void OpenBody() { OpenCharacterPage(0); }

	/** Hides the menu and opens the wardrobe (the dressing room). */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void OpenWardrobe() { OpenCharacterPage(1); }

	/** The block's place, perspective, spacing and logo, as the layout editor (§) sets them. */
	const FVisexMenuLayout& GetLayout() const { return Layout; }
	/** The layout as built (the class's own values, before any save): Reset goes back to it. */
	const FVisexMenuLayout& GetBuiltInLayout() const { return BuiltInLayout; }
	void ApplyLayout(const FVisexMenuLayout& InLayout);

	/** Open or close the menu layout editor (§, start menu only). */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void ToggleLayoutEditor();

	/** The menu's rows: Start, Body, Wardrobe, Photo mode, Settings, Quit. */
	enum : int32 { ItemStart = 0, ItemBody, ItemWardrobe, ItemPhoto, ItemSettings, ItemQuit };

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
	void TickMenuKey(float DeltaSeconds);
	static void JoinChannel1(APawn* Pawn);
	float MenuKeyAlpha = 0.f;
	UPROPERTY(Transient)
	TObjectPtr<AActor> MenuKey;

	void TickMenuWind(float DeltaSeconds);
	float WindCalm = -1.f;      // 1 = menu calm, 0 = full wind; <0 until first tick
	float WindClock = 0.f;      // the wind's own time, slowed while calm
	bool bWindSearched = false;
	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInstanceDynamic> WindMID;

	FTransform CameraBase;
	FTransform LevelCamera;
	void SetCornerShade(float Weight);
	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInstanceDynamic> CornerShadeMID;
	float CornerShadeBase = 0.93f;
	bool bHaveLevelCamera = false;
	bool bHaveBase = false;
	bool bOrbiting = false;
	float OrbitT = 0.f;
	float OrbitA0 = 0.f, OrbitR0 = 0.f, OrbitZ0 = 0.f;
	float MenuFocal = 0.f, MenuFStop = 0.f;
	FCameraFilmbackSettings MenuFilmback;
	bool bMenuConstrain = true;   // the cine camera's own aspect-ratio setting
	float MenuViewAspect = 0.f;   // the window shape the menu lens was last set for
	bool bHaveLens = false;
	/** The menu's focal length on the scaled sensor, widened for a window wider than the shot. */
	float MenuLensFocal() const;
	void SetMenuFocal(class UCineCameraComponent* Lens, float Focal);
	float ViewAspect() const;
	bool bOpen = false;
	bool bInputBound = false;
	bool bStickHeld = false;
	bool bPlaying = false;   // handed back to the player after Start
	// Start: she walks off the column on her own until the player takes her
	void BeginWalkOff();
	void TickWalkOff(float DeltaSeconds);
	float WalkOffClock = -1.f;    // seconds since she stepped off; <0 when not walking off
	float WalkOffWeight = 0.f;    // her own walk's share, falls as the stick takes over
	float LandedFor = -1.f;       // seconds since the camera landed; <0 before
	FVector WalkOffDir = FVector::ForwardVector;
	// focus: on her head every frame while the menu camera is the view
	void TickFocus();
	FQuat OrbitAim0 = FQuat::Identity;   // the menu's framing offset at Start, faded out early
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
	// sitting for Settings: down and up, the feet camera, and their own timer
	void SitForSettings();
	void StandFromSettings();
	void PlayLoopLater(class UAnimSequence* Clip, float Delay, bool bThenBeats);
	FTransform SettingsCameraTransform() const;
	float SettingsCamEase() const { const float X = FMath::Clamp(SettingsCamAlpha, 0.f, 1.f); return X * X * X * (X * (6.f * X - 15.f) + 10.f); }
	bool bSitting = false;
	FTimerHandle SitTimer;
	// idle beats: the rest timer, the beat's end, and the last one played
	void ScheduleIdleBeat();
	void EndIdleBeat();
	FTimerHandle BeatTimer;
	int32 LastBeat = INDEX_NONE;
	void OpenCharacterPage(int32 InPage);
	void PlacePanel();
	void PushLayoutToWidget();
	FVisexMenuLayout Layout;
	FVisexMenuLayout BuiltInLayout;
	bool bHaveLayout = false;
	UPROPERTY(Transient) TObjectPtr<class AVisexMenuLayoutEditor> LayoutEditor;
	void OpenBodyNow();
	int32 CharacterPage = 0;   // the page OpenBodyNow opens: 0 body, 1 wardrobe
	// Body/Wardrobe: she steps clear of the column first, and back onto it after
	void BeginStepOut();
	void BeginStepBack(float Wait, int32 Select);
	void TickStep(float DeltaSeconds);
	void EndStep();
	int32 StepDir = 0;           // +1 walking out, -1 backing onto the column, 0 still
	float StepClock = 0.f;
	FVector StepAway = FVector::ForwardVector;   // on the ground, from the drum toward her
	FVector StepHome = FVector::ZeroVector;      // where she backs to: the stand-in's spot
	int32 StepSelect = 0;        // the row the menu returns on
	float StepMinTime = 0.f;     // the camera's blend home: the menu waits for it too
	bool bSteppedOut = false;
	bool bStepOrient = true;     // her movement's own orient-to-movement, put back after

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
