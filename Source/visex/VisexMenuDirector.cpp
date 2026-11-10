#include "VisexMenuDirector.h"

#include "VisexMenuWidget.h"
#include "VisexBodyEditor.h"
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
	// and its lens, which the Start move turns into the play camera's
	if (UCineCameraComponent* Lens = GetLens())
	{
		if (!bHaveLens)
		{
			MenuFocal = Lens->CurrentFocalLength;
			MenuFStop = Lens->CurrentAperture;
			MenuFilmback = Lens->Filmback;
			bHaveLens = true;
		}
		// a bigger sensor with a matching focal length frames the same and blurs more
		const float Scale = FMath::Max(1.f, MenuSensorScale);
		Lens->Filmback = MenuFilmback;
		Lens->Filmback.SensorWidth *= Scale;
		Lens->Filmback.SensorHeight *= Scale;
		const float Focal = (MenuFocalLength > 0.f ? MenuFocalLength : MenuFocal) * Scale;
		const float Stop = MenuAperture > 0.f ? MenuAperture : MenuFStop;
		// the lens preset's range would clamp these
		Lens->LensSettings.MinFocalLength = FMath::Min(Lens->LensSettings.MinFocalLength, Focal);
		Lens->LensSettings.MaxFocalLength = FMath::Max(Lens->LensSettings.MaxFocalLength, Focal);
		Lens->LensSettings.MinFStop = FMath::Min(Lens->LensSettings.MinFStop, Stop);
		Lens->SetCurrentFocalLength(Focal);
		Lens->SetCurrentAperture(Stop);
		FCameraFocusSettings Focus = Lens->FocusSettings;
		Focus.ManualFocusDistance = FVector::Dist(CameraBase.GetLocation(), MenuAim);
		Lens->FocusSettings = Focus;
	}
	SetCornerShade(1.f);
	SpawnLeanColumn();
	Lean(bHaveLeaned);
	bHaveLeaned = true;
	bOrbiting = false;
	PC->SetViewTarget(MenuCamera);
	Clock = 0.f;

	// the panel rides with the camera, turned toward the figure on the right
	Panel->AttachToComponent(MenuCamera->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	// PanelOffset is set for the camera's own lens: a wider menu lens brings the panel in to keep its size on screen
	const float LensScale = (MenuFocalLength > 0.f && MenuFocal > 0.f) ? MenuFocalLength / MenuFocal : 1.f;
	Panel->SetRelativeLocation(PanelOffset * LensScale);
	Panel->SetRelativeRotation(FRotator(0.f, 180.f + PanelYaw, 0.f) + FRotator(0.f, 0.f, PanelRoll));
	// draw the panel's texture larger and shrink it back to the same size on screen
	const UVisexMenuWidget* MenuDefaults = Panel->GetWidgetClass()
		? Cast<UVisexMenuWidget>(Panel->GetWidgetClass()->GetDefaultObject()) : nullptr;
	const float SS = GetMenuWidget() ? FMath::Max(1.f, GetMenuWidget()->Supersample)
		: MenuDefaults ? FMath::Max(1.f, MenuDefaults->Supersample) : 1.f;
	// the widget pads its text for the shade to fade out in: grow the panel to match
	const float Pad = GetMenuWidget() ? GetMenuWidget()->ShadePadding : MenuDefaults ? MenuDefaults->ShadePadding : 0.f;
	Panel->SetDrawSize((FVector2D(1100.f, 1000.f) + FVector2D(2.f * Pad)) * SS);
	Panel->SetRelativeScale3D(FVector(PanelScale * LensScale / SS));
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

UVisexMenuWidget* AVisexMenuDirector::GetMenuWidget() const
{
	return Panel ? Cast<UVisexMenuWidget>(Panel->GetUserWidgetObject()) : nullptr;
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
	if (UVisexMenuWidget* W = GetMenuWidget())
	{
		// back on the main menu, on the Settings item
		const UVisexMenuWidget* Defaults = W->GetClass()->GetDefaultObject<UVisexMenuWidget>();
		W->SetPage(Defaults->Title, {}, 0, Defaults->Items, {}, 3);
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
	case 1:
		OpenBody();
		break;
	case 2:
		OpenPhoto();
		break;
	case 3:
		OpenSettings();
		break;
	case 4:
		UKismetSystemLibrary::QuitGame(this, UGameplayStatics::GetPlayerController(this, 0), EQuitPreference::Quit, false);
		break;
	default:
		StartGame();
		break;
	}
}

void AVisexMenuDirector::OpenBody()
{
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
		BodyEditor->Open();
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
	BackToMenu(PhotoMode ? PhotoMode->BlendTime : 0.f, 2);
}

void AVisexMenuDirector::HandleBodyClosed()
{
	bStudio = false;
	BackToMenu(BodyEditor ? BodyEditor->BlendTime : 0.f, 1);
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
	// she pushes off the column while the camera circles round to her back; the player gets
	// her as soon as she's mostly on her feet, so walking and the camera move overlap
	const float Up = StandUp();
	GetWorldTimerManager().SetTimer(HandBackTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
		if (PC && PC->GetPawn() && !bOpen)
		{
			PC->GetPawn()->EnableInput(PC);
			bWalkOffArmed = true;
		}
	}), FMath::Max(0.05f, Up * StandUpHandOver), false);
	// start of the orbit, in polar terms around her
	const FVector Pivot = PC->GetPawn()->GetActorLocation();
	const FVector S = MenuCamera->GetActorLocation() - Pivot;
	OrbitA0 = FMath::RadiansToDegrees(FMath::Atan2(S.Y, S.X));
	OrbitR0 = FVector2D(S.X, S.Y).Size();
	OrbitZ0 = S.Z;
	OrbitT = 0.f;
	bOrbiting = true;
}

