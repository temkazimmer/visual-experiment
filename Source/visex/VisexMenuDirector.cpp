#include "VisexMenuDirector.h"

#include "VisexMenuWidget.h"
#include "VisexBodyEditor.h"
#include "VisexMenuLayoutEditor.h"
#include "VisexPhotoMode.h"
#include "VisexQATour.h"
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
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
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
	MenuCamera->SetActorTransform(CameraBase);
	SettingsCamAlpha = 0.f;
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
	SetCornerShade(1.f);
	SpawnLeanColumn();
	FitLeanColumn();
	Lean(bHaveLeaned);
	bHaveLeaned = true;
	bOrbiting = false;
	WalkOffClock = -1.f;
	LandedFor = -1.f;
	PC->SetViewTarget(MenuCamera);
	Clock = 0.f;

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
		W->SetRenderOpacity(1.f);
		if (Page > 0) { CloseSettings(); }
		W->SetSelected(0);
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
	Panel->SetRelativeLocation(PanelOffset * LensScale);
	Panel->SetRelativeRotation(FRotator(PanelPitch, 180.f + PanelYaw, PanelRoll));
	// draw the panel's texture larger and shrink it back to the same size on screen
	const UVisexMenuWidget* MenuDefaults = Panel->GetWidgetClass()
		? Cast<UVisexMenuWidget>(Panel->GetWidgetClass()->GetDefaultObject()) : nullptr;
	const float SS = GetMenuWidget() ? FMath::Max(1.f, GetMenuWidget()->Supersample)
		: MenuDefaults ? FMath::Max(1.f, MenuDefaults->Supersample) : 1.f;
	// the widget pads its text for the shade to fade out in: grow the panel to match
	const float Pad = GetMenuWidget() ? GetMenuWidget()->ShadePadding : MenuDefaults ? MenuDefaults->ShadePadding : 0.f;
	Panel->SetDrawSize((FVector2D(1100.f, 1000.f) + FVector2D(2.f * Pad)) * SS);
	Panel->SetRelativeScale3D(FVector(PanelScale * LensScale / SS));
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
	if (!bOpen)
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
	if (!bOpen)
	{
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
	Page = 1;
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
	// she pushes off the column first; the page opens as she finds her feet
	const float Up = StandUp();
	if (Up > 0.f)
	{
		bOpen = false;
		FadeOut = 0.f;
		DisableInput(PC);
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
	// the menu steps aside while the body page has the screen
	bOpen = false;
	Panel->SetVisibility(false);
	DisableInput(PC);
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
	// she pushes off the column, then the photo camera takes over
	const float Up = StandUp();
	if (Up > 0.f)
	{
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
		PhotoMode->Open();
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
	// the scene returns; she walks back onto the column behind a quick dip to black
	SetSceneStripped(false);
	bStudio = false;
	BackToMenu(PhotoMode ? PhotoMode->BlendTime : 0.f, ItemPhoto);
}

void AVisexMenuDirector::HandleBodyClosed()
{
	bStudio = false;
	BackToMenu(BodyEditor ? BodyEditor->BlendTime : 0.f, CharacterPage == 1 ? ItemWardrobe : ItemBody);
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
		StartGame();
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
}

void AVisexMenuDirector::SetColumnBlocksPawn(bool bBlock)
{
	if (UStaticMeshComponent* Body = LeanColumn ? LeanColumn->FindComponentByClass<UStaticMeshComponent>() : nullptr)
	{
		// while she leans her capsule sits partly inside it
		Body->SetCollisionResponseToChannel(ECC_Pawn, bBlock ? ECR_Block : ECR_Ignore);
	}
}

void AVisexMenuDirector::Lean(bool bFromStanding)
{
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
		Anim->PlaySlotAnimationAsDynamicMontage(Down, Slot, 0.25f, 0.f, 1.f, 1);
		// LoadedLean keeps the clip alive; the timer holds it weakly too, so if that ever slips it
		// skips the idle instead of playing freed memory (0298)
		GetWorldTimerManager().SetTimer(StandTimer, FTimerDelegate::CreateWeakLambda(this, [this, WeakIdle = TWeakObjectPtr<UAnimSequence>(Idle)]()
		{
			UAnimSequence* Clip = WeakIdle.Get();
			UAnimInstance* A = GetPawnAnim();
			if (Clip && A)
			{
				A->PlaySlotAnimationAsDynamicMontage(Clip, Slot, 0.f, 0.3f, 1.f, 1000000);
			}
		}), Down->GetPlayLength(), false);
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

float AVisexMenuDirector::StandUp()
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
		Anim->PlaySlotAnimationAsDynamicMontage(Up, Slot, 0.15f, 0.35f, 1.f, 1);
		return Up->GetPlayLength();
	}
	Anim->StopSlotAnimation(0.4f, Slot);
	return 0.4f;
}

void AVisexMenuDirector::PlayLoopLater(UAnimSequence* Clip, float Delay, bool bThenBeats)
{
	// a short cross-fade just before the transition ends: both clips rest there (PAD), so no seam.
	// The clip is held in LoadedLean; the timer only holds it weakly (0298)
	GetWorldTimerManager().SetTimer(SitTimer, FTimerDelegate::CreateWeakLambda(this, [this, WeakClip = TWeakObjectPtr<UAnimSequence>(Clip), bThenBeats]()
	{
		UAnimSequence* Loop = WeakClip.Get();
		UAnimInstance* A = GetPawnAnim();
		if (Loop && A)
		{
			static const FName Slot(TEXT("DefaultSlot"));
			A->PlaySlotAnimationAsDynamicMontage(Loop, Slot, 0.15f, 0.3f, 1.f, 1000000);
		}
		if (bThenBeats)
		{
			ScheduleIdleBeat();
		}
	}), FMath::Max(Delay - 0.15f, 0.05f), false);
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
		Anim->PlaySlotAnimationAsDynamicMontage(Down, Slot, 0.4f, 0.f, 1.f, 1);
		PlayLoopLater(Sit, Down->GetPlayLength(), false);
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
		Anim->PlaySlotAnimationAsDynamicMontage(Up, Slot, 0.4f, 0.f, 1.f, 1);
		PlayLoopLater(Idle, Up->GetPlayLength(), true);
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
	// the menu shot, stepped a little in its own space, then turned onto a point on her
	const FVector Loc = CameraBase.TransformPositionNoScale(SettingsCamOffset);
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
	Clock += DeltaSeconds;

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

	// Settings: the camera sinks onto her feet while she sits, and climbs back after
	const float SitGoal = (bOpen && bSitting) ? 1.f : 0.f;
	const float SitAlpha0 = SettingsCamAlpha;
	SettingsCamAlpha = FMath::FInterpConstantTo(SettingsCamAlpha, SitGoal, DeltaSeconds, 1.f / FMath::Max(0.1f, SettingsCamTime));
	const bool bSitCamMoved = SitAlpha0 != SettingsCamAlpha;
	if (bOpen && MenuCamera && (DriftPeriod > 0.f || SettingsCamAlpha > 0.f || bSitCamMoved))
	{
		FTransform Base = CameraBase;
		if (SettingsCamAlpha > 0.f)
		{
			// round her, not through her: angle, distance and height each eased from shot to shot
			const float S = SettingsCamEase();
			const FTransform Feet = SettingsCameraTransform();
			const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
			const FVector C = Pawn ? Pawn->GetActorLocation() : Feet.GetLocation();
			const FVector D0 = (CameraBase.GetLocation() - C) * FVector(1.f, 1.f, 0.f);
			const FVector D1 = (Feet.GetLocation() - C) * FVector(1.f, 1.f, 0.f);
			const float A0 = FMath::Atan2(D0.Y, D0.X);
			const float A = A0 + FMath::FindDeltaAngleRadians(A0, FMath::Atan2(D1.Y, D1.X)) * S;
			const float R = FMath::Lerp(D0.Size(), D1.Size(), S);
			FVector Loc(C.X + R * FMath::Cos(A), C.Y + R * FMath::Sin(A), FMath::Lerp(CameraBase.GetLocation().Z, Feet.GetLocation().Z, S));
			Base = FTransform(FQuat::Slerp(CameraBase.GetRotation(), Feet.GetRotation(), S), Loc);
		}
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
		MenuCamera->SetActorLocationAndRotation(Loc, Rot);
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

	if (bOrbiting)
	{
		TickOrbit(DeltaSeconds);
	}
	// after the camera moved: focus on her for this frame's view, through the landing blend too
	if (bOpen || bOrbiting || (LandedFor >= 0.f && LandedFor <= LandingBlendTime + 0.1f))
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
