#include "VisexMenuDirector.h"

#include "VisexMenuWidget.h"
#include "Camera/CameraActor.h"
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
	PC->SetViewTarget(MenuCamera);
	CameraBase = MenuCamera->GetActorTransform();
	Clock = 0.f;

	// the panel rides with the camera, turned toward the figure on the right
	Panel->AttachToComponent(MenuCamera->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	Panel->SetRelativeLocation(PanelOffset);
	Panel->SetRelativeRotation(FRotator(0.f, 180.f + PanelYaw, 0.f));
	Panel->SetRelativeScale3D(FVector(PanelScale));
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
		StartGame();
		OnPhotoMode();
		break;
	case 2:
		UKismetSystemLibrary::QuitGame(this, UGameplayStatics::GetPlayerController(this, 0), EQuitPreference::Quit, false);
		break;
	default:
		StartGame();
		break;
	}
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
	PC->SetViewTargetWithBlend(PC->GetPawn(), BlendToPlayTime, EViewTargetBlendFunction::VTBlend_EaseInOut, 2.f);
	// hand control back a little before the swing lands, so it feels responsive
	GetWorldTimerManager().SetTimer(HandBackTimer, this, &AVisexMenuDirector::HandBack, FMath::Max(0.1f, BlendToPlayTime * 0.7f), false);
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
