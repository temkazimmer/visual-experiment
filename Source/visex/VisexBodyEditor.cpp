#include "VisexBodyEditor.h"

#include "VisexBodyComponent.h"
#include "VisexBodyWidget.h"
#include "CineCameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Components/InputComponent.h"
#include "Containers/Ticker.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "InputKeyEventArgs.h"
#include "HAL/IConsoleManager.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace
{
	/** How one body row is framed. The point looked at is the bone, moved Down (0..1) toward
	 *  the ground and Up in cm; Height is how many cm of her the frame shows top to bottom. */
	struct FPart
	{
		const TCHAR* Row;
		const TCHAR* Bone;
		float Down;
		float Up;
		float Height;
		float Focal;
		bool bBack;
	};
	const FPart Parts[] = {
		{TEXT("Skin"),      TEXT("spine_03"), 0.f,   8.f,  120.f, 50.f, false},
		{TEXT("Head"),      TEXT("head"),     0.f,   9.f,  44.f,  85.f, false},
		{TEXT("Neck"),      TEXT("neck_01"),  0.f,   2.f,  50.f,  85.f, false},
		{TEXT("Shoulders"), TEXT("spine_05"), 0.f,   2.f,  72.f,  65.f, false},
		{TEXT("Chest"),     TEXT("spine_04"), 0.f,   0.f,  72.f,  65.f, false},
		{TEXT("Bust"),      TEXT("spine_04"), 0.f,   2.f,  62.f,  70.f, false},
		{TEXT("BustLift"),  TEXT("spine_04"), 0.f,   2.f,  62.f,  70.f, false},
		{TEXT("BustApart"), TEXT("spine_04"), 0.f,   2.f,  62.f,  70.f, false},
		{TEXT("Waist"),     TEXT("spine_02"), 0.f,   0.f,  72.f,  65.f, false},
		{TEXT("Abs"),       TEXT("spine_02"), 0.f,   -2.f, 58.f,  75.f, false},
		{TEXT("Fat"),       TEXT("spine_02"), 0.f,   -4.f, 92.f,  55.f, false},
		{TEXT("Hips"),      TEXT("pelvis"),   0.f,   -2.f, 78.f,  65.f, false},
		{TEXT("Glutes"),    TEXT("pelvis"),   0.f,   -6.f, 68.f,  70.f, true},
		{TEXT("Legs"),      TEXT("pelvis"),   0.5f,  0.f,  128.f, 40.f, false},
		{TEXT("Thighs"),    TEXT("pelvis"),   0.3f,  0.f,  84.f,  60.f, false},
		{TEXT("ThighGap"),  TEXT("pelvis"),   0.2f,  0.f,  58.f,  75.f, false},
		{TEXT("Calves"),    TEXT("pelvis"),   0.7f,  0.f,  72.f,  65.f, true},
		{TEXT("ArmLength"), TEXT("spine_03"), 0.f,   -6.f, 112.f, 45.f, false},
		{TEXT("Arms"),      TEXT("spine_04"), 0.f,   -4.f, 88.f,  55.f, false},
		{TEXT("Hands"),     TEXT("hand_l"),   0.f,   -6.f, 42.f,  85.f, false},
		{TEXT("Feet"),      TEXT("pelvis"),   0.95f, 0.f,  46.f,  70.f, false},
	};
	/** How far right of centre she stands, as a share of half the frame's width. */
	const float SideShare = 0.39f;
}

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
	// drawn after depth of field and over the scene, so the text stays sharp in a close-up
	// (Tools/char_panel.py makes it; without it the engine's default panel material is used)
	if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Character/UI/M_BodyPanel.M_BodyPanel"), nullptr, LOAD_NoWarn))
	{
		Panel->SetMaterial(0, M);
	}
	// stencil 1 keeps the text out of screen filters (M_Anime80s, Tools/menu_panel_material.py)
	Panel->SetRenderCustomDepth(true);
	Panel->SetCustomDepthStencilValue(1);
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

	Camera->FocusSettings.FocusMethod = ECameraFocusMethod::Manual;
	bOpen = true;
	Tab = 0;
	Selected = 0;
	TabRow[0] = TabRow[1] = 0;
	Turn = SideYaw = TurnVelocity = 0.f;
	bBack = bRecentre = false;
	ShotRow = TEXT("?");
	Reframe(true);      // start on the full figure,
	Selected = 0;
	Reframe(false);     // then ease in to the first row

	Panel->SetRelativeLocation(PanelOffset);
	Panel->SetRelativeRotation(FRotator(0.f, 180.f + PanelYaw, 0.f));
	Panel->SetRelativeScale3D(FVector(PanelScale));
	Panel->SetTintColorAndOpacity(FLinearColor(PanelBrightness, PanelBrightness, PanelBrightness, 1.f));
	Panel->SetVisibility(true);

	PreviousView = PC->GetViewTarget();
	PC->SetViewTargetWithBlend(this, BlendTime, VTBlend_Cubic);
	EnableInput(PC);
	PC->SetIgnoreLookInput(true);   // nothing else may take the stick as a camera look
	BindKeys();
	Applied = BodyOf()->Profile;
	Refresh();
}

