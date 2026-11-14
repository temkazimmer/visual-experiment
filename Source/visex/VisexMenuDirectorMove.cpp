// The menu's root-motion moves (Docs/Research/FOOT_IK.md): her walks out to a page, back to the
// column and in on the first launch play start, loop and stop clips with root motion, so her
// capsule follows the clip and a planted foot stays put. Each clip hands on to the next over a
// short blend with the planted foot held where it is (HoldFoot); the code only steers her heading.
#include "VisexMenuDirector.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

static TAutoConsoleVariable<int32> CVarMenuRootMotion(TEXT("visex.Menu.RootMotion"), 1,
	TEXT("1: the menu's walks (out to a page, back, the arrival) play root-motion start/loop/stop clips. 0: the capsule-driven walk."));

static TAutoConsoleVariable<int32> CVarMenuArrivalTurn(TEXT("visex.Menu.ArrivalTurn"), 1,
	TEXT("1: the arrival curves in and turns into the lean on the walk-turn's pivot. 0: she walks in, stops and turns."));

static TAutoConsoleVariable<int32> CVarMenuStepBack(TEXT("visex.Menu.StepBack"), 1,
	TEXT("1: back from a page straight behind her, she steps back onto the column (two steps, no turns). 0: she turns, walks and turns."));

static const FName MoveSlot(TEXT("DefaultSlot"));

bool AVisexMenuDirector::RootMoves() const
{
	return CVarMenuRootMotion.GetValueOnGameThread() != 0;
}

const AVisexMenuDirector::FClipTrack& AVisexMenuDirector::Track(UAnimSequence* Clip)
{
	if (const FClipTrack* Have = Tracks.Find(Clip))
	{
		return *Have;
	}
	FClipTrack& K = Tracks.Add(Clip);
	LoadedLean.AddUnique(Clip);
#if WITH_EDITOR
	Clip->WaitOnExistingCompression();
#endif
	K.Len = Clip->GetPlayLength();
	const int32 N = FMath::Max(2, FMath::CeilToInt(K.Len * 60.f));
	K.Dt = K.Len / N;
	const FReferenceSkeleton& Ref = Clip->GetSkeleton()->GetReferenceSkeleton();
	const int32 Bones[3] = {Ref.FindBoneIndex(TEXT("ik_foot_root")), Ref.FindBoneIndex(TEXT("ik_foot_l")), Ref.FindBoneIndex(TEXT("ik_foot_r"))};
	for (int32 I = 0; I <= N; ++I)
	{
		const double T = K.Dt * I;
		FTransform X[3];
		for (int32 B = 0; B < 3; ++B)
		{
			if (Bones[B] != INDEX_NONE)
			{
				Clip->GetBoneTransform(X[B], FSkeletonPoseBoneIndex(Bones[B]), FAnimExtractContext(T), false);
			}
		}
		K.Feet[0].Add((X[1] * X[0]).GetLocation());
		K.Feet[1].Add((X[2] * X[0]).GetLocation());
		const FTransform RM = Clip->ExtractRootMotionFromRange(0.0, T, FAnimExtractContext());
		K.Root.Add(RM.GetTranslation());
		const float Yaw = RM.Rotator().Yaw;
		K.Yaw.Add(K.Yaw.Num() ? K.Yaw.Last() + FMath::FindDeltaAngleDegrees(FMath::UnwindDegrees(K.Yaw.Last()), Yaw) : Yaw);
	}
	return K;
}

static FVector SampleAt(const TArray<FVector>& V, float Dt, float T)
{
	const float X = FMath::Clamp(T / Dt, 0.f, float(V.Num() - 1));
	const int32 I = FMath::Min(FMath::FloorToInt(X), V.Num() - 2);
	return FMath::Lerp(V[I], V[I + 1], X - I);
}

FVector AVisexMenuDirector::ClipFoot(UAnimSequence* Clip, float T, int32 Foot)
{
	const FClipTrack& K = Track(Clip);
	return SampleAt(K.Feet[Foot & 1], K.Dt, T);
}

FVector AVisexMenuDirector::ClipRoot(UAnimSequence* Clip, float A, float B)
{
	// past the clip's end it loops: whole laps of its travel, then the rest
	const FClipTrack& K = Track(Clip);
	const FVector Lap = K.Root.Last();
	auto At = [&K, &Lap](float T)
	{
		const float Laps = FMath::FloorToFloat(T / K.Len);
		return SampleAt(K.Root, K.Dt, T - Laps * K.Len) + Lap * Laps;
	};
	return At(B) - At(A);
}

float AVisexMenuDirector::ClipYaw(UAnimSequence* Clip, float T)
{
	const FClipTrack& K = Track(Clip);
	const float Laps = FMath::FloorToFloat(T / K.Len);
	const float X = FMath::Clamp((T - Laps * K.Len) / K.Dt, 0.f, float(K.Yaw.Num() - 1));
	const int32 I = FMath::Min(FMath::FloorToInt(X), K.Yaw.Num() - 2);
	return FMath::Lerp(K.Yaw[I], K.Yaw[I + 1], X - I) + K.Yaw.Last() * Laps;
}

FVector AVisexMenuDirector::ClipVel(UAnimSequence* Clip, float T)
{
	// a start or a stop is read inside the clip; only the loop runs on round its end
	constexpr float H = 1.f / 60.f;
	const float Len = Clip->GetPlayLength();
	const bool bLoop = Clip == WalkLoop.Get();
	const float A = bLoop ? T - H : FMath::Clamp(T - H, 0.f, Len - 2.f * H), B = A + 2.f * H;
	return ClipRoot(Clip, A, B) / (2.f * H);
}

FVector AVisexMenuDirector::MeshToWorld(const FVector& V, float Yaw) const
{
	// her mesh sits at -90 yaw (and her body's scale) in the capsule; Yaw is the capsule's
	const ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	const FVector Local = Char && Char->GetMesh() ? Char->GetMesh()->GetRelativeTransform().TransformVector(V) : V;
	return FRotator(0.f, Yaw, 0.f).RotateVector(Local);
}

