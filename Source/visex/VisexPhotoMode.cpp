#include "VisexPhotoMode.h"

#include "VisexPoseWheel.h"
#include "VisexAnimInstance.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FName Slot(TEXT("DefaultSlot"));
	// same folder and order as the pose wheel, so index i here is its pose i
	const TCHAR* PoseFolder = TEXT("/Game/Characters/Poses");
}

AVisexPhotoMode::AVisexPhotoMode()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	WalkLoop = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/Menu/A_Photo_Walk.A_Photo_Walk")));
	RunLoop = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Characters/Menu/A_Photo_Run.A_Photo_Run")));
}

void AVisexPhotoMode::BuildMotions()
{
	Motions.Reset();
	Motions.Add({TEXT("Idle"), nullptr, INDEX_NONE});
	if (UAnimSequence* Walk = WalkLoop.LoadSynchronous()) { Motions.Add({TEXT("Walk"), Walk, INDEX_NONE, 1.f}); }
	if (UAnimSequence* Run = RunLoop.LoadSynchronous()) { Motions.Add({TEXT("Run"), Run, INDEX_NONE, 0.35f}); }

	TArray<FAssetData> Found;
	FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().GetAssetsByPath(PoseFolder, Found, true);
	Found.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.LexicalLess(B.AssetName); });
	int32 PoseIndex = 0;
	for (const FAssetData& Asset : Found)
	{
		FString Name = Asset.AssetName.ToString();
		if (Name.EndsWith(TEXT("_In")) || Name.EndsWith(TEXT("_Out")) || !Asset.IsInstanceOf(UAnimSequence::StaticClass()))
		{
			continue;
		}
		Name.RemoveFromStart(TEXT("A_Pose_"));
		Motions.Add({Name.Replace(TEXT("_"), TEXT(" ")), nullptr, PoseIndex++});
	}
}

void AVisexPhotoMode::Open()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (bOpen || !PC || !Char)
	{
		return;
	}
	BuildMotions();
	if (const UAnimInstance* Anim = Char->GetMesh() ? Char->GetMesh()->GetAnimInstance() : nullptr)
	{
		SavedRootMotion = Anim->RootMotionMode;
	}
	if (!Camera)
	{
		Camera = GetWorld()->SpawnActor<ACineCameraActor>(ACineCameraActor::StaticClass(), FTransform::Identity);
	}
	if (!Camera)
	{
		return;
	}
	if (UCineCameraComponent* Lens = Camera->GetCineCameraComponent())
	{
		Lens->SetCurrentFocalLength(FocalLength);
		Lens->SetCurrentAperture(Aperture);
		Lens->FocusSettings.FocusMethod = ECameraFocusMethod::Manual;
	}
	// start from wherever the view is now, so the move in reads as one shot
	const FVector From = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : Char->GetActorLocation() - Char->GetActorForwardVector() * StartDistance;
	const FVector Rel = From - Char->GetActorLocation();
	Yaw = FMath::RadiansToDegrees(FMath::Atan2(Rel.Y, Rel.X));
	Distance = StartDistance;
	Height = StartHeight;
	LookZ = -1.f;
	LookGoal = -1.f;
	bOpen = true;
	bPaused = false;
	PlaceCamera();

	PreviousView = PC->GetViewTarget();
	PC->SetViewTargetWithBlend(Camera, BlendTime, VTBlend_Cubic);
	Char->DisableInput(PC);
	EnableInput(PC);
	if (!bKeysBound && InputComponent)
	{
		const FKey NextKeys[] = {EKeys::Gamepad_RightShoulder, EKeys::E};
		const FKey PrevKeys[] = {EKeys::Gamepad_LeftShoulder, EKeys::Q};
		const FKey PauseKeys[] = {EKeys::Gamepad_FaceButton_Bottom, EKeys::SpaceBar};
		const FKey ShotKeys[] = {EKeys::Gamepad_FaceButton_Left, EKeys::P};
		const FKey BackKeys[] = {EKeys::Gamepad_FaceButton_Right, EKeys::Escape, EKeys::BackSpace};
		for (const FKey& K : NextKeys) { InputComponent->BindKey(K, IE_Pressed, this, &AVisexPhotoMode::Next); }
		for (const FKey& K : PrevKeys) { InputComponent->BindKey(K, IE_Pressed, this, &AVisexPhotoMode::Prev); }
		for (const FKey& K : PauseKeys) { InputComponent->BindKey(K, IE_Pressed, this, &AVisexPhotoMode::TogglePause); }
		for (const FKey& K : ShotKeys) { InputComponent->BindKey(K, IE_Pressed, this, &AVisexPhotoMode::TakeShot); }
		for (const FKey& K : BackKeys) { InputComponent->BindKey(K, IE_Pressed, this, &AVisexPhotoMode::Close); }
		// sticks as axes only: on Mac the LeftStick_* keys also fire for the right stick
		InputComponent->BindAxisKey(EKeys::Gamepad_RightX);
		InputComponent->BindAxisKey(EKeys::Gamepad_LeftX);
		InputComponent->BindAxisKey(EKeys::Gamepad_LeftY);
		InputComponent->BindAxisKey(EKeys::Gamepad_LeftTriggerAxis);
		InputComponent->BindAxisKey(EKeys::Gamepad_RightTriggerAxis);
		InputComponent->BindAxisKey(EKeys::MouseX);
		InputComponent->BindAxisKey(EKeys::MouseWheelAxis);
		bKeysBound = true;
	}
	SetMotion(0);
	ShowHud(true);
}