void AVisexBodyEditor::BindKeys()
{
	if (bKeysBound || !InputComponent)
	{
		return;
	}
	// no Gamepad_LeftStick_* keys: on Mac, UE 5.7's Apple controller code reports every stick
	// axis (the right stick too) as LeftStick_Up/Down, so the right stick jumped rows. The
	// left stick is read from its axes in Tick instead (StickNavigate).
	const FKey UpKeys[] = {EKeys::Gamepad_DPad_Up, EKeys::Up, EKeys::W};
	const FKey DownKeys[] = {EKeys::Gamepad_DPad_Down, EKeys::Down, EKeys::S};
	const FKey LeftKeys[] = {EKeys::Gamepad_DPad_Left, EKeys::Left, EKeys::A};
	const FKey RightKeys[] = {EKeys::Gamepad_DPad_Right, EKeys::Right, EKeys::D};
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
	InputComponent->BindAxisKey(EKeys::Gamepad_RightX);
	InputComponent->BindAxisKey(EKeys::Gamepad_LeftX);
	InputComponent->BindAxisKey(EKeys::Gamepad_LeftY);
	InputComponent->BindAxisKey(EKeys::MouseX);
	// the right stick only turns her: no R3 (F on the keyboard still faces her to the camera)
	InputComponent->BindKey(EKeys::F, IE_Pressed, this, &AVisexBodyEditor::FaceCamera);
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
		// leaving keeps what she shows now, both tabs, so Start plays with it
		Applied = Body->Profile;
		Body->SaveProfile(Applied);
		Body->Apply();
	}
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Char->SetActorRotation(StartRotation);
	}
	if (PC)
	{
		DisableInput(PC);
		PC->SetIgnoreLookInput(false);
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
	Reframe(false);
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
	Reframe(false);
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

void AVisexBodyEditor::FaceCamera()
{
	Turn = FMath::UnwindDegrees(Turn);
	TurnVelocity = 0.f;
	bRecentre = true;
}

AVisexBodyEditor::FShot AVisexBodyEditor::ShotNow(bool& bOutBack) const
{
	FShot Shot;
	bOutBack = false;
	const ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Char)
	{
		return Shot;
	}
	const FVector Feet = Char->GetActorLocation()
		- FVector(0.f, 0.f, Char->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const float SensorHeight = FMath::Max(Camera->Filmback.SensorHeight, 1.f);
	// she can be taller or shorter than the build the numbers were framed on (159 cm)
	const float Tall = Char->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() / 80.6f;

	const FPart* Part = nullptr;
	for (const FPart& P : Parts)
	{
		if (ShotRow == P.Row)
		{
			Part = &P;
		}
	}
	const USkeletalMeshComponent* Mesh = Char->GetMesh();
	if (!Part || !Mesh || Mesh->GetBoneIndex(Part->Bone) == INDEX_NONE)
	{
		// the whole figure: Height, the dress tab, APPLY
		Shot.Look = Feet + FVector(0.f, 0.f, LookHeight * Tall);
		Shot.Focal = FocalLength;
		Shot.Distance = Distance * Tall;
		Shot.Aperture = WideAperture;
		return Shot;
	}
	// aim where the part would be without the stick's turn, so turning her never moves the
	// camera: she spins in a frame that stays put
	const FVector Pivot = Char->GetActorLocation();
	FVector P = Pivot + FRotator(0.f, -Turn, 0.f).RotateVector(Mesh->GetSocketLocation(Part->Bone) - Pivot);
	P.Z = FMath::Lerp(P.Z, Feet.Z, Part->Down) + Part->Up * Tall;
	Shot.Look = P;
	Shot.Focal = Part->Focal;
	Shot.Distance = Part->Height * Tall * Part->Focal / SensorHeight;
	Shot.Aperture = CloseAperture;
	bOutBack = Part->bBack;
	return Shot;
}

void AVisexBodyEditor::Reframe(bool bSnap)
{
	const TArray<FVisexBodySlider>& Sliders = FVisexBodyProfile::Sliders();
	const FName Row = (!bSnap && Tab == 0 && Selected < Sliders.Num()) ? Sliders[Selected].Name : NAME_None;
	if (Row == ShotRow && !bSnap)
	{
		return;
	}
	ShotRow = Row;
	bool bNowBack = false;
	const FShot Target = ShotNow(bNowBack);
	SmoothLook = Target.Look;
	if (bNowBack != bBack)
	{
		// front rows show her front, back rows her back: start again from that side
		bBack = bNowBack;
		FaceCamera();
	}
	if (bSnap)
	{
		Current = Target;
		ShotTime = 1.f;
		PlaceCamera(Current);
	}
	else
	{
		From = Current;
		ShotTime = 0.f;
	}
}

void AVisexBodyEditor::PlaceCamera(const FShot& Shot)
{
	const ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Char)
	{
		return;
	}
	const float FeetZ = Char->GetActorLocation().Z - Char->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Fwd = FRotator(0.f, StartRotation.Yaw, 0.f).Vector();
	const FVector Right = FRotator(0.f, StartRotation.Yaw + 90.f, 0.f).Vector();
	// a little above the part, never under the ground; she stands right of centre
	FVector CamPos = Shot.Look + Fwd * Shot.Distance + FVector(0.f, 0.f, Shot.Distance * 0.038f);
	CamPos.Z = FMath::Max(CamPos.Z, FeetZ + 14.f);
	const float HalfWidth = Shot.Distance * Camera->Filmback.SensorWidth * 0.5f / FMath::Max(Shot.Focal, 1.f);
	const FVector Aim = Shot.Look + Right * HalfWidth * SideShare;
	SetActorLocationAndRotation(CamPos, (Aim - CamPos).Rotation());
	Camera->SetCurrentFocalLength(Shot.Focal);
	Camera->FocusSettings.ManualFocusDistance = Shot.Distance;
	Camera->CurrentAperture = Shot.Aperture;

	// the panel rides the camera; pushed back as the lens gets longer, it keeps its place
	// and size on screen
	const float K = Shot.Focal / FMath::Max(FocalLength, 1.f);
	Panel->SetRelativeLocation(FVector(PanelOffset.X * K, PanelOffset.Y, PanelOffset.Z));
	Panel->SetRelativeScale3D(FVector(PanelScale));
}

