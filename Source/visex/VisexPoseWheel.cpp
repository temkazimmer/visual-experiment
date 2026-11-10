#include "VisexPoseWheel.h"
#include "VisexAnimInstance.h"

#include "Animation/AnimClassInterface.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Misc/PackageName.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"

namespace
{
	const TCHAR* PoseFolder = TEXT("/Game/Characters/Poses");
	const TCHAR* WheelActionPath = TEXT("/Game/Input/Actions/IA_PoseWheel.IA_PoseWheel");
	const FName PoseSlot(TEXT("DefaultSlot"));
	const FLinearColor Ink(FColor(0xFF, 0xEB, 0xD1));    // the menu's warm off-white
	const FLinearColor Accent(FColor(0xFF, 0x8C, 0x38)); // the menu's orange

	FVector2D OnRing(float Angle, float Radius) // angle clockwise from the top, screen space
	{
		return FVector2D(FMath::Sin(Angle), -FMath::Cos(Angle)) * Radius;
	}
}

/** The ring itself: no boxes, thin lines and light type over a dimmed frame. */
class SVisexPoseWheel : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SVisexPoseWheel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&, TSharedPtr<FVisexPoseWheelView> InView) { View = InView; }

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1920., 1080.); }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Culling,
		FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		const float Fade = View.IsValid() ? View->Fade : 0.f;
		if (Fade <= 0.001f)
		{
			return Layer;
		}
		const FVector2D Size(Geo.GetLocalSize());
		const FVector2D Mid = Size * 0.5;
		const float K = Size.Y / 1080.f;
		const float Radius = 270.f * K * FMath::Lerp(0.92f, 1.f, Fade);
		const int32 Count = View->Labels.Num();
		const float Step = Count > 0 ? 2.f * PI / Count : 2.f * PI;

		// the frame dims so the ring reads against the dust
		FSlateDrawElement::MakeBox(Out, Layer, Geo.ToPaintGeometry(), FCoreStyle::Get().GetBrush("WhiteBrush"),
			ESlateDrawEffect::None, FLinearColor(0.02f, 0.012f, 0.008f, 0.55f * Fade));

		auto Arc = [&](float From, float To, float R, const FLinearColor& Color, float Thickness)
		{
			TArray<FVector2f> Points;
			const int32 Segments = FMath::Max(2, FMath::CeilToInt((To - From) / (2.f * PI) * 128.f));
			for (int32 i = 0; i <= Segments; ++i)
			{
				Points.Add(FVector2f(Mid + OnRing(FMath::Lerp(From, To, float(i) / Segments), R)));
			}
			FSlateDrawElement::MakeLines(Out, Layer + 1, Geo.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, Thickness);
		};
		auto Line = [&](const FVector2D& A, const FVector2D& B, const FLinearColor& Color, float Thickness)
		{
			TArray<FVector2f> Points{ FVector2f(Mid + A), FVector2f(Mid + B) };
			FSlateDrawElement::MakeLines(Out, Layer + 1, Geo.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, Thickness);
		};
		auto Type = [&](const FString& Text, const FVector2D& Centre, int32 Points, FName Face, int32 Spacing, const FLinearColor& Color)
		{
			FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(Face, FMath::Max(6, FMath::RoundToInt(Points * K)));
			Font.LetterSpacing = Spacing;
			const FVector2D Box(FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font));
			FSlateDrawElement::MakeText(Out, Layer + 2,
				Geo.ToPaintGeometry(Box, FSlateLayoutTransform(Mid + Centre - Box * 0.5)), Text, Font, ESlateDrawEffect::None, Color);
		};

		Arc(0.f, 2.f * PI, Radius, Ink.CopyWithNewOpacity(0.28f * Fade), 1.f);
		for (int32 i = 0; i < Count; ++i)
		{
			const float At = i * Step;
			const bool bOn = i == View->Selected;
			if (Count > 1) // a short tick where two poses meet
			{
				Line(OnRing(At + Step * 0.5f, Radius - 7.f * K), OnRing(At + Step * 0.5f, Radius + 7.f * K), Ink.CopyWithNewOpacity(0.4f * Fade), 1.f);
			}
			if (bOn)
			{
				const float Half = FMath::Min(Step * 0.5f - 0.06f, 0.5f);
				Arc(At - Half, At + Half, Radius, Accent.CopyWithNewOpacity(Fade), 2.5f);
			}
			Type(View->Labels[i].ToUpper(), OnRing(At, Radius + 58.f * K), bOn ? 19 : 16, TEXT("Light"), 300,
				Ink.CopyWithNewOpacity((bOn ? 1.f : 0.5f) * Fade));
		}

		// where the stick or mouse points
		const float Reach = View->Pointer.Size();
		if (Reach > 0.05f)
		{
			const FVector2D Dir(View->Pointer.X / Reach, -View->Pointer.Y / Reach);
			Line(Dir * (Radius - 34.f * K), Dir * (Radius - 12.f * K), Accent.CopyWithNewOpacity(FMath::Min(Reach * 2.f, 1.f) * Fade), 1.5f);
		}

		// the centre names what a release will do
		const bool bAny = View->Labels.IsValidIndex(View->Selected);
		Type(bAny ? View->Labels[View->Selected].ToUpper() : FString(TEXT("POSE")), FVector2D(0., -10. * K), 40, TEXT("Light"), 260,
			Ink.CopyWithNewOpacity((bAny ? 1.f : 0.45f) * Fade));
		Type(bAny ? TEXT("RELEASE") : TEXT("POINT  TO  CHOOSE"), FVector2D(0., 34. * K), 9, TEXT("Regular"), 600,
			Accent.CopyWithNewOpacity(Fade));
		return Layer + 2;
	}