void AVisexPhotoMode::Close()
{
	if (!bOpen)
	{
		return;
	}
	bOpen = false;
	ClearMotion();
	if (bPaused)
	{
		TogglePause();
	}
	ShowHud(false);
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		DisableInput(PC);
		if (PreviousView.IsValid())
		{
			PC->SetViewTargetWithBlend(PreviousView.Get(), BlendTime, VTBlend_Cubic);
		}
	}
	OnClosed.Broadcast();
}

void AVisexPhotoMode::ClearMotion()
{
	ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Char)
	{
		return;
	}
	if (PlayingPose != INDEX_NONE)
	{
		if (UVisexPoseWheelComponent* Wheel = Char->FindComponentByClass<UVisexPoseWheelComponent>())
		{
			Wheel->StopPose();
		}
		PlayingPose = INDEX_NONE;
	}
	if (UAnimInstance* Anim = Char->GetMesh() ? Char->GetMesh()->GetAnimInstance() : nullptr)
	{
		Anim->StopSlotAnimation(0.3f, Slot);
		Anim->SetRootMotionMode(SavedRootMotion);
		if (UVisexAnimInstance* Visex = Cast<UVisexAnimInstance>(Anim))
		{
			Visex->PhotoGait = -1.f;
		}
	}
}

void AVisexPhotoMode::SetMotion(int32 Index)
{
	if (!bOpen || Motions.Num() == 0)
	{
		return;
	}
	Motion = (Index % Motions.Num() + Motions.Num()) % Motions.Num();
	if (bPaused)
	{
		TogglePause();
	}
	ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	UAnimInstance* Anim = Char && Char->GetMesh() ? Char->GetMesh()->GetAnimInstance() : nullptr;
	const FMotion& M = Motions[Motion];
	// her catwalk layer follows speed; standing in place it needs telling how hard she walks
	if (UVisexAnimInstance* Visex = Cast<UVisexAnimInstance>(Anim))
	{
		Visex->PhotoGait = M.Loop ? M.Gait : -1.f;
	}
	if (M.Pose != INDEX_NONE)
	{
		// poses go through the wheel: it handles sitting down, the slope and getting up
		if (Anim)
		{
			Anim->StopSlotAnimation(0.2f, Slot);
			Anim->SetRootMotionMode(SavedRootMotion);
		}
		if (UVisexPoseWheelComponent* Wheel = Char ? Char->FindComponentByClass<UVisexPoseWheelComponent>() : nullptr)
		{
			Wheel->PlayPose(M.Pose);
			PlayingPose = M.Pose;
		}
	}
	else
	{
		if (PlayingPose != INDEX_NONE)
		{
			if (UVisexPoseWheelComponent* Wheel = Char ? Char->FindComponentByClass<UVisexPoseWheelComponent>() : nullptr)
			{
				Wheel->StopPose();
			}
			PlayingPose = INDEX_NONE;
		}
		if (Anim)
		{
			// walk and run carry root motion: keep it in the clip so she treads in place
			Anim->SetRootMotionMode(M.Loop ? ERootMotionMode::IgnoreRootMotion : SavedRootMotion.GetValue());
			if (M.Loop)
			{
				Anim->PlaySlotAnimationAsDynamicMontage(M.Loop, Slot, 0.35f, 0.35f, 1.f, 1000000);
			}
			else
			{
				Anim->StopSlotAnimation(0.35f, Slot);
			}
		}
	}
	RefreshHud();
}

void AVisexPhotoMode::TogglePause()
{
	ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Char || !Char->GetMesh())
	{
		return;
	}
	bPaused = !bPaused;
	// the skin, outfit and hair follow this mesh's pose, so freezing it freezes her
	Char->GetMesh()->bPauseAnims = bPaused;
	RefreshHud();
}

void AVisexPhotoMode::TakeShot()
{
	if (bOpen)
	{
		UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("HighResShot 2"));
	}
}

void AVisexPhotoMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bOpen || !InputComponent)
	{
		return;
	}
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	auto Axis = [this](const FKey& K) { return InputComponent->GetAxisKeyValue(K); };
	auto Key = [PC](const FKey& K) { return PC && PC->IsInputKeyDown(K) ? 1.f : 0.f; };
	auto Dead = [](float V) { return FMath::Abs(V) < 0.25f ? 0.f : V; };

	const float Orbit = Dead(Axis(EKeys::Gamepad_RightX)) + Key(EKeys::Right) - Key(EKeys::Left);
	const float Lift = Dead(Axis(EKeys::Gamepad_LeftY)) + Key(EKeys::W) - Key(EKeys::S);
	const float Turn = Dead(Axis(EKeys::Gamepad_LeftX)) + Key(EKeys::D) - Key(EKeys::A);
	const float Zoom = Axis(EKeys::Gamepad_RightTriggerAxis) - Axis(EKeys::Gamepad_LeftTriggerAxis) + Key(EKeys::Up) - Key(EKeys::Down);

	Yaw += Orbit * OrbitSpeed * DeltaSeconds + Axis(EKeys::MouseX) * 0.4f;
	Height = FMath::Clamp(Height + Lift * HeightSpeed * DeltaSeconds, MinHeight, MaxHeight);
	Distance = FMath::Clamp(Distance - Zoom * ZoomSpeed * DeltaSeconds - Axis(EKeys::MouseWheelAxis) * 25.f, MinDistance, MaxDistance);
	if (Turn != 0.f)
	{
		if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			Pawn->AddActorWorldRotation(FRotator(0.f, Turn * TurnSpeed * DeltaSeconds, 0.f));
		}
	}
	PlaceCamera();
}

void AVisexPhotoMode::PlaceCamera()
{
	const ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Char || !Camera)
	{
		return;
	}
	const FVector Feet = Char->GetActorLocation() - FVector(0.f, 0.f, Char->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const float A = FMath::DegreesToRadians(Yaw);
	FVector Loc = Feet + FVector(FMath::Cos(A) * Distance, FMath::Sin(A) * Distance, Height);
	// watch her hips (so floor poses stay framed), eased so the camera doesn't jitter with her;
	// the eye follows the camera down a little, so low shots still hold her whole
	const FVector Hips = Char->GetMesh() ? Char->GetMesh()->GetSocketLocation(TEXT("pelvis")) : Feet + FVector(0.f, 0.f, LookHeight);
	// a steady camera: walking and breathing move the hips a few cm, which it ignores; it only
	// re-aims when she really changes height (sitting or lying down, getting up)
	const float Target = FMath::Clamp(Hips.Z - Feet.Z, 20.f, LookHeight);
	if (LookGoal < 0.f || FMath::Abs(Target - LookGoal) > 25.f)
	{
		LookGoal = Target;
	}
	LookZ = LookZ < 0.f ? LookGoal : FMath::FInterpTo(LookZ, LookGoal, GetWorld()->GetDeltaSeconds(), 1.5f);
	const FVector Look = Feet + FVector(0.f, 0.f, FMath::Lerp(LookZ * 0.75f, LookZ, FMath::Clamp(Height / 150.f, 0.f, 1.f)));
	// keep the lens out of stone and sand: come in to whatever is between her and the camera
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VisexPhotoCamera), false, Char);
	if (GetWorld()->SweepSingleByChannel(Hit, Look, Loc, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(12.f), Params))
	{
		Loc = Hit.Location;
	}
	Camera->SetActorLocationAndRotation(Loc, (Look - Loc).Rotation());
	if (UCineCameraComponent* Lens = Camera->GetCineCameraComponent())
	{
		Lens->FocusSettings.ManualFocusDistance = FVector::Dist(Loc, Look);
	}
}

void AVisexPhotoMode::ShowHud(bool bShow)
{
	UGameViewportClient* Viewport = GEngine ? GEngine->GameViewport : nullptr;
	if (!Viewport)
	{
		return;
	}
	if (Hud.IsValid())
	{
		Viewport->RemoveViewportWidgetContent(Hud.ToSharedRef());
		Hud.Reset();
	}
	if (!bShow)
	{
		return;
	}
	FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle(TEXT("Light"), 34);
	TitleFont.LetterSpacing = 260;
	FSlateFontInfo SmallFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 11);
	SmallFont.LetterSpacing = 160;
	const FText Help = FText::FromString(TEXT(
		"L1  R1   MOTION      RIGHT STICK   ORBIT      LEFT STICK   HEIGHT · TURN HER\n"
		"TRIGGERS   DISTANCE      CROSS   FREEZE      SQUARE   SHOT      CIRCLE   BACK"));
	Hud = SNew(SBox)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(90.f, 0.f, 0.f, 70.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("PHOTO MODE"))).Font(SmallFont)
				.ColorAndOpacity(FLinearColor(1.f, 0.82f, 0.6f, 0.9f))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 18.f)
			[
				SAssignNew(Title, STextBlock).Font(TitleFont).ColorAndOpacity(FLinearColor::White)
				.ShadowOffset(FVector2D(1.f, 1.f)).ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.35f))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(Help).Font(SmallFont).LineHeightPercentage(1.6f)
				.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.7f))
			]
		];
	Viewport->AddViewportWidgetContent(Hud.ToSharedRef(), 20);
	RefreshHud();
}

void AVisexPhotoMode::RefreshHud()
{
	if (Title.IsValid() && Motions.IsValidIndex(Motion))
	{
		FString Text = Motions[Motion].Name.ToUpper();
		if (bPaused)
		{
			Text += TEXT("   ·   FROZEN");
		}
		Title->SetText(FText::FromString(Text));
	}
}

void AVisexPhotoMode::EndPlay(const EEndPlayReason::Type Reason)
{
	ShowHud(false);
	Super::EndPlay(Reason);
}
