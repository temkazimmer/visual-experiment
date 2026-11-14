#include "VisexMenuDirector.h"

#include "VisexMenuWidget.h"
#include "VisexBodyEditor.h"
#include "VisexBodyComponent.h"
#include "VisexBodyProfile.h"
#include "VisexMenuLayoutEditor.h"
#include "VisexPhotoMode.h"
#include "VisexQATour.h"
#include "VisexLaunchCover.h"
#include "VisexSettings.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/GameViewportClient.h"
#include "Engine/RectLight.h"
#include "Engine/SpotLight.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "VisexAnimInstance.h"
#include "visex.h"
#include "VisexFootTrace.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "UObject/UObjectIterator.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/CollisionProfile.h"
#include "Components/InputComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameUserSettings.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarMenuSlowWalk(TEXT("visex.Menu.SlowWalk"), 1, TEXT("Menu walks play the slow walk clip at her capsule's speed (TickWalkClip): 0 leaves them to the blend space"));
static TAutoConsoleVariable<int32> CVarMenuArrival(TEXT("visex.Menu.Arrival"), 1,
	TEXT("1: on the first menu after launch she walks in to the column and leans while the menu fades in. 0: she is already leaning."));
static TAutoConsoleVariable<int32> CVarMenuArrivalFreeze(TEXT("visex.Menu.ArrivalFreeze"), 0,
	TEXT("1: hold the arrival at its frame 0 (mid-stride, menu framing, no panel) for rendering; 0 lets it run on."));

AVisexMenuDirector::AVisexMenuDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Panel = CreateDefaultSubobject<UWidgetComponent>(TEXT("Panel"));
	Panel->SetupAttachment(RootComponent);
	Panel->SetWidgetSpace(EWidgetSpace::World);
	Panel->SetWidgetClass(UVisexMenuWidget::StaticClass());
	Panel->SetDrawSize(FVector2D(1100.f, 1000.f));
	Panel->SetPivot(FVector2D(0.5f, 0.5f));
	Panel->SetBlendMode(EWidgetBlendMode::Transparent);
	Panel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Panel->SetCastShadow(false);
	// the text only changes on input: no need to redraw a ~4600 px render target every frame
	Panel->SetManuallyRedraw(true);
}

void AVisexMenuDirector::BeginPlay()
{
	Super::BeginPlay();
	// the turn clips load now: a clip loaded at the turn itself can still be compressing (in
	// the editor) and reads as no turn at all
	for (const TSoftObjectPtr<UAnimSequence>& Soft : TurnClips)
	{
		if (UAnimSequence* Clip = Soft.LoadSynchronous())
		{
			LoadedLean.AddUnique(Clip);
		}
	}
	if (bMaxQualityOnPlay)
	{
		// the look is the point: Ultra (Cinematic) in Play and on a first launch; after that
		// the player's own Settings (VisexSettings.h)
		FVisexSettings::InitialApply();
	}
	FVisexSettings::ApplyVisuals(GetWorld());
	Panel->SetVisibility(false);
	if (StandIn)
	{
		StandIn->SetActorHiddenInGame(true);
		// it stands exactly where the player goes: it must not block the camera boom
		StandIn->SetActorEnableCollision(false);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("visexqa")))
	{
		// scripted walk through every screen for checking a packaged build (VisexQATour.h)
		if (AVisexQATour* Tour = GetWorld()->SpawnActor<AVisexQATour>())
		{
			Tour->Director = this;
		}
	}
	if (bShowMenuOnPlay)
	{
		// the pawn may not be possessed yet on the first frame
		GetWorldTimerManager().SetTimer(SetupTimer, this, &AVisexMenuDirector::TrySetup, 0.05f, true);
	}
}

void AVisexMenuDirector::TrySetup()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !PC->GetPawn() || !MenuCamera)
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(SetupTimer);
	ShowMenu();
}

void AVisexMenuDirector::PlacePawn()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn || !StandIn)
	{
		return;
	}
	FVector Loc = StandIn->GetActorLocation();
	if (const ACharacter* Char = Cast<ACharacter>(Pawn))
	{
		Loc.Z += Char->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}
	// the stand-in is a bare skeletal mesh; the character's mesh sits at -90 yaw inside it
	const FRotator Rot(0.f, StandIn->GetActorRotation().Yaw + 90.f, 0.f);
	Pawn->SetActorLocationAndRotation(Loc, Rot, false, nullptr, ETeleportType::TeleportPhysics);
	// after Start the follow camera swings round to behind her
	PC->SetControlRotation(FRotator(StartCameraPitch, Rot.Yaw + StartCameraYaw, 0.f));
}

void AVisexMenuDirector::ShowMenu()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !MenuCamera)
	{
		return;
	}
	PlacePawn();
	if (APawn* Pawn = PC->GetPawn())
	{
		Pawn->DisableInput(PC);
	}
	// the Start move flies MenuCamera away: remember where the shot lives
	bPlaying = false;
	if (!bHaveBase)
	{
		const FVector Feet = StandIn ? StandIn->GetActorLocation() : PC->GetPawn()->GetActorLocation();
		// the level's own camera spot, kept from the first time: after that MenuCamera is wherever
		// the menu (or the Start move) left it, and re-framing from there would compound
		if (!bHaveLevelCamera)
		{
			LevelCamera = MenuCamera->GetActorTransform();
			bHaveLevelCamera = true;
		}
		CameraBase = LevelCamera;
		MenuAim = Feet + FVector(0.f, 0.f, CameraAimHeight);
		if (bFloorCamera)
		{
			// on the sand by her feet, on the level camera's side of her, looking up
			const FVector Side = (CameraBase.GetLocation() - Feet).GetSafeNormal2D().RotateAngleAxis(FloorCameraYaw, FVector::UpVector);
			FVector Loc = Feet + Side * FloorCameraDistance;
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(MenuFloor), false, PC->GetPawn());
			if (LeanColumn) { Params.AddIgnoredActor(LeanColumn); }
			const float GroundZ = GetWorld()->LineTraceSingleByChannel(Hit, Loc + FVector(0.f, 0.f, 300.f), Loc - FVector(0.f, 0.f, 500.f), ECC_Visibility, Params)
				? Hit.Location.Z : Feet.Z;
			Loc.Z = GroundZ + FloorCameraHeight;
			MenuAim = Feet + FVector(0.f, 0.f, FloorCameraLookHeight);
			FRotator Look = (MenuAim - Loc).Rotation();
			Look.Yaw += FloorCameraPan;
			CameraBase = FTransform(Look, Loc);
		}
		else
		{
			// closer along the line to her, so she grows in place in the frame
			CameraBase.SetLocation(MenuAim + (CameraBase.GetLocation() - MenuAim) * FMath::Clamp(CameraPullIn, 0.1f, 1.f));
		}
		bHaveBase = true;
	}
	MenuCamera->SetActorTransform(Drifted(CameraBase));   // where the sway left it: no hop
	SettingsCamAlpha = 0.f;
	PanelLensK = 1.f;
	// and its lens, which the Start move turns into the play camera's
	if (UCineCameraComponent* Lens = GetLens())
	{
		if (!bHaveLens)
		{
			MenuFocal = Lens->CurrentFocalLength;
			MenuFStop = Lens->CurrentAperture;
			MenuFilmback = Lens->Filmback;
			bMenuConstrain = Lens->bConstrainAspectRatio;
			bHaveLens = true;
		}
		// a cine camera letterboxes to its sensor's 16:9 by default, the play camera fills the
		// window: on any other window the bars snapped off where the Start move lands (QA 4:3)
		Lens->bConstrainAspectRatio = bMenuFillsWindow ? false : bMenuConstrain;
		// a bigger sensor with a matching focal length frames the same and blurs more
		const float Scale = FMath::Max(1.f, MenuSensorScale);
		Lens->Filmback = MenuFilmback;
		Lens->Filmback.SensorWidth *= Scale;
		Lens->Filmback.SensorHeight *= Scale;
		const float Stop = MenuAperture > 0.f ? MenuAperture : MenuFStop;
		// the lens preset's range would clamp these
		Lens->LensSettings.MinFStop = FMath::Min(Lens->LensSettings.MinFStop, Stop);
		SetMenuFocal(Lens, MenuLensFocal());
		Lens->SetCurrentAperture(Stop);
		FCameraFocusSettings Focus = Lens->FocusSettings;
		Focus.ManualFocusDistance = FVector::Dist(CameraBase.GetLocation(), MenuAim);
		Lens->FocusSettings = Focus;
	}
	// and on her, this frame: the menu's aim point is her lean, not where she is now, and the
	// first frame back drew her soft before TickFocus caught up (QA 0558, the blur at the return)
	TickFocus();
	SetCornerShade(1.f);
	SpawnLeanColumn();
	FitLeanColumn();
	SetUpFootTrace();   // the drum is placed now too
	// the first menu after launch: the drum is placed (above, with her on her spot); now she
	// starts out on the sand and walks in to it while the panel fades in
	const bool bArrive = !bArrivalChecked && CVarMenuArrival.GetValueOnGameThread() != 0 && BeginArrival();
	bArrivalChecked = true;
	if (!bArrive && !bLeaning)
	{
		Lean(bHaveLeaned);
	}
	bHaveLeaned = true;
	bOrbiting = false;
	WalkOffClock = -1.f;
	LandedFor = -1.f;
	PC->SetViewTarget(MenuCamera);

	// the panel rides with the camera, turned toward the figure on the right
	Panel->AttachToComponent(MenuCamera->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	PlacePanel();
	if (UMaterialInterface* Mat = PanelMaterial.LoadSynchronous())
	{
		if (Panel->GetMaterial(0) != Mat)
		{
			Panel->SetMaterial(0, Mat);
		}
	}
	Panel->SetTintColorAndOpacity(FLinearColor(PanelBrightness, PanelBrightness, PanelBrightness, 1.f));
	// stencil 1 keeps the text out of screen filters (M_Anime80s); M_MenuPanel writes it
	Panel->SetRenderCustomDepth(true);
	Panel->SetCustomDepthStencilValue(1);
	Panel->SetVisibility(true);
	if (UVisexMenuWidget* W = GetMenuWidget())
	{
		W->SetRenderOpacity(bArriving ? 0.f : 1.f);
		if (Page > 0) { CloseSettings(); }
		W->SetSelected(0);
	}
	// the corner shade stays: the film's last frame has it
	if (!bArriving)
	{
		PanelInAlpha = 1.f;
		ArrivalClock = -1.f;
	}
	FadeOut = -1.f;
	MarkPanel(10);   // first layout, fonts

	if (!bInputBound)
	{
		EnableInput(PC);
		// No Gamepad_LeftStick_* keys: UE 5.7's Mac controller layer reports every stick axis,
		// right stick included, as LeftStick_Up/Down. The left stick is read from LeftY in Tick.
		const FKey UpKeys[] = {EKeys::Gamepad_DPad_Up, EKeys::Up, EKeys::W};
		const FKey DownKeys[] = {EKeys::Gamepad_DPad_Down, EKeys::Down, EKeys::S};
		InputComponent->BindAxisKey(EKeys::Gamepad_LeftY);
		const FKey OkKeys[] = {EKeys::Gamepad_FaceButton_Bottom, EKeys::Enter, EKeys::SpaceBar};
		const FKey MenuKeys[] = {EKeys::Gamepad_Special_Right, EKeys::O};
		for (const FKey& K : UpKeys) { InputComponent->BindKey(K, IE_Pressed, this, &AVisexMenuDirector::Up); }
		for (const FKey& K : DownKeys) { InputComponent->BindKey(K, IE_Pressed, this, &AVisexMenuDirector::Down); }
		for (const FKey& K : OkKeys) { InputComponent->BindKey(K, IE_Pressed, this, &AVisexMenuDirector::Confirm); }
		// Settings: left/right step a value, L1/R1 switch tabs, CIRCLE goes back. Not consumed,
		// so in play they still reach her; the handlers do nothing outside the Settings page.
		InputComponent->BindAxisKey(EKeys::Gamepad_LeftX);
		const FKey LeftKeys[] = {EKeys::Gamepad_DPad_Left, EKeys::Left, EKeys::A};
		const FKey RightKeys[] = {EKeys::Gamepad_DPad_Right, EKeys::Right, EKeys::D};
		const FKey PrevTabKeys[] = {EKeys::Gamepad_LeftShoulder, EKeys::Q};
		const FKey NextTabKeys[] = {EKeys::Gamepad_RightShoulder, EKeys::E, EKeys::Tab};
		const FKey BackKeys[] = {EKeys::Gamepad_FaceButton_Right, EKeys::Escape, EKeys::BackSpace};
		for (const FKey& K : LeftKeys) { InputComponent->BindKey(K, IE_Pressed, this, &AVisexMenuDirector::Left).bConsumeInput = false; }
		for (const FKey& K : RightKeys) { InputComponent->BindKey(K, IE_Pressed, this, &AVisexMenuDirector::Right).bConsumeInput = false; }
		for (const FKey& K : PrevTabKeys) { InputComponent->BindKey(K, IE_Pressed, this, &AVisexMenuDirector::PrevTab).bConsumeInput = false; }
		for (const FKey& K : NextTabKeys) { InputComponent->BindKey(K, IE_Pressed, this, &AVisexMenuDirector::NextTab).bConsumeInput = false; }
		for (const FKey& K : BackKeys) { InputComponent->BindKey(K, IE_Pressed, this, &AVisexMenuDirector::Back).bConsumeInput = false; }
		// the director's input stays on through play, so these work from the game too;
		// they don't consume, so the pawn still sees its own keys
		// § (the key left of 1): the menu layout editor
		InputComponent->BindKey(EKeys::Section, IE_Pressed, this, &AVisexMenuDirector::ToggleLayoutEditor).bConsumeInput = false;
		for (const FKey& K : MenuKeys)
		{
			InputComponent->BindKey(K, IE_Pressed, this, &AVisexMenuDirector::ToggleMenu).bConsumeInput = false;
		}
		bInputBound = true;
	}
	bOpen = true;
	OnMenuShown();
}

void AVisexMenuDirector::SetPanelOpacity(float Alpha)
{
	UVisexMenuWidget* W = GetMenuWidget();
	if (W && !FMath::IsNearlyEqual(W->GetRenderOpacity(), Alpha))
	{
		W->SetRenderOpacity(Alpha);
		MarkPanel();
	}
}

UCineCameraComponent* AVisexMenuDirector::GetLens() const
{
	const ACineCameraActor* Cine = Cast<ACineCameraActor>(MenuCamera);
	return Cine ? Cine->GetCineCameraComponent() : nullptr;
}

float AVisexMenuDirector::ViewAspect() const
{
	FVector2D Size(0.f);
	if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
	{
		Viewport->GetViewportSize(Size);
	}
	return Size.Y > 0.f ? Size.X / Size.Y : 16.f / 9.f;
}

float AVisexMenuDirector::MenuLensFocal() const
{
	float Focal = (MenuFocalLength > 0.f ? MenuFocalLength : MenuFocal) * FMath::Max(1.f, MenuSensorScale);
	if (bMenuFillsWindow && MenuFilmback.SensorHeight > 0.f)
	{
		const float Shot = MenuFilmback.SensorWidth / MenuFilmback.SensorHeight;
		const float Window = ViewAspect();
		// wider than the shot needs nothing: the engine keeps the vertical field
		// (BaseEngine.ini AspectRatio_MaintainYFOV), so her whole height stays and the sides grow
		if (Window < Shot)
		{
			// narrower than the shot: unboxed, the engine keeps the lens's vertical field
			// (LocalPlayer MaintainYFOV), so a 4:3 window cropped the sides and cut the panel's
			// text off ("tart", "ody", QA 0320). Shorten the lens by Window / Shot so the view's
			// horizontal field is the 16:9 shot's (tan of the half-angle grows by Shot / Window):
			// the whole composition, panel included, stays in, with more sky and ground.
			Focal *= Window / Shot;
		}
	}
	return Focal;
}