void AVisexMenuDirector::HoldFoot(UAnimSequence* FromClip, float FromTime, UAnimSequence* ToClip, float ToTime, int32 Foot, UAnimMontage* Incoming)
{
	ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	USkeletalMeshComponent* Mesh = Char ? Char->GetMesh() : nullptr;
	if (!Mesh || !ToClip || !Incoming || Foot < 0)
	{
		return;
	}
	// where the planted foot is now (in the outgoing clip, or no clip: her foot bone as last
	// drawn), less where the incoming clip has it: her capsule goes there
	const FVector Was = FromClip ? ClipFoot(FromClip, FromTime, Foot)
		: Mesh->GetComponentTransform().InverseTransformPosition(Mesh->GetBoneLocation(Foot ? FName(TEXT("foot_r")) : FName(TEXT("foot_l"))));
	FVector D = Mesh->GetComponentTransform().TransformVector(Was - ClipFoot(ToClip, ToTime, Foot));
	D.Z = 0.f;
	if (!FromClip && D.Size2D() > 12.f)
	{
		// her pose is far from the clip's (a photo pose): no foot to hold, the clip blends in
		UE_LOG(LogTemp, Log, TEXT("Menu move: (her pose) -> %s, %.1f cm off, not held"), *ToClip->GetName(), D.Size2D());
		return;
	}
	// a hold still easing out is folded into this one
	const FVector Left = bHolding ? Mesh->GetRelativeLocation() - HoldBase : FVector::ZeroVector;
	if (!bHolding)
	{
		HoldBase = Mesh->GetRelativeLocation();
	}
	Char->AddActorWorldOffset(D, false, nullptr, ETeleportType::None);
	// a lean or a turn stops where it is as it blends out (its own feet shuffle on; the walk
	// clips' planted feet keep pace with their root, so they play on)
	if (FromClip != WalkStart.Get() && FromClip != WalkLoop.Get())
	{
		if (UAnimInstance* Anim = Mesh->GetAnimInstance())
		{
			for (FAnimMontageInstance* Inst : Anim->MontageInstances)
			{
				if (Inst && Inst->Montage != Incoming)
				{
					Inst->Pause();
				}
			}
		}
	}
	HoldOffset = Left - Char->GetActorTransform().InverseTransformVectorNoScale(D);
	HoldMontage = Incoming;
	bHolding = true;
	Mesh->SetRelativeLocation(HoldBase + HoldOffset);
	if (!HoldHandle.IsValid())
	{
		HoldHandle = Mesh->RegisterOnBoneTransformsFinalizedDelegate(
			FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateUObject(this, &AVisexMenuDirector::TickHold));
	}
	UE_LOG(LogTemp, Log, TEXT("Menu move: %s -> %s at %.2f/%.2f s, %s foot held, capsule %.1f cm"),
		FromClip ? *FromClip->GetName() : TEXT("(her pose)"), *ToClip->GetName(), FromTime, ToTime, Foot ? TEXT("right") : TEXT("left"), D.Size2D());
}

void AVisexMenuDirector::TickHold()
{
	ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	USkeletalMeshComponent* Mesh = Char ? Char->GetMesh() : nullptr;
	if (!bHolding || !Mesh)
	{
		return;
	}
	// the incoming clip's weight, as her pose was just blended with it
	const UAnimInstance* Anim = Mesh->GetAnimInstance();
	const FAnimMontageInstance* Inst = Anim && HoldMontage ? Anim->GetActiveInstanceForMontage(HoldMontage) : nullptr;
	const float W = Inst ? FMath::Clamp(Inst->GetWeight(), 0.f, 1.f) : 1.f;
	Mesh->SetRelativeLocation(HoldBase + HoldOffset * (1.f - W));
	if (W >= 0.999f)
	{
		bHolding = false;
		HoldMontage = nullptr;
	}
}

float AVisexMenuDirector::PlayRootMotionMove(const TArray<FMoveSeg>& Segs, UAnimSequence* FromClip, float FromTime, float Rate)
{
	UAnimInstance* Anim = GetPawnAnim();
	if (!Anim || Segs.Num() == 0 || !Segs[0].Clip)
	{
		return 0.f;
	}
	MoveSegs = Segs;
	MoveRate = FMath::Max(0.25f, Rate);
	Anim->SetRootMotionMode(ERootMotionMode::RootMotionFromMontagesOnly);
	StartMoveSeg(0, FromClip, FromTime);
	return MoveLeft();
}

void AVisexMenuDirector::StartMoveSeg(int32 Index, UAnimSequence* FromClip, float FromTime)
{
	UAnimInstance* Anim = GetPawnAnim();
	const FMoveSeg& S = MoveSegs[Index];
	const float Len = S.Clip->GetPlayLength();
	const float From = FMath::Fmod(S.From, Len);
	// a loop plays round until the next takes over; a start or stop to its end, then blends out
	const int32 Loops = S.To > Len + 0.01f ? 1000000 : 1;
	MoveMontage = Anim->PlaySlotAnimationAsDynamicMontage(S.Clip, MoveSlot, S.Blend, 0.4f, MoveRate, Loops, -1.f, From);
	MoveIndex = Index;
	MoveClock = S.From;
	MoveLastPos = From;
	if (S.Lock >= 0 && FromClip && MoveMontage)
	{
		HoldFoot(FromClip, FromTime, S.Clip, From, S.Lock, MoveMontage);
	}
}

void AVisexMenuDirector::TickMove()
{
	UAnimInstance* Anim = GetPawnAnim();
	if (MoveIndex < 0)
	{
		return;
	}
	if (!Anim || !MoveMontage || !Anim->Montage_IsPlaying(MoveMontage))
	{
		StopMove();
		return;
	}
	// her clip time, from the montage itself (it holds while she's frozen, and wraps on a loop)
	const FMoveSeg& S = MoveSegs[MoveIndex];
	const float Len = S.Clip->GetPlayLength();
	const float Pos = Anim->Montage_GetPosition(MoveMontage);
	float Step = Pos - MoveLastPos;
	if (Step < -0.5f * Len)
	{
		Step += Len;
	}
	MoveLastPos = Pos;
	MoveClock += FMath::Max(0.f, Step);
	if (MoveClock >= S.To && MoveIndex + 1 < MoveSegs.Num())
	{
		StartMoveSeg(MoveIndex + 1, S.Clip, Pos);
	}
}

void AVisexMenuDirector::StopMove()
{
	// what plays goes on to its end (or until the next clip takes the slot)
	MoveIndex = -1;
	MoveSegs.Reset();
	if (ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Char->SetAnimRootMotionTranslationScale(1.f);
	}
}