private:
	TSharedPtr<FVisexPoseWheelView> View;
};

UVisexPoseWheelComponent::UVisexPoseWheelComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UVisexPoseWheelComponent::BeginPlay()
{
	Super::BeginPlay();

	WheelAction = LoadObject<UInputAction>(nullptr, WheelActionPath);
	if (!WheelAction)
	{
		UE_LOG(LogTemp, Warning, TEXT("Pose wheel: %s is missing, run Tools/setup_input.py"), WheelActionPath);
	}

	TArray<FAssetData> Found;
	FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().GetAssetsByPath(PoseFolder, Found, true);
	Found.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.LexicalLess(B.AssetName); });
	View = MakeShared<FVisexPoseWheelView>();
	TArray<FString> Packages; // each pose's package, to find the flows between them
	for (const FAssetData& Asset : Found)
	{
		const FString Name = Asset.AssetName.ToString();
		if (Name.EndsWith(TEXT("_In")) || Name.EndsWith(TEXT("_Out")) || Name.Contains(TEXT("_To_")))
		{
			continue; // the way into, out of and between poses, found through the poses themselves
		}
		if (UAnimSequence* Pose = Cast<UAnimSequence>(Asset.GetAsset()))
		{
			FString Label = Name;
			Label.RemoveFromStart(TEXT("A_Pose_"));
			const FString Path = Asset.PackageName.ToString();
			Poses.Add(Pose);
			Packages.Add(Path);
			PoseIns.Add(LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("%s_In.%s_In"), *Path, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet));
			PoseOuts.Add(LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("%s_Out.%s_Out"), *Path, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet));
			View->Labels.Add(Label.Replace(TEXT("_"), TEXT(" ")));
		}
	}
	View->Labels.Add(TEXT("Stand")); // the last place lets go of the pose

	// already down, she moves from one floor pose to the next without getting up
	const int32 N = Poses.Num();
	Flows.SetNum(N * N);
	for (int32 From = 0; From < N; ++From)
	{
		for (int32 To = 0; To < N; ++To)
		{
			if (From != To && PoseIns[From] && PoseIns[To])
			{
				const FString Name = FString::Printf(TEXT("%s_To_%s"), *FPackageName::GetShortName(Packages[From]),
					*FPackageName::GetShortName(Packages[To]).Replace(TEXT("A_Pose_"), TEXT("")));
				Flows[From * N + To] = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("%s/%s.%s"),
					*FPackageName::GetLongPackagePath(Packages[From]), *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
			}
		}
	}

	const APawn* Pawn = Cast<APawn>(GetOwner());
	UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	if (Viewport && Pawn && Pawn->IsLocallyControlled())
	{
		Widget = SNew(SVisexPoseWheel, View).Visibility(EVisibility::Collapsed);
		Viewport->AddViewportWidgetContent(Widget.ToSharedRef(), 40);
	}
}

void UVisexPoseWheelComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetOpen(false);
	if (Widget.IsValid())
	{
		if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(Widget.ToSharedRef());
		}
		Widget.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void UVisexPoseWheelComponent::BindInput()
{
	UInputComponent* Input = GetOwner() ? GetOwner()->InputComponent.Get() : nullptr;
	if (!WheelAction || Input == BoundInput.Get())
	{
		return;
	}
	BoundInput = Input;
	if (UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(Input))
	{
		Enhanced->BindAction(WheelAction, ETriggerEvent::Started, this, &UVisexPoseWheelComponent::OnWheelPressed);
		Enhanced->BindAction(WheelAction, ETriggerEvent::Completed, this, &UVisexPoseWheelComponent::OnWheelReleased);
		Enhanced->BindAction(WheelAction, ETriggerEvent::Canceled, this, &UVisexPoseWheelComponent::OnWheelReleased);
	}
}

void UVisexPoseWheelComponent::OnWheelPressed(const FInputActionValue&)
{
	SetOpen(true);
}

void UVisexPoseWheelComponent::OnWheelReleased(const FInputActionValue&)
{
	if (!bOpen)
	{
		return;
	}
	const int32 Choice = View->Selected;
	SetOpen(false);
	if (Poses.IsValidIndex(Choice))
	{
		PlayPose(Choice);
	}
	else if (Choice == Poses.Num())
	{
		StopPose();
	}
}

void UVisexPoseWheelComponent::SetOpen(bool bInOpen)
{
	if (bOpen == bInOpen)
	{
		return;
	}
	bOpen = bInOpen;
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr)
	{
		PC->SetIgnoreLookInput(bOpen); // the stick and mouse point at the ring instead
		PC->SetIgnoreMoveInput(bOpen);
	}
	if (bOpen && View.IsValid())
	{
		View->Selected = INDEX_NONE;
		View->Pointer = Cursor = FVector2D::ZeroVector;
	}
}

UAnimInstance* UVisexPoseWheelComponent::GetAnim() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	return Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
}

void UVisexPoseWheelComponent::PlayPose(int32 Index)
{
	if (!Poses.IsValidIndex(Index) || (Index == Current && Phase != EPhase::Exit && PoseMontage))
	{
		return;
	}
	if (Phase != EPhase::None && PoseOuts.IsValidIndex(Current) && PoseOuts[Current])
	{
		Pending = Index; // she gets up first, then strikes the new one
		bExitQueued = true;
		return;
	}
	StartPose(Index);
}

void UVisexPoseWheelComponent::StartPose(int32 Index)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UAnimInstance* Anim = GetAnim();
	if (!Anim || !Poses.IsValidIndex(Index))
	{
		return;
	}
	Current = Index;
	Pending = INDEX_NONE;
	bExitQueued = false;
	if (UAnimSequence* In = PoseIns[Index])
	{
		// sitting down: play once and stay on the last frame; no walking until she is up again
		UCharacterMovementComponent* Move = Character->GetCharacterMovement();
		if (Move && Move->IsMovingOnGround())
		{
			Move->DisableMovement();
			bMovementHeld = true;
		}
		PoseMontage = Anim->PlaySlotAnimationAsDynamicMontage(In, PoseSlot, 0.25f, 0.f, 1.f, 1);
		Phase = EPhase::Enter;
		PhaseTime = 0.f;
		EnterLength = In->GetPlayLength();
		bFlowing = false;
	}
	else
	{
		// a pose is one still frame: loop it and keep it up until something else takes over
		PoseMontage = Anim->PlaySlotAnimationAsDynamicMontage(Poses[Index], PoseSlot, BlendTime, BlendTime, 1.f, 1000000);
		Phase = EPhase::Hold;
	}
	if (PoseMontage)
	{
		PoseMontage->bEnableAutoBlendOut = false;
	}
	else
	{
		FinishPose();
	}
}