void AVisexMenuDirector::SetMenuFocal(UCineCameraComponent* Lens, float Focal)
{
	// the lens preset's range would clamp it
	Lens->LensSettings.MinFocalLength = FMath::Min(Lens->LensSettings.MinFocalLength, Focal);
	Lens->LensSettings.MaxFocalLength = FMath::Max(Lens->LensSettings.MaxFocalLength, Focal);
	Lens->SetCurrentFocalLength(Focal);
	MenuViewAspect = ViewAspect();
}

UVisexMenuWidget* AVisexMenuDirector::GetMenuWidget() const
{
	return Panel ? Cast<UVisexMenuWidget>(Panel->GetUserWidgetObject()) : nullptr;
}

void AVisexMenuDirector::PlacePanel()
{
	if (!bHaveLayout)
	{
		// the class's own values are the built-in layout; a saved one goes over them
		const UVisexMenuWidget* D = Panel && Panel->GetWidgetClass()
			? Cast<UVisexMenuWidget>(Panel->GetWidgetClass()->GetDefaultObject()) : nullptr;
		D = D ? D : GetDefault<UVisexMenuWidget>();
		Layout.Offset = PanelOffset;
		Layout.Yaw = PanelYaw;
		Layout.Pitch = PanelPitch;
		Layout.Roll = PanelRoll;
		Layout.Scale = PanelScale;
		Layout.LogoSize = D->TitleSize;
		Layout.LogoSpacing = D->TitleLetterSpacing;
		Layout.LogoGap = D->LogoGap;
		Layout.ItemSize = D->ItemSize;
		Layout.ItemGap = D->ItemGap;
		BuiltInLayout = Layout;
		FVisexMenuLayout::Load(Layout);
		bHaveLayout = true;
		PanelOffset = Layout.Offset;
		PanelYaw = Layout.Yaw;
		PanelPitch = Layout.Pitch;
		PanelRoll = Layout.Roll;
		PanelScale = Layout.Scale;
	}
	PushLayoutToWidget();
	// PanelOffset is set for the camera's own lens: a wider menu lens brings the panel in to keep its size on screen
	const float LensScale = (MenuFocalLength > 0.f && MenuFocal > 0.f) ? MenuFocalLength / MenuFocal : 1.f;

	// draw the panel's texture larger and shrink it back to the same size on screen
	const UVisexMenuWidget* MenuDefaults = Panel->GetWidgetClass()
		? Cast<UVisexMenuWidget>(Panel->GetWidgetClass()->GetDefaultObject()) : nullptr;
	const float SS = GetMenuWidget() ? FMath::Max(1.f, GetMenuWidget()->Supersample)
		: MenuDefaults ? FMath::Max(1.f, MenuDefaults->Supersample) : 1.f;
	// the widget pads its text for the shade to fade out in: grow the panel to match
	const float Pad = GetMenuWidget() ? GetMenuWidget()->ShadePadding : MenuDefaults ? MenuDefaults->ShadePadding : 0.f;
	Panel->SetDrawSize((FVector2D(1100.f, 1000.f) + FVector2D(2.f * Pad)) * SS);
	Panel->SetRelativeLocation(PanelOffset * LensScale);
	Panel->SetRelativeRotation(FRotator(PanelPitch, 180.f + PanelYaw, PanelRoll));
	Panel->SetRelativeScale3D(FVector(PanelScale * LensScale / SS));
}

void AVisexMenuDirector::ApplyPanelLens(float K)
{
	// The panel rides with the camera. When the lens goes from focal f to K * f, a point at camera
	// space (depth, right, up) lands on the same pixel if its depth is scaled by K. No rectangle
	// can do that for a panel both turned and rolled (best fit: 26 px off), so it is done per vertex:
	// while K < 1 the panel draws with M_MenuPanelLens, whose World Position Offset scales each
	// vertex's depth toward the camera by K, and its silhouette on screen is the main page's to
	// the pixel, whatever the widget draws. At K = 1 it is back on the plain panel material.
	PanelLensK = K;
	if (!Panel || !MenuCamera)
	{
		return;
	}
	UMaterialInterface* Base = PanelMaterial.LoadSynchronous();
	UMaterialInterface* Lens = K < 0.9999f ? SettingsPanelMaterial.LoadSynchronous() : nullptr;
	UMaterialInterface* Want = Lens ? Lens : Base;
	if (Want && Panel->GetMaterial(0) != Want)
	{
		Panel->SetMaterial(0, Want);
	}
	if (Lens)
	{
		if (UMaterialInstanceDynamic* MID = Panel->GetMaterialInstance())
		{
			const FTransform Cam = GetLens() ? GetLens()->GetComponentTransform() : MenuCamera->GetActorTransform();
			MID->SetScalarParameterValue(TEXT("LensK"), K);
			MID->SetVectorParameterValue(TEXT("LensCamPos"), FLinearColor(Cam.GetLocation()));
			MID->SetVectorParameterValue(TEXT("LensCamFwd"), FLinearColor(Cam.GetRotation().GetForwardVector()));
		}
	}
}

void AVisexMenuDirector::PushLayoutToWidget()
{
	if (UVisexMenuWidget* W = GetMenuWidget())
	{
		W->TitleSize = Layout.LogoSize;
		W->TitleLetterSpacing = Layout.LogoSpacing;
		W->LogoGap = Layout.LogoGap;
		W->ItemSize = Layout.ItemSize;
		W->ItemGap = Layout.ItemGap;
		W->BuildLabel = Layout.bBuildNumber ? GetDefault<UVisexMenuWidget>()->BuildLabel : FText::GetEmpty();
		if (Page == 0)
		{
			W->Title = FText::FromString(Layout.LogoText());
			W->Refresh();
		}
		MarkPanel();
	}
}

void AVisexMenuDirector::ApplyLayout(const FVisexMenuLayout& InLayout)
{
	Layout = InLayout;
	bHaveLayout = true;
	PanelOffset = Layout.Offset;
	PanelYaw = Layout.Yaw;
	PanelPitch = Layout.Pitch;
	PanelRoll = Layout.Roll;
	PanelScale = Layout.Scale;
	if (Panel && MenuCamera)
	{
		PlacePanel();
	}
}

void AVisexMenuDirector::ToggleLayoutEditor()
{
	// the start menu's own page only: not settings, not the body page, not in play
	if (!LayoutEditor && (!bOpen || Page > 0))
	{
		return;
	}
	if (!LayoutEditor)
	{
		LayoutEditor = GetWorld()->SpawnActor<AVisexMenuLayoutEditor>();
	}
	if (LayoutEditor)
	{
		LayoutEditor->Toggle(this);
	}
}

void AVisexMenuDirector::MoveSelection(int32 Delta)
{
	if (!bOpen || PanelInAlpha < 1.f)
	{
		return;
	}
	if (UVisexMenuWidget* W = GetMenuWidget())
	{
		W->SetSelected(W->GetSelected() + Delta);
		MarkPanel();
	}
}

void AVisexMenuDirector::Confirm()
{
	if (!bOpen || PanelInAlpha < 1.f)
	{
		return;
	}
	if (bArriving)
	{
		// the panel is in but she is still walking: it goes through once she is down
		ArrivalQueued = 1;
		return;
	}
	const UVisexMenuWidget* W = GetMenuWidget();
	const int32 Row = W ? W->GetSelected() : 0;
	if (Page > 0)
	{
		// on a value row CROSS steps it on; on Back it goes back
		const bool bBack = Page == 1 ? FVisexSettings::IsBack(Row) : FVisexSettings::IsVisualBack(Row);
		if (bBack) { CloseSettings(); } else { SettingsStep(1); }
		return;
	}
	OnItemChosen(Row);
}

void AVisexMenuDirector::OpenSettings()
{
	// only from the open menu with her settled: not while she steps out to or walks back from the
	// column (StepDir), where ShowMenu would later reset the page under a sitting figure. Input
	// already waits for bOpen; this also covers Blueprint and script callers
	if (!bOpen || StepDir != 0 || Page > 0)
	{
		return;
	}
	Page = 1;
	BeginMotion(TEXT("settings in"));
	ShowSettingsPage(0);
	SitForSettings();
}

void AVisexMenuDirector::ShowSettingsPage(int32 Select)
{
	MarkPanel();
	if (UVisexMenuWidget* W = GetMenuWidget())
	{
		const TArray<FText> Tabs = {FText::FromString(TEXT("GRAPHICS")), FText::FromString(TEXT("VISUALS"))};
		W->SetPage(FText::FromString(TEXT("SETTINGS")), Tabs, Page - 1,
			Page == 1 ? FVisexSettings::Labels() : FVisexSettings::VisualLabels(),
			Page == 1 ? FVisexSettings::Values() : FVisexSettings::VisualValues(), Select);
	}
}

void AVisexMenuDirector::CloseSettings()
{
	MarkPanel();
	FVisexSettings::Save();
	Page = 0;
	BeginMotion(TEXT("settings out"));
	StandFromSettings();
	if (UVisexMenuWidget* W = GetMenuWidget())
	{
		// back on the main menu, on the Settings item
		const UVisexMenuWidget* Defaults = W->GetClass()->GetDefaultObject<UVisexMenuWidget>();
		W->SetPage(FText::FromString(Layout.LogoText()), {}, 0, Defaults->Items, {}, ItemSettings);
	}
}

void AVisexMenuDirector::SettingsStep(int32 Dir)
{
	UVisexMenuWidget* W = GetMenuWidget();
	if (!bOpen || Page == 0 || !W)
	{
		return;
	}
	if (Page == 1)
	{
		FVisexSettings::Adjust(W->GetSelected(), Dir);
		W->SetValues(FVisexSettings::Values());   // the preset row follows the others (Custom)
		MarkPanel();
	}
	else
	{
		FVisexSettings::AdjustVisual(GetWorld(), W->GetSelected(), Dir);
		W->SetValues(FVisexSettings::VisualValues());
		MarkPanel();
	}
}

void AVisexMenuDirector::SettingsTab(int32 Dir)
{
	if (!bOpen || Page == 0)
	{
		return;
	}
	Page = Page == 1 ? 2 : 1;
	ShowSettingsPage(0);
}

void AVisexMenuDirector::OnItemChosen_Implementation(int32 Index)
{
	switch (Index)
	{
	case ItemBody:
		OpenBody();
		break;
	case ItemWardrobe:
		OpenWardrobe();
		break;
	case ItemPhoto:
		OpenPhoto();
		break;
	case ItemSettings:
		OpenSettings();
		break;
	case ItemQuit:
		UKismetSystemLibrary::QuitGame(this, UGameplayStatics::GetPlayerController(this, 0), EQuitPreference::Quit, false);
		break;
	default:
		StartGame();
		break;
	}
}

void AVisexMenuDirector::OpenCharacterPage(int32 InPage)
{
	CharacterPage = InPage;
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!bOpen || !PC)
	{
		return;
	}
	BeginMotion(InPage == 1 ? TEXT("wardrobe in") : TEXT("body in"));
	StepThen = 0;
	// she pushes off the column first; the page opens as she finds her feet
	const float Up = StandUp(RootMoves() && LeanColumn != nullptr);
	if (Up > 0.f)
	{
		bOpen = false;
		FadeOut = 0.f;
		DisableInput(PC);
		if (LeanColumn)
		{
			// she pushes off, then walks clear of the stone: turning her on the page swept her
			// legs into it. The page opens where she stops.
			GetWorldTimerManager().SetTimer(StandTimer, this, &AVisexMenuDirector::BeginStepOut,
				RootMoves() ? LeanUpHandOff : FMath::Clamp(Up * StandUpHandOver, 0.02f, FMath::Max(0.02f, WalkOffMaxDelay)), false);
			return;
		}
		GetWorldTimerManager().SetTimer(StandTimer, this, &AVisexMenuDirector::OpenBodyNow, Up * 0.8f, false);
		return;
	}
	OpenBodyNow();
}

void AVisexMenuDirector::OpenBodyNow()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		return;
	}
	if (!BodyEditor)
	{
		UClass* Cls = BodyEditorClass ? BodyEditorClass.Get() : AVisexBodyEditor::StaticClass();
		BodyEditor = GetWorld()->SpawnActor<AVisexBodyEditor>(Cls, GetActorTransform());
		if (BodyEditor)
		{
			BodyEditor->OnClosed.AddDynamic(this, &AVisexMenuDirector::HandleBodyClosed);
		}
	}
	if (BodyEditor)
	{
		BodyEditor->BlendTime = PageBlendTime;   // the same camera move onto every page, and back
		// the page fills the window from the first frame of the blend onto it, as the menu does:
		// a blend letterboxes while either view does, so the bars snapped on as it started
		UnboxPageCamera(false);
		UnboxLens(BodyEditor->FindComponentByClass<UCineCameraComponent>());
		BodyEditor->OpenPage(CharacterPage);
	}
	if (!BodyEditor || !BodyEditor->IsOpen())
	{
		// no body to edit on this pawn: back to the menu
		EnableInput(PC);
		ShowMenu();
		return;
	}
	bStudio = true;
	StudioStrength = 1.f;
	EndMotion(BodyEditor->BlendTime);   // the page's camera blend
	// the menu steps aside while the body page has the screen
	bOpen = false;
	Panel->SetVisibility(false);
	DisableInput(PC);
}

float AVisexMenuDirector::ColumnGapAt(FVector Point) const
{
	const UStaticMeshComponent* Stone = LeanColumn ? LeanColumn->FindComponentByClass<UStaticMeshComponent>() : nullptr;
	if (!Stone)
	{
		return BIG_NUMBER;
	}
	// the drum lies along its actor's X, its axis at the stone's pivot: square off it on the ground
	const FVector Along = LeanColumn->GetActorForwardVector().GetSafeNormal2D();
	FVector D = Point - Stone->GetComponentLocation();
	D.Z = 0.f;
	D -= Along * FVector::DotProduct(D, Along);
	return D.Size() - LeanColumnSurfaceRadius;
}

float AVisexMenuDirector::MeasureReach() const
{
	const ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	const USkeletalMeshComponent* Mesh = Char ? Char->GetMesh() : nullptr;
	if (!Mesh)
	{
		return 0.f;
	}
	// her farthest bone from the axis the page turns her on (fingers and toes too; the IK
	// targets and virtual bones aren't her), then the flesh and cloth past it
	const FVector Axis = Char->GetActorLocation();
	float Reach = 0.f;
	for (int32 i = 0; i < Mesh->GetNumBones(); ++i)
	{
		const FName Bone = Mesh->GetBoneName(i);
		const FString Name = Bone.ToString();
		if (Name.StartsWith(TEXT("ik_")) || Name.StartsWith(TEXT("VB ")) || Bone == TEXT("root"))
		{
			continue;
		}
		Reach = FMath::Max(Reach, FVector::Dist2D(Mesh->GetBoneLocation(Bone), Axis));
	}
	const float S = Mesh->GetComponentScale().Z / FMath::Max(LeanAuthoredScale, 0.01f);
	return Reach + BodyReachMargin * S;
}