void AVisexMenuDirector::HandBack()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC && PC->GetPawn())
	{
		PC->GetPawn()->EnableInput(PC);
	}
	SetColumnBlocksPawn(true);
	bWalkOffArmed = false;   // the stand-up is over by now; don't cut whatever plays next
	bPlaying = true;
	OnGameStarted();
}

void AVisexMenuDirector::ToggleMenu()
{
	if (bOpen)
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
	UCineCameraComponent* Lens = GetLens();
	UMaterialInterface* Mat = CornerShade.LoadSynchronous();
	if (!Lens || !Mat)
	{
		return;
	}
	for (FWeightedBlendable& B : Lens->PostProcessSettings.WeightedBlendables.Array)
	{
		if (B.Object == Mat)
		{
			B.Weight = Weight;
			return;
		}
	}
	Lens->PostProcessSettings.WeightedBlendables.Array.Add(FWeightedBlendable(Weight, Mat));
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
	// A drum lying across behind her, settled a little into the sand. Built so the animation
	// side can read it exactly: actor tagged LeanColumn, uniform scale, pivot on the top of the
	// stone at the middle of its length, X along the drum, Z up, no roll; collision is a capsule
	// sized to the stone's real surface.
	const FVector Feet = Char->GetActorLocation() - FVector(0.f, 0.f, Char->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const FVector Fwd = Char->GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Fwd);
	const float R = LeanColumnRadius;
	const FVector Axis = Feet - Fwd * (LeanColumnGap + R) + Right * LeanColumnShift + FVector(0.f, 0.f, R - 3.f);
	const FRotator Rot = FRotationMatrix::MakeFromXZ(Right, FVector::UpVector).Rotator();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Column = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Rot, Axis + FVector(0.f, 0.f, R)), Params);
	if (!Column)
	{
		return;
	}
	USceneComponent* Top = NewObject<USceneComponent>(Column, TEXT("Top"));
	Top->SetMobility(EComponentMobility::Movable);
	Column->SetRootComponent(Top);
	Top->RegisterComponent();
	Column->SetActorLocationAndRotation(Axis + FVector(0.f, 0.f, R), Rot);
	Column->Tags.Add(TEXT("LeanColumn"));

	// the stone: its own length runs along its Z (100 cm), radius 50; its X faced her before
	UStaticMeshComponent* Stone = NewObject<UStaticMeshComponent>(Column, TEXT("Stone"));
	Stone->SetMobility(EComponentMobility::Movable);
	Stone->SetStaticMesh(StoneMesh);
	Stone->SetupAttachment(Top);
	Stone->SetRelativeRotation(FRotationMatrix::MakeFromZX(FVector(1.f, 0.f, 0.f), FVector(0.f, -1.f, 0.f)).Rotator());
	Stone->SetRelativeScale3D(FVector(R / 50.f, R / 50.f, LeanColumnLength / 100.f));
	Stone->SetRelativeLocation(FVector(0.f, 0.f, -R));
	// traceable for the measurement below only; the capsule does the colliding
	Stone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Stone->SetCollisionResponseToAllChannels(ECR_Ignore);
	Stone->RegisterComponent();
	// centre the stone's bounds on the axis (its pivot is a hair off)
	const FVector Off = Stone->Bounds.Origin - Axis;
	Stone->AddWorldOffset(FVector(-FVector::DotProduct(Off, Fwd) * Fwd - FVector(0.f, 0.f, Off.Z)));

	// the real surface: trace down onto the stone across its top, along the middle of its length
	TArray<float> Radii;
	FHitResult Hit;
	FCollisionQueryParams Trace(SCENE_QUERY_STAT(LeanColumnSurface), true);
	for (int32 i = -4; i <= 4; ++i)
	{
		for (int32 j = -3; j <= 3; ++j)
		{
			const FVector Along = Right * (LeanColumnLength * 0.35f * i / 4.f);
			const float Ang = FMath::DegreesToRadians(12.f * j);
			const FVector Dir = Fwd * FMath::Sin(Ang) + FVector::UpVector * FMath::Cos(Ang);
			const FVector Start = Axis + Along + Dir * (R * 2.f);
			if (Stone->LineTraceComponent(Hit, Start, Axis + Along, Trace))
			{
				// distance from the drum's axis, across the drum
				const FVector D = Hit.Location - Axis;
				Radii.Add(FVector2D(FVector::DotProduct(D, Fwd), D.Z).Size());
			}
		}
	}
	float Surface = R;
	if (Radii.Num() >= 5)
	{
		Radii.Sort();
		Surface = Radii[Radii.Num() / 2];
	}
	LeanColumnSurfaceRadius = Surface;
	// pivot onto the measured top
	Column->SetActorLocation(Axis + FVector(0.f, 0.f, Surface));
	Stone->AddWorldOffset(FVector(0.f, 0.f, R - Surface));   // the stone stays where it was

	UCapsuleComponent* Body = NewObject<UCapsuleComponent>(Column, TEXT("Collision"));
	Body->SetMobility(EComponentMobility::Movable);
	Body->SetupAttachment(Top);
	Body->SetRelativeLocation(FVector(0.f, 0.f, -Surface));
	Body->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));   // capsule axis along the drum
	Body->SetCapsuleSize(Surface, LeanColumnLength * 0.5f);
	Body->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Body->RegisterComponent();

	UE_LOG(LogTemp, Log, TEXT("LeanColumn: surface radius %.1f (from %d hits), top %.1f, axis %s, gap %.1f"),
		Surface, Radii.Num(), Axis.Z + Surface, *Axis.ToString(), LeanColumnGap);
	LeanColumn = Column;
	SetColumnBlocksPawn(false);
}