UAnimSequence* UVisexPoseWheelComponent::FlowTo(int32 From, int32 To) const
{
	const int32 N = Poses.Num();
	return Poses.IsValidIndex(From) && Poses.IsValidIndex(To) && Flows.IsValidIndex(From * N + To) ? Flows[From * N + To].Get() : nullptr;
}

void UVisexPoseWheelComponent::StartFlow(int32 Index)
{
	UAnimInstance* Anim = GetAnim();
	UAnimSequence* Flow = FlowTo(Current, Index);
	if (!Anim || !Flow)
	{
		return;
	}
	PoseMontage = Anim->PlaySlotAnimationAsDynamicMontage(Flow, PoseSlot, 0.2f, 0.f, 1.f, 1);
	if (PoseMontage)
	{
		PoseMontage->bEnableAutoBlendOut = false;
	}
	Current = Index;
	Pending = INDEX_NONE;
	bExitQueued = false;
	Phase = EPhase::Enter;
	PhaseTime = 0.f;
	EnterLength = Flow->GetPlayLength();
	bFlowing = true;
}

void UVisexPoseWheelComponent::BeginExit()
{
	UAnimInstance* Anim = GetAnim();
	UAnimSequence* Out = PoseOuts.IsValidIndex(Current) ? PoseOuts[Current].Get() : nullptr;
	bExitQueued = false;
	if (!Anim || !Out)
	{
		if (Anim && PoseMontage)
		{
			Anim->Montage_Stop(BlendTime, PoseMontage);
		}
		const int32 Next = Pending;
		FinishPose();
		StartPose(Next);
		return;
	}
	PoseMontage = Anim->PlaySlotAnimationAsDynamicMontage(Out, PoseSlot, 0.15f, BlendTime, 1.f, 1);
	Phase = EPhase::Exit;
}

void UVisexPoseWheelComponent::FinishPose()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* Move = Character ? Character->GetCharacterMovement() : nullptr;
	if (bMovementHeld && Move && Move->MovementMode == MOVE_None)
	{
		Move->SetMovementMode(MOVE_Walking);
	}
	bMovementHeld = false;
	bExitQueued = false;
	PoseMontage = nullptr;
	Phase = EPhase::None;
	Current = Pending = INDEX_NONE;
}

void UVisexPoseWheelComponent::StopPose()
{
	if (Phase == EPhase::None)
	{
		return;
	}
	Pending = INDEX_NONE;
	if (Phase != EPhase::Exit)
	{
		bExitQueued = true; // taken up in Tick once she has finished sitting down
	}
}

void UVisexPoseWheelComponent::UpdateFootIK(float DeltaTime)
{
	// The Anim Blueprint's foot IK (a Control Rig node after the pose slot) plants both feet on
	// the ground. On the floor poses that drags a raised leg down, so it fades out while she
	// is down there and back in once she is up. Standing poses keep it.
	const bool bOnFloor = Phase != EPhase::None && PoseIns.IsValidIndex(Current) && PoseIns[Current];
	FootIKAlpha = FMath::FInterpConstantTo(FootIKAlpha, bOnFloor ? 0.f : 1.f, DeltaTime, 4.f);
	UAnimInstance* Anim = GetAnim();
	IAnimClassInterface* AnimClass = Anim ? IAnimClassInterface::GetFromClass(Anim->GetClass()) : nullptr;
	if (!AnimClass)
	{
		return;
	}
	for (const FStructProperty* Node : AnimClass->GetAnimNodeProperties())
	{
		if (Node->Struct->GetFName() == TEXT("AnimNode_ControlRig"))
		{
			if (const FFloatProperty* Alpha = FindFProperty<FFloatProperty>(Node->Struct, TEXT("Alpha")))
			{
				Alpha->SetPropertyValue_InContainer(Node->ContainerPtrToValuePtr<void>(Anim), FootIKAlpha);
			}
		}
	}
}

