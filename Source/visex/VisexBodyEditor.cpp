#include "VisexBodyEditor.h"

#include "VisexBodyComponent.h"
#include "VisexBodyWidget.h"
#include "CineCameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AVisexBodyEditor::AVisexBodyEditor()
{
	PrimaryActorTick.bCanEverTick = true;
	Camera = CreateDefaultSubobject<UCineCameraComponent>(TEXT("Camera"));
	RootComponent = Camera;

	Panel = CreateDefaultSubobject<UWidgetComponent>(TEXT("Panel"));
	Panel->SetupAttachment(Camera);
	Panel->SetWidgetSpace(EWidgetSpace::World);
	Panel->SetWidgetClass(UVisexBodyWidget::StaticClass());
	Panel->SetDrawSize(FVector2D(1100.f, 1240.f));
	Panel->SetTickWhenOffscreen(true);   // keep the panel current in captures and photo mode
	Panel->SetPivot(FVector2D(0.5f, 0.5f));
	Panel->SetBlendMode(EWidgetBlendMode::Transparent);
	Panel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Panel->SetCastShadow(false);
}

void AVisexBodyEditor::BeginPlay()
{
	Super::BeginPlay();
	Panel->SetVisibility(false);
	if (bOpenOnPlay)
	{
		GetWorldTimerManager().SetTimer(OpenTimer, this, &AVisexBodyEditor::TryOpen, 0.1f, true);
	}
}

void AVisexBodyEditor::TryOpen()
{
	if (BodyOf())
	{
		GetWorldTimerManager().ClearTimer(OpenTimer);
		Open();
	}
}

UVisexBodyComponent* AVisexBodyEditor::BodyOf() const
{
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	return Pawn ? Pawn->FindComponentByClass<UVisexBodyComponent>() : nullptr;
}

UVisexBodyWidget* AVisexBodyEditor::Widget() const
{
	return Panel ? Cast<UVisexBodyWidget>(Panel->GetUserWidgetObject()) : nullptr;
}

void AVisexBodyEditor::Open()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (bOpen || !PC || !Char || !BodyOf())
	{
		return;
	}
	Char->DisableInput(PC);
	StartRotation = Char->GetActorRotation();

	// a closer, full-body shot from the front; she stands right of centre
	const FVector Feet = Char->GetActorLocation()
		- FVector(0.f, 0.f, Char->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const FVector Fwd = FRotator(0.f, StartRotation.Yaw, 0.f).Vector();
	const FVector Right = FRotator(0.f, StartRotation.Yaw + 90.f, 0.f).Vector();
	const FVector CamPos = Feet + Fwd * Distance + FVector(0.f, 0.f, CameraHeight);
	const FVector Target = Feet + FVector(0.f, 0.f, LookHeight) + Right * SideShift;
	SetActorLocationAndRotation(CamPos, (Target - CamPos).Rotation());
	Camera->SetCurrentFocalLength(FocalLength);
	Camera->FocusSettings.FocusMethod = ECameraFocusMethod::Manual;
	Camera->FocusSettings.ManualFocusDistance = Distance;
	Camera->CurrentAperture = 2.8f;

	Panel->SetRelativeLocation(PanelOffset);
	Panel->SetRelativeRotation(FRotator(0.f, 180.f + PanelYaw, 0.f));
	Panel->SetRelativeScale3D(FVector(PanelScale));
	Panel->SetTintColorAndOpacity(FLinearColor(PanelBrightness, PanelBrightness, PanelBrightness, 1.f));
	Panel->SetVisibility(true);

	PreviousView = PC->GetViewTarget();
	PC->SetViewTargetWithBlend(this, BlendTime, VTBlend_Cubic);
	EnableInput(PC);
	BindKeys();
	bOpen = true;
	Tab = 0;
	Selected = 0;
	TabRow[0] = TabRow[1] = 0;
	Applied = BodyOf()->Profile;
	Refresh();
}

void AVisexBodyEditor::BindKeys()
{
	if (bKeysBound || !InputComponent)
	{
		return;
	}
	const FKey UpKeys[] = {EKeys::Gamepad_DPad_Up, EKeys::Gamepad_LeftStick_Up, EKeys::Up, EKeys::W};
	const FKey DownKeys[] = {EKeys::Gamepad_DPad_Down, EKeys::Gamepad_LeftStick_Down, EKeys::Down, EKeys::S};
	const FKey LeftKeys[] = {EKeys::Gamepad_DPad_Left, EKeys::Gamepad_LeftStick_Left, EKeys::Left, EKeys::A};
	const FKey RightKeys[] = {EKeys::Gamepad_DPad_Right, EKeys::Gamepad_LeftStick_Right, EKeys::Right, EKeys::D};
	for (const FKey& K : UpKeys) { InputComponent->BindKey(K, IE_Pressed, this, &AVisexBodyEditor::Up); }
	for (const FKey& K : DownKeys) { InputComponent->BindKey(K, IE_Pressed, this, &AVisexBodyEditor::Down); }
	for (const FKey& K : LeftKeys)
	{
		InputComponent->BindKey(K, IE_Pressed, this, &AVisexBodyEditor::Left);
		InputComponent->BindKey(K, IE_Repeat, this, &AVisexBodyEditor::Left);
	}
	for (const FKey& K : RightKeys)
	{
		InputComponent->BindKey(K, IE_Pressed, this, &AVisexBodyEditor::Right);
		InputComponent->BindKey(K, IE_Repeat, this, &AVisexBodyEditor::Right);
	}
	const FKey PrevKeys[] = {EKeys::Gamepad_LeftShoulder, EKeys::Q};
	const FKey NextKeys[] = {EKeys::Gamepad_RightShoulder, EKeys::E, EKeys::Tab};
	for (const FKey& K : PrevKeys) { InputComponent->BindKey(K, IE_Pressed, this, &AVisexBodyEditor::PrevTab); }
	for (const FKey& K : NextKeys) { InputComponent->BindKey(K, IE_Pressed, this, &AVisexBodyEditor::NextTab); }
	const FKey ApplyKeys[] = {EKeys::Gamepad_FaceButton_Bottom, EKeys::Enter, EKeys::SpaceBar};
	for (const FKey& K : ApplyKeys) { InputComponent->BindKey(K, IE_Pressed, this, &AVisexBodyEditor::ApplyTab); }
	InputComponent->BindKey(EKeys::Gamepad_FaceButton_Top, IE_Pressed, this, &AVisexBodyEditor::ResetSelected);
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &AVisexBodyEditor::ResetSelected);
	const FKey BackKeys[] = {EKeys::Gamepad_FaceButton_Right, EKeys::Escape, EKeys::BackSpace};
	for (const FKey& K : BackKeys) { InputComponent->BindKey(K, IE_Pressed, this, &AVisexBodyEditor::Close); }
	bKeysBound = true;
}