void AVisexMenuDirector::BeginStepOut()
{
	ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Char || !LeanColumn || bOpen)
	{
		StepThen == 1 ? OpenPhotoNow() : OpenBodyNow();
		return;
	}
	// away from the drum, square off its axis (her facing, give or take)
	const FVector Along = LeanColumn->GetActorForwardVector().GetSafeNormal2D();
	FVector Away = Char->GetActorLocation() - LeanColumn->GetActorLocation();
	Away.Z = 0.f;
	Away -= Along * FVector::DotProduct(Away, Along);
	StepAway = Away.GetSafeNormal2D();
	if (StepAway.IsNearlyZero())
	{
		StepAway = Char->GetActorForwardVector().GetSafeNormal2D();
	}
	// her reach with the sliders at their ends: Height scales all of her, Shoulders and Hips
	// set where her arms hang. The page can take her there while she turns.
	float Grow = 1.f, Wide = 1.f;
	if (const UVisexBodyComponent* Body = Char->FindComponentByClass<UVisexBodyComponent>())
	{
		auto ToEnd = [Body](const TCHAR* Name)
		{
			const FVisexBodySlider* Row = FVisexBodyProfile::Find(Name);
			const float Now = Body->GetValue(Name);
			return Row && Now > KINDA_SMALL_NUMBER ? FMath::Max(1.f, Row->Max / Now) : 1.f;
		};
		Grow = ToEnd(TEXT("Height"));
		Wide = FMath::Max(ToEnd(TEXT("Shoulders")), ToEnd(TEXT("Hips")));
	}
	const float Reach = MeasureReach();
	BodyReach = Reach * Grow * (1.f + BodyReachWidthShare * (Wide - 1.f));
	BodyStepTarget = FMath::Max(BodyReach + BodyClearance, BodyWalkGap);
	UE_LOG(LogTemp, Log, TEXT("Body step-out: reach %.1f now, %.1f at the sliders' ends; gap %.1f -> %.1f"),
		Reach, BodyReach, ColumnGapAt(Char->GetActorLocation()), BodyStepTarget);
	if (BeginRootStepOut())
	{
		// the walk's root motion takes her out, along her facing
		UCharacterMovementComponent* Move = Char->GetCharacterMovement();
		bStepOrient = Move->bOrientRotationToMovement;
		Move->bOrientRotationToMovement = false;
		bSteppedOut = true;
		StepClock = 0.f;
		StepDir = 1;
		return;
	}
	// the rest of the push-off blends out under the first step (its root would hold her)
	if (UAnimInstance* Anim = GetPawnAnim())
	{
		Anim->StopSlotAnimation(0.5f, FName(TEXT("DefaultSlot")));
	}
	UCharacterMovementComponent* Move = Char->GetCharacterMovement();
	bStepOrient = Move->bOrientRotationToMovement;
	Move->bOrientRotationToMovement = false;   // she keeps her facing, out and back
	bSteppedOut = true;
	StepClock = 0.f;
	PlanWalk(BodyStepTarget - ColumnGapAt(Char->GetActorLocation()), ColumnGapAt(Char->GetActorLocation()));
	StepDir = 1;
}

void AVisexMenuDirector::BeginStepBack(float Wait, int32 Select)
{
	ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	bSteppedOut = false;
	if (!Char || !StandIn)
	{
		BackToMenu(Wait, Select);
		return;
	}
	// she turns to the column, walks to her spot while the camera blends home, turns round on
	// it and leans as she did
	StepHome = StandIn->GetActorLocation();
	HomeYaw = StandIn->GetActorRotation().Yaw + 90.f;   // as PlacePawn faces her
	FVector To = StepHome - Char->GetActorLocation();
	To.Z = 0.f;
	TurnFrom = Char->GetActorRotation().Yaw;
	TurnTo = To.Size() >= ReturnWalkMin ? To.Rotation().Yaw : HomeYaw;
	TurnTo = TurnFrom + FMath::FindDeltaAngleDegrees(TurnFrom, TurnTo);
	TurnTime = ReturnTurnTime * FMath::Max(0.35f, FMath::Abs(TurnTo - TurnFrom) / 180.f);
	ReturnPhase = To.Size() >= ReturnWalkMin ? 0 : 2;
	PhaseClock = 0.f;
	PlanWalk(To.Size(), 0.f);
	if (UAnimInstance* Anim = GetPawnAnim())
	{
		Anim->StopSlotAnimation(0.4f, FName(TEXT("DefaultSlot")));
	}
	StepSelect = Select;
	StepMinTime = Wait;
	UCharacterMovementComponent* Move = Char->GetCharacterMovement();
	bStepOrient = Move->bOrientRotationToMovement;
	Move->bOrientRotationToMovement = false;
	StepClock = 0.f;
	StepDir = -1;
	StopMove();
	bRootMove = false;
	MoveSide = 0.f;
	if (RootMoves() && ReturnPhase == 0)
	{
		// the walk back by root motion, planned from where the first turn will leave her feet;
		// that turn aims her along it
		FTurnPick P1;
		const float Need1 = TurnTo - TurnFrom;
		const bool bTurn1 = PickTurn(Need1, true, P1);
		MoveSide = bTurn1 ? FMath::Sign(Need1) : 0.f;
		bRootMove = PlanRootReturn(Char->GetActorLocation(), P1.Clip, P1.Start + P1.End * P1.Rate, false, PlanSegs, MoveYaw, PlanScale);
		if (bRootMove)
		{
			TurnTo = TurnFrom + Need1 + FMath::FindDeltaAngleDegrees(TurnFrom + Need1, MoveYaw);
		}
	}
	if (!StartTurnClip(ReturnPhase == 0))
	{
		// too small a turn for a clip: she turns as she walks, or eases round on her spot (a
		// root-motion walk sets off at once, steering the turn)
		if (ReturnPhase == 0 && !bRootMove)
		{
			ReturnPhase = 1;
		}
	}
}

bool AVisexMenuDirector::StartTurnClip(bool bThenWalk)
{
	TurnClip = nullptr;
	TurnMontage = nullptr;
	const float Need = TurnTo - TurnFrom;
	UAnimInstance* Anim = GetPawnAnim();
	FTurnPick P;
	if (!Anim || !PickTurn(Need, bThenWalk, P))
	{
		UE_LOG(LogTemp, Log, TEXT("Return turn: need %.1f deg, eased (no clip)"), Need);
		return false;
	}
	TurnClip = P.Clip;
	TurnCurve = MoveTemp(P.Curve);
	TurnDt = P.Dt;
	TurnScale = P.Scale;
	TurnStart = P.Start;
	TurnRate = P.Rate;
	TurnEnd = P.End;
	TurnLeanAt = P.LeanAt;
	bTurnThenWalk = bThenWalk;
	if (!bTurnRootSaved)
	{
		TurnRootMotion = Anim->RootMotionMode;
		bTurnRootSaved = true;
	}
	Anim->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);   // the yaw is put on her actor here
	TurnMontage = Anim->PlaySlotAnimationAsDynamicMontage(TurnClip, FName(TEXT("DefaultSlot")), 0.25f, bThenWalk ? 0.35f : 0.25f, TurnRate, 1, -1.f, TurnStart);
	if (TurnMontage)
	{
		// never its own root yaw on top of hers, whatever the root motion mode is when it blends out
		TurnMontage->bEnableRootMotionTranslation = false;
		TurnMontage->bEnableRootMotionRotation = false;
	}
	UE_LOG(LogTemp, Log, TEXT("Return turn: need %.1f deg, %s (%.1f deg, x%.2f), done at %.2f of %.2f s"),
		Need, *TurnClip->GetName(), TurnCurve.Last(), TurnScale, TurnEnd, TurnClip->GetPlayLength());
	return true;
}

float AVisexMenuDirector::TurnYawAt(float T) const
{
	if (TurnCurve.Num() == 0)
	{
		return 0.f;
	}
	const float X = FMath::Clamp(T / FMath::Max(1e-4f, TurnDt), 0.f, float(TurnCurve.Num() - 1));
	const int32 I = FMath::Min(FMath::FloorToInt(X), TurnCurve.Num() - 2);
	return I < 0 ? TurnCurve[0] : FMath::Lerp(TurnCurve[I], TurnCurve[I + 1], X - I);
}

void AVisexMenuDirector::RestoreTurnRootMotion()
{
	if (bTurnRootSaved)
	{
		if (UAnimInstance* Anim = GetPawnAnim())
		{
			Anim->SetRootMotionMode(TurnRootMotion);
		}
		bTurnRootSaved = false;
	}
}

void AVisexMenuDirector::TickStep(float DeltaSeconds)
{
	ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Char)
	{
		StepDir = 0;
		return;
	}
	StepClock += DeltaSeconds;
	if (StepDir < 0)
	{
		TickReturn(DeltaSeconds);
		return;
	}
	if (bRootMove)
	{
		// the clips walk her: the page opens as she comes to a stop
		if (!IsMoving() || MoveLeft() <= PageOpenEarly)
		{
			EndStep();
		}
		return;
	}
	const float Gap = ColumnGapAt(Char->GetActorLocation());
	const float Left = BodyStepTarget - Gap;
	if (Left <= 1.f || StepClock > WalkTime + 0.8f)
	{
		EndStep();
		return;
	}
	// her catwalk, eased in and out over the whole walk, landing on her mark
	const float Want = WalkWant(StepClock, Gap - WalkFrom);
	UCharacterMovementComponent* Move = Char->GetCharacterMovement();
	// her pace is the plan's, exactly (the movement's own acceleration lagged it, then overshot)
	Move->Velocity = StepAway * Want + FVector(0.f, 0.f, Move->Velocity.Z);
	Char->AddMovementInput(StepAway, FMath::Clamp(Want / FMath::Max(1.f, Move->GetMaxSpeed()), 0.f, 1.f));
}

void AVisexMenuDirector::HoldGait(bool bHold)
{
	// her catwalk posture (chest up, shoulders back) by her speed, but never below the floor
	// while she turns and walks between the lean and a page; -1 hands it back to her speed
	UVisexAnimInstance* Anim = Cast<UVisexAnimInstance>(GetPawnAnim());
	if (!Anim || (!bHold && !bGaitHeld))
	{
		return;
	}
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	const float Speed = Pawn ? float(Pawn->GetVelocity().Size2D()) : 0.f;
	Anim->PhotoGait = bHold ? FMath::Max(MenuGaitFloor, FMath::SmoothStep(30.f, 150.f, Speed)) : -1.f;
	bGaitHeld = bHold;
}

void AVisexMenuDirector::UnboxPageCamera(bool bUnbox)
{
	// a blend between two views letterboxes while either does (FMinimalViewInfo::BlendViewInfo
	// ORs bConstrainAspectRatio): the page cameras box to their 16:9 sensor, the menu camera fills
	// the window, so the bars snapped off as the blend home finished, a jump mid-walk (QA 0558).
	// (and on as the blend onto a page started). The page's camera fills the window too, from the
	// blend onto it to the blend home, keeping its horizontal field (a taller sensor where the
	// window is narrower than 16:9: the bars turn into more of the same shot). Each open starts
	// from the page's own lens.
	if (!bUnbox)
	{
		if (UCineCameraComponent* Lens = UnboxedLens.Get())
		{
			Lens->Filmback = UnboxedFilmback;
			Lens->bConstrainAspectRatio = bUnboxedConstrain;
		}
		UnboxedLens = nullptr;
		return;
	}
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	APlayerCameraManager* Cam = PC ? PC->PlayerCameraManager.Get() : nullptr;
	// the page's view: incoming while the blend onto it runs, else the current (or outgoing,
	// mid-blend home: GetViewTarget would return the incoming menu camera then)
	AActor* PageView = Cam ? (Cam->PendingViewTarget.Target && Cam->PendingViewTarget.Target != MenuCamera
		? Cam->PendingViewTarget.Target.Get() : Cam->ViewTarget.Target.Get()) : nullptr;
	UnboxLens(PageView && PageView != MenuCamera ? PageView->FindComponentByClass<UCineCameraComponent>() : nullptr);
}

void AVisexMenuDirector::UnboxLens(UCineCameraComponent* Lens)
{
	if (!Lens || !bMenuFillsWindow || !Lens->bConstrainAspectRatio || Lens->Filmback.SensorHeight <= 0.f)
	{
		return;
	}
	UnboxedLens = Lens;
	UnboxedFilmback = Lens->Filmback;
	bUnboxedConstrain = Lens->bConstrainAspectRatio;
	const float Window = ViewAspect();
	if (Window < Lens->Filmback.SensorWidth / Lens->Filmback.SensorHeight)
	{
		Lens->Filmback.SensorHeight = Lens->Filmback.SensorWidth / Window;
	}
	Lens->bConstrainAspectRatio = false;
}

void AVisexMenuDirector::PlanWalk(float Dist, float From)
{
	// smootherstep: the peak pace is 1.875x the mean, so this takes her to MenuWalkSpeed
	WalkDist = FMath::Max(0.f, Dist);
	WalkFrom = From;
	WalkTime = FMath::Max(0.4f, 1.875f * WalkDist / FMath::Max(10.f, MenuWalkSpeed));
}

float AVisexMenuDirector::WalkWant(float T, float Done) const
{
	const float U = FMath::Clamp(T / FMath::Max(0.05f, WalkTime), 0.f, 1.f);
	const float Plan = WalkDist * U * U * U * (U * (6.f * U - 15.f) + 10.f);
	const float Pace = WalkDist / FMath::Max(0.05f, WalkTime) * 30.f * U * U * (1.f - U) * (1.f - U);
	// the plan's pace, and a pull back onto the plan where her movement lags or leads it
	return FMath::Clamp(Pace + 2.f * (Plan - Done), 0.f, MenuWalkSpeed * 1.1f);
}