void AVisexBodyEditor::StickNavigate(float DeltaSeconds)
{
	// the left stick as four buttons, from its axes: past 0.5 it presses (the larger axis
	// wins), under 0.3 it lets go; held, a slider keeps stepping and the rows step slower
	const float X = InputComponent ? InputComponent->GetAxisKeyValue(EKeys::Gamepad_LeftX) : 0.f;
	const float Y = InputComponent ? InputComponent->GetAxisKeyValue(EKeys::Gamepad_LeftY) : 0.f;
	int32 Dir = StickDir;
	if (FMath::Max(FMath::Abs(X), FMath::Abs(Y)) < 0.3f)
	{
		Dir = 0;
	}
	else if (FMath::Max(FMath::Abs(X), FMath::Abs(Y)) > 0.5f)
	{
		Dir = FMath::Abs(X) > FMath::Abs(Y) ? (X > 0.f ? 2 : 4) : (Y > 0.f ? 1 : 3);   // up right down left
	}
	if (Dir != StickDir)
	{
		StickDir = Dir;
		StickHeld = 0.f;
		StickNext = 0.4f;
		if (Dir != 0)
		{
			StickPress(Dir);
		}
		return;
	}
	if (Dir == 0)
	{
		return;
	}
	StickHeld += DeltaSeconds;
	if (StickHeld >= StickNext)
	{
		StickNext += (Dir == 1 || Dir == 3) ? 0.18f : 0.06f;
		StickPress(Dir);
	}
}

void AVisexBodyEditor::StickPress(int32 Dir)
{
	switch (Dir)
	{
	case 1: Up(); break;
	case 2: Right(); break;
	case 3: Down(); break;
	case 4: Left(); break;
	default: break;
	}
}