float AVisexMenuDirector::MoveLeft() const
{
	if (MoveIndex < 0)
	{
		return 0.f;
	}
	float Left = FMath::Max(0.f, MoveSegs[MoveIndex].To - MoveClock);
	for (int32 I = MoveIndex + 1; I < MoveSegs.Num(); ++I)
	{
		Left += MoveSegs[I].To - MoveSegs[I].From;
	}
	return Left / MoveRate;
}

bool AVisexMenuDirector::FindCut(UAnimSequence* A, FVector2D AWin, UAnimSequence* B, FVector2D BWin, int32& Foot, float& TA, float& TB, float MaxCost)
{
	Foot = 0;
	TA = AWin.X;
	TB = BWin.X;
	if (!A || !B)
	{
		return false;
	}
	// a foot is planted while it's low and still on the ground: its own motion in the clip
	// cancels the root's
	auto Planted = [this](UAnimSequence* C, int32 F, float T)
	{
		constexpr float H = 1.f / 60.f;
		for (const float X : {T, T + MoveBlend * 0.5f, T + MoveBlend})
		{
			FVector Speed = (ClipFoot(C, X + H, F) - ClipFoot(C, X - H, F)) / (2.f * H) + ClipVel(C, X);
			Speed.Z = 0.f;
			if (ClipFoot(C, X, F).Z > 10.f || Speed.Size() > 20.f)
			{
				return false;
			}
		}
		return true;
	};
	float Best = BIG_NUMBER;
	for (float X = AWin.X; X <= AWin.Y; X += 1.f / 60.f)
	{
		for (float Y = BWin.X; Y <= BWin.Y; Y += 1.f / 60.f)
		{
			const float DV = (ClipVel(A, X) - ClipVel(B, Y)).Size2D();
			if (DV > 20.f)
			{
				continue;
			}
			for (int32 F = 0; F < 2; ++F)
			{
				// the swinging foot nearest where the next clip has it, the pace nearest
				const float Cost = (ClipFoot(A, X, 1 - F) - ClipFoot(B, Y, 1 - F)).Size() + 0.2f * DV;
				if (Cost < Best && Planted(A, F, X) && Planted(B, F, Y))
				{
					Best = Cost;
					Foot = F;
					TA = X;
					TB = Y;
				}
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("Menu move: cut %s %.2f -> %s %.2f, %s foot planted, other off by %.1f cm"),
		*A->GetName(), TA, *B->GetName(), TB, Foot ? TEXT("right") : TEXT("left"), Best);
	return Best < MaxCost;
}

FVector AVisexMenuDirector::MoveTravel(const TArray<FMoveSeg>& Segs)
{
	// the root travel of each clip as it plays, and the capsule's jump at each held hand-off
	FVector Sum = FVector::ZeroVector;
	for (int32 I = 0; I < Segs.Num(); ++I)
	{
		const FMoveSeg& S = Segs[I];
		Sum += ClipRoot(S.Clip, S.From, S.To);
		if (I > 0 && S.Lock >= 0)
		{
			const FMoveSeg& P = Segs[I - 1];
			Sum += ClipFoot(P.Clip, FMath::Fmod(P.To, P.Clip->GetPlayLength()), S.Lock) - ClipFoot(S.Clip, FMath::Fmod(S.From, S.Clip->GetPlayLength()), S.Lock);
		}
	}
	Sum.Z = 0.f;
	return Sum;
}

bool AVisexMenuDirector::PickTurn(float Need, bool bThenWalk, FTurnPick& Out)
{
	Out = FTurnPick();
	if (FMath::Abs(Need) < TurnClipMin)
	{
		return false;
	}
	// her root yaw through each clip, unwrapped; the nearest turn (by ratio) on the same side wins
	float BestErr = BIG_NUMBER;
	for (const TSoftObjectPtr<UAnimSequence>& Soft : TurnClips)
	{
		UAnimSequence* Clip = Soft.LoadSynchronous();
		if (!Clip)
		{
			continue;
		}
		LoadedLean.AddUnique(Clip);
#if WITH_EDITOR
		Clip->WaitOnExistingCompression();
#endif
		const float Len = Clip->GetPlayLength();
		const int32 N = FMath::Max(8, FMath::CeilToInt(Len * 60.f));
		TArray<float> Curve;
		float Prev = 0.f, Acc = 0.f;
		for (int32 I = 0; I <= N; ++I)
		{
			const float Yaw = Clip->ExtractRootMotionFromRange(0.0, Len * I / N, FAnimExtractContext()).Rotator().Yaw;
			Acc += FMath::FindDeltaAngleDegrees(Prev, Yaw);
			Prev = Yaw;
			Curve.Add(Acc);
		}
		const float Total = Curve.Last();
		if (Total * Need <= 0.f || FMath::Abs(Total) < 10.f)
		{
			continue;
		}
		const float Err = FMath::Abs(FMath::Loge(Need / Total));
		if (Err < BestErr)
		{
			BestErr = Err;
			Out.Clip = Clip;
			Out.Curve = MoveTemp(Curve);
			Out.Dt = Len / N;
			Out.Scale = Need / Total;
		}
	}
	return Out.Clip && FinishTurnPick(bThenWalk, Out);
}

bool AVisexMenuDirector::FinishTurnPick(bool bThenWalk, FTurnPick& Out) const
{
	const float Len = Out.Clip->GetPlayLength();
	const float Total = Out.Curve.Last();
	auto At = [&Out, Total, Len](float Share)
	{
		for (int32 I = 0; I < Out.Curve.Num(); ++I)
		{
			if (FMath::Abs(Out.Curve[I]) >= FMath::Abs(Total) * Share)
			{
				return I * Out.Dt;
			}
		}
		return Len;
	};
	// in her seconds: the clip from its first step, at the menu's pace. Root-motion walks: into
	// the walk, or the lean, once TurnHandOff of the yaw is in, feet down; the capsule walk went
	// into its walk part-way (the walk steered the rest), and the sink took over the last steps
	const bool bRoot = RootMoves();
	const float Landed = At(bThenWalk ? (bRoot ? TurnHandOff : TurnIntoWalk) : 0.98f);
	Out.Start = FMath::Min(TurnClipLeadIn, Landed * 0.5f);
	Out.Rate = FMath::Max(0.5f, bArriving ? TurnClipRate : ReturnTurnRate);
	Out.End = (FMath::Min(Landed + (bThenWalk ? 0.f : 0.05f), Len - 0.2f) - Out.Start) / Out.Rate;
	Out.LeanAt = -1.f;
	if (!bThenWalk && (bRoot || !bArriving))
	{
		Out.LeanAt = FMath::Max(0.f, At(bRoot ? TurnHandOff : TurnIntoLean) - Out.Start) / Out.Rate;
	}
	return true;
}

bool AVisexMenuDirector::BeginRootStepOut()
{
	// out from the lean: the lean-up hands its planted left foot to the walk start's, the start
	// hands its planted right to the stop's, and she stops a step clear of the stone
	ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	UAnimInstance* Anim = GetPawnAnim();
	UAnimSequence* Start = WalkStart.LoadSynchronous();
	UAnimSequence* Stop = WalkStop.LoadSynchronous();
	UAnimSequence* Up = LeanUp.LoadSynchronous();
	int32 Foot = 1;
	float A = 0.f, B = 0.f;
	if (!RootMoves() || !Char || !Anim || !Start || !Stop || !Up || !FindCut(Start, WalkCutFrom, Stop, WalkCutInto, Foot, A, B))
	{
		return false;
	}
	TArray<FMoveSeg> Segs;
	Segs.Add({Start, WalkStartFromUp, A, 0, MoveBlend});
	Segs.Add({Stop, B, WalkStopAt, Foot, MoveBlend});
	// the lean-up where it is now (it's the slot's montage while she pushes off): halfway up,
	// her right foot already lifting into the walk's first step
	float UpAt = LeanUpHandOff;
	if (const UAnimMontage* Cur = Anim->GetCurrentActiveMontage())
	{
		UpAt = Anim->Montage_GetPosition(Cur);
	}
	const float Yaw = Char->GetActorRotation().Yaw;
	const FVector Travel = MeshToWorld(MoveTravel(Segs) + ClipFoot(Up, UpAt, 0) - ClipFoot(Start, WalkStartFromUp, 0), Yaw)
		+ MeshToWorld(ClipRoot(Stop, WalkStopAt, Stop->GetPlayLength()), Yaw);
	const float Gap = ColumnGapAt(Char->GetActorLocation());
	BodyStepTarget = ColumnGapAt(Char->GetActorLocation() + Travel);
	// her left foot held where it really is (a beat or the plant pass may have it a few cm off
	// the lean-up's own), so nothing slides as the push-off becomes the first step
	const float Secs = PlayRootMotionMove(Segs, nullptr, 0.f, 1.f);
	if (MoveMontage)
	{
		HoldFootNow(Start, WalkStartFromUp, 0, MoveMontage);
	}
	bRootMove = true;
	UE_LOG(LogTemp, Log, TEXT("Body step-out (root motion): lean-up at %.2f s, %.1f cm in %.2f s, gap %.1f -> %.1f, %.1f clear at the sliders' ends"),
		UpAt, Travel.Size2D(), Secs, Gap, BodyStepTarget, BodyStepTarget - BodyReach);
	return true;
}

bool AVisexMenuDirector::PlanRootReturn(const FVector& From, UAnimSequence* Turn1Clip, float Turn1At, bool bArrival, TArray<FMoveSeg>& Segs, float& Yaw, float& Scale)
{
	UAnimSequence* Start = WalkStart.LoadSynchronous();
	UAnimSequence* Loop = WalkLoop.LoadSynchronous();
	UAnimSequence* Stop = WalkStop.LoadSynchronous();
	UAnimSequence* Down = LeanDown.LoadSynchronous();
	Segs.Reset();
	Scale = 1.f;
	if (!RootMoves() || !Start || !Loop || !Stop || !Down)
	{
		return false;
	}
	FVector To = StepHome - From;
	To.Z = 0.f;
	// the last turn, onto her lean facing (the first turn's way round: the two make a whole turn,
	// and their end stances cancel out), and the holds onto it and from it onto the lean-down
	float Need2 = FMath::FindDeltaAngleDegrees(To.Rotation().Yaw, HomeYaw);
	if (MoveSide != 0.f && Need2 * MoveSide < 0.f)
	{
		Need2 += 360.f * MoveSide;
	}
	FTurnPick P2;
	const bool bTurn2 = PickTurn(Need2, false, P2);
	UAnimSequence* Last = bTurn2 ? P2.Clip : Stop;
	const float LastAt = bTurn2 ? P2.Start + P2.LeanAt * P2.Rate : WalkStopAt;
	const FVector IntoTurn = bTurn2 ? ClipFoot(Stop, WalkStopAt, 0) - ClipFoot(P2.Clip, P2.Start, 0) : FVector::ZeroVector;
	FVector V = To - MeshToWorld(ClipFoot(Last, LastAt, 0) - ClipFoot(Down, LeanDownFrom, 0), HomeYaw);
	V.Z = 0.f;
	// the walk: what it covers as she faces +X, with every hold in it
	auto Cover = [&](const TArray<FMoveSeg>& S, const FVector& Before)
	{
		FVector W = MeshToWorld(Before + MoveTravel(S) + IntoTurn, 0.f);
		W.Z = 0.f;
		return W;
	};
	FVector W = FVector::ZeroVector;
	if (bArrival)
	{
		// in from off frame, already walking: the loop from wherever puts her stop on the mark
		int32 Foot = 0;
		float A = 0.f, B = 0.f;
		const float Len = Loop->GetPlayLength();
		if (!FindCut(Loop, FVector2D(0.3f, Len - 0.4f), Stop, FVector2D(2.6f, 3.8f), Foot, A, B))
		{
			return false;
		}
		auto Make = [&](float T)
		{
			TArray<FMoveSeg> S;
			const float F = FMath::Fmod(A - T + 100.f * Len, Len);
			S.Add({Loop, F, F + T, -1, 0.f});
			S.Add({Stop, B, WalkStopAt, Foot, MoveBlend});
			return S;
		};
		float Lo = 0.3f, Hi = 40.f;
		for (int32 I = 0; I < 40; ++I)
		{
			const float Mid = 0.5f * (Lo + Hi);
			(Cover(Make(Mid), FVector::ZeroVector).Size2D() < V.Size2D() ? Lo : Hi) = Mid;
		}
		Segs = Make(0.5f * (Lo + Hi));
		W = Cover(Segs, FVector::ZeroVector);
	}
	else
	{
		// from the first turn's stance into the start; then the short walk (two steps) or the
		// long one (four), whichever lands nearer, the rest taken up by a little more or less stride
		const FVector Before = Turn1Clip ? ClipFoot(Turn1Clip, Turn1At, 0) - ClipFoot(Start, WalkStartFrom, 0) : FVector::ZeroVector;
		const FVector2D Wins[2][2] = {{WalkCutFrom, WalkCutInto}, {FVector2D(1.9f, 2.6f), FVector2D(2.6f, 3.3f)}};
		float Best = BIG_NUMBER;
		for (const auto& Win : Wins)
		{
			int32 Foot = 1;
			float A = 0.f, B = 0.f;
			if (!FindCut(Start, Win[0], Stop, Win[1], Foot, A, B))
			{
				continue;
			}
			TArray<FMoveSeg> S;
			S.Add({Start, WalkStartFrom, A, Turn1Clip ? 0 : -1, MoveBlend});
			S.Add({Stop, B, WalkStopAt, Foot, MoveBlend});
			const FVector C = Cover(S, Before);
			if (FMath::Abs(C.Size2D() - V.Size2D()) < Best)
			{
				Best = FMath::Abs(C.Size2D() - V.Size2D());
				Segs = S;
				W = C;
			}
		}
		if (Segs.Num() == 0)
		{
			return false;
		}
		// the part root motion covers takes the difference (the holds are where they are)
		const FVector Root = MeshToWorld(ClipRoot(Start, WalkStartFrom, Segs[0].To) + ClipRoot(Stop, Segs[1].From, WalkStopAt), 0.f);
		Scale = 1.f + (V.Size2D() - W.Size2D()) / FMath::Max(10.f, Root.Size2D());
	}
	Yaw = V.Rotation().Yaw - W.Rotation().Yaw;
	UE_LOG(LogTemp, Log, TEXT("Menu move: walk %s %.1f cm (planned %.1f, stride x%.3f), heading %.1f, last turn %.1f deg %s"),
		bArrival ? TEXT("in") : TEXT("back"), V.Size2D(), W.Size2D(), Scale, Yaw, Need2, bTurn2 ? *P2.Clip->GetName() : TEXT("(none)"));
	return FMath::Abs(Scale - 1.f) <= 0.12f;
}

void AVisexMenuDirector::BeginRootTurnHome()
{
	// stopped: the turn round onto her lean facing, the first turn's way round, from her stance
	ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	UAnimInstance* Anim = GetPawnAnim();
	UAnimSequence* Stop = WalkStop.LoadSynchronous();
	const float StopAt = Anim && MoveMontage && Anim->Montage_IsPlaying(MoveMontage) ? Anim->Montage_GetPosition(MoveMontage) : WalkStopAt;
	StopMove();
	ReturnPhase = 2;
	PhaseClock = 0.f;
	TurnFrom = Char->GetActorRotation().Yaw;
	float Need = FMath::FindDeltaAngleDegrees(TurnFrom, HomeYaw);
	if (MoveSide != 0.f && Need * MoveSide < 0.f)
	{
		Need += 360.f * MoveSide;
	}
	TurnTo = TurnFrom + Need;
	TurnTime = ReturnTurnTime * FMath::Max(0.35f, FMath::Abs(Need) / 180.f);
	UE_LOG(LogTemp, Log, TEXT("Menu move: stopped %.1f cm from her spot at %.2f s"), FVector::Dist2D(Char->GetActorLocation(), StepHome), StepClock);
	if (StartTurnClip(false))
	{
		HoldFoot(Stop, StopAt, TurnClip, TurnStart, 0, TurnMontage);
		return;
	}
	// too small a turn for a clip: into the lean from the stop, her yaw eased round
	LeanWithHold(Stop, StopAt);
}

void AVisexMenuDirector::LeanWithHold(UAnimSequence* FromClip, float FromTime, float DownFrom)
{
	const float From = DownFrom >= 0.f ? DownFrom : LeanDownFrom;
	Lean(true, From, LeanDownRate);
	UAnimInstance* Anim = GetPawnAnim();
	if (UAnimMontage* M = Anim && bLeaning ? Anim->GetCurrentActiveMontage() : nullptr)
	{
		HoldFoot(FromClip, FromTime, LeanDown.LoadSynchronous(), From, 0, M);
	}
}

void AVisexMenuDirector::HoldFootNow(UAnimSequence* ToClip, float ToTime, int32 Foot, UAnimMontage* Incoming)
{
	HoldFoot(nullptr, 0.f, ToClip, ToTime, Foot, Incoming);
}

int32 AVisexMenuDirector::ClipPlantedFoot(UAnimSequence* Clip, float T)
{
	// the one foot standing still in the clip (its own motion against the root's)
	constexpr float H = 1.f / 60.f;
	// (both down: -1, as a turn about either would slide the other)
	int32 Out = -1, Down = 0;
	for (int32 F = 0; F < 2; ++F)
	{
		FVector V = (ClipFoot(Clip, T + H, F) - ClipFoot(Clip, T - H, F)) / (2.f * H) + ClipVel(Clip, T);
		V.Z = 0.f;
		if (ClipFoot(Clip, T, F).Z < 11.f && V.Size() < 25.f)
		{
			Out = F;
			++Down;
		}
	}
	return Down == 1 ? Out : -1;
}

void AVisexMenuDirector::TurnAboutPlanted(float NewYaw, float Natural, int32 ForcePivot)
{
	ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	USkeletalMeshComponent* Mesh = Char ? Char->GetMesh() : nullptr;
	if (!Mesh)
	{
		return;
	}
	// her feet as last drawn; the planted one barely moved since the frame before
	const FVector Feet[2] = {Mesh->GetBoneLocation(TEXT("foot_l")), Mesh->GetBoneLocation(TEXT("foot_r"))};
	int32 Pivot = -1;
	if (FootFrame + 1 == GFrameCounter)
	{
		const float Moved[2] = {float(FVector::Dist2D(Feet[0], FootWas[0])), float(FVector::Dist2D(Feet[1], FootWas[1]))};
		const int32 F = Moved[0] <= Moved[1] ? 0 : 1;
		const float Dt = FMath::Max(GetWorld()->GetDeltaSeconds(), 1e-3f);
		Pivot = Moved[F] < 30.f * Dt ? F : -1;
	}
	if (ForcePivot >= 0)
	{
		Pivot = ForcePivot;
	}
	FootWas[0] = Feet[0];
	FootWas[1] = Feet[1];
	FootFrame = GFrameCounter;
	// the clip's own turn goes round her capsule (its feet are drawn for that); the rest round
	// the planted foot, which then stays where it is
	const float Old = Char->GetActorRotation().Yaw;
	const float Extra = FMath::FindDeltaAngleDegrees(Old + Natural, NewYaw);
	FVector Loc = Char->GetActorLocation();
	if (Pivot >= 0)
	{
		const FVector P(Feet[Pivot].X, Feet[Pivot].Y, Loc.Z);
		Loc = P + FRotator(0.f, Extra, 0.f).RotateVector(Loc - P);
	}
	Char->SetActorLocationAndRotation(Loc, FRotator(0.f, NewYaw, 0.f), false, nullptr, ETeleportType::None);
}

bool AVisexMenuDirector::BeginRootStepBack()
{
	// her spot straight behind her, a short way: she steps back onto it, left then right, and
	// sinks into the lean as the right lands (no turning round to walk, then round again)
	ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	UAnimSequence* BS = WalkBackStart.LoadSynchronous();
	UAnimSequence* BE = WalkBackStop.LoadSynchronous();
	UAnimSequence* Down = LeanDown.LoadSynchronous();
	if (!RootMoves() || CVarMenuStepBack.GetValueOnGameThread() == 0 || !Char || !GetPawnAnim() || !BS || !BE || !Down)
	{
		return false;
	}
	FVector To = StepHome - Char->GetActorLocation();
	To.Z = 0.f;
	const FVector Back = -Char->GetActorForwardVector().GetSafeNormal2D();
	const float Off = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(float(FVector::DotProduct(To.GetSafeNormal(), Back)), -1.f, 1.f)));
	const float Facing = FMath::Abs(FMath::FindDeltaAngleDegrees(Char->GetActorRotation().Yaw, HomeYaw));
	int32 Foot = 0;
	float A = 0.f, B = 0.f;
	if (To.Size() < 20.f || To.Size() > StepBackMax || Off > StepBackAngle || Facing > StepBackAngle
		|| !FindCut(BS, StepBackCutFrom, BE, StepBackCutInto, Foot, A, B, 20.f))
	{
		UE_LOG(LogTemp, Log, TEXT("Menu move: no step back (%.1f cm, %.1f deg off straight back, facing %.1f off)"), To.Size(), Off, Facing);
		return false;
	}
	// where she sets off in the backward start (its weight shift, both feet down) takes up how
	// far her spot is from where the clips and holds would put her
	USkeletalMeshComponent* Mesh = Char->GetMesh();
	const FVector RightNow = Mesh->GetComponentTransform().InverseTransformPosition(Mesh->GetBoneLocation(TEXT("foot_r")));
	TArray<FMoveSeg> Segs;
	float BestErr = BIG_NUMBER, From = StepBackFrom;
	// (from a pose far from the clip's, a photo pose, nothing is held and the clip blends in
	// from it: she may set off later in it, mid-step, for a shorter step back)
	const bool bHeld = FVector(RightNow - ClipFoot(BS, StepBackFrom, 1)).Size2D() <= 12.f;
	for (float F = 0.f; F <= (bHeld ? 0.8f : 1.35f); F += 0.05f)
	{
		Segs.Reset();
		Segs.Add({BS, F, A, -1, 0.3f});
		Segs.Add({BE, B, StepBackLeanAt, Foot, MoveBlend});
		const FVector Hold = RightNow - ClipFoot(BS, F, 1);
		const FVector Plan = MeshToWorld(MoveTravel(Segs) + ClipFoot(BE, StepBackLeanAt, 0) - ClipFoot(Down, LeanDownFromBack, 0)
			+ (Hold.Size2D() <= 12.f ? FVector(Hold.X, Hold.Y, 0.f) : FVector::ZeroVector), Char->GetActorRotation().Yaw);
		const float Err = FVector::Dist2D(Plan, To) + 0.5f * FMath::Abs(F - StepBackFrom);
		if (Err < BestErr)
		{
			BestErr = Err;
			From = F;
		}
	}
	Segs.Reset();
	Segs.Add({BS, From, A, -1, 0.3f});
	Segs.Add({BE, B, StepBackLeanAt, Foot, MoveBlend});
	LoadedLean.AddUnique(BS);
	LoadedLean.AddUnique(BE);
	LoadedLean.AddUnique(Down);
	ReturnPhase = 3;
	PhaseClock = 0.f;
	bRootMove = true;
	StepBackYaw = Char->GetActorRotation().Yaw;
	const float Secs = PlayRootMotionMove(Segs, nullptr, 0.f, 1.f);
	if (MoveMontage)
	{
		HoldFootNow(BS, From, 1, MoveMontage);
	}
	UE_LOG(LogTemp, Log, TEXT("Menu move: step back %.1f cm from %.2f s (planned %.1f cm off), %.1f deg off straight back, %.2f s to the lean"),
		To.Size(), From, BestErr, Off, Secs);
	return true;
}