void AVisexMenuDirector::TickReturn(float DeltaSeconds)
{
	ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	PhaseClock += DeltaSeconds;
	FVector To = StepHome - Char->GetActorLocation();
	To.Z = 0.f;
	const float Left = To.Size();
	auto Ease = [](float X) { X = FMath::Clamp(X, 0.f, 1.f); return X * X * X * (X * (6.f * X - 15.f) + 10.f); };
	const UCharacterMovementComponent* Move = Char->GetCharacterMovement();
	const float MaxSpeed = FMath::Max(1.f, Move->GetMaxSpeed());
	// a turn clip: her actor takes the clip's root yaw, scaled onto the turn she needs, and the
	// last of it settles onto the exact facing
	auto TickClipTurn = [&]() -> bool
	{
		const float T = FMath::Min(PhaseClock, TurnEnd);
		const float ClipYaw = TurnYawAt(TurnStart + T * TurnRate) * TurnScale;
		// into a walk the clip's yaw runs on and the walk steers the rest; onto the lean the
		// last of it settles on the exact facing
		const float Fix = bTurnThenWalk ? 0.f : FMath::Clamp((T - (TurnEnd - 0.3f)) / 0.3f, 0.f, 1.f);
		const float Yaw = FMath::Lerp(ClipYaw, TurnTo - TurnFrom, Ease(Fix));
		Char->SetActorRotation(FRotator(0.f, TurnFrom + Yaw, 0.f));
		return PhaseClock >= TurnEnd;
	};
	if (ReturnPhase == 0)
	{
		// she turns to her spot in place (the clip's own steps), then walks
		if (bRootMove && (!TurnClip || TickClipTurn()))
		{
			// the walk takes over from the turn's stance, its planted left foot held
			UAnimInstance* Anim = GetPawnAnim();
			UAnimSequence* From = TurnClip;
			const float At = From && Anim && TurnMontage ? Anim->Montage_GetPosition(TurnMontage) : 0.f;
			RestoreTurnRootMotion();
			TurnClip = nullptr;
			ReturnPhase = 1;
			PhaseClock = 0.f;
			PlayRootMotionMove(PlanSegs, From, At, ReturnWalkRate);
			Char->SetAnimRootMotionTranslationScale(PlanScale);
			return;
		}
		if (!TurnClip || TickClipTurn())
		{
			RestoreTurnRootMotion();
			if (UAnimInstance* Anim = GetPawnAnim())
			{
				Anim->StopSlotAnimation(0.35f, FName(TEXT("DefaultSlot")));
			}
			TurnClip = nullptr;
			ReturnPhase = 1;
			PhaseClock = 0.f;
			PlanWalk(Left, 0.f);
		}
		return;
	}
	if (ReturnPhase == 1 && bRootMove)
	{
		// the clips walk her; she keeps to the planned heading, and turns once stopped
		Char->SetActorRotation(FRotator(0.f, FMath::FixedTurn(Char->GetActorRotation().Yaw, MoveYaw, 90.f * DeltaSeconds), 0.f));
		if (MoveAtEnd() || !IsMoving())
		{
			BeginRootTurnHome();
		}
		return;
	}
	if (ReturnPhase == 1)
	{
		// her normal walk to the spot, facing where she goes: a calm start, then the pace that
		// settles to rest right on it
		// (the arrival: already at her pace as she comes into frame, settling onto the spot; the
		// return from a page: the same eased walk as the way out)
		constexpr float Settle = 90.f;   // cm/s^2
		const float Ramp = FMath::SmoothStep(0.f, 1.f, PhaseClock / FMath::Max(0.05f, WalkOffRamp));
		const float Want = bArriving ? FMath::Min(ArrivalSpeed * Ramp, FMath::Sqrt(2.f * Settle * FMath::Max(0.f, Left)))
			: WalkWant(PhaseClock, WalkDist - Left);
		if (Left > 8.f && FVector::DotProduct(To, Char->GetActorForwardVector()) > 0.f)
		{
			// the arrival steers at a steady rate; the return eases the last of its turn out
			const float Yaw = bArriving ? FMath::FixedTurn(Char->GetActorRotation().Yaw, To.Rotation().Yaw, 160.f * DeltaSeconds)
				: FMath::RInterpTo(Char->GetActorRotation(), FRotator(0.f, To.Rotation().Yaw, 0.f), DeltaSeconds, 6.f).Yaw;
			Char->SetActorRotation(FRotator(0.f, Yaw, 0.f));
		}
		// past her spot (a long frame) counts as on it: never a step back onto it
		const bool bPast = Left < 15.f && FVector::DotProduct(To, Char->GetActorForwardVector()) < 0.f;
		// (from a page she turns as she comes to rest: the turn's first step is her last)
		if (Left <= (bArriving ? 2.f : ReturnPivotAt) || bPast || StepClock > (bArriving ? 25.f : 12.f) || (!bArriving && PhaseClock > WalkTime + 0.8f))
		{
			// on her spot: the half turn that puts the column at her back
			ReturnPhase = 2;
			PhaseClock = 0.f;
			TurnFrom = Char->GetActorRotation().Yaw;
			TurnTo = TurnFrom + FMath::FindDeltaAngleDegrees(TurnFrom, HomeYaw);
			TurnTime = ReturnTurnTime * FMath::Max(0.35f, FMath::Abs(TurnTo - TurnFrom) / 180.f);
			StartTurnClip(false);
			return;
		}
		if (!bArriving)
		{
			UCharacterMovementComponent* Walk = Char->GetCharacterMovement();
			Walk->Velocity = Char->GetActorForwardVector().GetSafeNormal2D() * Want + FVector(0.f, 0.f, Walk->Velocity.Z);
		}
		Char->AddMovementInput(To.GetSafeNormal2D(), FMath::Clamp(Want / MaxSpeed, bArriving ? 0.02f : 0.f, 1.f));
		return;
	}
	if (TurnClip)
	{
		// turning round on her spot, held on it, into the lean (from a page she pivots on her last
		// step in: drawn onto the spot at about the pace she walked in at)
		if (Left > 0.5f && !bRootMove)
		{
			Char->AddActorWorldOffset(To * FMath::Min(1.f, DeltaSeconds * 4.f));
		}
		if (TurnLeanAt >= 0.f && PhaseClock >= TurnLeanAt && !bLeaning)
		{
			// the sink takes over the turn's last steps: her last step lands into the lean (root
			// motion: from the turn's stance, its planted left foot held)
			UAnimInstance* Anim = GetPawnAnim();
			if (bRootMove && Anim && TurnMontage)
			{
				LeanWithHold(TurnClip, Anim->Montage_GetPosition(TurnMontage));
			}
			else
			{
				Lean(true, LeanDownFrom, LeanDownRate);
			}
		}
		if (TickClipTurn())
		{
			EndStep();
		}
		return;
	}
	// a small turn: round on her spot through small side steps, drawn back onto it as they fade
	const float A = PhaseClock / FMath::Max(0.05f, TurnTime);
	Char->SetActorRotation(FRotator(0.f, FMath::Lerp(TurnFrom, TurnTo, Ease(A)), 0.f));
	const float Side = FMath::Sign(TurnTo - TurnFrom) * ReturnShuffleSpeed * FMath::Sin(PI * FMath::Clamp(A, 0.f, 1.f));
	const FVector Tangent = FVector::CrossProduct(FVector::UpVector, Char->GetActorForwardVector()).GetSafeNormal2D();
	const FVector Want = Tangent * Side + To * 2.5f;
	if (bRootMove)
	{
		// (the root-motion walk is already leaning in from its stop)
	}
	else if (A < 0.85f && Want.Size() > 3.f)
	{
		Char->AddMovementInput(Want.GetSafeNormal2D(), FMath::Clamp(Want.Size() / MaxSpeed, 0.f, 1.f));
	}
	else if (Left > 0.5f)
	{
		Char->AddActorWorldOffset(To * FMath::Min(1.f, DeltaSeconds * 4.f));
	}
	if (A >= 1.f)
	{
		EndStep();
	}
}

void AVisexMenuDirector::EndStep()
{
	const int32 Was = StepDir;
	StepDir = 0;
	// (a walk out plays on to its stop under the page)
	if (Was < 0)
	{
		StopMove();
	}
	bRootMove = false;
	HoldGait(false);   // before a page opens: photo mode drives the layer itself
	RestoreTurnRootMotion();
	TurnClip = nullptr;
	if (ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Char->GetCharacterMovement()->bOrientRotationToMovement = bStepOrient;
		UE_LOG(LogTemp, Log, TEXT("Body step %s: gap to the stone %.1f after %.2f s"),
			Was > 0 ? TEXT("out") : TEXT("back"), ColumnGapAt(Char->GetActorLocation()), StepClock);
	}
	if (Was > 0)
	{
		StepThen == 1 ? OpenPhotoNow() : OpenBodyNow();
		return;
	}
	if (bArriving)
	{
		EndArrival();
		return;
	}
	// home: the menu (and her lean) once the camera has blended back too
	BackToMenu(FMath::Max(0.05f, StepMinTime - StepClock), StepSelect);
}

bool AVisexMenuDirector::BeginArrival()
{
	ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Char || !StandIn || !bLeanOnColumn || !LeanColumn || !LeanIdle.LoadSynchronous())
	{
		return false;
	}
	// her spot and facing on the lean, as PlacePawn puts her; she starts out on the sand
	StepHome = StandIn->GetActorLocation();
	HomeYaw = StandIn->GetActorRotation().Yaw + 90.f;
	FVector Start(ArrivalStart.X, ArrivalStart.Y, StepHome.Z);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MenuArrival), false, Char);
	Params.AddIgnoredActor(LeanColumn);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start + FVector(0.f, 0.f, 300.f), Start - FVector(0.f, 0.f, 600.f), ECC_Visibility, Params))
	{
		Start.Z = Hit.Location.Z;
	}
	// out of frame, mid-stride, heading for her spot; TickReturn steers her on to it from there
	const FVector Dir = (StepHome - Start).GetSafeNormal2D();
	const float StartYaw = Dir.Rotation().Yaw;
	const float Half = Char->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	Char->SetActorLocationAndRotation(Start + FVector(0.f, 0.f, Half), FRotator(0.f, StartYaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	GetWorldTimerManager().ClearTimer(StandTimer);
	GetWorldTimerManager().ClearTimer(BeatTimer);
	if (UAnimInstance* Anim = GetPawnAnim())
	{
		Anim->StopSlotAnimation(0.f, FName(TEXT("DefaultSlot")));
	}
	bLeaning = false;
	// the lean-return's walk (TickReturn), already at her pace as she comes into the frame
	UCharacterMovementComponent* Move = Char->GetCharacterMovement();
	bStepOrient = Move->bOrientRotationToMovement;
	Move->bOrientRotationToMovement = false;
	Move->Velocity = Dir * ArrivalSpeed;
	bSteppedOut = false;
	ReturnPhase = 1;
	PhaseClock = WalkOffRamp;
	StepClock = 0.f;
	StepMinTime = 0.f;
	StepSelect = 0;
	StepDir = -1;
	bArriving = true;
	bArrivalRan = true;
	ArrivalClock = 0.f;
	ArrivalInFrame = -1.f;
	PanelInAlpha = 0.f;
	ArrivalQueued = 0;
	bPlanted[0] = bPlanted[1] = false;
	SkateMax = SkateSum = 0.f;
	SkateSteps = 0;
	StopMove();
	MoveSide = 0.f;
	bRootMove = RootMoves() && PlanRootReturn(Start, nullptr, 0.f, true, PlanSegs, MoveYaw, PlanScale);
	if (bRootMove)
	{
		// mid-stride in the loop from the first frame: its root motion walks her in
		Char->SetActorRotation(FRotator(0.f, MoveYaw, 0.f));
		PlayRootMotionMove(PlanSegs, nullptr, 0.f, 1.f);
	}
	else
	{
		PreRollWalk(ArrivalPreRoll);
	}
	bArrivalFrozen = false;
	FreezeHer(ArrivalHeld());
	UE_LOG(LogTemp, Log, TEXT("Menu arrival: starts at (%.1f, %.1f, %.1f) yaw %.1f, %.0f cm to her spot at %.0f cm/s, panel %.1f s after she's in frame%s"),
		Char->GetActorLocation().X, Char->GetActorLocation().Y, Char->GetActorLocation().Z, StartYaw,
		FVector::Dist2D(Start, StepHome), ArrivalSpeed, ArrivalPanelFadeStart,
		bArrivalFrozen ? TEXT(", held") : TEXT(""));
	return true;
}

void AVisexMenuDirector::EndArrival()
{
	bArriving = false;
	PlacePawn();   // exactly on her spot (the turn held her within a couple of cm)
	if (!bLeaning)
	{
		Lean(true);
	}
	UE_LOG(LogTemp, Log, TEXT("Menu arrival: leaning at %.2f s, panel %.2f; planted-foot skate mean %.1f max %.1f cm over %d steps"),
		ArrivalClock, PanelInAlpha, SkateSteps ? SkateSum / SkateSteps : 0.f, SkateMax, SkateSteps);
	const int32 Queued = ArrivalQueued;
	ArrivalQueued = 0;
	if (Queued == 1) { Confirm(); }
	else if (Queued == 2) { ToggleMenu(); }
}

bool AVisexMenuDirector::IsArriving() const
{
	return bArriving || (!bArrivalChecked && bShowMenuOnPlay && CVarMenuArrival.GetValueOnGameThread() != 0);
}

bool AVisexMenuDirector::ArrivalHeld() const
{
	// she waits off frame under the launch cover's black until it starts to fade
	const UVisexLaunchCover* Cover = GetGameInstance() ? GetGameInstance()->GetSubsystem<UVisexLaunchCover>() : nullptr;
	return CVarMenuArrivalFreeze.GetValueOnGameThread() != 0 || (Cover && Cover->IsHolding());
}

void AVisexMenuDirector::PreRollWalk(float Seconds)
{
	// frame 0 is mid-stride: walk her up to it in fixed steps (movement and pose) from behind,
	// then put her exactly on it. Same steps every launch, so the same stride phase.
	ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	USkeletalMeshComponent* Mesh = Char ? Char->GetMesh() : nullptr;
	const int32 N = FMath::RoundToInt(Seconds * 60.f);
	if (!Mesh || N <= 0)
	{
		return;
	}
	const FVector At = Char->GetActorLocation();
	const FRotator Yaw = Char->GetActorRotation();
	const FVector Dir = Yaw.Vector();
	UCharacterMovementComponent* Move = Char->GetCharacterMovement();
	const float Input = FMath::Clamp(ArrivalSpeed / FMath::Max(1.f, Move->GetMaxSpeed()), 0.f, 1.f);
	Char->SetActorLocation(At - Dir * ArrivalSpeed * Seconds, false, nullptr, ETeleportType::TeleportPhysics);
	Move->Velocity = Dir * ArrivalSpeed;
	for (int32 I = 0; I < N; ++I)
	{
		Char->AddMovementInput(Dir, Input);
		Move->TickComponent(1.f / 60.f, LEVELTICK_All, nullptr);
		Char->SetActorRotation(Yaw);
		Mesh->TickAnimation(1.f / 60.f, false);
		Mesh->RefreshBoneTransforms();
		Mesh->HandleExistingParallelEvaluationTask(true, true);
	}
	UE_LOG(LogTemp, Log, TEXT("Menu arrival: pre-rolled %.2f s, landed %.1f cm off frame 0, %.0f cm/s"),
		Seconds, FVector::Dist2D(Char->GetActorLocation(), At), Move->Velocity.Size2D());
	Char->SetActorLocation(FVector(At.X, At.Y, Char->GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
}

void AVisexMenuDirector::FreezeHer(bool bFreeze)
{
	// her movement and pose stop exactly as they are, stride and velocity kept for the release
	// (a zero time dilation let a frame through and zeroed her velocity)
	ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Char || bFreeze == bArrivalFrozen)
	{
		bArrivalFrozen = bFreeze && Char;
		return;
	}
	UCharacterMovementComponent* Move = Char->GetCharacterMovement();
	if (bFreeze) { FrozenVelocity = Move->Velocity; }
	Move->SetComponentTickEnabled(!bFreeze);
	Move->Velocity = FrozenVelocity;
	Char->GetMesh()->bPauseAnims = bFreeze;
	bArrivalFrozen = bFreeze;
}

void AVisexMenuDirector::PlayArrival()
{
	if (bOpen && !bArriving && StepDir == 0 && Page == 0 && !bSitting)
	{
		PlacePawn();
		if (BeginArrival())
		{
			if (UVisexMenuWidget* W = GetMenuWidget()) { W->SetRenderOpacity(0.f); }
			MarkPanel();
		}
	}
}

void AVisexMenuDirector::TickArrivalPanel(float DeltaSeconds)
{
	ArrivalClock += DeltaSeconds;
	if (ArrivalInFrame < 0.f)
	{
		// she walks in from out of frame: the panel follows her in, a beat after she shows
		const ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
		APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
		FVector2D At;
		int32 W = 0, H = 0;
		if (PC) { PC->GetViewportSize(W, H); }
		const bool bIn = Char && W > 0 && PC->ProjectWorldLocationToScreen(Char->GetActorLocation(), At)
			&& At.X > 0.03f * W && At.X < 0.97f * W;
		if (bIn || ArrivalClock >= ArrivalPanelLatest || !bArriving)
		{
			ArrivalInFrame = FMath::Min(ArrivalClock, ArrivalPanelLatest);
			UE_LOG(LogTemp, Log, TEXT("Menu arrival: in frame at %.2f s%s"), ArrivalClock, bIn ? TEXT("") : TEXT(" (not seen, timed)"));
		}
	}
	if (PanelInAlpha < 1.f && ArrivalInFrame >= 0.f)
	{
		// the panel and its corner shade fade in together while she walks
		const float A = FMath::Clamp((ArrivalClock - ArrivalInFrame - ArrivalPanelFadeStart) / FMath::Max(0.05f, ArrivalPanelFadeTime), 0.f, 1.f);
		const float E = A >= 1.f ? 1.f : A * A * (3.f - 2.f * A);
		if (E != PanelInAlpha)
		{
			PanelInAlpha = E;
			if (UVisexMenuWidget* W = GetMenuWidget()) { W->SetRenderOpacity(E); }
			MarkPanel();
			if (E >= 1.f)
			{
				UE_LOG(LogTemp, Log, TEXT("Menu arrival: panel in at %.2f s"), ArrivalClock);
			}
		}
	}
	if (bArriving)
	{
		TickFootSkate();
	}
	else if (PanelInAlpha >= 1.f)
	{
		ArrivalClock = -1.f;
	}
}

void AVisexMenuDirector::ResetSkate()
{
	bPlanted[0] = bPlanted[1] = false;
	SkateMax = SkateSum = 0.f;
	SkateSteps = 0;
}

void AVisexMenuDirector::BeginMotion(const TCHAR* Name)
{
	GetWorldTimerManager().ClearTimer(MotionTimer);
	MotionName = Name;
	MotionStart = GetWorld()->GetTimeSeconds();
	ResetSkate();
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	bMotionBoxed = PC && PC->PlayerCameraManager && PC->PlayerCameraManager->GetCameraCacheView().bConstrainAspectRatio;
}

void AVisexMenuDirector::EndMotion(float SettleIn)
{
	if (MotionStart < 0.0)
	{
		return;
	}
	// settled once the last move (a camera blend, a clip's hand-off) has run out too
	auto Log = [this]()
	{
		if (MotionStart < 0.0) { return; }
		UE_LOG(LogTemp, Log, TEXT("Menu motion: %s %.2f s, planted-foot skate mean %.1f max %.1f cm over %d steps"),
			*MotionName, GetWorld()->GetTimeSeconds() - MotionStart, SkateSteps ? SkateSum / SkateSteps : 0.f, SkateMax, SkateSteps);
		MotionStart = -1.0;
	};
	if (SettleIn > 0.01f)
	{
		GetWorldTimerManager().SetTimer(MotionTimer, FTimerDelegate::CreateWeakLambda(this, Log), SettleIn, false);
		return;
	}
	Log();
}

void AVisexMenuDirector::TickFootSkate()
{
	// a foot counts as planted while it is low and nearly still; its skate is how far it
	// moved on the ground from touch-down to lift-off
	const ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	const USkeletalMeshComponent* Mesh = Char ? Char->GetMesh() : nullptr;
	const float Dt = GetWorld()->GetDeltaSeconds();
	if (!Mesh || Dt <= 0.f)
	{
		return;
	}
	const float FeetZ = Char->GetActorLocation().Z - Char->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	static const FName Bones[2] = {FName(TEXT("ball_l")), FName(TEXT("ball_r"))};
	for (int32 F = 0; F < 2; ++F)
	{
		const FVector P = Mesh->GetSocketLocation(Bones[F]);
		const float Speed = FVector::Dist2D(P, LastFoot[F]) / Dt;
		LastFoot[F] = P;
		const bool bDown = P.Z - FeetZ < 10.f && Speed < 25.f;
		if (bDown && !bPlanted[F])
		{
			PlantAt[F] = P;
			PlantFor[F] = 0.f;
		}
		else if (bDown)
		{
			PlantFor[F] += Dt;
		}
		else if (bPlanted[F] && PlantFor[F] > 0.12f)
		{
			const float Skate = FVector::Dist2D(P, PlantAt[F]);
			SkateMax = FMath::Max(SkateMax, Skate);
			SkateSum += Skate;
			++SkateSteps;
			UE_LOG(LogTemp, Log, TEXT("Menu step: %s planted %.2f s, skate %.1f cm"), *Bones[F].ToString(), PlantFor[F], Skate);
		}
		bPlanted[F] = bDown;
	}
}

void AVisexMenuDirector::OpenPhoto()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!bOpen || !PC)
	{
		return;
	}
	bOpen = false;
	FadeOut = 0.f;
	DisableInput(PC);
	BeginMotion(TEXT("photo in"));
	StepThen = 1;
	// she pushes off the column and walks out as for the body page, then the photo camera
	// takes over: one way of moving for every page
	const float Up = StandUp(RootMoves() && LeanColumn != nullptr);
	if (Up > 0.f)
	{
		if (LeanColumn)
		{
			GetWorldTimerManager().SetTimer(StandTimer, this, &AVisexMenuDirector::BeginStepOut,
				RootMoves() ? LeanUpHandOff : FMath::Clamp(Up * StandUpHandOver, 0.02f, FMath::Max(0.02f, WalkOffMaxDelay)), false);
			return;
		}
		GetWorldTimerManager().SetTimer(StandTimer, this, &AVisexMenuDirector::OpenPhotoNow, Up * 0.8f, false);
		return;
	}
	OpenPhotoNow();
}

