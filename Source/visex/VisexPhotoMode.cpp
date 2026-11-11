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
#include "GameFramework/CharacterMovementComponent.h"
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
	auto Seq = [](const TCHAR* Name) { return TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(FString::Printf(TEXT("/Game/Characters/PhotoSeq/%s.%s"), Name, Name))); };
	SitLoop = Seq(TEXT("A_PhotoSeq_Sit"));
	SitDownClip = Seq(TEXT("A_PhotoSeq_SitDown"));
	StandUpClip = Seq(TEXT("A_PhotoSeq_StandUp"));
	SeqWalk = Seq(TEXT("A_PhotoSeq_Walk"));
	SeqJog = Seq(TEXT("A_PhotoSeq_Jog"));
	SeqRun = Seq(TEXT("A_PhotoSeq_Run"));
}

UAnimSequence* AVisexPhotoMode::Clip(const TSoftObjectPtr<UAnimSequence>& Soft) const
{
	// only ever handed out while LoadedLoops holds it (BuildMotions)
	UAnimSequence* A = Soft.Get();
	return A && LoadedLoops.Contains(A) ? A : nullptr;
}

void AVisexPhotoMode::BuildMotions()
{
	Motions.Reset();
	LoadedLoops.Reset();
	for (const TSoftObjectPtr<UAnimSequence>* S : {&SitLoop, &SitDownClip, &StandUpClip, &SeqWalk, &SeqJog, &SeqRun})
	{
		if (UAnimSequence* A = S->LoadSynchronous()) { LoadedLoops.Add(A); }
	}
	// the sequence: sit by the column, get up and stroll, walk, jog, run, jump, the poses, go back
	const bool bSeq = Clip(SitLoop) && Clip(SitDownClip) && Clip(StandUpClip) && Clip(SeqWalk) && Clip(SeqJog) && Clip(SeqRun);
	if (bSeq)
	{
		Motions.Add({TEXT("Sit"), nullptr, INDEX_NONE, -1.f, EKind::Sit});
		Motions.Add({TEXT("Slow walk"), Clip(SeqWalk), INDEX_NONE, 0.f, EKind::Move, 0.5f});
		Motions.Add({TEXT("Walk"), Clip(SeqWalk), INDEX_NONE, 0.f, EKind::Move, 1.f});
		Motions.Add({TEXT("Run"), Clip(SeqJog), INDEX_NONE, 0.f, EKind::Move, 1.f});
		Motions.Add({TEXT("Dash"), Clip(SeqRun), INDEX_NONE, 0.f, EKind::Move, 1.f});
		Motions.Add({TEXT("Jump"), Clip(SeqJog), INDEX_NONE, 0.f, EKind::Jump, 1.f});
	}
	else
	{
		Motions.Add({TEXT("Idle"), nullptr, INDEX_NONE});
		if (UAnimSequence* Walk = WalkLoop.LoadSynchronous()) { LoadedLoops.Add(Walk); Motions.Add({TEXT("Walk"), Walk, INDEX_NONE, 1.f}); }
		if (UAnimSequence* Run = RunLoop.LoadSynchronous()) { LoadedLoops.Add(Run); Motions.Add({TEXT("Run"), Run, INDEX_NONE, 0.35f}); }
	}

	TArray<FAssetData> Found;
	FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().GetAssetsByPath(PoseFolder, Found, true);
	Found.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.LexicalLess(B.AssetName); });
	int32 PoseIndex = 0;
	for (const FAssetData& Asset : Found)
	{
		FString Name = Asset.AssetName.ToString();
		// the same list as the pose wheel (VisexPoseWheel.cpp), so the indices match: flows between poses are not poses
		if (Name.EndsWith(TEXT("_In")) || Name.EndsWith(TEXT("_Out")) || Name.Contains(TEXT("_To_")) || !Asset.IsInstanceOf(UAnimSequence::StaticClass()))
		{
			continue;
		}
		Name.RemoveFromStart(TEXT("A_Pose_"));
		Motions.Add({Name.Replace(TEXT("_"), TEXT(" ")), nullptr, PoseIndex++});
	}
	if (bSeq)
	{
		Motions.Add({TEXT("Return"), Clip(SeqWalk), INDEX_NONE, 0.f, EKind::Return, 1.f});
	}
	for (FMotion& M : Motions)
	{
		if (M.Pose != INDEX_NONE) { M.Kind = EKind::Pose; }
	}
}