void AVisexMenuDirector::TickStepBack()
{
	ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	UAnimInstance* Anim = GetPawnAnim();
	UAnimSequence* BE = WalkBackStop.Get();
	UAnimSequence* Down = LeanDown.Get();
	if (!Char || !Anim || !Down || (ReturnPhase == 3 && !BE))
	{
		EndStep();
		return;
	}
	const float Dt = GetWorld()->GetDeltaSeconds();
	FVector Need = StepHome - Char->GetActorLocation();
	Need.Z = 0.f;
	const float Yaw = Char->GetActorRotation().Yaw;
	if (ReturnPhase == 3)
	{
		// what the clips still have to cover, through the holds and the lean-down's: aimed at her
		// spot and stretched (a few %) to land on it
		FVector Rest = FVector::ZeroVector, Root = FVector::ZeroVector;
		for (int32 I = FMath::Max(0, MoveIndex); I < MoveSegs.Num(); ++I)
		{
			const FMoveSeg& S = MoveSegs[I];
			const float From = I == MoveIndex ? MoveClock : S.From;
			if (From < S.To)
			{
				Root += ClipRoot(S.Clip, From, S.To);
			}
			if (I > MoveIndex && S.Lock >= 0)
			{
				const FMoveSeg& P = MoveSegs[I - 1];
				Rest += ClipFoot(P.Clip, FMath::Fmod(P.To, P.Clip->GetPlayLength()), S.Lock) - ClipFoot(S.Clip, FMath::Fmod(S.From, S.Clip->GetPlayLength()), S.Lock);
			}
		}
		Rest += Root + ClipFoot(BE, StepBackLeanAt, 0) - ClipFoot(Down, LeanDownFromBack, 0);
		const FVector Will = MeshToWorld(Rest, Yaw), RootW = MeshToWorld(Root, Yaw);
		const float Want = Will.Size2D() > 12.f && Need.Size2D() > 12.f
			? Yaw + FMath::Clamp(FMath::FindDeltaAngleDegrees(Will.Rotation().Yaw, Need.Rotation().Yaw), -StepBackAngle, StepBackAngle) : HomeYaw;
		// (not while the clip blends in over her standing pose and its first foot is held: both
		// feet are down, so any turn or stretch there slides one)
		const FMoveSeg& Cur = MoveSegs[FMath::Clamp(MoveIndex, 0, MoveSegs.Num() - 1)];
		const int32 Pivot = ClipPlantedFoot(Cur.Clip, FMath::Fmod(MoveClock, Cur.Clip->GetPlayLength()));
		const bool bSteer = !bHolding && Pivot >= 0;
		TurnAboutPlanted(bSteer ? FMath::FixedTurn(Yaw, Want, 25.f * Dt) : Yaw, 0.f, Pivot);
		if (bSteer && RootW.Size2D() > 5.f)
		{
			Char->SetAnimRootMotionTranslationScale(FMath::Clamp(1.f + float(FVector::DotProduct(Need - Will, RootW.GetSafeNormal2D())) / RootW.Size2D(), 0.95f, 1.05f));
		}
		if (MoveAtEnd() || !IsMoving())
		{
			const float At = MoveMontage && Anim->Montage_IsPlaying(MoveMontage) ? Anim->Montage_GetPosition(MoveMontage) : StepBackLeanAt;
			UE_LOG(LogTemp, Log, TEXT("Menu move: stepped back at %.2f s, %.1f cm from her spot (%.1f planned by the lean-down)"),
				StepClock, Need.Size2D(), MeshToWorld(ClipFoot(BE, At, 0) - ClipFoot(Down, LeanDownFromBack, 0), Yaw).Size2D());
			StopMove();
			LeanWithHold(BE, At, LeanDownFromBack);
			ReturnPhase = 4;
			PhaseClock = 0.f;
		}
		return;
	}
	// sinking into the lean: the last of her facing, round her planted left foot, and the last
	// centimetre or so onto her spot, then the menu
	TurnAboutPlanted(FMath::FixedTurn(Yaw, HomeYaw, 25.f * Dt));
	if (!bHolding && Need.Size2D() > 0.05f)
	{
		Char->AddActorWorldOffset(Need.GetClampedToMaxSize(4.f * Dt));
	}
	if (PhaseClock > 0.9f || (FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw, HomeYaw)) < 0.3f && Need.Size2D() < 0.5f && !bHolding))
	{
		UE_LOG(LogTemp, Log, TEXT("Menu move: on her spot, %.1f cm and %.1f deg off, at %.2f s"), Need.Size2D(), FMath::FindDeltaAngleDegrees(Yaw, HomeYaw), StepClock);
		EndStep();
	}
}

