#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CineCameraSettings.h"
#include "Animation/AnimEnums.h"
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
	FVector SettingsCamOffset = FVector(150.f, 0.f, 0.f);
	/** The Settings lens (mm), on the same scale as MenuFocalLength (the camera's own Super 35
	 *  size filmback; MenuSensorScale multiplies both alike): 6 mm is a ~126 deg horizontal view,
	 *  about a 9 mm on full frame. The panel keeps its exact on-screen shape (ApplyPanelLens). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Settings", meta = (ClampMin = "0"))
	float SettingsFocalLength = 6.f;
	/** The panel's material while the Settings lens is wide: M_MenuPanel plus a per-vertex depth
	 *  scale that keeps the panel's on-screen shape exact (Tools/menu_panel_lens.py rebuilds it from
	 *  M_MenuPanel: rerun it when M_MenuPanel changes). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Settings")
	TSoftObjectPtr<class UMaterialInterface> SettingsPanelMaterial = TSoftObjectPtr<class UMaterialInterface>(FSoftObjectPath(TEXT("/Game/VisEx/UI/M_MenuPanelLens.M_MenuPanelLens")));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Settings")
	FVector SettingsCamAim = FVector(0.f, -20.f, 80.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Settings")
	float SettingsCamLookUp = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Settings")
	float SettingsCamPan = -17.f;   // her slightly right of centre (Tema)
	/** Focus slides this far (0..1) from her toward the raised foot's ball (SettingsFocusBone). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Settings")
	FName SettingsFocusBone = TEXT("ball_l");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Settings", meta = (ClampMin = "0", ClampMax = "1"))
	float SettingsFocusMix = 0.f;   // at 6 mm and this close, f/2.8 cannot hold her face and a foot: keep her face
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
	/** FitLeanColumn has placed the drum for her body: the launch cover waits on it. */
	bool HasFittedColumn() const { return bColumnFitted; }
	/** Keeps the handheld drift at its start, the frame the intro film ends on, while the launch
	 *  cover holds and cross-fades. */
	void HoldDrift() { Clock = 0.f; }
	/** The 3D menu panel's opacity: the launch cover fades it in after the film's last frame. */
	void SetPanelOpacity(float Alpha);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean")
	TSoftObjectPtr<class UAnimSequence> LeanIdle = TSoftObjectPtr<class UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/Menu/A_Menu_Lean.A_Menu_Lean")));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean")
	TSoftObjectPtr<class UAnimSequence> LeanUp = TSoftObjectPtr<class UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/Menu/A_Menu_Lean_Up.A_Menu_Lean_Up")));

	/** The menu's walk: a genuinely slow, planted walk with root motion (the speed it is played at
	 *  follows her capsule). Starts at MenuWalkClipStart, in double support. */
	UPROPERTY(EditAnywhere, Category = "Menu|Walk")
	TSoftObjectPtr<class UAnimSequence> MenuWalkClip = TSoftObjectPtr<class UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/PhotoSeq/A_PhotoSeq_Walk.A_PhotoSeq_Walk")));
	UPROPERTY(EditAnywhere, Category = "Menu|Walk")
	float MenuWalkClipStart = 0.45f;

	/** The root-motion walks (visex.Menu.RootMotion): her capsule follows these clips, start, loop
	 *  and stop, each handed to the next over MoveBlend with the planted foot held where it is. */
	UPROPERTY(EditAnywhere, Category = "Menu|Walk")
	TSoftObjectPtr<class UAnimSequence> WalkStart = TSoftObjectPtr<class UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/Menu/A_Menu_Walk_Start.A_Menu_Walk_Start")));
	UPROPERTY(EditAnywhere, Category = "Menu|Walk")
	TSoftObjectPtr<class UAnimSequence> WalkLoop = TSoftObjectPtr<class UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/Menu/A_Menu_Walk_Loop.A_Menu_Walk_Loop")));
	UPROPERTY(EditAnywhere, Category = "Menu|Walk")
	TSoftObjectPtr<class UAnimSequence> WalkStop = TSoftObjectPtr<class UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/Menu/A_Menu_Walk_Stop.A_Menu_Walk_Stop")));
	/** WalkStart's clip time the walk starts at: her left foot planted, the right about to lift. */
	UPROPERTY(EditAnywhere, Category = "Menu|Walk")
	float WalkStartFrom = 1.0f;
	/** The lean-up's clip time the walk takes over: halfway up, her left foot still where it
	 *  rested and the right already lifting, so the push-off is the walk's first step (later, the
	 *  lean-up slid her left foot 12 cm and stepped the right back before it stepped forward). */
	UPROPERTY(EditAnywhere, Category = "Menu|Walk")
	float LeanUpHandOff = 0.45f;
	/** WalkStart's clip time the lean-up hands to: left planted, the right foot lifting. */
	UPROPERTY(EditAnywhere, Category = "Menu|Walk")
	float WalkStartFromUp = 0.95f;
	/** The lean-up's still first frames, skipped when she stands up into a walk (clip s). */
	UPROPERTY(EditAnywhere, Category = "Menu|Walk")
	float LeanUpFrom = 0.12f;
	/** The walk back from a page that's straight behind her (visex.Menu.StepBack): she steps back
	 *  onto the column, left then right, and sinks into the lean as the right lands, no turning
	 *  round (Tools/char_menu_walk_back_clips.py). */
	UPROPERTY(EditAnywhere, Category = "Menu|Walk")
	TSoftObjectPtr<class UAnimSequence> WalkBackStart = TSoftObjectPtr<class UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/Menu/A_Menu_WalkB_Start.A_Menu_WalkB_Start")));
	UPROPERTY(EditAnywhere, Category = "Menu|Walk")
	TSoftObjectPtr<class UAnimSequence> WalkBackStop = TSoftObjectPtr<class UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/Menu/A_Menu_WalkB_Stop.A_Menu_WalkB_Stop")));
	/** WalkBackStart's clip time she sets off from (her weight going back), and the windows the
	 *  start hands to the stop in (left planted), and the stop to the lean-down (right landing). */
	UPROPERTY(EditAnywhere, Category = "Menu|Walk")
	float StepBackFrom = 0.45f;
	UPROPERTY(EditAnywhere, Category = "Menu|Walk")
	FVector2D StepBackCutFrom = FVector2D(1.5f, 1.85f);
	UPROPERTY(EditAnywhere, Category = "Menu|Walk")
	FVector2D StepBackCutInto = FVector2D(3.2f, 3.6f);
	UPROPERTY(EditAnywhere, Category = "Menu|Walk")
	float StepBackLeanAt = 3.62f;
	/** The lean-down's clip time the step back sinks from: left planted, the right settling. */
	UPROPERTY(EditAnywhere, Category = "Menu|Walk")
	float LeanDownFromBack = 1.45f;
	/** The step back only when her spot is this close (cm) and within this angle of straight
	 *  behind her, and she faces within it of her lean facing; else she turns and walks. */
	UPROPERTY(EditAnywhere, Category = "Menu|Walk")
	float StepBackMax = 120.f;
	UPROPERTY(EditAnywhere, Category = "Menu|Walk")
	float StepBackAngle = 35.f;
	/** The short walk hands WalkStart (in this window, clip s) to WalkStop (WalkCutInto), right foot down. */
	UPROPERTY(EditAnywhere, Category = "Menu|Walk")
	FVector2D WalkCutFrom = FVector2D(1.35f, 1.8f);
	UPROPERTY(EditAnywhere, Category = "Menu|Walk")
	FVector2D WalkCutInto = FVector2D(3.3f, 3.8f);
	/** WalkStop's clip time she has stopped at, feet side by side: a page opens, or she turns. */
	UPROPERTY(EditAnywhere, Category = "Menu|Walk")
	float WalkStopAt = 4.0f;
	UPROPERTY(EditAnywhere, Category = "Menu|Walk")
	float MoveBlend = 0.25f;
	/** A turn hands on to the walk (or the lean) once this share of its yaw is in. */
	UPROPERTY(EditAnywhere, Category = "Menu|Walk")
	float TurnHandOff = 0.82f;
	/** The arrival's walk-turn (Tools/char_menu_walk_clips.py): the loop hands to it mid-walk
	 *  (in this window), she plants her left foot and pivots, and sinks into the lean at ArriveLeanAt. */
	UPROPERTY(EditAnywhere, Category = "Menu|Walk")
	TSoftObjectPtr<class UAnimSequence> WalkTurnIn = TSoftObjectPtr<class UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/Menu/A_Menu_Walk_Turn180R.A_Menu_Walk_Turn180R")));
	UPROPERTY(EditAnywhere, Category = "Menu|Walk")
	FVector2D ArriveCutInto = FVector2D(0.9f, 2.3f);
	UPROPERTY(EditAnywhere, Category = "Menu|Walk")
	float ArriveLeanAt = 3.58f;
	/** The most the arrival's loop curves (deg) to land her; past it the start heading takes the rest. */
	UPROPERTY(EditAnywhere, Category = "Menu|Walk")
	float ArriveArcMax = 45.f;
	/** The page opens this long before her walk out has stopped (s). */
	UPROPERTY(EditAnywhere, Category = "Menu|Walk")
	float PageOpenEarly = 0.3f;
	/** The walk back's play rate (its turns keep ReturnTurnRate). */
	UPROPERTY(EditAnywhere, Category = "Menu|Walk")
	float ReturnWalkRate = 1.1f;
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

	/** Her top pace on the menu walks, out to a page and back (cm/s): her catwalk at an
	 *  athlete's purpose. Each walk eases in and out over its whole length (smootherstep) and
	 *  peaks here; the catwalk layer is full from 150. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean", meta = (ClampMin = "10"))
	float MenuWalkSpeed = 145.f;

	/** The camera blend onto a page and back (Body, Wardrobe, Photo mode; s): with the push-off
	 *  and the walk out it brings each page in about 3 s after the click. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean", meta = (ClampMin = "0.1"))
	float PageBlendTime = 1.5f;

	/** The turn clips' pace on the menu (1 = as captured); their still lead-in is skipped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean", meta = (ClampMin = "0.5"))
	float TurnClipRate = 1.55f;

	/** Clip seconds of stillness before each turn clip's first step (skipped). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean", meta = (ClampMin = "0"))
	float TurnClipLeadIn = 0.3f;

	/** Share of the first return turn landed when she sets off walking (the walk takes the rest). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean", meta = (ClampMin = "0.5", ClampMax = "1"))
	float TurnIntoWalk = 0.65f;

	/** The return from a page: its turns' pace (the arrival keeps TurnClipRate). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean", meta = (ClampMin = "0.5"))
	float ReturnTurnRate = 2.f;

	/** The return: the last turn starts this far from her spot (cm), on her last step in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean", meta = (ClampMin = "2"))
	float ReturnPivotAt = 35.f;

	/** The return: the sink into the lean starts at this share of the last turn's yaw, from
	 *  LeanDownFrom (clip s: past its still start and the foot shuffle) at LeanDownRate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean", meta = (ClampMin = "0.5", ClampMax = "1"))
	float TurnIntoLean = 0.7f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean", meta = (ClampMin = "0"))
	float LeanDownFrom = 1.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean", meta = (ClampMin = "0.5"))
	float LeanDownRate = 1.4f;

	/** The catwalk posture (chest up, shoulders back) held at least this much through the menu's
	 *  turns and walks, where her speed alone would let it drop (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean", meta = (ClampMin = "0", ClampMax = "1"))
	float MenuGaitFloor = 0.6f;

	/** Body/Wardrobe: the least gap from her axis to the stone she walks out to (cm), so the
	 *  way out and back is a real little walk, not a shuffle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean", meta = (ClampMin = "0"))
	float BodyWalkGap = 110.f;

	/** The return: she turns to the column in place, walks to her spot facing it, and turns
	 *  round there before she sinks into the lean. Seconds for a half turn; less for less. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean", meta = (ClampMin = "0.1"))
	float ReturnTurnTime = 1.4f;

	/** Her feet's pace through a turn too small for a clip (cm/s, at the turn's middle). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean", meta = (ClampMin = "0"))
	float ReturnShuffleSpeed = 20.f;

	/** The turn-in-place clips (Tools/char_turn_clips.py, root yaw only): the one whose turn is
	 *  nearest the one she needs plays, its root yaw scaled to land her facing exactly that way. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean")
	TArray<TSoftObjectPtr<class UAnimSequence>> TurnClips = {
		TSoftObjectPtr<class UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/Turn/A_Turn_90L.A_Turn_90L"))),
		TSoftObjectPtr<class UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/Turn/A_Turn_90R.A_Turn_90R"))),
		TSoftObjectPtr<class UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/Turn/A_Turn_180L.A_Turn_180L"))),
		TSoftObjectPtr<class UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/Turn/A_Turn_180R.A_Turn_180R")))};

	/** Turns smaller than this (deg) skip the clips: she turns as she walks, or eases round. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean", meta = (ClampMin = "0"))
	float TurnClipMin = 40.f;

	/** Closer to her spot than this (cm) she doesn't walk: she turns and settles back onto it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean", meta = (ClampMin = "0"))
	float ReturnWalkMin = 35.f;

	/** Farther than this (cm, after photo mode) she isn't walked back: a dip to black instead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Lean", meta = (ClampMin = "0"))
	float ReturnWalkMax = 600.f;

	// first launch: she walks in to the column and leans while the panel fades in (visex.Menu.Arrival)
	/** Where she starts (world XY, on the ground), out of the menu camera's frame (left, the
	 *  steps side: she crosses in profile, in frame ~1.6 s later); she walks
	 *  in from there at her own pace, facing her spot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Arrival")
	FVector2D ArrivalStart = FVector2D(9529.f, -1322.f);
	/** Unused (the start faces her spot); kept so saved levels load. */
	UPROPERTY()
	float ArrivalStartYaw = 75.f;
	/** Her pace (cm/s): calm, about 0.6 of the walk's 190. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Arrival", meta = (ClampMin = "10"))
	float ArrivalSpeed = 115.f;
	/** Seconds of walk her pose is run through before she starts, so she starts mid-stride. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Arrival", meta = (ClampMin = "0"))
	float ArrivalPreRoll = 1.5f;
	/** Seconds after she is first in frame until the panel starts to fade in, and the fade's
	 *  length (it also starts by ArrivalPanelLatest s after the walk began, in frame or not). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Arrival", meta = (ClampMin = "0"))
	float ArrivalPanelFadeStart = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Arrival", meta = (ClampMin = "0.05"))
	float ArrivalPanelFadeTime = 1.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Arrival", meta = (ClampMin = "0"))
	float ArrivalPanelLatest = 5.f;
	/** Replays the arrival from frame 0 (for tuning and the film); visex.Menu.ArrivalFreeze 1 holds it there. */
	UFUNCTION(BlueprintCallable, Category = "Menu|Arrival")
	void PlayArrival();
	/** True from launch until she is down on the lean (and while a replay runs). */
	UFUNCTION(BlueprintPure, Category = "Menu|Arrival")
	bool IsArriving() const;
	/** The arrival fades the panel in during her walk: the launch cover leaves it alone. */
	bool FadesOwnPanel() const { return bArrivalRan || IsArriving(); }

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
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Menu")
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
	/** Keep the rows' left edges on one vertical screen line (UVisexMenuWidget::AlignSlope). */
	void UpdateAlign();
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
	bool bColumnFitted = false;
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
	// From/Rate: the lean-down from that clip time at that pace (the return from a page lands
	// its last step into the sink); LeanSettleAt is when she rests on the stone (world seconds)
	void Lean(bool bFromStanding, float From = 0.f, float Rate = 1.f);
	double LeanSettleAt = 0.0;
	/** Plays the push-off; returns its length in seconds (0 if she wasn't leaning). */
	float StandUp(bool bIntoWalk = false);   // bIntoWalk: held on its last frame until a walk takes over
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
	static constexpr float SitBlend = 0.3f;   // the sit clips hand over to each other this long
	// Settings: the jump onto the drum and down again, retimed as it plays: a settle into the
	// crouch, an eased spring, a soft landing (clip time -> play rate), the dolly matched to it
	UPROPERTY(Transient) TObjectPtr<class UAnimMontage> SitMontage;
	int32 SitWarp = 0;           // 1 the jump up, 2 the step down, 0 none
	float SettingsCamDur = 0.f;  // this dolly's seconds (0: SettingsCamTime)
	static float SitRate(int32 Warp, float ClipT);
	static float SitWarpTime(int32 Warp, float Len);   // her seconds for the whole clip
	void TickSitWarp();
	// the panel's layout transform before any lens compensation, and the compensation in force
	void ApplyPanelLens(float K);
	float PanelLensK = 1.f;
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
	int32 StepDir = 0;           // +1 walking out, -1 returning to the column, 0 still
	void TickReturn(float DeltaSeconds);
	// the first-launch arrival: a walk-back (StepDir -1) from ArrivalStart that ends in Lean
	bool BeginArrival();
	void EndArrival();
	void TickArrivalPanel(float DeltaSeconds);
	void TickFootSkate();
	bool bArrivalChecked = false;   // the first ShowMenu has decided
	bool bArriving = false;
	/** Frame 0 is held: the launch cover still shows, or visex.Menu.ArrivalFreeze is 1. */
	bool ArrivalHeld() const;
	void FreezeHer(bool bFreeze);
	void PreRollWalk(float Seconds);
	bool bArrivalFrozen = false;
	bool bArrivalRan = false;
	FVector FrozenVelocity = FVector::ZeroVector;
	float ArrivalClock = -1.f;      // seconds since the arrival began; <0 when no fade runs
	float ArrivalInFrame = -1.f;    // ArrivalClock when she first came into the menu camera's frame
	float PanelInAlpha = 1.f;       // the panel's fade-in; input waits for 1
	int32 ArrivalQueued = 0;        // pressed while she finished walking: 1 Confirm, 2 ToggleMenu
	FVector PlantAt[2] = {FVector::ZeroVector, FVector::ZeroVector};           // the planted foot's spot on the ground, per foot
	FVector LastFoot[2] = {FVector::ZeroVector, FVector::ZeroVector};
	float PlantFor[2] = {0.f, 0.f};
	bool bPlanted[2] = {false, false};
	float SkateMax = 0.f, SkateSum = 0.f;
	int32 SkateSteps = 0;
	// menu motion timing: one transition at a time (enter or exit a page), click to settled,
	// with the planted-foot skate over it. Logged as "Menu motion: <name> ..."
	void BeginMotion(const TCHAR* Name);
	void EndMotion(float SettleIn = 0.f);
	void ResetSkate();
	/** Visible surfaces block the FootTrace channel her Foot Placement traces (visex.h). */
	void SetUpFootTrace();
	/** The menu's walks: the slow walk clip at her capsule's pace (visex.Menu.SlowWalk). */
	void TickWalkClip(float DeltaSeconds, bool bMenuWalk);
	/** One clip of a root-motion move: Clip from From to To (clip s; past its end loops it),
	 *  blended in over Blend with foot Lock (0 left, 1 right, -1 none) held where it was. */
	struct FMoveSeg { class UAnimSequence* Clip = nullptr; float From = 0.f, To = 0.f; int32 Lock = -1; float Blend = 0.25f; };
	/** Plays Segs one after another on DefaultSlot with root motion: her capsule follows the
	 *  clips, the code only steers her heading. The first takes over from FromClip at FromTime.
	 *  Returns the move's length (s). VisexMenuDirectorMove.cpp */
	float PlayRootMotionMove(const TArray<FMoveSeg>& Segs, class UAnimSequence* FromClip, float FromTime, float Rate = 1.f);
	void StartMoveSeg(int32 Index, class UAnimSequence* FromClip, float FromTime);
	void TickMove();
	void StopMove();
	bool IsMoving() const { return MoveIndex >= 0; }
	bool MoveAtEnd() const { return MoveIndex >= 0 && MoveIndex == MoveSegs.Num() - 1 && MoveClock >= MoveSegs.Last().To; }
	float MoveLeft() const;
	bool RootMoves() const;
	TArray<FMoveSeg> MoveSegs;
	int32 MoveIndex = -1;
	float MoveClock = 0.f, MoveLastPos = 0.f, MoveRate = 1.f;
	bool bRootMove = false;      // this step out, back or arrival walks by root motion
	float MoveYaw = 0.f;         // the heading the walk back steers to
	float MoveSide = 0.f;        // the walk back's last turn goes this way round (her first turn's)
	TArray<FMoveSeg> PlanSegs;   // the walk back (or in), planned before its first turn
	float PlanScale = 1.f;       // and the root translation scale that lands it
	// the planted-foot hold across a hand-off: her capsule jumps to where the incoming clip wants
	// it and her mesh is held back by as much, eased onto it with the incoming clip's weight
	void HoldFoot(class UAnimSequence* FromClip, float FromTime, class UAnimSequence* ToClip, float ToTime, int32 Foot, class UAnimMontage* Incoming);
	void TickHold();   // after her bones are final each frame
	UPROPERTY(Transient) TObjectPtr<class UAnimMontage> HoldMontage;
	FVector HoldOffset = FVector::ZeroVector, HoldBase = FVector::ZeroVector;
	bool bHolding = false;
	int32 HoldFootIdx = -1;      // the foot the hold keeps on its mark
	FVector HoldTarget = FVector::ZeroVector;   // where (world)
	FDelegateHandle HoldHandle;
	// each clip sampled at 60 Hz: ik_foot_l/r in mesh space, root travel from its start
	struct FClipTrack { float Dt = 1.f / 60.f, Len = 0.f; TArray<FVector> Feet[2]; TArray<FVector> Root; TArray<float> Yaw; };
	TMap<const class UAnimSequence*, FClipTrack> Tracks;
	const FClipTrack& Track(class UAnimSequence* Clip);
	FVector ClipFoot(class UAnimSequence* Clip, float T, int32 Foot);
	FVector ClipRoot(class UAnimSequence* Clip, float A, float B);   // root travel A..B (mesh space), wrapping
	FVector ClipVel(class UAnimSequence* Clip, float T);
	FVector MeshToWorld(const FVector& V, float Yaw) const;
	/** The best hand-off from A (clip s A0..A1) to B (B0..B1): the same foot planted through the
	 *  blend in both, the other foot nearest. */
	bool FindCut(class UAnimSequence* A, FVector2D AWin, class UAnimSequence* B, FVector2D BWin, int32& Foot, float& TA, float& TB, float MaxCost = 15.f);
	FVector MoveTravel(const TArray<FMoveSeg>& Segs);   // mesh space: root travel and the holds between
	bool BeginRootStepOut();
	/** The walk back (or in) to her spot: Segs, the heading, and the root translation scale that
	 *  lands it, through the turn at its end and the lean-down's hold. Arrival: from a mid-loop. */
	bool PlanRootReturn(const FVector& From, class UAnimSequence* Turn1Clip, float Turn1At, bool bArrival, TArray<FMoveSeg>& Segs, float& Yaw, float& Scale);
	void BeginRootTurnHome();
	/** The arrival by the walk-turn (visex.Menu.ArrivalTurn): the loop curving by ArriveArc deg,
	 *  into WalkTurnIn's plant and pivot, and the lean-down from its pivot foot. Plans Segs, the
	 *  first heading and the arc that land her on her spot facing HomeYaw. */
	bool PlanRootArrival(const FVector& From, TArray<FMoveSeg>& Segs, float& Yaw, float& Arc);
	/** Where Segs end (world offset) and her yaw there, from Yaw, the loop curving Arc deg; from
	 *  segment Index at clip time Clock. With the lean-down's hold. */
	FVector SimMove(const TArray<FMoveSeg>& Segs, int32 Index, float Clock, float Yaw, float ArcLeft, float& EndYaw);
	void TickRootArrival(float DeltaSeconds);
	float ClipYaw(class UAnimSequence* Clip, float T);   // its root yaw since its start (deg)
	bool bArriveTurn = false;    // this arrival walks the walk-turn
	float ArriveArc = 0.f;       // deg the loop curves by
	float ArriveArcDone = 0.f;   // of it put on her so far
	float ArriveClockWas = 0.f;  // the loop's clip time as the arc was last put on
	void LeanWithHold(class UAnimSequence* FromClip, float FromTime, float DownFrom = -1.f);
	/** The walk back by stepping back onto the column (ReturnPhase 3): true if it's under way. */
	bool BeginRootStepBack();
	void TickStepBack();
	float StepBackYaw = 0.f;     // the facing she steps back along (eased onto HomeYaw by the lean)
	/** HoldFoot from where her Foot is in the world now (whatever pose she's in), not a clip's. */
	void HoldFootNow(class UAnimSequence* ToClip, float ToTime, int32 Foot, class UAnimMontage* Incoming);
	/** Her yaw to NewYaw: Natural deg of it about her capsule (a turn clip's own yaw), the rest
	 *  about her planted foot, so steering never slides a foot on the ground. */
	void TurnAboutPlanted(float NewYaw, float Natural = 0.f, int32 Pivot = -1);   // Pivot: that foot, not the stillest
	int32 ClipPlantedFoot(class UAnimSequence* Clip, float T);   // the foot standing still in the clip, or -1
	FVector FootWas[2] = {FVector::ZeroVector, FVector::ZeroVector};
	uint64 FootFrame = 0;        // the frame FootWas was read on
	float TurnRawWas = 0.f;      // the turn clip's own yaw (unscaled) as last applied
	UPROPERTY(Transient) TObjectPtr<class UAnimMontage> TurnMontage;
	UPROPERTY(Transient) TObjectPtr<class UAnimMontage> MoveMontage;
	UPROPERTY(Transient) TObjectPtr<class UAnimMontage> WalkMontage;
	float WalkClipSpeed = 126.5f;   // cm/s, the clip's own root motion (measured when it starts)
	float WalkClipIdle = 0.f;       // seconds she has been all but still on a menu walk
	// the walk clip is a root-motion clip: while it plays, her anim ignores root motion (the capsule
	// moves her), or its zeroed translation held her still on the Start walk-off; then restored
	bool bWalkRootOff = false;
	uint8 WalkRootMode = 3;         // ERootMotionMode to restore (RootMotionFromMontagesOnly)
	FString MotionName;
	double MotionStart = -1.0;
	FTimerHandle MotionTimer;
	bool bMotionBoxed = false;
	int32 ReturnPhase = 0;       // 0 turning to her spot, 1 walking there, 2 turning round on it
	float PhaseClock = 0.f;
	float TurnFrom = 0.f, TurnTo = 0.f, TurnTime = 0.f;   // yaw (deg) and seconds of this turn
	// the turn clip playing (null: the eased fallback): its root yaw sampled over time, the
	// scale onto the turn she needs, and the clip time she's done at
	bool StartTurnClip(bool bThenWalk);
	/** The turn clip for Need (deg) and its timing, as StartTurnClip plays it: clip seconds
	 *  skipped (Start), play rate, her seconds to its end and to the lean's start (LeanAt). */
	struct FTurnPick { class UAnimSequence* Clip = nullptr; TArray<float> Curve; float Dt = 0.f, Scale = 1.f, Start = 0.f, Rate = 1.f, End = 0.f, LeanAt = -1.f; };
	bool PickTurn(float Need, bool bThenWalk, FTurnPick& Out);
	bool FinishTurnPick(bool bThenWalk, FTurnPick& Out) const;
	UPROPERTY(Transient) TObjectPtr<class UAnimSequence> TurnClip;
	TArray<float> TurnCurve;
	float TurnDt = 0.f, TurnScale = 1.f, TurnEnd = 0.f;
	float TurnStart = 0.f, TurnRate = 1.f;   // clip seconds skipped, and its play rate
	float TurnLeanAt = -1.f;     // the return's last turn: her seconds into it when the sink starts
	bool bTurnThenWalk = false;
	float TurnYawAt(float T) const;
	TEnumAsByte<ERootMotionMode::Type> TurnRootMotion = ERootMotionMode::RootMotionFromMontagesOnly;
	bool bTurnRootSaved = false;
	void RestoreTurnRootMotion();
	float HomeYaw = 0.f;         // her facing on the lean
	float StepClock = 0.f;
	FVector StepAway = FVector::ForwardVector;   // on the ground, from the drum toward her
	FVector StepHome = FVector::ZeroVector;      // where she backs to: the stand-in's spot
	int32 StepSelect = 0;        // the row the menu returns on
	float StepMinTime = 0.f;     // the camera's blend home: the menu waits for it too
	bool bSteppedOut = false;
	bool bStepOrient = true;     // her movement's own orient-to-movement, put back after
	int32 StepThen = 0;          // what the walk out opens: 0 the body page, 1 photo mode
	// the walk under way: its length (cm) and time (s), eased in and out over the whole of it
	float WalkDist = 0.f, WalkTime = 0.f, WalkFrom = 0.f;
	void PlanWalk(float Dist, float From);
	float WalkWant(float Clock, float Done) const;   // the pace (cm/s) to be at now
	bool bGaitHeld = false;      // the catwalk posture is held through the transition
	void HoldGait(bool bHold);
	FTransform Drifted(const FTransform& Base) const;   // the menu camera's handheld sway on Base
	void UnboxPageCamera(bool bUnbox);   // the page's camera fills the window; false: put it back
	void UnboxLens(class UCineCameraComponent* Lens);
	TWeakObjectPtr<class UCineCameraComponent> UnboxedLens;
	FCameraFilmbackSettings UnboxedFilmback;
	bool bUnboxedConstrain = true;

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