void AVisexMenuDirector::SetColumnBlocksPawn(bool bBlock)
{
	if (UCapsuleComponent* Body = LeanColumn ? LeanColumn->FindComponentByClass<UCapsuleComponent>() : nullptr)
	{
		// while she leans her capsule sits partly inside it
		Body->SetCollisionResponseToChannel(ECC_Pawn, bBlock ? ECR_Block : ECR_Ignore);
	}
}

void AVisexMenuDirector::Lean(bool bFromStanding)
{
	GetWorldTimerManager().ClearTimer(StandTimer);
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
	if (Down)
	{
		Anim->PlaySlotAnimationAsDynamicMontage(Down, Slot, 0.25f, 0.f, 1.f, 1);
		GetWorldTimerManager().SetTimer(StandTimer, FTimerDelegate::CreateWeakLambda(this, [this, Idle]()
		{
			if (UAnimInstance* A = GetPawnAnim())
			{
				A->PlaySlotAnimationAsDynamicMontage(Idle, Slot, 0.f, 0.3f, 1.f, 1000000);
			}
		}), Down->GetPlayLength(), false);
	}
	else
	{
		Anim->PlaySlotAnimationAsDynamicMontage(Idle, Slot, 0.3f, 0.3f, 1.f, 1000000);
	}
}

