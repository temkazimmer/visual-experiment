#include "VisexMenuDirector.h"

#include "VisexMenuWidget.h"
#include "VisexBodyEditor.h"
#include "VisexPhotoMode.h"
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
#include "Components/InputComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
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
}

void AVisexMenuDirector::BeginPlay()
{
	Super::BeginPlay();
	Panel->SetVisibility(false);
	if (StandIn)
	{
		StandIn->SetActorHiddenInGame(true);
		// it stands exactly where the player goes: it must not block the camera boom
		StandIn->SetActorEnableCollision(false);
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
	const FVector Aim = (StandIn ? StandIn->GetActorLocation() : PC->GetPawn()->GetActorLocation()) + FVector(0.f, 0.f, CameraAimHeight);
	if (!bHaveBase)
	{
		CameraBase = MenuCamera->GetActorTransform();
		// closer along the line to her, so she grows in place in the frame
		CameraBase.SetLocation(Aim + (CameraBase.GetLocation() - Aim) * FMath::Clamp(CameraPullIn, 0.1f, 1.f));
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
			bHaveLens = true;
		}
		Lens->SetCurrentFocalLength(MenuFocal);
		Lens->SetCurrentAperture(MenuFStop);
		FCameraFocusSettings Focus = Lens->FocusSettings;
		Focus.ManualFocusDistance = FVector::Dist(CameraBase.GetLocation(), Aim);
		Lens->FocusSettings = Focus;
	}
	SpawnLeanColumn();
	Lean(bHaveLeaned);
	bHaveLeaned = true;
	bOrbiting = false;
	PC->SetViewTarget(MenuCamera);
	Clock = 0.f;

	// the panel rides with the camera, turned toward the figure on the right
	Panel->AttachToComponent(MenuCamera->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	Panel->SetRelativeLocation(PanelOffset);
	Panel->SetRelativeRotation(FRotator(0.f, 180.f + PanelYaw, 0.f));
	// draw the panel's texture larger and shrink it back to the same size on screen
	const UVisexMenuWidget* MenuDefaults = Panel->GetWidgetClass()
		? Cast<UVisexMenuWidget>(Panel->GetWidgetClass()->GetDefaultObject()) : nullptr;
	const float SS = GetMenuWidget() ? FMath::Max(1.f, GetMenuWidget()->Supersample)
		: MenuDefaults ? FMath::Max(1.f, MenuDefaults->Supersample) : 1.f;
	Panel->SetDrawSize(FVector2D(1100.f, 1000.f) * SS);
	Panel->SetRelativeScale3D(FVector(PanelScale / SS));
	Panel->SetTintColorAndOpacity(FLinearColor(PanelBrightness, PanelBrightness, PanelBrightness, 1.f));
	Panel->SetVisibility(true);
	if (UVisexMenuWidget* W = GetMenuWidget())
	{
		W->SetRenderOpacity(1.f);
		W->SetSelected(0);
	}
	FadeOut = -1.f;

	if (!bInputBound)
	{
		EnableInput(PC);
		// No Gamepad_LeftStick_* keys: UE 5.7's Mac controller layer reports every stick axis,
		// right stick included, as LeftStick_Up/Down. The left stick is read from LeftY in Tick.
		const FKey UpKeys[] = {EKeys::Gamepad_DPad_Up, EKeys::Up, EKeys::W};
		const FKey DownKeys[] = {EKeys::Gamepad_DPad_Down, EKeys::Down, EKeys::S};
		InputComponent->BindAxisKey(EKeys::Gamepad_LeftY);
		const FKey OkKeys[] = {EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_Special_Right, EKeys::Enter, EKeys::SpaceBar};
		for (const FKey& K : UpKeys) { InputComponent->BindKey(K, IE_Pressed, this, &AVisexMenuDirector::Up); }
		for (const FKey& K : DownKeys) { InputComponent->BindKey(K, IE_Pressed, this, &AVisexMenuDirector::Down); }
		for (const FKey& K : OkKeys) { InputComponent->BindKey(K, IE_Pressed, this, &AVisexMenuDirector::Confirm); }
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
	}
}

void AVisexMenuDirector::Confirm()
{
	if (!bOpen)
	{
		return;
	}
	const UVisexMenuWidget* W = GetMenuWidget();
	OnItemChosen(W ? W->GetSelected() : 0);
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
		for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
		{
			const FString Label = It->GetActorNameOrLabel();
			const bool bGround = Label.StartsWith(TEXT("Dunes")) || Label.StartsWith(TEXT("LowerGround")) || Label == TEXT("Plaza")
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
	// she pushes off the column while the camera circles round to her back
	StandUp();
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
	OnGameStarted();
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
	UStaticMesh* Mesh = LeanColumnMesh.LoadSynchronous();
	if (!Mesh)
	{
		return;
	}
	// a drum lying across behind her, its top at her hip
	const FVector Feet = Char->GetActorLocation() - FVector(0.f, 0.f, Char->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const FVector Fwd = Char->GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Fwd);
	const FVector Centre = Feet - Fwd * (LeanColumnGap + LeanColumnRadius) + Right * LeanColumnShift
		+ FVector(0.f, 0.f, LeanColumnRadius - 3.f);   // settled a little into the sand

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* Column = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(Centre), Params);
	if (!Column)
	{
		return;
	}
	UStaticMeshComponent* Comp = Column->GetStaticMeshComponent();
	Comp->SetMobility(EComponentMobility::Movable);
	Comp->SetStaticMesh(Mesh);
	// the drum's length runs along her left-right; the stone's own size is 50 cm radius, 100 cm long
	Column->SetActorRotation(FRotationMatrix::MakeFromZX(Right, Fwd).Rotator());
	Column->SetActorScale3D(FVector(LeanColumnRadius / 50.f, LeanColumnRadius / 50.f, LeanColumnLength / 100.f));
	FVector Origin, Extent;
	Column->GetActorBounds(false, Origin, Extent);
	Column->AddActorWorldOffset(Centre - Origin);
	LeanColumn = Column;
	SetColumnBlocksPawn(false);
}

void AVisexMenuDirector::SetColumnBlocksPawn(bool bBlock)
{
	if (const AStaticMeshActor* Column = Cast<AStaticMeshActor>(LeanColumn))
	{
		// while she leans her capsule sits partly inside it
		Column->GetStaticMeshComponent()->SetCollisionResponseToChannel(ECC_Pawn, bBlock ? ECR_Block : ECR_Ignore);
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
		const float FovStart = 2.f * FMath::RadiansToDegrees(FMath::Atan(HalfW / FMath::Max(1.f, MenuFocal)));
		const float Fov = FMath::Lerp(FovStart, Follow->FieldOfView, S);
		Lens->SetCurrentFocalLength(HalfW / FMath::Tan(FMath::DegreesToRadians(Fov * 0.5f)));
		const float MaxStop = FMath::Max(MenuFStop, Lens->LensSettings.MaxFStop);
		Lens->SetCurrentAperture(FMath::Exp(FMath::Lerp(FMath::Loge(FMath::Max(0.7f, MenuFStop)), FMath::Loge(MaxStop), S)));
		FCameraFocusSettings Focus = Lens->FocusSettings;
		Focus.ManualFocusDistance = FVector::Dist(Loc, Look);
		Lens->FocusSettings = Focus;
	}

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
			if (It->bUnbound) { Post = *It; }
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