void AVisexBodyEditor::Close()
{
	if (!bOpen)
	{
		return;
	}
	bOpen = false;
	if (UVisexBodyComponent* Body = BodyOf())
	{
		// back out: whatever was only being tried goes away
		Body->Profile = Applied;
		Body->Apply();
	}	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Char->SetActorRotation(StartRotation);
	}
	if (PC)
	{
		DisableInput(PC);
		if (PreviousView.IsValid())
		{
			PC->SetViewTargetWithBlend(PreviousView.Get(), BlendTime, VTBlend_Cubic);
		}
	}
	Panel->SetVisibility(false);
	OnClosed.Broadcast();
}

void AVisexBodyEditor::Select(int32 Delta)
{
	const int32 N = UVisexBodyWidget::RowCount(Tab);
	Selected = ((Selected + Delta) % N + N) % N;
	TabRow[Tab] = Selected;
	// dress: moving onto an outfit tries it on
	const TArray<FVisexOutfit>& Outfits = FVisexBodyProfile::Outfits();
	if (Tab == 1 && Selected < Outfits.Num())
	{
		if (UVisexBodyComponent* Body = BodyOf())
		{
			Body->SetOutfit(Outfits[Selected].Name);
		}
	}
	Refresh();
}

void AVisexBodyEditor::SwitchTab(int32 Delta)
{
	if (!bOpen)
	{
		return;
	}
	TabRow[Tab] = Selected;
	Tab = ((Tab + Delta) % 2 + 2) % 2;
	Selected = TabRow[Tab];
	if (Tab == 1 && Selected == 0)
	{
		// land on what she has on
		if (const UVisexBodyComponent* Body = BodyOf())
		{
			Selected = TabRow[1] = FVisexBodyProfile::OutfitIndex(Body->Profile.Outfit);
		}
	}
	Refresh();
}

void AVisexBodyEditor::Adjust(float Steps)
{
	UVisexBodyComponent* Body = BodyOf();
	const TArray<FVisexBodySlider>& Sliders = FVisexBodyProfile::Sliders();
	if (!bOpen || !Body || Tab != 0 || Selected >= Sliders.Num())
	{
		return;
	}
	const FVisexBodySlider& S = Sliders[Selected];
	const float Step = (S.Max - S.Min) / 40.f;
	Body->SetValue(S.Name, Body->GetValue(S.Name) + Step * Steps);
	Refresh();
}

void AVisexBodyEditor::ResetSelected()
{
	UVisexBodyComponent* Body = BodyOf();
	const TArray<FVisexBodySlider>& Sliders = FVisexBodyProfile::Sliders();
	if (Body && Tab == 0 && Selected < Sliders.Num())
	{
		Body->ResetValue(Sliders[Selected].Name);
		Refresh();
	}
}

void AVisexBodyEditor::ApplyTab()
{
	UVisexBodyComponent* Body = BodyOf();
	if (!bOpen || !Body)
	{
		return;
	}
	if (Tab == 0)
	{
		Applied.Values = Body->Profile.Values;
	}
	else
	{
		Applied.Outfit = Body->Profile.Outfit;
	}
	Body->SaveProfile(Applied);   // only what was applied is kept
	Refresh();
}

void AVisexBodyEditor::Refresh()
{
	const UVisexBodyComponent* Body = BodyOf();
	if (UVisexBodyWidget* W = Widget(); W && Body)
	{
		FVisexCharacterView View;
		View.Tab = Tab;
		View.Selected = Selected;
		View.Shown = &Body->Profile;
		View.Applied = &Applied;
		View.bBodyDirty = !Body->Profile.Values.OrderIndependentCompareEqual(Applied.Values);
		View.bDressDirty = Body->Profile.Outfit != Applied.Outfit;
		W->Show(View);
	}
}

void AVisexBodyEditor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bOpen)
	{
		return;
	}
	if (ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Char->AddActorWorldRotation(FRotator(0.f, TurnSpeed * DeltaSeconds, 0.f));
	}
}
