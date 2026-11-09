#include "VisexPoseWheel.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetRegistryModule.h"
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
	for (const FAssetData& Asset : Found)
	{
		const FString Name = Asset.AssetName.ToString();
		if (Name.EndsWith(TEXT("_In")) || Name.EndsWith(TEXT("_Out")))
		{
			continue; // the way into and out of a pose, found through the pose itself
		}
		if (UAnimSequence* Pose = Cast<UAnimSequence>(Asset.GetAsset()))
		{
			FString Label = Name;
			Label.RemoveFromStart(TEXT("A_Pose_"));
			const FString Path = Asset.PackageName.ToString();
			Poses.Add(Pose);
			PoseIns.Add(LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("%s_In.%s_In"), *Path, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet));
			PoseOuts.Add(LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("%s_Out.%s_Out"), *Path, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet));
			View->Labels.Add(Label.Replace(TEXT("_"), TEXT(" ")));
		}
	}
	View->Labels.Add(TEXT("Stand")); // the last place lets go of the pose

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

void UVisexPoseWheelComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	BindInput();

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	APlayerController* PC = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;

	if (Phase != EPhase::None && Character)
	{
		UAnimInstance* Anim = GetAnim();
		const UCharacterMovementComponent* Move = Character->GetCharacterMovement();
		// with walking switched off there is no acceleration, so read what the player asks for
		const bool bWantsMove = Move && (!Move->GetCurrentAcceleration().IsNearlyZero() || !Character->GetLastMovementInputVector().IsNearlyZero());
		PhaseTime += DeltaTime;
		if (Anim && Phase == EPhase::Enter && PoseIns[Current] && !(Move && Move->IsFalling())
			&& PhaseTime >= PoseIns[Current]->GetPlayLength() - FMath::Max(0.1f, 3.f * DeltaTime))
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
			BeginExit(); // moving breaks the pose; a seated one gets up first
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