void AVisexPhotoMode::FindColumn()
{
	ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Char)
	{
		return;
	}
	// the menu's fallen drum (AVisexMenuDirector::SpawnLeanColumn, tagged LeanColumn): pivot on
	// the top of the stone at the middle of its length, X along the drum
	TArray<AActor*> Tagged;
	UGameplayStatics::GetAllActorsWithTag(this, TEXT("LeanColumn"), Tagged);
	AActor* Best = nullptr;
	for (AActor* A : Tagged)
	{
		if (!Best || FVector::DistSquared(A->GetActorLocation(), Char->GetActorLocation()) < FVector::DistSquared(Best->GetActorLocation(), Char->GetActorLocation()))
		{
			Best = A;
		}
	}
	Column = Best;
	const FVector Loc = Char->GetActorLocation();
	if (!Best)
	{
		// no column: she sits where she stands, facing the way she faces
		SitSpot = Loc;
		Away = Char->GetActorForwardVector().GetSafeNormal2D();
		return;
	}
	const FVector Along = Best->GetActorForwardVector().GetSafeNormal2D();
	const FVector Mid = Best->GetActorLocation();
	FVector Out = FVector::CrossProduct(FVector::UpVector, Along).GetSafeNormal2D();
	if (FVector::DotProduct(Loc - Mid, Out) < 0.f)
	{
		Out = -Out;
	}
	Away = Out;
	// beside the middle, where the menu sat her; her own height (the sand under her)
	const float Along2 = FMath::Clamp(FVector::DotProduct(Loc - Mid, Along), -80.f, 80.f);
	SitSpot = Mid + Along * Along2 + Out * SitGap;
	SitSpot.Z = Loc.Z;
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
	FindColumn();
	Phase = EPhase::None;
	if (Motions.Num() > 0 && Motions[0].Kind == EKind::Sit)
	{
		// she sits with her back to the stone: the camera comes round to face her, a little low
		Yaw = FMath::RadiansToDegrees(FMath::Atan2(Away.Y, Away.X)) + 18.f;
		Height = 95.f;
	}
	bFree = false;
	Roll = 0.f;
	Focal = FocalLength;
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
		const FKey FreeKeys[] = {EKeys::Gamepad_FaceButton_Top, EKeys::Tab};
		for (const FKey& K : FreeKeys) { InputComponent->BindKey(K, IE_Pressed, this, &AVisexPhotoMode::ToggleFree); }
		InputComponent->BindKey(EKeys::Gamepad_LeftThumbstick, IE_Pressed, this, &AVisexPhotoMode::LevelRoll);
		InputComponent->BindAxisKey(EKeys::Gamepad_RightY);
		InputComponent->BindAxisKey(EKeys::MouseY);
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
	if (SavedAirBrake >= 0.f)
	{
		Char->GetCharacterMovement()->BrakingDecelerationFalling = SavedAirBrake;
		SavedAirBrake = -1.f;
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
		Phase = EPhase::None;
		if (UVisexAnimInstance* Visex = Cast<UVisexAnimInstance>(Anim))
		{
			Visex->PhotoGait = -1.f;
		}
	}
}