FVector AVisexMenuDirector::SimMove(const TArray<FMoveSeg>& Segs, int32 Index, float AtTime, float Heading, float ArcLeft, float& EndYaw)
{
	// the clips' root (and its yaw) step by step, the loop (segment 0) curving by ArcLeft over
	// what's left of it, the capsule's jump at each held hand-off, and the lean-down's at the end
	const ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	const FTransform MeshRel = Char && Char->GetMesh() ? Char->GetMesh()->GetRelativeTransform() : FTransform::Identity;
	auto ToWorld = [&MeshRel](const FVector& V, float Y) { return FRotator(0.f, Y, 0.f).RotateVector(MeshRel.TransformVector(V)); };
	UAnimSequence* Down = LeanDown.Get();
	FVector P = FVector::ZeroVector;
	constexpr float H = 1.f / 60.f;
	for (int32 I = FMath::Max(0, Index); I < Segs.Num(); ++I)
	{
		const FMoveSeg& S = Segs[I];
		if (I > Index && S.Lock >= 0)
		{
			const FMoveSeg& Prev = Segs[I - 1];
			P += ToWorld(ClipFoot(Prev.Clip, FMath::Fmod(Prev.To, Prev.Clip->GetPlayLength()), S.Lock)
				- ClipFoot(S.Clip, FMath::Fmod(S.From, S.Clip->GetPlayLength()), S.Lock), Heading);
		}
		for (float T = I == Index ? AtTime : S.From; T < S.To - 1e-4f;)
		{
			const float Dt = FMath::Min(H, S.To - T);
			const float Y0 = ClipYaw(S.Clip, T);
			P += ToWorld(FRotator(0.f, -Y0, 0.f).RotateVector(ClipRoot(S.Clip, T, T + Dt)), Heading);
			Heading += ClipYaw(S.Clip, T + Dt) - Y0;
			if (I == 0 && ArcLeft != 0.f)
			{
				const float Step = ArcLeft * Dt / FMath::Max(Dt, S.To - T);
				Heading += Step;
				ArcLeft -= Step;
			}
			T += Dt;
		}
	}
	if (Down && Segs.Num())
	{
		const FMoveSeg& L = Segs.Last();
		P += ToWorld(ClipFoot(L.Clip, FMath::Fmod(L.To, L.Clip->GetPlayLength()), 0) - ClipFoot(Down, LeanDownFrom, 0), Heading);
	}
	P.Z = 0.f;
	EndYaw = Heading;
	return P;
}