void AVisexMenuDirector::OpenPhotoNow()
{
	if (!PhotoMode)
	{
		UClass* Cls = PhotoModeClass ? PhotoModeClass.Get() : AVisexPhotoMode::StaticClass();
		PhotoMode = GetWorld()->SpawnActor<AVisexPhotoMode>(Cls, GetActorTransform());
		if (PhotoMode)
		{
			PhotoMode->OnClosed.AddDynamic(this, &AVisexMenuDirector::HandlePhotoClosed);
		}
	}
	if (PhotoMode)
	{
		PhotoMode->BlendTime = PageBlendTime;
		UnboxPageCamera(false);
		PhotoMode->Open();
		UnboxPageCamera(true);   // its camera is made in Open: unboxed as the blend onto it starts
	}
	if (!PhotoMode || !PhotoMode->IsOpen())
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			EnableInput(PC);
		}
		ShowMenu();
		return;
	}
	Panel->SetVisibility(false);
	// only sand and fog, and a soft light on her
	SetSceneStripped(true);
	bStudio = true;
	StudioStrength = PhotoStudioStrength;
	EndMotion(PhotoMode->BlendTime);
	OnPhotoMode();
}

void AVisexMenuDirector::SetSceneStripped(bool bStrip)
{
	if (bStrip)
	{
		Stripped.Reset();
		// whatever she stands on stays, however it is named
		const AActor* Under = nullptr;
		if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			FHitResult Hit;
			FCollisionQueryParams Q(TEXT("StripFloor"), true, Pawn);
			if (GetWorld()->LineTraceSingleByChannel(Hit, Pawn->GetActorLocation(), Pawn->GetActorLocation() - FVector(0, 0, 1000), ECC_Visibility, Q))
			{
				Under = Hit.GetActor();
			}
		}
		for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
		{
			// labels exist only in editor and non-Shipping builds: Shipping sees "StaticMeshActor_12",
			// stripped the dunes too and she fell through the world. The VisexGround tag
			// (Tools/tag_ground.py) survives cooking; the label check stays for older levels.
			const FString Label = It->GetActorNameOrLabel();
			const bool bGround = *It == Under || It->ActorHasTag(TEXT("VisexGround"))
				|| Label.StartsWith(TEXT("Dunes")) || Label.StartsWith(TEXT("LowerGround")) || Label == TEXT("Plaza")
				|| Label.StartsWith(TEXT("StepRamp"));   // walkable ramp under the pawn-ignoring steps
			if (!bGround && !It->IsHidden())
			{
				// hidden stone keeps its collision unless told: it would catch the photo camera
				// and tilt her floor poses on things that aren't there
				It->SetActorHiddenInGame(true);
				It->SetActorEnableCollision(false);
				Stripped.Add(*It);
			}
		}
		if (LeanColumn && !LeanColumn->IsHidden())
		{
			LeanColumn->SetActorHiddenInGame(true);
			LeanColumn->SetActorEnableCollision(false);
			Stripped.Add(LeanColumn.Get());
		}
	}
	else
	{
		for (const TWeakObjectPtr<AActor>& A : Stripped)
		{
			if (A.IsValid())
			{
				A->SetActorHiddenInGame(false);
				A->SetActorEnableCollision(true);
			}
		}
		Stripped.Reset();
	}
}

void AVisexMenuDirector::HandlePhotoClosed()
{
	// the scene returns, and she goes back to the column
	BeginMotion(TEXT("photo out"));
	UnboxPageCamera(true);
	SetSceneStripped(false);
	bStudio = false;
	const float Blend = PhotoMode ? PhotoMode->BlendTime : 0.f;
	// near enough, she walks back to the column as from the body page; far off, the dip
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	const float Off = Pawn && StandIn ? FVector::Dist2D(Pawn->GetActorLocation(), StandIn->GetActorLocation()) : 0.f;
	if (bLeanOnColumn && LeanColumn && Off > 5.f && Off <= ReturnWalkMax)
	{
		BeginStepBack(Blend, ItemPhoto);
		return;
	}
	BackToMenu(Blend, ItemPhoto);
}

void AVisexMenuDirector::HandleBodyClosed()
{
	bStudio = false;
	const float Blend = BodyEditor ? BodyEditor->BlendTime : 0.f;
	const int32 Row = CharacterPage == 1 ? ItemWardrobe : ItemBody;
	BeginMotion(CharacterPage == 1 ? TEXT("wardrobe out") : TEXT("body out"));
	UnboxPageCamera(true);
	if (bSteppedOut)
	{
		BeginStepBack(Blend, Row);
		return;
	}
	BackToMenu(Blend, Row);
}

void AVisexMenuDirector::BackToMenu(float BlendTime, int32 Select)
{
	// standing up walks her a few metres off the column, and the menu puts her straight back
	// on it: hide that jump under a short fade while the camera blends home
	const float Wait = FMath::Max(0.05f, BlendTime);
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	APlayerCameraManager* Cam = PC ? PC->PlayerCameraManager.Get() : nullptr;
	const bool bMoved = bLeanOnColumn && PC && PC->GetPawn() && StandIn
		&& FVector::Dist2D(PC->GetPawn()->GetActorLocation(), StandIn->GetActorLocation()) > 30.f;
	const float Dip = bMoved ? FMath::Min(ReturnFadeTime, Wait) : 0.f;
	if (Cam && Dip > 0.f)
	{
		Cam->StartCameraFade(0.f, 1.f, Dip, FLinearColor::Black, false, true);
	}
	GetWorldTimerManager().SetTimer(BodyTimer, FTimerDelegate::CreateWeakLambda(this, [this, Select, Dip]()
	{
		APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
		if (PC)
		{
			EnableInput(PC);
		}
		ShowMenu();
		// settled once she rests on the stone
		EndMotion(float(FMath::Max(0.0, LeanSettleAt - GetWorld()->GetTimeSeconds())));
		if (UVisexMenuWidget* W = GetMenuWidget())
		{
			W->SetSelected(Select);
		}
		if (PC && PC->PlayerCameraManager && Dip > 0.f)
		{
			PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, ReturnFadeTime * 1.6f, FLinearColor::Black);
		}
	}), Wait, false);
}

void AVisexMenuDirector::StartGame()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!bOpen || !PC || !PC->GetPawn())
	{
		return;
	}
	bOpen = false;
	FadeOut = 0.f;
	// she pushes off the column while the camera circles round to her back, then walks off on
	// her own; walking and the camera move overlap, both timed off the stand-up clip
	const float Up = StandUp();
	GetWorldTimerManager().SetTimer(HandBackTimer, this, &AVisexMenuDirector::BeginWalkOff,
		FMath::Clamp(Up * StandUpHandOver, 0.02f, FMath::Max(0.02f, WalkOffMaxDelay)), false);
	// tick between the player's input and her movement: the director reads the stick she was
	// given this frame and adds her own walk before the movement component spends it
	AddTickPrerequisiteActor(PC);
	if (ACharacter* Char = Cast<ACharacter>(PC->GetPawn()))
	{
		Char->GetCharacterMovement()->AddTickPrerequisiteActor(this);
	}
	// start of the orbit, in polar terms around her
	const FVector Pivot = PC->GetPawn()->GetActorLocation();
	const FVector S = MenuCamera->GetActorLocation() - Pivot;
	OrbitA0 = FMath::RadiansToDegrees(FMath::Atan2(S.Y, S.X));
	OrbitR0 = FVector2D(S.X, S.Y).Size();
	OrbitZ0 = S.Z;
	// the menu frames her off-centre: keep that framing for the first frames and ease it out,
	// rather than snapping to the orbit's watch on frame one
	const FVector Look = Pivot + FVector(0.f, 0.f, OrbitLookHeight - 90.f);
	OrbitAim0 = (Look - MenuCamera->GetActorLocation()).Rotation().Quaternion().Inverse() * MenuCamera->GetActorQuat();
	OrbitT = 0.f;
	LandedFor = -1.f;
	bOrbiting = true;
}

void AVisexMenuDirector::BeginWalkOff()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn || bOpen)
	{
		return;
	}
	Pawn->EnableInput(PC);
	// the rest of the stand-up blends out under the first steps: its root motion would hold her
	// in place, and the push off the column reads in the blend
	if (UAnimInstance* Anim = GetPawnAnim())
	{
		Anim->StopSlotAnimation(0.5f, FName(TEXT("DefaultSlot")));
	}
	WalkOffDir = Pawn->GetActorForwardVector().GetSafeNormal2D();
	WalkOffClock = 0.f;
	WalkOffWeight = 1.f;
}

void AVisexMenuDirector::TickWalkOff(float DeltaSeconds)
{
	ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Char || bOpen)
	{
		WalkOffClock = -1.f;
		return;
	}
	WalkOffClock += DeltaSeconds;
	// the stick takes her over: her own walk fades out under it instead of snapping off, and the
	// two add up meanwhile, so there's no pop in speed or heading
	const FVector Player = Char->GetPendingMovementInputVector();
	if (Player.SizeSquared() > 0.01f)
	{
		WalkOffWeight = FMath::Max(0.f, WalkOffWeight - DeltaSeconds / 0.6f);
	}
	// once the camera has landed she eases to a stop unless the player walks her on
	if (LandedFor >= 0.f)
	{
		WalkOffWeight = FMath::Min(WalkOffWeight, 1.f - FMath::Clamp(LandedFor / FMath::Max(0.05f, WalkOffCoast), 0.f, 1.f));
	}
	if (WalkOffWeight <= 0.f)
	{
		WalkOffClock = -1.f;
		return;
	}
	const UCharacterMovementComponent* Move = Char->GetCharacterMovement();
	const float Pace = FMath::Clamp(WalkOffSpeed / FMath::Max(1.f, Move->GetMaxSpeed()), 0.f, 1.f);
	const float Ramp = WalkOffRamp > 0.f ? FMath::SmoothStep(0.f, 1.f, WalkOffClock / WalkOffRamp) : 1.f;
	// smooth at both ends: the ease-out of the weight too
	const float W = FMath::SmoothStep(0.f, 1.f, WalkOffWeight);
	Char->AddMovementInput(WalkOffDir, FMath::Max(0.02f, Pace * Ramp) * W);
}

void AVisexMenuDirector::TickFocus()
{
	UCineCameraComponent* Lens = GetLens();
	const ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	const USkeletalMeshComponent* Mesh = Char ? Char->GetMesh() : nullptr;
	if (!Lens || !Mesh)
	{
		return;
	}
	FVector Target = Char->GetActorLocation() + FVector(0.f, 0.f, 60.f);
	for (const FName& Bone : FocusBones)
	{
		if (Mesh->GetBoneIndex(Bone) != INDEX_NONE)
		{
			Target = Mesh->GetSocketLocation(Bone);
			break;
		}
	}
	// sitting for Settings the focus slides partway toward her raised foot, with the camera
	if (SettingsCamAlpha > 0.f && Mesh->GetBoneIndex(SettingsFocusBone) != INDEX_NONE)
	{
		const FVector Foot = Mesh->GetSocketLocation(SettingsFocusBone);
		Target = FMath::Lerp(Target, FMath::Lerp(Target, Foot, SettingsFocusMix), SettingsCamEase());
	}
	// the bones are last frame's (she animates after the director ticks): lead by her velocity
	Target += Char->GetVelocity() * GetWorld()->GetDeltaSeconds();
	// depth of field is sharp on a plane, so focus on her depth along the lens, not the
	// straight-line distance: she sits off-centre in the menu frame
	const FTransform View = Lens->GetComponentTransform();
	const float Depth = FVector::DotProduct(Target - View.GetLocation(), View.GetRotation().GetForwardVector());
	FCameraFocusSettings Focus = Lens->FocusSettings;
	Focus.ManualFocusDistance = FMath::Max(10.f, Depth);
	Lens->FocusSettings = Focus;
}

void AVisexMenuDirector::HandBack()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC && PC->GetPawn())
	{
		PC->GetPawn()->EnableInput(PC);
	}
	SetColumnBlocksPawn(true);
	bPlaying = true;
	LandedFor = 0.f;
	OnGameStarted();
}

void AVisexMenuDirector::ToggleMenu()
{
	if (bOpen && Page > 0)
	{
		// in Settings, Options is Back: she gets up and the camera climbs back to the menu
		CloseSettings();
	}
	else if (bOpen)
	{
		if (PanelInAlpha >= 1.f && bArriving) { ArrivalQueued = 2; }
		else if (PanelInAlpha >= 1.f) { StartGame(); }
	}
	else if (bPlaying)
	{
		bPlaying = false;
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			if (PC->GetPawn())
			{
				PC->GetPawn()->DisableInput(PC);
			}
			if (ACharacter* Char = Cast<ACharacter>(PC->GetPawn()))
			{
				Char->GetCharacterMovement()->StopMovementImmediately();
			}
		}
		// she is somewhere out on the sand: a dip to black covers the trip back to the column
		BackToMenu(0.45f, 0);
	}
}