void AVisexBodyEditor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bOpen)
	{
		return;
	}
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Char)
	{
		return;
	}

	StickNavigate(DeltaSeconds);

	// turn: right stick, or a drag with the left mouse button; it coasts to a stop
	float Stick = InputComponent ? InputComponent->GetAxisKeyValue(EKeys::Gamepad_RightX) : 0.f;
	Stick = FMath::Abs(Stick) < 0.15f ? 0.f : Stick;
	const bool bDrag = PC && InputComponent && PC->IsInputKeyDown(EKeys::LeftMouseButton);
	const float Mouse = bDrag ? InputComponent->GetAxisKeyValue(EKeys::MouseX) * MouseTurn : 0.f;
	if (Stick != 0.f || Mouse != 0.f)
	{
		bRecentre = false;
	}
	if (bRecentre)
	{
		Turn = FMath::FInterpTo(Turn, 0.f, DeltaSeconds, 5.f);
		bRecentre = !FMath::IsNearlyZero(Turn, 0.05f);
	}
	else
	{
		// pushing the stick right brings her front round to the right of the frame
		float Want = -Stick * StickTurnSpeed + TurnSpeed;
		float Response = TurnResponse;
		if (bDrag && DeltaSeconds > KINDA_SMALL_NUMBER)
		{
			Want = -Mouse / DeltaSeconds;
			Response = 20.f;
		}
		TurnVelocity = FMath::FInterpTo(TurnVelocity, Want, DeltaSeconds, Response);
		Turn += TurnVelocity * DeltaSeconds;
	}
	SideYaw = FMath::FInterpTo(SideYaw, bBack ? 180.f : 0.f, DeltaSeconds, 4.5f);
	Char->SetActorRotation(FRotator(0.f, StartRotation.Yaw + SideYaw + Turn, 0.f));

	// camera: ease to the row's framing, then stay with the part as she breathes and turns
	bool bUnused = false;
	FShot Target = ShotNow(bUnused);
	SmoothLook = FMath::VInterpTo(SmoothLook, Target.Look, DeltaSeconds, 5.f);
	Target.Look = SmoothLook;
	ShotTime = FMath::Min(1.f, ShotTime + DeltaSeconds / FMath::Max(CloseUpTime, 0.01f));
	const float A = FMath::InterpEaseInOut(0.f, 1.f, ShotTime, 2.f);
	Current.Look = FMath::Lerp(From.Look, Target.Look, A);
	Current.Distance = FMath::Lerp(From.Distance, Target.Distance, A);
	Current.Focal = FMath::Lerp(From.Focal, Target.Focal, A);
	Current.Aperture = FMath::Lerp(From.Aperture, Target.Aperture, A);
	PlaceCamera(Current);
}

#if !UE_BUILD_SHIPPING
namespace
{
	/** visex.Pad <Left|Right> <x> <y> [seconds]: holds a stick the way the Mac controller layer
	 *  reports it (UE 5.7, AppleControllerInterface): the stick's analog axes every frame, plus
	 *  its quirk of pressing LeftStick_Up/Down for any axis past 0.24. Fed to the player
	 *  controller as real key input (Slate would route it to whatever has focus). For testing. */
	void PadCommand(const TArray<FString>& Args, UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		if (Args.Num() < 3 || !PC)
		{
			return;
		}
		const bool bRight = Args[0].Equals(TEXT("Right"), ESearchCase::IgnoreCase);
		const float X = FCString::Atof(*Args[1]);
		const float Y = FCString::Atof(*Args[2]);
		const float Seconds = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 0.5f;
		const FKey AxisX = bRight ? EKeys::Gamepad_RightX : EKeys::Gamepad_LeftX;
		const FKey AxisY = bRight ? EKeys::Gamepad_RightY : EKeys::Gamepad_LeftY;
		TArray<FKey> Quirk;
		for (const float V : {X, Y})
		{
			if (FMath::Abs(V) >= 0.24f)
			{
				Quirk.AddUnique(V > 0.f ? EKeys::Gamepad_LeftStick_Up : EKeys::Gamepad_LeftStick_Down);
			}
		}
		const FInputDeviceId Device = FInputDeviceId::CreateFromInternalId(0);   // the first gamepad
		TWeakObjectPtr<APlayerController> Weak(PC);
		auto Send = [Device](APlayerController* P, const FKey& Key, EInputEvent Event, float Amount) {
			FViewport* Viewport = P->GetLocalPlayer() && P->GetLocalPlayer()->ViewportClient
				? P->GetLocalPlayer()->ViewportClient->Viewport : nullptr;
			FInputKeyEventArgs Args(Viewport, Device, Key, Event, Amount, false, FPlatformTime::Cycles64());
			P->InputKey(Args);
		};
		for (const FKey& Key : Quirk)
		{
			Send(PC, Key, IE_Pressed, 1.f);
		}
		const double End = FPlatformTime::Seconds() + Seconds;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([=](float) {
			APlayerController* P = Weak.Get();
			if (!P)
			{
				return false;
			}
			const bool bDone = FPlatformTime::Seconds() >= End;
			Send(P, AxisX, IE_Axis, bDone ? 0.f : X);
			Send(P, AxisY, IE_Axis, bDone ? 0.f : Y);
			if (bDone)
			{
				for (const FKey& Key : Quirk)
				{
					Send(P, Key, IE_Released, 0.f);
				}
			}
			return !bDone;
		}));
	}
	FAutoConsoleCommandWithWorldAndArgs GPadCommand(TEXT("visex.Pad"),
		TEXT("visex.Pad <Left|Right> <x> <y> [seconds]: hold a gamepad stick as the Mac controller layer reports it"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PadCommand));
}
#endif