void UVisexPoseWheelComponent::UpdateGround(float DeltaTime)
{
	// The floor poses are made on flat ground, but the dunes slope: a body lying across a 10
	// degree slope floats 30 cm off it at one end. While she is down there, the ground under
	// her is sampled, and the body is tilted onto it and lowered to touch it.
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!Mesh || !Capsule)
	{
		return;
	}
	const bool bOnFloor = IsOnFloor();
	if (!bOnFloor && GroundAlpha <= 0.f)
	{
		return;
	}
	if (GroundAlpha <= 0.f)
	{
		MeshBaseLocation = Mesh->GetRelativeLocation();
		MeshBaseRotation = Mesh->GetRelativeRotation().Quaternion();
	}
	// eased in while she kneels and sits, eased out while she gets up
	GroundAlpha = FMath::FInterpConstantTo(GroundAlpha, bOnFloor ? 1.f : 0.f, DeltaTime, bOnFloor ? 0.9f : 0.7f);
	if (GroundAlpha <= 0.f)
	{
		Mesh->SetRelativeLocationAndRotation(MeshBaseLocation, MeshBaseRotation);
		return;
	}

	const FTransform Actor = Character->GetActorTransform();
	const float Bottom = Capsule->GetComponentLocation().Z - Capsule->GetScaledCapsuleHalfHeight();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VisexPoseGround), false, Character);
	const auto GroundAt = [&](float Fwd, float Right, float& OutZ)
	{
		const FVector P = Actor.TransformPosition(FVector(Fwd, Right, 0.f));
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, FVector(P.X, P.Y, Bottom + 80.f), FVector(P.X, P.Y, Bottom - 120.f), ECC_Visibility, Params))
		{
			OutZ = Hit.ImpactPoint.Z;
			return true;
		}
		return false;
	};
	constexpr float Reach = 70.f; // about half of her length lying down
	float Z0 = Bottom, ZF = 0.f, ZB = 0.f, ZR = 0.f, ZL = 0.f;
	GroundAt(0.f, 0.f, Z0);
	const bool bF = GroundAt(Reach, 0.f, ZF), bB = GroundAt(-Reach, 0.f, ZB);
	const bool bR = GroundAt(0.f, Reach, ZR), bL = GroundAt(0.f, -Reach, ZL);
	const float SlopeF = (bF && bB) ? (ZF - ZB) / (2.f * Reach) : bF ? (ZF - Z0) / Reach : bB ? (Z0 - ZB) / Reach : 0.f;
	const float SlopeR = (bR && bL) ? (ZR - ZL) / (2.f * Reach) : bR ? (ZR - Z0) / Reach : bL ? (Z0 - ZL) / Reach : 0.f;
	const float MaxSlope = FMath::Tan(FMath::DegreesToRadians(28.f));
	FVector Normal = FVector(-FMath::Clamp(SlopeF, -MaxSlope, MaxSlope), -FMath::Clamp(SlopeR, -MaxSlope, MaxSlope), 1.f).GetSafeNormal();
	if (Normal.IsNearlyZero())
	{
		Normal = FVector::UpVector;
	}
	// in the actor's frame: the tilt turns up onto the ground's normal, about the body's root,
	// which sits at the capsule's bottom; the drop takes that root down to the sand under it
	const float Ease = FMath::SmoothStep(0.f, 1.f, GroundAlpha);
	const FQuat Tilt = FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(FVector::UpVector, Normal), Ease);
	const float Drop = FMath::Clamp(Z0 - Bottom, -40.f, 5.f) * Ease;
	AppliedSeat = SeatFixZ * Ease; // her lowest skin onto the ground, from the contact probe
	// and tipped along her length so the head end and the feet end both touch (dunes curve)
	const FQuat Tip = PitchAxis.IsNearlyZero() ? FQuat::Identity : FQuat(PitchAxis, FMath::DegreesToRadians(PitchFix * Ease));
	Mesh->SetRelativeLocationAndRotation(MeshBaseLocation + FVector(0.f, 0.f, Drop + AppliedSeat), Tip * Tilt * MeshBaseRotation);
}

bool UVisexPoseWheelComponent::IsOnFloor() const
{
	return PoseIns.IsValidIndex(Current) && PoseIns[Current]
		&& ((Phase == EPhase::Enter && (PhaseTime > 0.9f || bFlowing)) || Phase == EPhase::Hold);
}

static TAutoConsoleVariable<float> CVarContact(TEXT("visex.Contact"), 1.f, TEXT("Hands and hips meet the surface under them: 0 off, 1 on"));
static TAutoConsoleVariable<float> CVarContactSnap(TEXT("visex.Contact.Snap"), 6.f, TEXT("A hand or the hips this close above a surface (cm) are meant to rest on it"));

