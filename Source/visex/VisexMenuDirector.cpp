#include "VisexMenuDirector.h"

#include "VisexMenuWidget.h"
#include "VisexBodyEditor.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Components/CapsuleComponent.h"
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
	if (!bHaveBase)
	{
		CameraBase = MenuCamera->GetActorTransform();
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
	}
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
		const FKey UpKeys[] = {EKeys::Gamepad_DPad_Up, EKeys::Gamepad_LeftStick_Up, EKeys::Up, EKeys::W};
		const FKey DownKeys[] = {EKeys::Gamepad_DPad_Down, EKeys::Gamepad_LeftStick_Down, EKeys::Down, EKeys::S};
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
		StartGame();
		OnPhotoMode();
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
	if (!BodyEditor)
	{
		UClass* Cls = BodyEditorClass ? BodyEditorClass.Get() : AVisexBodyEditor::StaticClass();
		BodyEditor = GetWorld()->SpawnActor<AVisexBodyEditor>(Cls, GetActorTransform());
		if (!BodyEditor)
		{
			return;
		}
		BodyEditor->OnClosed.AddDynamic(this, &AVisexMenuDirector::HandleBodyClosed);
	}
	BodyEditor->Open();
	if (!BodyEditor->IsOpen())
	{
		return;   // no body to edit on this pawn: stay on the menu
	}
	// the menu steps aside while the body page has the screen
	bOpen = false;
	Panel->SetVisibility(false);
	DisableInput(PC);
}

void AVisexMenuDirector::HandleBodyClosed()
{
	// the body page blends back to the menu shot; bring the menu up once it has landed
	const float Wait = BodyEditor ? FMath::Max(0.05f, BodyEditor->BlendTime) : 0.05f;
	GetWorldTimerManager().SetTimer(BodyTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			EnableInput(PC);
		}
		ShowMenu();
		if (UVisexMenuWidget* W = GetMenuWidget())
		{
			W->SetSelected(1);
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
	OnGameStarted();
}

void AVisexMenuDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Clock += DeltaSeconds;

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