float AVisexMenuDirector::StandUp()
{
	if (!bLeaning)
	{
		return 0.f;
	}
	bLeaning = false;
	GetWorldTimerManager().ClearTimer(StandTimer);
	UAnimInstance* Anim = GetPawnAnim();
	if (!Anim)
	{
		return 0.f;
	}
	static const FName Slot(TEXT("DefaultSlot"));
	if (UAnimSequence* Up = LeanUp.LoadSynchronous())
	{
		Anim->PlaySlotAnimationAsDynamicMontage(Up, Slot, 0.15f, 0.35f, 1.f, 1);
		return Up->GetPlayLength();
	}
	Anim->StopSlotAnimation(0.4f, Slot);
	return 0.4f;
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

	if (bOpen && MenuCamera && DriftPeriod > 0.f)
	{
		// slow, unsynchronised sways read as a handheld camera breathing, not a loop
		const float W = 2.f * PI / DriftPeriod;
		FRotator Rot = CameraBase.Rotator();
		Rot.Yaw += DriftDegrees * FMath::Sin(Clock * W);
		Rot.Pitch += 0.5f * DriftDegrees * FMath::Sin(Clock * W * 0.73f + 1.3f);
		const FVector Right = CameraBase.GetRotation().GetRightVector();
		const FVector Loc = CameraBase.GetLocation()
			+ Right * DriftDistance * FMath::Sin(Clock * W * 1.21f + 0.4f)
			+ FVector::UpVector * 0.5f * DriftDistance * FMath::Sin(Clock * W * 0.91f + 2.1f);
		MenuCamera->SetActorLocationAndRotation(Loc, Rot);
	}

	// the first push of the stick after the hand-over cuts the rest of the stand-up: its root
	// motion would otherwise hold her in place while the player is already asking her to walk
	if (bWalkOffArmed)
	{
		const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
		if (!Pawn || bOpen)
		{
			bWalkOffArmed = false;
		}
		else if (Pawn->GetLastMovementInputVector().SizeSquared() > 0.01f)
		{
			bWalkOffArmed = false;
			if (UAnimInstance* Anim = GetPawnAnim())
			{
				Anim->StopSlotAnimation(0.3f, FName(TEXT("DefaultSlot")));
			}
		}
	}

	if (bOrbiting)
	{
		TickOrbit(DeltaSeconds);
	}

	if (bStudio || StudioAlpha > 0.f)
	{
		TickStudio(DeltaSeconds);
	}

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
	const FRotator Watch = (Look - Loc).Rotation();
	const float Settle = FMath::SmoothStep(0.7f, 1.f, T);
	const FQuat Rot = FQuat::Slerp(Watch.Quaternion(), Follow->GetComponentQuat(), Settle);
	MenuCamera->SetActorLocationAndRotation(Loc, Rot);

	// lens: from the menu's cine lens to the play camera's field of view, depth of field
	// opening out, so the cut at the end is invisible
	if (UCineCameraComponent* Lens = GetLens())
	{
		const float HalfW = Lens->Filmback.SensorWidth * 0.5f;
		const float StartFocal = (MenuFocalLength > 0.f ? MenuFocalLength : MenuFocal) * FMath::Max(1.f, MenuSensorScale);
		const float FovStart = 2.f * FMath::RadiansToDegrees(FMath::Atan(HalfW / FMath::Max(1.f, StartFocal)));
		const float Fov = FMath::Lerp(FovStart, Follow->FieldOfView, S);
		Lens->SetCurrentFocalLength(HalfW / FMath::Tan(FMath::DegreesToRadians(Fov * 0.5f)));
		const float StartStop = MenuAperture > 0.f ? MenuAperture : MenuFStop;
		const float MaxStop = FMath::Max(StartStop, Lens->LensSettings.MaxFStop);
		Lens->SetCurrentAperture(FMath::Exp(FMath::Lerp(FMath::Loge(FMath::Max(0.5f, StartStop)), FMath::Loge(MaxStop), S)));
		FCameraFocusSettings Focus = Lens->FocusSettings;
		Focus.ManualFocusDistance = FVector::Dist(Loc, Look);
		Lens->FocusSettings = Focus;
	}

	SetCornerShade(1.f - S);

	if (T >= 1.f)
	{
		bOrbiting = false;
		PC->SetViewTarget(Pawn);      // same place, same angle: an invisible cut
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

	// she joins channel 1 (and keeps 0), clothes included, which can change while the page is open
	TArray<UPrimitiveComponent*> Parts;
	Pawn->GetComponents(Parts);
	for (UPrimitiveComponent* P : Parts)
	{
		if (!P->LightingChannels.bChannel1)
		{
			P->SetLightingChannels(true, true, P->LightingChannels.bChannel2);
		}
	}

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