void AVisexPhotoMode::SetView(float InYaw, float InDistance, float InHeight)
{
	Yaw = InYaw;
	Distance = FMath::Clamp(InDistance, MinDistance, MaxDistance);
	Height = FMath::Clamp(InHeight, MinHeight, MaxHeight);
	PlaceCamera();
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
	UAnimSequence* Loop = M.Loop.Get();
	// her catwalk layer follows speed; standing in place it needs telling how hard she walks
	if (UVisexAnimInstance* Visex = Cast<UVisexAnimInstance>(Anim))
	{
		Visex->PhotoGait = Loop ? M.Gait : -1.f;
	}
	if (M.Kind == EKind::Sit || M.Kind == EKind::Move || M.Kind == EKind::Jump || M.Kind == EKind::Return)
	{
		if (PlayingPose != INDEX_NONE)
		{
			if (UVisexPoseWheelComponent* Wheel = Char ? Char->FindComponentByClass<UVisexPoseWheelComponent>() : nullptr)
			{
				Wheel->StopPose();
			}
			PlayingPose = INDEX_NONE;
		}
		if (UVisexAnimInstance* Visex = Cast<UVisexAnimInstance>(Anim))
		{
			Visex->PhotoGait = M.Kind == EKind::Sit ? -1.f : M.Gait;   // the mocap walks as it was caught
		}
		const bool bSeated = Phase == EPhase::SitDown || Phase == EPhase::Seated;
		if (M.Kind == EKind::Sit)
		{
			if (!bSeated && Char)
			{
				// from wherever she is, she is put by the stone and sits (the photo camera blends in meanwhile)
				Char->GetCharacterMovement()->StopMovementImmediately();
				Char->SetActorLocationAndRotation(SitSpot, Away.Rotation(), false, nullptr, ETeleportType::TeleportPhysics);
				PlayClip(Clip(SitDownClip), 1.f, false, false, 0.4f);
				Phase = EPhase::SitDown;
				PhaseTime = 0.f;
			}
		}
		else if (M.Kind == EKind::Return)
		{
			PlayClip(Clip(SeqWalk), 1.f, true, true);
			Phase = EPhase::Approach;
			PhaseTime = 0.f;
		}
		else if (bSeated || Phase == EPhase::StandUp)
		{
			// get up first; TickSequence starts the stroll when she is on her feet
			if (Phase != EPhase::StandUp)
			{
				PlayClip(Clip(StandUpClip), 1.f, false, false, 0.3f);
				Phase = EPhase::StandUp;
				PhaseTime = 0.f;
			}
		}
		else
		{
			StartMoving(M);
		}
		RefreshHud();
		return;
	}
	Phase = EPhase::None;
	if (M.Pose != INDEX_NONE)
	{
		// poses go through the wheel: it handles sitting down, the slope and getting up
		// stop only a walk/run loop: the slot is shared with the wheel, and stopping a pose under
		// it makes the wheel think something took the slot, so it drops the next pose
		if (Anim && PlayingPose == INDEX_NONE)
		{
			Anim->StopSlotAnimation(0.2f, Slot);
		}
		if (Anim)
		{
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
			Anim->SetRootMotionMode(Loop ? ERootMotionMode::IgnoreRootMotion : SavedRootMotion.GetValue());
			if (Loop)
			{
				Anim->PlaySlotAnimationAsDynamicMontage(Loop, Slot, 0.35f, 0.35f, 1.f, 1000000);
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
	if (!bPaused)
	{
		TickSequence(DeltaSeconds);
	}
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	auto Axis = [this](const FKey& K) { return InputComponent->GetAxisKeyValue(K); };
	auto Key = [PC](const FKey& K) { return PC && PC->IsInputKeyDown(K) ? 1.f : 0.f; };
	auto Dead = [](float V) { return FMath::Abs(V) < 0.25f ? 0.f : V; };

	// lens and tilt, in both modes: held, they change smoothly
	const float LensIn = Key(EKeys::Gamepad_DPad_Up) + Key(EKeys::Z) - Key(EKeys::Gamepad_DPad_Down) - Key(EKeys::X);
	const float Tilt = Key(EKeys::Gamepad_DPad_Right) + Key(EKeys::V) - Key(EKeys::Gamepad_DPad_Left) - Key(EKeys::C);
	Focal = FMath::Clamp(Focal * FMath::Exp(LensIn * 0.9f * DeltaSeconds), MinFocal, MaxFocal);
	Roll = FMath::Clamp(Roll + Tilt * RollSpeed * DeltaSeconds, -MaxRoll, MaxRoll);

	if (bFree)
	{
		const float MoveY = Dead(Axis(EKeys::Gamepad_LeftY)) + Key(EKeys::W) - Key(EKeys::S);
		const float MoveX = Dead(Axis(EKeys::Gamepad_LeftX)) + Key(EKeys::D) - Key(EKeys::A);
		const float Rise = Axis(EKeys::Gamepad_RightTriggerAxis) - Axis(EKeys::Gamepad_LeftTriggerAxis) + Key(EKeys::R) - Key(EKeys::F);
		const float AimX = Dead(Axis(EKeys::Gamepad_RightX)) + Key(EKeys::Right) - Key(EKeys::Left);
		const float AimY = Dead(Axis(EKeys::Gamepad_RightY)) + Key(EKeys::Up) - Key(EKeys::Down);
		FlyCamera(DeltaSeconds, MoveX, MoveY, Rise, AimX, AimY);
		// the mouse aims directly (degrees per count), not as a rate
		FreeAim.Yaw += Axis(EKeys::MouseX) * 0.25f;
		FreeAim.Pitch = FMath::Clamp(FreeAim.Pitch + Axis(EKeys::MouseY) * 0.25f, -89.f, 89.f);
		ApplyLens(FreeLoc, FreeAim);
		return;
	}

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

void AVisexPhotoMode::ToggleFree()
{
	if (!bOpen || !Camera)
	{
		return;
	}
	bFree = !bFree;
	if (bFree)
	{
		// take over from wherever the orbit camera is, looking where it looked
		FreeLoc = Camera->GetActorLocation();
		FreeAim = Camera->GetActorRotation();
		FreeAim.Roll = 0.f;
	}
	else if (const ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		// back to orbiting: from where the free camera now stands, round her
		const FVector Feet = Char->GetActorLocation() - FVector(0.f, 0.f, Char->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		const FVector Rel = FreeLoc - Feet;
		Yaw = FMath::RadiansToDegrees(FMath::Atan2(Rel.Y, Rel.X));
		Distance = FMath::Clamp(FVector2D(Rel.X, Rel.Y).Size(), MinDistance, MaxDistance);
		Height = FMath::Clamp(Rel.Z, MinHeight, MaxHeight);
	}
	RefreshHud();
}

void AVisexPhotoMode::SetFreeView(FVector Location, FRotator Aim, float InRoll, float InFocal)
{
	if (!bOpen)
	{
		return;
	}
	bFree = true;
	FreeLoc = Location;
	FreeAim = FRotator(FMath::Clamp(Aim.Pitch, -89.f, 89.f), Aim.Yaw, 0.f);
	Roll = FMath::Clamp(InRoll, -MaxRoll, MaxRoll);
	Focal = FMath::Clamp(InFocal, MinFocal, MaxFocal);
	ApplyLens(FreeLoc, FreeAim);
	RefreshHud();
}

void AVisexPhotoMode::FlyCamera(float DeltaSeconds, float MoveX, float MoveY, float Rise, float AimX, float AimY)
{
	FreeAim.Yaw += AimX * LookSpeed * DeltaSeconds;
	FreeAim.Pitch = FMath::Clamp(FreeAim.Pitch + AimY * LookSpeed * DeltaSeconds, -89.f, 89.f);

	// slow and precise close to her, quicker across the scene
	float Speed = FlySpeedFar;
	if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		const float Near = FVector::Dist(FreeLoc, Pawn->GetActorLocation());
		Speed = FMath::GetMappedRangeValueClamped(FVector2D(100.f, 900.f), FVector2D(FlySpeedNear, FlySpeedFar), Near);
	}
	const FRotator Flat(0.f, FreeAim.Yaw, 0.f);
	const FVector Move = (FreeAim.Vector() * MoveY + FRotationMatrix(Flat).GetUnitAxis(EAxis::Y) * MoveX) * Speed
		+ FVector::UpVector * Rise * Speed * 0.8f;
	if (Move.IsNearlyZero())
	{
		return;
	}
	// the lens slides along stone and sand instead of going into them
	FVector To = FreeLoc + Move * DeltaSeconds;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VisexFreeCamera), false, UGameplayStatics::GetPlayerPawn(this, 0));
	const FCollisionShape Ball = FCollisionShape::MakeSphere(10.f);
	if (GetWorld()->SweepSingleByChannel(Hit, FreeLoc, To, FQuat::Identity, ECC_Camera, Ball, Params))
	{
		const FVector Slide = FVector::VectorPlaneProject(To - Hit.Location, Hit.ImpactNormal);
		To = Hit.Location + Hit.ImpactNormal * 0.5f;
		FHitResult SlideHit;
		if (!GetWorld()->SweepSingleByChannel(SlideHit, To, To + Slide, FQuat::Identity, ECC_Camera, Ball, Params))
		{
			To += Slide;
		}
	}
	FreeLoc = To;
}

void AVisexPhotoMode::ApplyLens(const FVector& Loc, const FRotator& Aim)
{
	if (!Camera)
	{
		return;
	}
	Camera->SetActorLocationAndRotation(Loc, FRotator(Aim.Pitch, Aim.Yaw, Roll));
	UCineCameraComponent* Lens = Camera->GetCineCameraComponent();
	const ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	const USkeletalMeshComponent* Mesh = Char ? Char->GetMesh() : nullptr;
	if (!Lens || !Mesh)
	{
		return;
	}
	Lens->SetCurrentFocalLength(Focal);
	// focus on the part of her the camera points at: of a few bones, the nearest to the aim line
	const FVector Forward = Aim.Vector();
	static const FName Bones[] = {TEXT("head"), TEXT("spine_03"), TEXT("pelvis"), TEXT("hand_l"), TEXT("hand_r"),
		TEXT("calf_l"), TEXT("calf_r"), TEXT("foot_l"), TEXT("foot_r")};
	float Best = -1.f;
	float Focus = Lens->FocusSettings.ManualFocusDistance;
	for (const FName& Bone : Bones)
	{
		if (Mesh->GetBoneIndex(Bone) == INDEX_NONE) { continue; }
		const FVector To = Mesh->GetSocketLocation(Bone) - Loc;
		const float Along = FVector::DotProduct(To, Forward);
		if (Along <= 1.f) { continue; }
		const float Cos = Along / To.Size();
		if (Cos > Best) { Best = Cos; Focus = Along; }
	}
	Lens->FocusSettings.ManualFocusDistance = FMath::FInterpTo(Lens->FocusSettings.ManualFocusDistance, Focus, GetWorld()->GetDeltaSeconds(), 8.f);
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
	// (never closer than the near limit, so the lens can't end up inside her)
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VisexPhotoCamera), false, Char);
	if (GetWorld()->SweepSingleByChannel(Hit, Look, Loc, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(12.f), Params))
	{
		const FVector Back = Hit.Location - Look;
		Loc = Look + (Loc - Look).GetSafeNormal() * FMath::Max(Back.Size(), (double)MinDistance);
	}
	const FRotator Aim = (Look - Loc).Rotation();
	Camera->SetActorLocationAndRotation(Loc, FRotator(Aim.Pitch, Aim.Yaw, Roll));
	if (UCineCameraComponent* Lens = Camera->GetCineCameraComponent())
	{
		Lens->SetCurrentFocalLength(Focal);
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
				SAssignNew(Help, STextBlock).Font(SmallFont).LineHeightPercentage(1.6f)
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
	if (Help.IsValid())
	{
		Help->SetText(FText::FromString(bFree
			? TEXT("FREE CAMERA      LEFT STICK   FLY      RIGHT STICK   AIM      TRIGGERS   RISE · SINK      TRIANGLE   ORBIT\n"
				   "D-PAD ↕   LENS      D-PAD ↔   TILT      L3   LEVEL      L1  R1   MOTION      CROSS   FREEZE      SQUARE   SHOT      CIRCLE   BACK")
			: TEXT("L1  R1   MOTION      RIGHT STICK   ORBIT      LEFT STICK   HEIGHT · TURN HER      TRIANGLE   FREE CAMERA\n"
				   "TRIGGERS   DISTANCE      D-PAD   LENS · TILT      CROSS   FREEZE      SQUARE   SHOT      CIRCLE   BACK")));
	}
}

void AVisexPhotoMode::EndPlay(const EEndPlayReason::Type Reason)
{
	ShowHud(false);
	Super::EndPlay(Reason);
}

void AVisexPhotoMode::PlayClip(UAnimSequence* InClip, float Rate, bool bLoop, bool bRootMotion, float Blend)
{
	ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	UAnimInstance* Anim = Char && Char->GetMesh() ? Char->GetMesh()->GetAnimInstance() : nullptr;
	if (!Anim || !InClip)
	{
		return;
	}
	// root motion carries her over the sand at the clip's own pace, so the feet never slide;
	// the sit clips are in place
	Anim->SetRootMotionMode(bRootMotion ? ERootMotionMode::RootMotionFromMontagesOnly : ERootMotionMode::IgnoreRootMotion);
	Anim->PlaySlotAnimationAsDynamicMontage(InClip, Slot, Blend, bLoop ? Blend : 0.25f, Rate, bLoop ? 1000000 : 1);
}

void AVisexPhotoMode::StartMoving(const FMotion& M)
{
	PlayClip(M.Loop.Get(), M.Rate, true, true, 0.5f);
	Phase = EPhase::Moving;
	PhaseTime = 0.f;
	JumpClock = 0.f;
}

void AVisexPhotoMode::SteerTo(const FVector& Dir, float DegPerSec, float DeltaSeconds)
{
	ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	const FVector D = Dir.GetSafeNormal2D();
	if (!Char || D.IsNearlyZero())
	{
		return;
	}
	FRotator R = Char->GetActorRotation();
	const float Delta = FMath::FindDeltaAngleDegrees(R.Yaw, D.Rotation().Yaw);
	R.Yaw += FMath::Clamp(Delta, -DegPerSec * DeltaSeconds, DegPerSec * DeltaSeconds);
	Char->SetActorRotation(R);
}

void AVisexPhotoMode::TickSequence(float DeltaSeconds)
{
	ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Char)
	{
		return;
	}
	if (Phase != EPhase::Air && SavedAirBrake >= 0.f && !Char->GetCharacterMovement()->IsFalling())
	{
		Char->GetCharacterMovement()->BrakingDecelerationFalling = SavedAirBrake;
		SavedAirBrake = -1.f;
	}
	if (Phase == EPhase::None || !Motions.IsValidIndex(Motion))
	{
		return;
	}
	// the menu hides the scene for photo mode, her column too; she needs that one back (seen only)
	if (Column.IsValid() && Column->IsHidden())
	{
		Column->SetActorHiddenInGame(false);
	}
	PhaseTime += DeltaSeconds;
	auto Ends = [this](const TSoftObjectPtr<UAnimSequence>& S) { const UAnimSequence* A = Clip(S); return !A || PhaseTime >= A->GetPlayLength() - 0.3f; };
	const FMotion& M = Motions[Motion];
	const FVector Loc = Char->GetActorLocation();
	// the menu's hand-over (its stand-up off the lean) can take the slot after Open: sit again
	UAnimInstance* Anim = Char->GetMesh() ? Char->GetMesh()->GetAnimInstance() : nullptr;
	if (Anim && Phase == EPhase::SitDown && PhaseTime < 1.5f && !Anim->IsPlayingSlotAnimation(Clip(SitDownClip), Slot))
	{
		PlayClip(Clip(SitDownClip), 1.f, false, false, 0.3f);
		PhaseTime = 0.f;
	}
	else if (Anim && Phase == EPhase::Seated && !Anim->IsPlayingSlotAnimation(Clip(SitLoop), Slot))
	{
		PlayClip(Clip(SitLoop), 1.f, true, false, 0.3f);
	}
	const FVector Along = FVector::CrossProduct(FVector::UpVector, Away);
	const FVector Mid = Column.IsValid() ? Column->GetActorLocation() : SitSpot - Away * SitGap;
	switch (Phase)
	{
	case EPhase::SitDown:
		if (Ends(SitDownClip))
		{
			PlayClip(Clip(SitLoop), 1.f, true, false, 0.3f);
			Phase = EPhase::Seated;
		}
		break;
	case EPhase::StandUp:
		if (Ends(StandUpClip))
		{
			if (M.Kind == EKind::Move || M.Kind == EKind::Jump) { StartMoving(M); } else { Phase = EPhase::None; }
		}
		break;
	case EPhase::Moving:
	{
		// free on the plaza; past the leash she curves round the stone instead of leaving
		FVector ToHome = SitSpot - Loc;
		ToHome.Z = 0.f;
		const float D = ToHome.Size();
		if (D > Leash)
		{
			FVector Tan = FVector::CrossProduct(FVector::UpVector, ToHome.GetSafeNormal());
			if (FVector::DotProduct(Tan, Char->GetActorForwardVector()) < 0.f) { Tan = -Tan; }
			const float Pull = FMath::Clamp((D - Leash) / 300.f, 0.f, 2.f);
			SteerTo(Tan + ToHome.GetSafeNormal() * Pull, 30.f + 15.f * Pull, DeltaSeconds);
		}
		JumpClock += DeltaSeconds;
		UCharacterMovementComponent* Move = Char->GetCharacterMovement();
		if (M.Kind == EKind::Jump && JumpClock > JumpEvery && Move && !Move->IsFalling())
		{
			// a running leap: the slot lets go so her own jump and fall poses play under it
			// root motion off first: a montage blending out would still steer her, and zero her speed
			if (Anim)
			{
				Anim->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
				Anim->StopSlotAnimation(0.2f, Slot);
			}
			const float Speed = Char->GetVelocity().Size2D() > 50.f ? Char->GetVelocity().Size2D() : 220.f;
			// she brakes hard in the air (her movement's 1500): a leap carries its speed to the ground
			if (Move && SavedAirBrake < 0.f)
			{
				SavedAirBrake = Move->BrakingDecelerationFalling;
				Move->BrakingDecelerationFalling = 0.f;
			}
			Char->LaunchCharacter(Char->GetActorForwardVector() * Speed + FVector(0.f, 0.f, JumpSpeed), true, true);
			Phase = EPhase::Air;
			PhaseTime = 0.f;
		}
		break;
	}
	case EPhase::Air:
		if (PhaseTime > 0.2f && Char->GetCharacterMovement() && !Char->GetCharacterMovement()->IsFalling())
		{
			StartMoving(M);   // lands into the jog again; the clock waits for the next leap
		}
		break;
	case EPhase::Approach:
	{
		// back to the stone: round its end if she is behind it, then in from the front
		const FVector Rel = Loc - Mid;
		const float A = FVector::DotProduct(Rel, Away);
		const float L = FVector::DotProduct(Rel, Along);
		const float LSit = FVector::DotProduct(SitSpot - Mid, Along);
		const FVector Front = SitSpot + Away * 70.f;
		FVector Target = Front;
		if (A < 90.f && FMath::Abs(L) < 260.f)
		{
			Target = Mid + Along * FMath::Sign(L == 0.f ? 1.f : L) * 280.f + Away * 220.f;
		}
		else if (A < 160.f && FMath::Abs(L - LSit) > 80.f)
		{
			Target = SitSpot + Away * 240.f;
		}
		FVector To = Target - Loc;
		To.Z = 0.f;
		SteerTo(To, To.Size() < 200.f ? 120.f : 60.f, DeltaSeconds);
		if (Target.Equals(Front) && To.Size() < 50.f)
		{
			// she stops, turns her back to the stone and sits
			if (Anim)
			{
				Anim->StopSlotAnimation(0.5f, Slot);
				Anim->SetRootMotionMode(SavedRootMotion);
			}
			Phase = EPhase::Turn;
			PhaseTime = 0.f;
		}
		break;
	}
	case EPhase::Turn:
	{
		const FRotator R = FMath::RInterpTo(Char->GetActorRotation(), Away.Rotation(), DeltaSeconds, 3.5f);
		FVector P = FMath::VInterpTo(Loc, SitSpot, DeltaSeconds, 2.5f);
		P.Z = Loc.Z;
		Char->SetActorLocationAndRotation(P, FRotator(0.f, R.Yaw, 0.f));
		if (PhaseTime > 1.2f)
		{
			Char->SetActorLocationAndRotation(FVector(SitSpot.X, SitSpot.Y, Loc.Z), Away.Rotation());
			PlayClip(Clip(SitDownClip), 1.f, false, false, 0.4f);
			Phase = EPhase::SitDown;
			PhaseTime = 0.f;
			Motion = 0;   // the loop closes: she is on SIT again
			RefreshHud();
		}
		break;
	}
	default:
		break;
	}
}