bool AVisexMenuDirector::PlanRootArrival(const FVector& From, TArray<FMoveSeg>& Segs, float& Yaw, float& Arc)
{
	// in from off frame mid-loop, curving, into the walk-turn: she plants her left foot, pivots
	// round on it and sinks into the lean from it. The loop's length and its arc land her.
	UAnimSequence* Loop = WalkLoop.LoadSynchronous();
	UAnimSequence* Turn = WalkTurnIn.LoadSynchronous();
	UAnimSequence* Down = LeanDown.LoadSynchronous();
	Segs.Reset();
	int32 Foot = 0;
	float A = 0.f, B = 0.f;
	if (!RootMoves() || CVarMenuArrivalTurn.GetValueOnGameThread() == 0 || !Loop || !Turn || !Down || !FindCut(Loop, FVector2D(0.3f, Loop->GetPlayLength() - 0.4f), Turn, ArriveCutInto, Foot, A, B))
	{
		return false;
	}
	LoadedLean.AddUnique(Turn);
	const float Len = Loop->GetPlayLength();
	const float TurnYaw = ClipYaw(Turn, ArriveLeanAt) - ClipYaw(Turn, B);
	auto Make = [&](float T)
	{
		TArray<FMoveSeg> S;
		const float F = FMath::Fmod(A - T + 100.f * Len, Len);
		S.Add({Loop, F, F + T, -1, 0.f});
		S.Add({Turn, B, ArriveLeanAt, Foot, MoveBlend});
		return S;
	};
	FVector Need = StepHome - From;
	Need.Z = 0.f;
	float BestErr = BIG_NUMBER, BestT = 1.f, BestArc = 0.f;
	auto Try = [&](float Ar)
	{
		// the loop's length that covers the distance, for this arc; then how far round it misses
		float Lo = 0.2f, Hi = 30.f, EndYaw = 0.f;
		const float Y0 = HomeYaw - Ar - TurnYaw;
		for (int32 I = 0; I < 24; ++I)
		{
			const float Mid = 0.5f * (Lo + Hi);
			const TArray<FMoveSeg> S = Make(Mid);
			(SimMove(S, 0, S[0].From, Y0, Ar, EndYaw).Size2D() < Need.Size2D() ? Lo : Hi) = Mid;
		}
		const TArray<FMoveSeg> S = Make(0.5f * (Lo + Hi));
		const float Err = FVector::Dist2D(SimMove(S, 0, S[0].From, Y0, Ar, EndYaw), Need);
		if (Err < BestErr)
		{
			BestErr = Err;
			BestT = 0.5f * (Lo + Hi);
			BestArc = Ar;
		}
	};
	for (float Ar = -ArriveArcMax; Ar <= ArriveArcMax + 0.01f; Ar += 3.f)
	{
		Try(Ar);
	}
	for (float Ar = BestArc - 3.f; Ar <= BestArc + 3.01f; Ar += 0.25f)
	{
		Try(FMath::Clamp(Ar, -ArriveArcMax, ArriveArcMax));
	}
	Segs = Make(BestT);
	Arc = BestArc;
	Yaw = HomeYaw - Arc - TurnYaw;
	UE_LOG(LogTemp, Log, TEXT("Menu move: walk-turn in %.1f cm, loop %.2f s curving %.1f deg, turn %.1f deg, heading %.1f, lands %.1f cm off"),
		Need.Size2D(), BestT, Arc, TurnYaw, Yaw, BestErr);
	return BestErr < 25.f;
}