void UVisexPoseWheelComponent::UpdateContacts(float DeltaTime)
{
	// Measured on her skin, not her bones (VisexSkinProbe): the lowest point of each hand, of
	// her buttocks and of the rest of the lower body, and the surface straight under each.
	// A hand within Snap of a surface, or inside it, is moved onto it by the Anim Blueprint's
	// last step; on a floor pose the whole body is moved so her hips touch without pushing a
	// knee or a foot through. Gaps are read before the fix (what was drawn, minus the fix).
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	UVisexAnimInstance* Anim = Cast<UVisexAnimInstance>(GetAnim());
	if (!Mesh || !Anim || !Mesh->GetSkeletalMeshAsset())
	{
		return;
	}
	if (!Probe.IsBuiltFor(Mesh))
	{
		if (ProbeFailedFor.Get() == Mesh->GetSkeletalMeshAsset() || !Probe.Build(Mesh))
		{
			ProbeFailedFor = Mesh->GetSkeletalMeshAsset();
			return;
		}
	}
	const UCharacterMovementComponent* Move = Character->GetCharacterMovement();
	const bool bOn = CVarContact.GetValueOnGameThread() > 0.f && !(Move && Move->IsFalling());
	const float Snap = CVarContactSnap.GetValueOnGameThread();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VisexContact), false, Character);
	const auto SurfaceUnder = [&](const FVector& P, double From, double& OutZ)
	{
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, FVector(P.X, P.Y, From), FVector(P.X, P.Y, P.Z - 40.0), ECC_Visibility, Params))
		{
			OutZ = Hit.ImpactPoint.Z;
			return true;
		}
		return false;
	};
	const auto ByHeight = [](const FVector& V) { return V.Z; };

	// hands
	static const FName HandBones[2] = {TEXT("hand_l"), TEXT("hand_r")};
	float* Gaps[2] = {&HandGapL, &HandGapR};
	float* Fixes[2] = {&HandFixZL, &HandFixZR};
	for (int32 i = 0; i < 2; ++i)
	{
		// The skin is read from the last drawn pose, which already carries the fix, so the fix
		// steps toward zero drawn gap (an integrator) instead of being set from it: setting it
		// directly chases a frame-old pose and runs away.
		FVector Low;
		double Z = 0.0;
		float Step = -(*Fixes[i]);               // nothing to rest on: let go
		*Gaps[i] = 999.f;
		if (Probe.Lowest(Mesh, i == 0 ? FVisexSkinProbe::HandL : FVisexSkinProbe::HandR, ByHeight, Low)
			&& SurfaceUnder(Low, FMath::Max(Low.Z, Mesh->GetSocketLocation(HandBones[i]).Z) + 30.0, Z))
		{
			(i == 0 ? ProbeLowL : ProbeLowR) = Low;
			(i == 0 ? ProbeHitL : ProbeHitR) = FVector(Low.X, Low.Y, Z);
			const float Drawn = float(Low.Z - Z);
			const float Raw = Drawn - *Fixes[i];    // about where the pose itself puts it
			*Gaps[i] = Raw;
			// resting (close above, or inside but not buried under an overhang): onto it
			const float K = Raw < -15.f ? 0.f : 1.f - FMath::SmoothStep(Snap, Snap + 4.f, Raw);
			Step = bOn && K > 0.f ? -Drawn * K - *Fixes[i] * (1.f - K) : -(*Fixes[i]);
		}
		*Fixes[i] = FMath::Clamp(*Fixes[i] + Step * FMath::Min(1.f, DeltaTime * 6.f), -12.f, 15.f);
	}
	const FVector Up = Mesh->GetComponentTransform().InverseTransformVector(FVector::UpVector);
	Anim->HandFixL = Up * HandFixZL;
	Anim->HandFixR = Up * HandFixZR;

	// hips, and the rest of the lower body as a floor guard
	FVector Seat, Body;
	double SZ = 0.0, BZ = 0.0;
	const bool bSeat = Probe.Lowest(Mesh, FVisexSkinProbe::Seat, ByHeight, Seat) && SurfaceUnder(Seat, Seat.Z + 30.0, SZ);
	const bool bBody = Probe.Lowest(Mesh, FVisexSkinProbe::Body, ByHeight, Body) && SurfaceUnder(Body, Body.Z + 30.0, BZ);
	SeatGap = bSeat ? float(Seat.Z - SZ) - AppliedSeat : 999.f;
	BodyGap = bBody ? float(Body.Z - BZ) - AppliedSeat : 999.f;
	// The body on a floor pose: its lowest skin is brought onto the ground (no part through it,
	// none of it hovering), and a lying body is tipped along its length until its head end and
	// its feet end touch alike. Integrators again: the drawn gaps carry the fix already.
	float SeatStep = -SeatFixZ, PitchStep = -PitchFix;
	HeadEndGap = FeetEndGap = 999.f;
	if (bOn && IsOnFloor() && bBody)
	{
		SeatStep = -float(Body.Z - BZ);
		const FVector Head = Mesh->GetSocketLocation(TEXT("head"));
		const FVector Feet = 0.5f * (Mesh->GetSocketLocation(TEXT("foot_l")) + Mesh->GetSocketLocation(TEXT("foot_r")));
		const FVector Along = FVector(Head.X - Feet.X, Head.Y - Feet.Y, 0.f);
		// lying, not kneeling or sitting up: the trunk (pelvis to head) within 40 degrees of level
		const FVector Trunk = (Head - Mesh->GetSocketLocation(TEXT("pelvis"))).GetSafeNormal();
		if (Along.Size() > 60.f && FMath::Abs(Trunk.Z) < 0.64f)
		{
			const FVector Axis = Along.GetSafeNormal(), Mid = 0.5f * (Head + Feet);
			FVector HeadLow, FeetLow;
			double HZ = 0.0, FZ = 0.0;
			const bool bH = Probe.Lowest(Mesh, FVisexSkinProbe::Body, [&](const FVector& V) { return ((V - Mid) | Axis) > 0.0 ? V.Z : 1e9; }, HeadLow)
				&& SurfaceUnder(HeadLow, HeadLow.Z + 40.0, HZ);
			const bool bF = Probe.Lowest(Mesh, FVisexSkinProbe::Body, [&](const FVector& V) { return ((V - Mid) | Axis) < 0.0 ? V.Z : 1e9; }, FeetLow)
				&& SurfaceUnder(FeetLow, FeetLow.Z + 40.0, FZ);
			if (bH && bF)
			{
				HeadEndGap = float(HeadLow.Z - HZ);
				FeetEndGap = float(FeetLow.Z - FZ);
				const float Span = FMath::Max(float((HeadLow - FeetLow) | Axis), 40.f);
				// a positive turn about Axis x Up lifts the head end, so a high head end turns negative
				PitchAxis = Character->GetActorTransform().InverseTransformVectorNoScale(FVector::CrossProduct(Axis, FVector::UpVector)).GetSafeNormal();
				PitchStep = -FMath::RadiansToDegrees(FMath::Atan2(HeadEndGap - FeetEndGap, Span));
			}
		}
	}
	SeatFixZ = FMath::Clamp(SeatFixZ + SeatStep * FMath::Min(1.f, DeltaTime * 3.f), -15.f, 30.f);
	PitchFix = FMath::Clamp(PitchFix + PitchStep * FMath::Min(1.f, DeltaTime * 2.f), -28.f, 28.f);
	PitchFixDeg = PitchFix;
}

void UVisexPoseWheelComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	BindInput();
	UpdateFootIK(DeltaTime);
	UpdateGround(DeltaTime);
	UpdateContacts(DeltaTime);

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	APlayerController* PC = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
	if (UVisexAnimInstance* Visex = Cast<UVisexAnimInstance>(GetAnim()))
	{
		Visex->HoldAlpha = Phase == EPhase::Hold ? 1.f : 0.f; // the held pose breathes (moving hold)
	}

	if (Phase != EPhase::None && Character)
	{
		UAnimInstance* Anim = GetAnim();
		const UCharacterMovementComponent* Move = Character->GetCharacterMovement();
		// with walking switched off there is no acceleration, so read what the player asks for
		const bool bWantsMove = Move && (!Move->GetCurrentAcceleration().IsNearlyZero() || !Character->GetLastMovementInputVector().IsNearlyZero());
		PhaseTime += DeltaTime;
		if (Anim && Phase == EPhase::Enter && PoseIns[Current] && !(Move && Move->IsFalling())
			&& PhaseTime >= EnterLength - FMath::Max(0.1f, 3.f * DeltaTime))
		{
			// a one-shot montage lets go of the slot at its end, so the still pose (the same
			// frame, looped) takes over just before that
			PoseMontage = Anim->PlaySlotAnimationAsDynamicMontage(Poses[Current], PoseSlot, 0.1f, BlendTime, 1.f, 1000000);
			if (PoseMontage)
			{
				PoseMontage->bEnableAutoBlendOut = false;
			}
			Phase = EPhase::Hold;
		}
		else if (!Anim || !PoseMontage || !Anim->Montage_IsActive(PoseMontage))
		{
			// the way out has played, or a dash or another montage took the slot
			const int32 Next = Phase == EPhase::Exit ? Pending : INDEX_NONE;
			FinishPose();
			StartPose(Next);
		}
		else if (Move && Move->IsFalling())
		{
			Anim->Montage_Stop(BlendTime, PoseMontage);
			FinishPose();
		}
		else if (Phase == EPhase::Hold && (bExitQueued || (bWantsMove && !bOpen)))
		{
			if (bExitQueued && FlowTo(Current, Pending))
			{
				StartFlow(Pending); // another floor pose: she moves into it without getting up
			}
			else
			{
				BeginExit(); // moving breaks the pose; a seated one gets up first
			}
		}
	}

	if (!View.IsValid())
	{
		return;
	}
	if (bOpen && (!PC || !Character->InputEnabled()))
	{
		SetOpen(false); // the menu or Body page took the input away mid-hold
	}
	if (bOpen && PC)
	{
		float LX = 0.f, LY = 0.f, RX = 0.f, RY = 0.f, MX = 0.f, MY = 0.f;
		PC->GetInputAnalogStickState(EControllerAnalogStick::CAS_LeftStick, LX, LY);
		PC->GetInputAnalogStickState(EControllerAnalogStick::CAS_RightStick, RX, RY);
		PC->GetInputMouseDelta(MX, MY);
		const FVector2D Left(LX, LY), Right(RX, RY);
		const FVector2D Stick = Right.SizeSquared() >= Left.SizeSquared() ? Right : Left;
		if (Stick.Size() > 0.45f)
		{
			View->Pointer = Cursor = Stick.GetClampedToMaxSize(1.);
		}
		else if (!FMath::IsNearlyZero(MX) || !FMath::IsNearlyZero(MY))
		{
			Cursor = (Cursor + FVector2D(MX, MY) * 0.012).GetClampedToMaxSize(1.);
			View->Pointer = Cursor;
		}
		else if (Stick.Size() > 0.1f)
		{
			View->Pointer = Stick; // easing back to the centre keeps the choice
		}
		if (Cursor.Size() > 0.4f && View->Labels.Num() > 0)
		{
			const float Step = 2.f * PI / View->Labels.Num();
			const float Angle = FMath::Atan2(Cursor.X, Cursor.Y); // clockwise from the top
			View->Selected = ((FMath::RoundToInt(Angle / Step) % View->Labels.Num()) + View->Labels.Num()) % View->Labels.Num();
		}
	}
	View->Fade = FMath::FInterpTo(View->Fade, bOpen ? 1.f : 0.f, DeltaTime, 14.f);
	if (Widget.IsValid())
	{
		Widget->SetVisibility(View->Fade > 0.002f ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
		Widget->Invalidate(EInvalidateWidgetReason::Paint);
	}
}

void UVisexPoseWheelSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	ACharacter* Character = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	if (Character && Character->HasActorBegunPlay() && !Character->FindComponentByClass<UVisexPoseWheelComponent>())
	{
		UVisexPoseWheelComponent* Wheel = NewObject<UVisexPoseWheelComponent>(Character, TEXT("PoseWheel"));
		Wheel->RegisterComponent();
	}
}