void AVisexMenuDirector::UpdateAlign()
{
	UVisexMenuWidget* W = bOpen && Panel ? GetMenuWidget() : nullptr;
	UCineCameraComponent* Lens = GetLens();
	if (!W || !Lens)
	{
		return;
	}
	// a vertical line on screen is the plane through the lens holding its up axis; find the line
	// where that plane crosses the panel, as layout x per layout y (the panel is flat: affine)
	const FTransform& T = Panel->GetComponentTransform();
	const FVector2D Draw = Panel->GetCurrentDrawSize();
	const FVector2D Piv = Panel->GetPivot();
	const float SS = FMath::Max(1.f, W->Supersample);
	const float Pad = W->ShadePadding;
	const float Y0 = W->GetAlignY0();
	auto ToWorld = [&](float X, float Y)
	{
		const double U = (Pad + X) * SS, V = (Pad + Y) * SS;
		return T.TransformPosition(FVector(0., -(U - Draw.X * Piv.X), -(V - Draw.Y * Piv.Y)));
	};
	const FVector A = ToWorld(0.f, Y0);
	const FVector N = FVector::CrossProduct(Lens->GetUpVector(), A - Lens->GetComponentLocation());
	const double Dx = FVector::DotProduct(N, ToWorld(1.f, Y0) - A);
	const float Slope = FMath::Abs(Dx) > UE_SMALL_NUMBER ? float(-FVector::DotProduct(N, ToWorld(0.f, Y0 + 1.f) - A) / Dx) : 0.f;
	if (!FMath::IsNearlyEqual(W->AlignSlope, Slope, 1e-4f) || W->bAlignChanged)
	{
		W->AlignSlope = Slope;
		W->bAlignChanged = false;
		MarkPanel();
	}
}

void AVisexMenuDirector::SetCornerShade(float Weight)
{
	// a post-process material's blendable weight doesn't fade it: any weight above 0 draws it at
	// full strength, so the old fade held the darkening to the last frame and popped it off at
	// the hand-over (QA 0259/0269: luma 0.24 -> 0.38). Fade the material's own Strength instead.
	UCineCameraComponent* Lens = GetLens();
	UMaterialInterface* Mat = CornerShade.LoadSynchronous();
	if (!Lens || !Mat)
	{
		return;
	}
	if (!CornerShadeMID)
	{
		CornerShadeMID = UMaterialInstanceDynamic::Create(Mat, this);
		float Base = 0.f;
		CornerShadeBase = Mat->GetScalarParameterValue(FName(TEXT("Strength")), Base) ? Base : 0.93f;
	}
	CornerShadeMID->SetScalarParameterValue(TEXT("Strength"), CornerShadeBase * FMath::Clamp(Weight, 0.f, 1.f));
	const float BlendWeight = Weight > KINDA_SMALL_NUMBER ? 1.f : 0.f;
	for (FWeightedBlendable& B : Lens->PostProcessSettings.WeightedBlendables.Array)
	{
		if (B.Object == CornerShadeMID)
		{
			B.Weight = BlendWeight;
			return;
		}
	}
	Lens->PostProcessSettings.WeightedBlendables.Array.Add(FWeightedBlendable(BlendWeight, CornerShadeMID));
}

void AVisexMenuDirector::ReframeMenu()
{
	bHaveBase = false;
	if (bOpen)
	{
		ShowMenu();
	}
}

UAnimInstance* AVisexMenuDirector::GetPawnAnim() const
{
	const ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	return Char && Char->GetMesh() ? Char->GetMesh()->GetAnimInstance() : nullptr;
}

void AVisexMenuDirector::TickWalkClip(float DeltaSeconds, bool bMenuWalk)
{
	// The menu's walks play a genuinely slow walk (A_PhotoSeq_Walk, ~127 cm/s, 6 cm bob) at the
	// rate her capsule moves, so each planted foot stays put: the locomotion blend space had a
	// 3 m/s stride slowed to ~0.6x, bouncy and skating (Docs/Research/FOOT_IK.md). Turn clips and
	// the lean own the slot while they play; this takes it only once they are done or going.
	ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	UAnimInstance* Anim = GetPawnAnim();
	if (!Char || !Anim)
	{
		return;
	}
	const bool bWalkClip = WalkMontage && Anim->Montage_IsPlaying(WalkMontage);
	bool bOtherClip = false;
	for (const FAnimMontageInstance* Inst : Anim->MontageInstances)
	{
		bOtherClip |= Inst && Inst->Montage && Inst->Montage != WalkMontage && Inst->IsActive() && !Inst->IsStopped();
	}
	const float Speed = Char->GetVelocity().Size2D();
	WalkClipIdle = Speed < 3.f ? WalkClipIdle + DeltaSeconds : 0.f;
	const bool bWant = CVarMenuSlowWalk.GetValueOnGameThread() != 0 && bMenuWalk && !bOtherClip && WalkClipIdle < 0.15f
		&& (bWalkClip || Speed > 8.f);
	if (!bWant)
	{
		if (bWalkClip && !bOtherClip)
		{
			Anim->Montage_Stop(0.45f, WalkMontage);
		}
		return;
	}
	const float Rate = FMath::Clamp(Speed / FMath::Max(10.f, WalkClipSpeed), 0.05f, 1.3f);
	if (!bWalkClip)
	{
		UAnimSequence* Clip = MenuWalkClip.LoadSynchronous();
		if (!Clip)
		{
			return;
		}
		LoadedLean.AddUnique(Clip);
		WalkClipSpeed = float(Clip->ExtractRootMotionFromRange(0.0, Clip->GetPlayLength(), FAnimExtractContext()).GetTranslation().Size2D()
			/ FMath::Max(0.1, double(Clip->GetPlayLength())));
		// from double support, so the first steps start from her stance
		WalkMontage = Anim->PlaySlotAnimationAsDynamicMontage(Clip, FName(TEXT("DefaultSlot")), 0.35f, 0.45f, Rate, 1000000, -1.f, MenuWalkClipStart);
		if (WalkMontage)
		{
			// the capsule moves her (TickStep, TickReturn): the clip's root motion is dropped
			WalkMontage->bEnableRootMotionTranslation = false;
			WalkMontage->bEnableRootMotionRotation = false;
		}
		return;
	}
	Anim->Montage_SetPlayRate(WalkMontage, Rate);
}

void AVisexMenuDirector::SetUpFootTrace()
{
	// the scene got it at BeginPlay (UVisexFootTrace); this adds what the menu spawned since
	UVisexFootTrace::SetUp(GetWorld());
}

void AVisexMenuDirector::SpawnLeanColumn()
{
	const ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (LeanColumn || !bLeanOnColumn || !Char)
	{
		return;
	}
	UStaticMesh* StoneMesh = LeanColumnMesh.LoadSynchronous();
	if (!StoneMesh)
	{
		return;
	}
	// The first menu comes up on the second frame after launch. Shadow the virtual shadow map
	// cached in those first frames went stale and was never invalidated (its own depth pyramid,
	// which culls invalidations, isn't valid yet), so the drum and the sand under her stood in
	// black until the camera left (QA 0401, 0441; with r.Shadow.Virtual.Cache 0 it's lit). Keep
	// the cache off for the launch half second, so it starts from pages drawn with the menu in
	// place. Costs a few uncached shadow frames once; on the core ticker so it comes back even
	// if Play ends first.
	if (IConsoleVariable* Cache = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.Virtual.Cache")))
	{
		const int32 Was = Cache->GetInt();
		if (Was != 0)
		{
			Cache->Set(0, ECVF_SetByCode);
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Was](float)
			{
				if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.Virtual.Cache")))
				{
					V->Set(Was, ECVF_SetByCode);
				}
				return false;
			}), 0.5f);
		}
	}
	// A fallen drum lying across behind her, settled into the sand. Built so the animation side
	// can read it exactly: actor tagged LeanColumn, scale 1, pivot on the top of the stone at the
	// middle of its length, X along the drum, Z up, no roll. FitLeanColumn places and sizes it.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Column = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), Char->GetActorTransform(), Params);
	if (!Column)
	{
		return;
	}
	USceneComponent* Top = NewObject<USceneComponent>(Column, TEXT("Top"));
	Top->SetMobility(EComponentMobility::Movable);
	Column->SetRootComponent(Top);
	Top->RegisterComponent();
	Column->Tags.Add(TEXT("LeanColumn"));

	// SM_LeanColumn_HD: built at size along its X, pivot on its axis. Its collision is its own
	// drawn triangles (complex as simple), so traces land on the stone she is drawn against.
	UStaticMeshComponent* Stone = NewObject<UStaticMeshComponent>(Column, TEXT("Stone"));
	Stone->SetMobility(EComponentMobility::Movable);
	Stone->SetStaticMesh(StoneMesh);
	Stone->SetupAttachment(Top);
	// queries only: the contact traces (Visibility) and the camera hit it; the player capsule
	// only while she walks off (SetColumnBlocksPawn), never at spawn
	Stone->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Stone->SetCollisionObjectType(ECC_WorldStatic);
	Stone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Stone->SetCollisionResponseToAllChannels(ECR_Ignore);
	Stone->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Stone->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
	Stone->RegisterComponent();
	LeanColumn = Column;
	SetColumnBlocksPawn(false);
	FitLeanColumn();
}

void AVisexMenuDirector::FitLeanColumn()
{
	const ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	UStaticMeshComponent* Stone = LeanColumn ? LeanColumn->FindComponentByClass<UStaticMeshComponent>() : nullptr;
	if (!Char || !Char->GetMesh() || !Stone)
	{
		return;
	}
	// Her body: Height scales her whole mesh (VisexBodyComponent::Apply), and the lean clip is
	// posed in her mesh space, so her seat scales with it. Leg length lifts the mesh but the lean
	// keeps her pelvis and hands where they were. Solve the drum that meets the seat at the posed
	// angle while its underside stays in the sand.
	const FVector Feet = Char->GetActorLocation() - FVector(0.f, 0.f, Char->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const FVector Fwd = Char->GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Fwd);
	const USkeletalMeshComponent* Mesh = Char->GetMesh();
	const float S = Mesh->GetComponentScale().Z;
	const float S0 = FMath::Max(LeanAuthoredScale, 0.01f);
	const float SeatZ = S * LeanSeat.X;                         // pelvis bone above her feet
	const float SeatFwd = S * LeanSeat.Y;                       // in front of her root
	const float Flesh = S * LeanSeat.Z;                         // pelvis bone off the stone
	const float C = FMath::Cos(FMath::DegreesToRadians(LeanSeatAngle));
	const float Sn = FMath::Sin(FMath::DegreesToRadians(LeanSeatAngle));
	// seat = axis + (R + flesh) * (sin, cos); axis height = R - settle
	const float R = FMath::Max(20.f, (SeatZ + LeanColumnSettle - Flesh * C) / (1.f + C));
	const float AxisFwd = SeatFwd - (R + Flesh) * Sn;
	const FVector Axis = Feet + Fwd * AxisFwd + Right * (LeanColumnShift * S / S0) + FVector(0.f, 0.f, R - LeanColumnSettle);
	const float K = R / FMath::Max(LeanColumnMeshRadius, 1.f);

	LeanColumn->SetActorLocationAndRotation(Axis + FVector(0.f, 0.f, R), FRotationMatrix::MakeFromXZ(Right, FVector::UpVector).Rotator());
	Stone->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -R), FRotator::ZeroRotator);
	Stone->SetRelativeScale3D(FVector(K));
	Stone->UpdateComponentToWorld();

	// the stone as placed: trace down onto its top along the middle of its length
	TArray<float> Radii;
	FHitResult Hit;
	FCollisionQueryParams Trace(SCENE_QUERY_STAT(LeanColumnSurface), true);
	for (int32 i = -4; i <= 4; ++i)
	{
		for (int32 j = -3; j <= 3; ++j)
		{
			const FVector Along = Right * (LeanColumnLength * K * 0.3f * i / 4.f);
			const float Ang = FMath::DegreesToRadians(12.f * j);
			const FVector Dir = Fwd * FMath::Sin(Ang) + FVector::UpVector * FMath::Cos(Ang);
			if (Stone->LineTraceComponent(Hit, Axis + Along + Dir * (R * 2.f), Axis + Along, Trace))
			{
				const FVector D = Hit.Location - Axis;
				Radii.Add(FVector2D(FVector::DotProduct(D, Fwd), D.Z).Size());
			}
		}
	}
	Radii.Sort();
	LeanColumnSurfaceRadius = Radii.Num() ? Radii.Last() : R;
	UE_LOG(LogTemp, Log, TEXT("LeanColumn fit: body scale %.3f, seat %.1f up %.1f fwd -> radius %.1f (scale %.3f, traced outer %.1f from %d hits), axis %.1f behind her, top %.1f"),
		S, SeatZ, SeatFwd, R, K, LeanColumnSurfaceRadius, Radii.Num(), -AxisFwd, R * 2.f - LeanColumnSettle);
	bColumnFitted = true;
}

void AVisexMenuDirector::SetColumnBlocksPawn(bool bBlock)
{
	if (UStaticMeshComponent* Body = LeanColumn ? LeanColumn->FindComponentByClass<UStaticMeshComponent>() : nullptr)
	{
		// while she leans her capsule sits partly inside it
		Body->SetCollisionResponseToChannel(ECC_Pawn, bBlock ? ECR_Block : ECR_Ignore);
	}
}

void AVisexMenuDirector::Lean(bool bFromStanding, float From, float Rate)
{
	LeanSettleAt = GetWorld()->GetTimeSeconds();
	GetWorldTimerManager().ClearTimer(StandTimer);
	GetWorldTimerManager().ClearTimer(SitTimer);
	bSitting = false;
	UAnimInstance* Anim = GetPawnAnim();
	UAnimSequence* Idle = bLeanOnColumn ? LeanIdle.LoadSynchronous() : nullptr;
	if (!Anim || !Idle)
	{
		bLeaning = false;
		return;
	}
	SetColumnBlocksPawn(false);
	bLeaning = true;
	static const FName Slot(TEXT("DefaultSlot"));
	UAnimSequence* Down = bFromStanding ? LeanDown.LoadSynchronous() : nullptr;
	LoadedLean.AddUnique(Idle);
	if (Down) { LoadedLean.AddUnique(Down); }
	if (Down)
	{
		// The loop cross-fades in over the lean-down's last SitBlend seconds, where she already
		// rests on the stone. A zero-time hand-off at the clip's end let the montage run out a
		// frame or more before the loop started, and her standing base idle flashed through
		// ("lean pop", 0534). LoadedLean keeps the clip alive; the timer holds it weakly too, so
		// if that ever slips it skips the idle instead of playing freed memory (0298)
		From = FMath::Clamp(From, 0.f, FMath::Max(0.f, Down->GetPlayLength() - 0.5f));
		Rate = FMath::Max(0.5f, Rate);
		const float Left = (Down->GetPlayLength() - From) / Rate;
		LeanSettleAt += Left;
		Anim->PlaySlotAnimationAsDynamicMontage(Down, Slot, From > 0.f ? 0.35f : 0.25f, SitBlend, Rate, 1, -1.f, From);
		GetWorldTimerManager().SetTimer(StandTimer, FTimerDelegate::CreateWeakLambda(this, [this, WeakIdle = TWeakObjectPtr<UAnimSequence>(Idle)]()
		{
			UAnimSequence* Clip = WeakIdle.Get();
			UAnimInstance* A = GetPawnAnim();
			if (Clip && A)
			{
				A->PlaySlotAnimationAsDynamicMontage(Clip, Slot, SitBlend, 0.3f, 1.f, 1000000);
			}
		}), FMath::Max(Left - SitBlend, 0.05f), false);
	}
	else
	{
		Anim->PlaySlotAnimationAsDynamicMontage(Idle, Slot, 0.3f, 0.3f, 1.f, 1000000);
	}
	ScheduleIdleBeat();
}

void AVisexMenuDirector::ScheduleIdleBeat()
{
	GetWorldTimerManager().ClearTimer(BeatTimer);
	if (bLeaning && !bSitting && IdleBeats.Num() > 0)
	{
		const float Rest = FMath::FRandRange(IdleBeatRest.X, FMath::Max(IdleBeatRest.X, IdleBeatRest.Y));
		GetWorldTimerManager().SetTimer(BeatTimer, FTimerDelegate::CreateUObject(this, &AVisexMenuDirector::PlayIdleBeat, -1), Rest, false);
	}
}