void AVisexMenuDirector::TickRootArrival(float DeltaSeconds)
{
	ACharacter* Char = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	UAnimInstance* Anim = GetPawnAnim();
	if (!Char || !Anim)
	{
		return;
	}
	const float Yaw = Char->GetActorRotation().Yaw;
	if (MoveAtEnd() || !IsMoving())
	{
		// planted on her left foot, round: the sink takes over from it, her last degrees eased in
		UAnimSequence* Turn = WalkTurnIn.Get();
		const float At = MoveMontage && Anim->Montage_IsPlaying(MoveMontage) ? Anim->Montage_GetPosition(MoveMontage) : ArriveLeanAt;
		UE_LOG(LogTemp, Log, TEXT("Menu move: walk-turn round at %.2f s, %.1f cm from her spot (%.1f planned by the lean-down), %.1f deg off"),
			StepClock, FVector::Dist2D(Char->GetActorLocation(), StepHome),
			MeshToWorld(ClipFoot(Turn, At, 0) - ClipFoot(LeanDown.Get(), LeanDownFrom, 0), Yaw).Size2D(), FMath::FindDeltaAngleDegrees(Yaw, HomeYaw));
		StopMove();
		LeanWithHold(Turn, At);
		ReturnPhase = 4;
		PhaseClock = 0.f;
		return;
	}
	if (MoveIndex != 0)
	{
		return;   // the walk-turn's own root yaw turns her
	}
	// the loop: its arc, at the pace planned, and the course corrected toward her spot (the
	// remaining path turned onto it, its length a few % longer or shorter)
	const FMoveSeg& S = MoveSegs[0];
	const float Rest = FMath::Max(0.f, S.To - MoveClock);
	const float ArcLeft = ArriveArc - ArriveArcDone;
	const float Step = Rest > 1e-3f ? ArcLeft * FMath::Clamp((MoveClock - ArriveClockWas) / Rest, 0.f, 1.f) : ArcLeft;
	ArriveClockWas = MoveClock;
	ArriveArcDone += Step;
	float EndYaw = 0.f;
	FVector Need = StepHome - Char->GetActorLocation();
	Need.Z = 0.f;
	const FVector Will = SimMove(MoveSegs, 0, MoveClock, Yaw + Step, ArriveArc - ArriveArcDone, EndYaw);
	float Fix = 0.f;
	if (Will.Size2D() > 40.f && Need.Size2D() > 40.f)
	{
		Fix = FMath::Clamp(FMath::FindDeltaAngleDegrees(Will.Rotation().Yaw, Need.Rotation().Yaw), -8.f * DeltaSeconds, 8.f * DeltaSeconds);
		Char->SetAnimRootMotionTranslationScale(FMath::Clamp(Need.Size2D() / Will.Size2D(), 0.97f, 1.03f));
	}
	TurnAboutPlanted(Yaw + Step + Fix);
}