void AVisexMenuDirector::PlayIdleBeat(int32 Index)
{
	UAnimInstance* Anim = GetPawnAnim();
	if (!bLeaning || bSitting || !Anim || IdleBeats.Num() == 0)
	{
		return;
	}
	if (!IdleBeats.IsValidIndex(Index))
	{
		// never the same beat twice running
		Index = FMath::RandRange(0, IdleBeats.Num() - 1);
		if (Index == LastBeat && IdleBeats.Num() > 1)
		{
			Index = (Index + 1 + FMath::RandRange(0, IdleBeats.Num() - 2)) % IdleBeats.Num();
		}
	}
	UAnimSequence* Clip = IdleBeats[Index].LoadSynchronous();
	UAnimSequence* Idle = LeanIdle.LoadSynchronous();
	if (!Clip || !Idle)
	{
		ScheduleIdleBeat();
		return;
	}
	// held in LoadedLean so GC can't free either while BeatTimer waits (see 0298)
	LoadedLean.AddUnique(Clip);
	LoadedLean.AddUnique(Idle);
	LastBeat = Index;
	static const FName Slot(TEXT("DefaultSlot"));
	// the beat opens and closes on the lean's own pose, so slow cross-fades hide the seams
	constexpr float Blend = 0.6f;
	Anim->PlaySlotAnimationAsDynamicMontage(Clip, Slot, Blend, Blend, 1.f, 1);
	GetWorldTimerManager().SetTimer(BeatTimer, this, &AVisexMenuDirector::EndIdleBeat,
		FMath::Max(Clip->GetPlayLength() - Blend, 0.1f), false);
}

void AVisexMenuDirector::EndIdleBeat()
{
	UAnimInstance* Anim = GetPawnAnim();
	UAnimSequence* Idle = LeanIdle.Get();   // loaded and held in LoadedLean by PlayIdleBeat
	if (!bLeaning || bSitting || !Anim || !Idle)
	{
		return;
	}
	static const FName Slot(TEXT("DefaultSlot"));
	Anim->PlaySlotAnimationAsDynamicMontage(Idle, Slot, 0.6f, 0.3f, 1.f, 1000000);
	ScheduleIdleBeat();
}

float AVisexMenuDirector::StandUp(bool bIntoWalk)
{
	if (!bLeaning)
	{
		return 0.f;
	}
	bLeaning = false;
	GetWorldTimerManager().ClearTimer(StandTimer);
	GetWorldTimerManager().ClearTimer(BeatTimer);
	UAnimInstance* Anim = GetPawnAnim();
	if (!Anim)
	{
		return 0.f;
	}
	static const FName Slot(TEXT("DefaultSlot"));
	if (UAnimSequence* Up = LeanUp.LoadSynchronous())
	{
		LoadedLean.AddUnique(Up);
		Anim->PlaySlotAnimationAsDynamicMontage(Up, Slot, 0.15f, 0.35f, 1.f, 1, bIntoWalk ? 0.f : -1.f);
		return Up->GetPlayLength();
	}
	Anim->StopSlotAnimation(0.4f, Slot);
	return 0.4f;
}

void AVisexMenuDirector::PlayLoopLater(UAnimSequence* Clip, float Delay, bool bThenBeats)
{
	// a cross-fade over the transition's last SitBlend seconds: both clips rest there (PAD), so no
	// seam. The transition blends out over the same time, so the base idle never shows between
	// them (0.15 s let her standing idle flash through the jump's landing). The clip is held in
	// LoadedLean; the timer only holds it weakly (0298)
	const float Blend = SitBlend;
	GetWorldTimerManager().SetTimer(SitTimer, FTimerDelegate::CreateWeakLambda(this, [this, WeakClip = TWeakObjectPtr<UAnimSequence>(Clip), bThenBeats, Blend]()
	{
		UAnimSequence* Loop = WeakClip.Get();
		UAnimInstance* A = GetPawnAnim();
		if (Loop && A)
		{
			static const FName Slot(TEXT("DefaultSlot"));
			A->PlaySlotAnimationAsDynamicMontage(Loop, Slot, Blend, 0.3f, 1.f, 1000000);
		}
		if (bThenBeats)
		{
			ScheduleIdleBeat();
		}
	}), FMath::Max(Delay - Blend, 0.05f), false);
}

float AVisexMenuDirector::SitRate(int32 Warp, float ClipT)
{
	// play rate by clip time. Up (A_Menu_Sit_Settings_Down, 2.5 s): still to 0.35, the crouch to
	// 1.0, the spring to 1.5, the landing on the drum to 2.0, settling after. Down (_Up, 1.7 s):
	// still to 0.5, the hop down to 1.0, rising onto the lean after
	static const FVector2D UpKeys[] = {{0.f, 1.f}, {0.35f, 0.85f}, {0.7f, 0.7f}, {1.f, 0.85f}, {1.25f, 1.f},
		{1.5f, 0.8f}, {1.75f, 0.65f}, {2.f, 0.8f}, {2.5f, 1.f}};
	static const FVector2D DownKeys[] = {{0.f, 0.85f}, {0.5f, 0.7f}, {0.8f, 0.55f}, {1.1f, 0.6f}, {1.7f, 0.7f}};
	const FVector2D* K = Warp == 1 ? UpKeys : DownKeys;
	const int32 N = Warp == 1 ? UE_ARRAY_COUNT(UpKeys) : UE_ARRAY_COUNT(DownKeys);
	if (ClipT <= K[0].X) { return K[0].Y; }
	for (int32 I = 1; I < N; ++I)
	{
		if (ClipT <= K[I].X)
		{
			// eased between keys, so the pace itself never kinks
			const float A = (ClipT - K[I - 1].X) / FMath::Max(1e-3f, K[I].X - K[I - 1].X);
			return FMath::Lerp(K[I - 1].Y, K[I].Y, FMath::SmoothStep(0.f, 1.f, A));
		}
	}
	return K[N - 1].Y;
}

float AVisexMenuDirector::SitWarpTime(int32 Warp, float Len)
{
	float T = 0.f;
	constexpr int32 Steps = 200;
	for (int32 I = 0; I < Steps; ++I)
	{
		T += (Len / Steps) / FMath::Max(0.1f, SitRate(Warp, Len * (I + 0.5f) / Steps));
	}
	return T;
}

void AVisexMenuDirector::TickSitWarp()
{
	UAnimInstance* Anim = GetPawnAnim();
	if (!Anim || !SitMontage || !Anim->Montage_IsPlaying(SitMontage))
	{
		SitWarp = 0;
		SitMontage = nullptr;
		return;
	}
	Anim->Montage_SetPlayRate(SitMontage, SitRate(SitWarp, Anim->Montage_GetPosition(SitMontage)));
}

void AVisexMenuDirector::SitForSettings()
{
	UAnimInstance* Anim = GetPawnAnim();
	UAnimSequence* Sit = bSitForSettings && bLeaning && !bSitting ? SettingsSit.LoadSynchronous() : nullptr;
	if (!Anim || !Sit)
	{
		return;
	}
	bSitting = true;
	GetWorldTimerManager().ClearTimer(StandTimer);   // a lean-down still waiting to start its loop
	GetWorldTimerManager().ClearTimer(BeatTimer);    // no glances or toe play while she sits
	GetWorldTimerManager().ClearTimer(SitTimer);
	LoadedLean.AddUnique(Sit);
	static const FName Slot(TEXT("DefaultSlot"));
	if (UAnimSequence* Down = SettingsSitDown.LoadSynchronous())
	{
		LoadedLean.AddUnique(Down);
		// the way down opens on the lean's own pose: a beat in progress blends into it
		// retimed as it plays (TickSitWarp); the camera dollies in over the same seconds
		const float Dur = SitWarpTime(1, Down->GetPlayLength());
		SitMontage = Anim->PlaySlotAnimationAsDynamicMontage(Down, Slot, 0.5f, SitBlend, SitRate(1, 0.f), 1);
		SitWarp = 1;
		SettingsCamDur = Dur;
		PlayLoopLater(Sit, Dur, false);
		EndMotion(Dur);
	}
	else
	{
		Anim->PlaySlotAnimationAsDynamicMontage(Sit, Slot, 0.8f, 0.3f, 1.f, 1000000);
	}
}

void AVisexMenuDirector::StandFromSettings()
{
	if (!bSitting)
	{
		return;
	}
	bSitting = false;
	GetWorldTimerManager().ClearTimer(SitTimer);
	UAnimInstance* Anim = GetPawnAnim();
	UAnimSequence* Idle = LeanIdle.LoadSynchronous();
	if (!Anim || !Idle)
	{
		return;
	}
	LoadedLean.AddUnique(Idle);
	static const FName Slot(TEXT("DefaultSlot"));
	if (UAnimSequence* Up = SettingsSitUp.LoadSynchronous())
	{
		LoadedLean.AddUnique(Up);
		// closed mid-way down: a longer blend carries her from wherever she is onto the way up
		const float Dur = SitWarpTime(2, Up->GetPlayLength());
		SitMontage = Anim->PlaySlotAnimationAsDynamicMontage(Up, Slot, 0.4f, SitBlend, SitRate(2, 0.f), 1);
		SitWarp = 2;
		// the dolly out lands a beat after her, on the menu shot
		SettingsCamDur = Dur + 0.3f;
		PlayLoopLater(Idle, Dur, true);
		EndMotion(FMath::Max(Dur, SettingsCamDur * SettingsCamAlpha));
	}
	else
	{
		Anim->PlaySlotAnimationAsDynamicMontage(Idle, Slot, 0.8f, 0.3f, 1.f, 1000000);
		ScheduleIdleBeat();
	}
}

FTransform AVisexMenuDirector::SettingsCameraTransform() const
{
	const ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	const USkeletalMeshComponent* Mesh = Char ? Char->GetMesh() : nullptr;
	if (!Mesh)
	{
		return CameraBase;
	}
	// the menu shot dollied along the ground in its own heading (no climb), then turned onto her
	const FTransform Heading(FRotator(0.f, CameraBase.Rotator().Yaw, 0.f), CameraBase.GetLocation());
	const FVector Loc = Heading.TransformPositionNoScale(SettingsCamOffset);
	const FVector Aim = Mesh->GetComponentTransform().TransformPosition(SettingsCamAim);
	FRotator Look = (Aim - Loc).Rotation();
	Look.Pitch += SettingsCamLookUp;
	Look.Yaw += SettingsCamPan;
	Look.Roll = 0.f;
	return FTransform(Look, Loc);
}

void AVisexMenuDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bOpen)
	{
		Clock += DeltaSeconds;   // the sway holds still while a page has the screen
	}
	if (bArriving)
	{
		if (bArrivalFrozen && !ArrivalHeld())
		{
			FreezeHer(false);
			UE_LOG(LogTemp, Log, TEXT("Menu arrival: released from frame 0"));
		}
	}
	UpdateAlign();

	if (PanelDirty > 0 && Panel && Panel->IsVisible())
	{
		Panel->RequestRenderUpdate();
		--PanelDirty;
	}

	// left stick steps the selection: one step per push, past 0.5, re-armed under 0.3
	if (bOpen && InputComponent)
	{
		const float Y = InputComponent->GetAxisKeyValue(EKeys::Gamepad_LeftY);
		if (!bStickHeld && FMath::Abs(Y) > 0.5f)
		{
			bStickHeld = true;
			MoveSelection(Y > 0.f ? -1 : 1);
		}
		else if (bStickHeld && FMath::Abs(Y) < 0.3f)
		{
			bStickHeld = false;
		}
	}

	// Settings: the camera dollies in at its own height while she sits, the lens opening wide,
	// and back out after
	const float SitGoal = (bOpen && bSitting) ? 1.f : 0.f;
	const float SitAlpha0 = SettingsCamAlpha;
	if (SitWarp != 0)
	{
		TickSitWarp();
	}
	SettingsCamAlpha = FMath::FInterpConstantTo(SettingsCamAlpha, SitGoal, DeltaSeconds,
		1.f / FMath::Max(0.1f, SettingsCamDur > 0.f ? SettingsCamDur : SettingsCamTime));
	// her soles stay planted on the sand except while she sits on the drum (the sit keys its feet)
	if (UVisexAnimInstance* Plant = Cast<UVisexAnimInstance>(GetPawnAnim())) { Plant->PlantAlpha = 1.f - SettingsCamAlpha; }
	const bool bSitCamMoved = SitAlpha0 != SettingsCamAlpha;
	if (bOpen && MenuCamera && bSitCamMoved && bHaveLens)
	{
		// focal eased in log space, so the field of view opens evenly
		const float Ratio = (SettingsFocalLength > 0.f && MenuFocalLength > 0.f) ? SettingsFocalLength / MenuFocalLength : 1.f;
		const float K = FMath::Exp(FMath::Loge(Ratio) * SettingsCamEase());
		if (UCineCameraComponent* Lens = GetLens())
		{
			SetMenuFocal(Lens, MenuLensFocal() * K);
		}
		PanelLensK = K;   // the panel follows once the camera has moved (below)
	}
	if (bOpen && MenuCamera && (DriftPeriod > 0.f || SettingsCamAlpha > 0.f || bSitCamMoved))
	{
		FTransform Base = CameraBase;
		if (SettingsCamAlpha > 0.f)
		{
			// a straight dolly: it stops short of her, so it never passes through her
			const float S = SettingsCamEase();
			const FTransform Shot = SettingsCameraTransform();
			Base = FTransform(FQuat::Slerp(CameraBase.GetRotation(), Shot.GetRotation(), S),
				FMath::Lerp(CameraBase.GetLocation(), Shot.GetLocation(), S));
		}
		MenuCamera->SetActorTransform(Drifted(Base));
		if (SettingsCamAlpha > 0.f || bSitCamMoved)
		{
			// every frame while it is wide: the camera sways, and the offset is taken from where it is now
			ApplyPanelLens(PanelLensK);
		}
	}
	// the window changed shape under the menu: keep her whole height in the frame
	if (bOpen && !bOrbiting && bHaveLens && bMenuFillsWindow && !FMath::IsNearlyEqual(ViewAspect(), MenuViewAspect, 0.001f))
	{
		if (UCineCameraComponent* Lens = GetLens())
		{
			SetMenuFocal(Lens, MenuLensFocal());
		}
	}

	if (LandedFor >= 0.f)
	{
		LandedFor += DeltaSeconds;
	}
	if (WalkOffClock >= 0.f)
	{
		TickWalkOff(DeltaSeconds);
	}
	TickMove();
	if (StepDir != 0 && !bArrivalFrozen)
	{
		HoldGait(true);
		TickStep(DeltaSeconds);
	}
	TickWalkClip(DeltaSeconds, (StepDir != 0 && !bArrivalFrozen) || WalkOffClock >= 0.f);
	if (MotionStart >= 0.0 && !bArriving)
	{
		TickFootSkate();
		// a letterbox snapping on or off reads as a camera jump: log each change of it
		const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
		if (const APlayerCameraManager* Cam = PC ? PC->PlayerCameraManager.Get() : nullptr)
		{
			const bool bBoxed = Cam->GetCameraCacheView().bConstrainAspectRatio;
			if (bBoxed != bMotionBoxed)
			{
				UE_LOG(LogTemp, Log, TEXT("Menu motion: %s view %s at %.2f s"), *MotionName,
					bBoxed ? TEXT("letterboxed") : TEXT("fills the window"), GetWorld()->GetTimeSeconds() - MotionStart);
				bMotionBoxed = bBoxed;
			}
		}
	}
	if (ArrivalClock >= 0.f && !bArrivalFrozen)
	{
		TickArrivalPanel(DeltaSeconds);
	}

	if (bOrbiting)
	{
		TickOrbit(DeltaSeconds);
	}
	// after the camera moved: focus on her for this frame's view, through the landing blend too
	if (bOpen || bOrbiting || StepDir != 0 || (LandedFor >= 0.f && LandedFor <= LandingBlendTime + 0.1f))
	{
		TickFocus();
	}

	if (bStudio || StudioAlpha > 0.f)
	{
		TickStudio(DeltaSeconds);
	}
	TickMenuKey(DeltaSeconds);
	TickMenuWind(DeltaSeconds);

	if (FadeOut >= 0.f)
	{
		MarkPanel();
		FadeOut += DeltaSeconds;
		const float A = FMath::Clamp(1.f - FadeOut / 0.6f, 0.f, 1.f);
		if (UVisexMenuWidget* W = GetMenuWidget())
		{
			W->SetRenderOpacity(A);
		}
		if (A <= 0.f)
		{
			Panel->SetVisibility(false);
			FadeOut = -1.f;
		}
	}
}

FTransform AVisexMenuDirector::Drifted(const FTransform& Base) const
{
	const float W = DriftPeriod > 0.f ? 2.f * PI / DriftPeriod : 0.f;
	const float Sway = DriftPeriod > 0.f ? 1.f : 0.f;
	// slow, unsynchronised sways read as a handheld camera breathing, not a loop
	FRotator Rot = Base.Rotator();
	Rot.Yaw += Sway * DriftDegrees * FMath::Sin(Clock * W);
	Rot.Pitch += Sway * 0.5f * DriftDegrees * FMath::Sin(Clock * W * 0.73f + 1.3f);
	const FVector Right = Base.GetRotation().GetRightVector();
	const FVector Loc = Base.GetLocation()
		+ Right * Sway * DriftDistance * FMath::Sin(Clock * W * 1.21f + 0.4f)
		+ FVector::UpVector * Sway * 0.5f * DriftDistance * FMath::Sin(Clock * W * 0.91f + 2.1f);
	return FTransform(Rot, Loc);
}

void AVisexMenuDirector::TickOrbit(float DeltaSeconds)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const UCameraComponent* Follow = Pawn ? Pawn->FindComponentByClass<UCameraComponent>() : nullptr;
	if (!Pawn || !Follow || !MenuCamera)
	{
		bOrbiting = false;
		return;
	}
	OrbitT = FMath::Min(1.f, OrbitT + DeltaSeconds / FMath::Max(0.1f, BlendToPlayTime));
	const float T = OrbitT;
	const float S = FMath::SmoothStep(0.f, 1.f, T);

	// end of the orbit: the play camera behind her (it is live, so the landing is exact)
	const FVector Pivot = Pawn->GetActorLocation();
	const FVector E = Follow->GetComponentLocation() - Pivot;
	const float A1 = FMath::RadiansToDegrees(FMath::Atan2(E.Y, E.X));
	const float R1 = FVector2D(E.X, E.Y).Size();
	float Delta = FMath::FindDeltaAngleDegrees(OrbitA0, A1);
	if (OrbitDirection != 0 && FMath::Sign(Delta) != FMath::Sign((float)OrbitDirection))
	{
		Delta += OrbitDirection > 0 ? 360.f : -360.f;
	}
	const float A = FMath::DegreesToRadians(OrbitA0 + Delta * S);
	const float R = FMath::Lerp(OrbitR0, R1, S);
	// hug the ground first, rise into the play camera's height at the end
	const float Rise = FMath::SmoothStep(OrbitStayLow, 1.f, T);
	const float Z = FMath::Lerp(OrbitZ0, E.Z, Rise);
	const FVector Loc = Pivot + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, Z);

	// watch her, then settle into the play camera's exact angle
	const FVector Look = Pivot + FVector(0.f, 0.f, OrbitLookHeight - 90.f);
	// the menu's off-centre framing eases out over the first part, so frame one doesn't jump
	const FQuat Offset = FQuat::Slerp(OrbitAim0, FQuat::Identity, FMath::SmoothStep(0.f, 0.4f, T));
	const FQuat Watch = (Look - Loc).Rotation().Quaternion() * Offset;
	const float Settle = FMath::SmoothStep(0.7f, 1.f, T);
	const FQuat Rot = FQuat::Slerp(Watch, Follow->GetComponentQuat(), Settle);
	MenuCamera->SetActorLocationAndRotation(Loc, Rot);

	// lens: from the menu's cine lens to the play camera's field of view, depth of field
	// opening out, so the cut at the end is invisible. It starts from the window-fitted menu
	// lens and ends on the play camera's own field, so it lands exactly at any window shape
	if (UCineCameraComponent* Lens = GetLens())
	{
		const float HalfW = Lens->Filmback.SensorWidth * 0.5f;
		const float StartFocal = MenuLensFocal();
		const float FovStart = 2.f * FMath::RadiansToDegrees(FMath::Atan(HalfW / FMath::Max(1.f, StartFocal)));
		const float Fov = FMath::Lerp(FovStart, Follow->FieldOfView, S);
		Lens->SetCurrentFocalLength(HalfW / FMath::Tan(FMath::DegreesToRadians(Fov * 0.5f)));
		const float StartStop = MenuAperture > 0.f ? MenuAperture : MenuFStop;
		const float MaxStop = FMath::Max(StartStop, Lens->LensSettings.MaxFStop);
		Lens->SetCurrentAperture(FMath::Exp(FMath::Lerp(FMath::Loge(FMath::Max(0.5f, StartStop)), FMath::Loge(MaxStop), S)));
		// focus follows her head in Tick, after this move
	}

	SetCornerShade(1.f - S);

	if (T >= 1.f)
	{
		bOrbiting = false;
		// same place, same angle, but not quite the same picture: the menu cine camera rendered
		// visibly darker than the play camera in motion (QA 0259: luma 0.24 -> 0.38 on the cut),
		// so blend the hand-over instead of cutting
		PC->SetViewTargetWithBlend(Pawn, LandingBlendTime, VTBlend_EaseInOut, 2.f);
		HandBack();
	}
}

void AVisexMenuDirector::TickStudio(float DeltaSeconds)
{
	if (!bHaveStudioBase)
	{
		// the scene's own lights, read once, so the page can dim them and give them back
		for (TActorIterator<ADirectionalLight> It(GetWorld()); It && !Sun.IsValid(); ++It) { Sun = *It; }
		for (TActorIterator<ASkyLight> It(GetWorld()); It && !Sky.IsValid(); ++It) { Sky = *It; }
		for (TActorIterator<APostProcessVolume> It(GetWorld()); It && !Post.IsValid(); ++It)
		{
			if (It->bUnbound && !It->ActorHasTag(FVisexSettings::StyleTag)) { Post = *It; }
		}
		BaseSun = Sun.IsValid() ? Sun->GetLightComponent()->Intensity : 0.f;
		BaseSky = Sky.IsValid() ? Sky->GetLightComponent()->Intensity : 0.f;
		if (Post.IsValid())
		{
			BaseVignette = Post->Settings.VignetteIntensity;
			bBaseVignetteOverride = Post->Settings.bOverride_VignetteIntensity;
		}
		bHaveStudioBase = true;
	}

	const float Step = DeltaSeconds / FMath::Max(0.05f, StudioBlendTime);
	StudioAlpha = FMath::Clamp(StudioAlpha + (bStudio ? Step : -Step), 0.f, 1.f);
	// photo mode uses a gentler version of the page's look: StudioStrength scales all of it
	const float S = FMath::SmoothStep(0.f, 1.f, StudioAlpha) * StudioStrength;

	if (Sun.IsValid()) { Sun->GetLightComponent()->SetIntensity(BaseSun * FMath::Lerp(1.f, StudioSunScale, S)); }
	if (Sky.IsValid()) { Sky->GetLightComponent()->SetIntensity(BaseSky * FMath::Lerp(1.f, StudioSkyScale, S)); }
	if (Post.IsValid())
	{
		Post->Settings.bOverride_VignetteIntensity = StudioAlpha > 0.f ? true : bBaseVignetteOverride;
		Post->Settings.VignetteIntensity = FMath::Lerp(BaseVignette, StudioVignette, S);
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	SpawnStudioLights();
	ULocalLightComponent* Spot = StudioSpot ? StudioSpot->FindComponentByClass<ULocalLightComponent>() : nullptr;
	ULocalLightComponent* Rim = StudioRim ? StudioRim->FindComponentByClass<ULocalLightComponent>() : nullptr;
	const bool bOn = StudioAlpha > 0.f && Pawn && PC->PlayerCameraManager && Spot && Rim;
	if (Spot) { Spot->SetIntensity(0.f); }
	if (Rim) { Rim->SetIntensity(0.f); }
	if (!bOn)
	{
		return;
	}

	JoinChannel1(Pawn);

	float Half = 90.f;
	if (const ACharacter* C = Cast<ACharacter>(Pawn))
	{
		Half = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}
	const FVector Feet = Pawn->GetActorLocation() - FVector(0.f, 0.f, Half);
	const FVector ToCam = PC->PlayerCameraManager->GetCameraLocation() - Feet;
	const float CamYaw = FMath::RadiansToDegrees(FMath::Atan2(ToCam.Y, ToCam.X));
	PlaceStudioLight(Spot, SpotPlacement, Feet, CamYaw, 110.f);
	PlaceStudioLight(Rim, RimPlacement, Feet, CamYaw, 110.f);
	Spot->SetIntensity(SpotIntensity * S);
	Rim->SetIntensity(RimIntensity * S);
}

void AVisexMenuDirector::JoinChannel1(APawn* Pawn)
{
	// she joins channel 1 (and keeps 0), clothes included, which can change while a page is open
	TArray<UPrimitiveComponent*> Parts;
	Pawn->GetComponents(Parts);
	for (UPrimitiveComponent* P : Parts)
	{
		if (!P->LightingChannels.bChannel1)
		{
			P->SetLightingChannels(true, true, P->LightingChannels.bChannel2);
		}
	}
}

void AVisexMenuDirector::TickMenuKey(float DeltaSeconds)
{
	// on while the main menu page is open; the Character page has its own studio lights
	const bool bWant = bOpen && !bStudio;
	if (!bWant && MenuKeyAlpha <= 0.f)
	{
		return;
	}
	const float Step = DeltaSeconds / FMath::Max(0.05f, MenuKeyBlendTime);
	MenuKeyAlpha = FMath::Clamp(MenuKeyAlpha + (bWant ? Step : -Step), 0.f, 1.f);

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!MenuKey)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ARectLight* L = GetWorld()->SpawnActor<ARectLight>(GetActorLocation() - FVector(0.f, 0.f, 5000.f), FRotator::ZeroRotator, P))
		{
			URectLightComponent* C = Cast<URectLightComponent>(L->GetLightComponent());
			C->SetMobility(EComponentMobility::Movable);
			C->SetIntensityUnits(ELightUnits::Candelas);   // spawned lights default to unitless
			C->SetIntensity(0.f);
			C->SetTemperature(MenuKeyTemperature);
			C->bUseTemperature = true;
			C->SetLightingChannels(false, true, false);   // her only: the sand and stone keep the scene's light
			C->SetVolumetricScatteringIntensity(0.f);     // no glow in the haze
			C->SetAttenuationRadius(1500.f);
			C->SetSourceWidth(160.f);                     // big and soft: diffuse low sun through dust
			C->SetSourceHeight(160.f);
			C->SetBarnDoorAngle(88.f);
			MenuKey = L;
		}
	}
	ULocalLightComponent* Key = MenuKey ? MenuKey->FindComponentByClass<ULocalLightComponent>() : nullptr;
	if (!Key)
	{
		return;
	}
	if (!Pawn || !PC->PlayerCameraManager || MenuKeyAlpha <= 0.f)
	{
		Key->SetIntensity(0.f);
		return;
	}
	JoinChannel1(Pawn);
	float Half = 90.f;
	if (const ACharacter* C = Cast<ACharacter>(Pawn))
	{
		Half = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}
	const FVector Feet = Pawn->GetActorLocation() - FVector(0.f, 0.f, Half);
	const FVector ToCam = PC->PlayerCameraManager->GetCameraLocation() - Feet;
	const float CamYaw = FMath::RadiansToDegrees(FMath::Atan2(ToCam.Y, ToCam.X));
	PlaceStudioLight(Key, MenuKeyPlacement, Feet, CamYaw, 120.f);
	Key->SetIntensity(MenuKeyIntensity * FMath::SmoothStep(0.f, 1.f, MenuKeyAlpha));
}

void AVisexMenuDirector::PlaceStudioLight(ULocalLightComponent* Light, const FVector& Placement, const FVector& Feet, float CamYaw, float AimHeight) const
{
	const float Yaw = FMath::DegreesToRadians(CamYaw + Placement.X);
	const FVector Loc = Feet + FVector(FMath::Cos(Yaw) * Placement.Y, FMath::Sin(Yaw) * Placement.Y, Placement.Z);
	const FVector Aim = Feet + FVector(0.f, 0.f, AimHeight);
	Light->GetOwner()->SetActorLocationAndRotation(Loc, (Aim - Loc).Rotation());
}

void AVisexMenuDirector::SpawnStudioLights()
{
	if (StudioSpot && StudioRim)
	{
		return;
	}
	// lighting channel 1 only: these touch her (and her clothes), not the stone or sand
	auto Setup = [](ULocalLightComponent* L, float Temperature)
	{
		L->SetMobility(EComponentMobility::Movable);
		// the editor gives placed lights candelas; spawned ones default to unitless (~50x dimmer)
		L->SetIntensityUnits(ELightUnits::Candelas);
		L->SetIntensity(0.f);
		L->SetTemperature(Temperature);
		L->bUseTemperature = true;
		L->SetLightingChannels(false, true, false);
		L->SetVolumetricScatteringIntensity(0.f);   // no glow blobs in the haze
		L->SetAttenuationRadius(1200.f);
	};
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Away = GetActorLocation() - FVector(0.f, 0.f, 5000.f);
	if (!StudioSpot)
	{
		if (ASpotLight* L = GetWorld()->SpawnActor<ASpotLight>(Away, FRotator::ZeroRotator, P))
		{
			USpotLightComponent* C = Cast<USpotLightComponent>(L->GetLightComponent());
			Setup(C, 4800.f);
			C->SetOuterConeAngle(44.f);
			C->SetSourceRadius(12.f);
			StudioSpot = L;
		}
	}
	if (!StudioRim)
	{
		if (ARectLight* L = GetWorld()->SpawnActor<ARectLight>(Away, FRotator::ZeroRotator, P))
		{
			URectLightComponent* C = Cast<URectLightComponent>(L->GetLightComponent());
			Setup(C, 4000.f);
			C->SetSourceWidth(40.f);
			C->SetSourceHeight(200.f);
			C->SetBarnDoorAngle(70.f);
			StudioRim = L;
		}
	}
}

void AVisexMenuDirector::TickMenuWind(float DeltaSeconds)
{
	if (!bWindSearched)
	{
		bWindSearched = true;
		for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
		{
			UStaticMeshComponent* C = It->GetStaticMeshComponent();
			if (C && C->GetStaticMesh() && C->GetStaticMesh()->GetName() == TEXT("SM_BlowingSand"))
			{
				WindMID = C->CreateDynamicMaterialInstance(0);
				break;
			}
		}
	}
	if (!WindMID)
	{
		return;
	}
	const UWorld* W = GetWorld();
	const float Now = W->GetTimeSeconds();
	if (WindCalm < 0.f)
	{
		WindCalm = bOpen ? 1.f : 0.f;   // start calm on the menu, no fade-in
		WindClock = Now;
	}
	const float Step = DeltaSeconds / FMath::Max(0.05f, MenuWindBlendTime);
	WindCalm = FMath::Clamp(WindCalm + (bOpen ? Step : -Step), 0.f, 1.f);
	const float A = FMath::SmoothStep(0.f, 1.f, WindCalm);
	// the material scrolls its gusts by time * speed: slowing the clock (not the speed)
	// keeps the pattern in place while the wind changes, with no jump
	WindClock += DeltaSeconds * FMath::Lerp(1.f, MenuWindSpeed, A);
	WindMID->SetScalarParameterValue(TEXT("WindOffset"), WindClock - Now);
	WindMID->SetScalarParameterValue(TEXT("Opacity"), FMath::Lerp(0.45f, MenuWindOpacity, A));
}
