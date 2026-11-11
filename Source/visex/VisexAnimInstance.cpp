#include "VisexAnimInstance.h"

#include "Animation/AnimNodeBase.h"
#include "BonePose.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"

// Catwalk tuning. Live in the console (also during Play): e.g. `visex.Catwalk.HipRoll 9`.
static TAutoConsoleVariable<float> CVarCatwalk(TEXT("visex.Catwalk"), 1.f, TEXT("Catwalk layer on the walk: 0 off, 1 full"));
static TAutoConsoleVariable<float> CVarCatwalkHipRoll(TEXT("visex.Catwalk.HipRoll"), 11.f, TEXT("Degrees the swing-side hip drops"));
static TAutoConsoleVariable<float> CVarCatwalkHipYaw(TEXT("visex.Catwalk.HipYaw"), 7.f, TEXT("Degrees the hip turns forward with the leading leg"));
static TAutoConsoleVariable<float> CVarCatwalkHipShift(TEXT("visex.Catwalk.HipShift"), 2.f, TEXT("Centimetres the pelvis moves over the standing leg"));
static TAutoConsoleVariable<float> CVarCatwalkLine(TEXT("visex.Catwalk.Line"), 3.5f, TEXT("Degrees each leg angles inward, so the feet land near one line"));
static TAutoConsoleVariable<float> CVarCatwalkShoulders(TEXT("visex.Catwalk.Shoulders"), 3.4f, TEXT("How much the chest counters the hips (1 = equal and opposite)"));
static TAutoConsoleVariable<float> CVarCatwalkHead(TEXT("visex.Catwalk.Head"), 1.f, TEXT("How much the head keeps the walk clip's own aim against the chest's motion (0..1)"));
static TAutoConsoleVariable<float> CVarCatwalkShoulderRoll(TEXT("visex.Catwalk.ShoulderRoll"), 0.4f, TEXT("How much the shoulders tilt against the hip drop (1 = equal and opposite)"));
// from the Girl Walk&Run struts (Docs/Research, learning log 2026-11-10): spine 4 deg back, chest
// 4-5 frames behind the hips, arms ~40 cm of swing, head within ~1 cm of its line
static TAutoConsoleVariable<float> CVarCatwalkPosture(TEXT("visex.Catwalk.Posture"), 11.f, TEXT("Degrees the spine leans back while walking, head kept level (proud)"));
static TAutoConsoleVariable<float> CVarCatwalkChestLag(TEXT("visex.Catwalk.ChestLag"), 4.5f, TEXT("How quickly the chest's counter-turn follows the hips (lower = later; 0 = no lag)"));
static TAutoConsoleVariable<float> CVarCatwalkArmSwing(TEXT("visex.Catwalk.ArmSwing"), 0.3f, TEXT("Share of the clip's fore-aft arm swing kept (0..1)"));
static TAutoConsoleVariable<float> CVarCatwalkHeadLine(TEXT("visex.Catwalk.HeadLine"), 1.0f, TEXT("How much of the head's side-to-side sway the neck takes out, back onto the walk clip's own line (0..1)"));
static TAutoConsoleVariable<float> CVarCatwalkClear(TEXT("visex.Catwalk.Clear"), 20.f, TEXT("Centimetres the thigh axes keep apart as the legs cross: the swinging thigh turns out a little (8 degrees at most, eased) to pass beside the other (0 off)"));
static TAutoConsoleVariable<float> CVarCatwalkSprint(TEXT("visex.Catwalk.Sprint"), 0.35f, TEXT("Share of the layer kept when sprinting (0..1)"));

// Moving hold, from the Female Everyday idles (learning log 2026-11-10). Live: `visex.Hold.Breath 2`.
static TAutoConsoleVariable<float> CVarLimits(TEXT("visex.Limits"), 1.f, TEXT("Human joint limits on every pose, applied last: 0 off, 1 on"));
static TAutoConsoleVariable<float> CVarHold(TEXT("visex.Hold"), 1.f, TEXT("Moving-hold layer on held poses: 0 off, 1 full"));
static TAutoConsoleVariable<float> CVarHoldBreath(TEXT("visex.Hold.Breath"), 2.4f, TEXT("Degrees the chest opens back on each breath in"));
static TAutoConsoleVariable<float> CVarHoldBreathPeriod(TEXT("visex.Hold.BreathPeriod"), 4.f, TEXT("Seconds per breath"));
static TAutoConsoleVariable<float> CVarHoldCollar(TEXT("visex.Hold.Collar"), 4.f, TEXT("Degrees the collarbones lift on each breath in"));
static TAutoConsoleVariable<float> CVarHoldSway(TEXT("visex.Hold.Sway"), 1.2f, TEXT("Degrees the upper body drifts side to side and turns, slowly"));
static TAutoConsoleVariable<float> CVarHoldSwayPeriod(TEXT("visex.Hold.SwayPeriod"), 7.5f, TEXT("Seconds per drift"));
static TAutoConsoleVariable<float> CVarHoldLyingBreath(TEXT("visex.Hold.LyingBreath"), 1.6f, TEXT("Breath depth lying down, relative to upright (Lie Down Dozing: chest 3.7 cm vs 1-2 cm standing)"));
static TAutoConsoleVariable<float> CVarHoldPin(TEXT("visex.Hold.Pin"), 30.f, TEXT("A hand lower than this (cm above her feet) is resting on something: it stays put while the body breathes"));
static TAutoConsoleVariable<float> CVarHoldHead(TEXT("visex.Hold.Head"), 1.5f, TEXT("Degrees the head drifts on its own (turn and tilt)"));

// Toes at push-off (see Toes()). Live: `visex.Toes.Big 0.6`.
static TAutoConsoleVariable<float> CVarToes(TEXT("visex.Toes"), 1.f, TEXT("Toe layer on the walk and run: 0 off, 1 full"));
static TAutoConsoleVariable<float> CVarToesBig(TEXT("visex.Toes.Big"), 0.45f, TEXT("Share of the ball's bend the big toe adds at its knuckle (it extends past the others)"));
static TAutoConsoleVariable<float> CVarToesLift(TEXT("visex.Toes.Lift"), 0.25f, TEXT("Share of the ball's bend the small toes add at their knuckles"));
static TAutoConsoleVariable<float> CVarToesGrip(TEXT("visex.Toes.Grip"), 0.3f, TEXT("Share of the ball's bend the small toes curl their tips down while on the ground (they grip)"));
static TAutoConsoleVariable<float> CVarToesPlant(TEXT("visex.Toes.Plant"), 0.85f, TEXT("While a toe is on the ground, how much of its knuckle's lift the middle joint takes back, so the pad stays down and the knuckle rises (0..1)"));
static TAutoConsoleVariable<float> CVarToesSpread(TEXT("visex.Toes.Spread"), 3.f, TEXT("Degrees the little toe fans out at full push-off (the others less)"));
static TAutoConsoleVariable<float> CVarToesFull(TEXT("visex.Toes.Full"), 25.f, TEXT("Degrees of ball bend that count as a full push-off"));

void FVisexAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	if (const UVisexAnimInstance* Inst = Cast<UVisexAnimInstance>(InAnimInstance))
	{
		Profile = Inst->BodyProfile;
		bHasProfile = Inst->bReshapeBody;
		PhotoGait = Inst->PhotoGait;
		HoldTarget = FMath::Clamp(Inst->HoldAlpha, 0.f, 1.f) * CVarHold.GetValueOnGameThread();
		HandFix[0] = Inst->HandFixL;
		HandFix[1] = Inst->HandFixR;
	}

	Dt = DeltaSeconds;
	GaitTarget = 0.f;
	if (const ACharacter* Character = Cast<ACharacter>(InAnimInstance->TryGetPawnOwner()))
	{
		const UCharacterMovementComponent* Move = Character->GetCharacterMovement();
		if (Move && !Move->IsFalling())
		{
			const float Speed = Character->GetVelocity().Size2D();
			const float In = FMath::SmoothStep(30.f, 150.f, Speed);
			const float Sprint = FMath::SmoothStep(260.f, 480.f, Speed);
			GaitTarget = In * FMath::Lerp(1.f, CVarCatwalkSprint.GetValueOnGameThread(), Sprint) * CVarCatwalk.GetValueOnGameThread();
		}
	}
	if (PhotoGait >= 0.f)
	{
		GaitTarget = PhotoGait * CVarCatwalk.GetValueOnGameThread(); // a walk clip played in place
	}

	// the toe layer runs whenever she walks or runs (any speed), not only under the catwalk
	ToesTarget = 0.f;
	if (const ACharacter* Character = Cast<ACharacter>(InAnimInstance->TryGetPawnOwner()))
	{
		const UCharacterMovementComponent* Move = Character->GetCharacterMovement();
		if (Move && !Move->IsFalling())
		{
			ToesTarget = FMath::SmoothStep(30.f, 150.f, float(Character->GetVelocity().Size2D()));
		}
	}
	if (PhotoGait >= 0.f)
	{
		ToesTarget = PhotoGait > 0.f ? 1.f : 0.f;
	}
	ToesTarget *= CVarToes.GetValueOnGameThread();
	ToesTune.Big = CVarToesBig.GetValueOnGameThread();
	ToesTune.Lift = CVarToesLift.GetValueOnGameThread();
	ToesTune.Grip = CVarToesGrip.GetValueOnGameThread();
	ToesTune.Spread = CVarToesSpread.GetValueOnGameThread();
	ToesTune.Plant = CVarToesPlant.GetValueOnGameThread();
	ToesTune.Full = FMath::Max(CVarToesFull.GetValueOnGameThread(), 1.f);
	Tune.HipRoll = CVarCatwalkHipRoll.GetValueOnGameThread();
	Tune.HipYaw = CVarCatwalkHipYaw.GetValueOnGameThread();
	Tune.HipShift = CVarCatwalkHipShift.GetValueOnGameThread();
	Tune.Line = CVarCatwalkLine.GetValueOnGameThread();
	Tune.Shoulders = CVarCatwalkShoulders.GetValueOnGameThread();
	Tune.Head = CVarCatwalkHead.GetValueOnGameThread();
	Tune.Posture = CVarCatwalkPosture.GetValueOnGameThread();
	Tune.ChestLag = CVarCatwalkChestLag.GetValueOnGameThread();
	Tune.ArmSwing = CVarCatwalkArmSwing.GetValueOnGameThread();
	Tune.HeadLine = CVarCatwalkHeadLine.GetValueOnGameThread();
	Tune.ShoulderRoll = CVarCatwalkShoulderRoll.GetValueOnGameThread();
	Tune.Clear = CVarCatwalkClear.GetValueOnGameThread();
	bLimits = CVarLimits.GetValueOnGameThread() > 0.5f;
	HoldTune.Breath = CVarHoldBreath.GetValueOnGameThread();
	HoldTune.BreathPeriod = FMath::Max(CVarHoldBreathPeriod.GetValueOnGameThread(), 0.5f);
	HoldTune.Collar = CVarHoldCollar.GetValueOnGameThread();
	HoldTune.Sway = CVarHoldSway.GetValueOnGameThread();
	HoldTune.SwayPeriod = FMath::Max(CVarHoldSwayPeriod.GetValueOnGameThread(), 0.5f);
	HoldTune.Head = CVarHoldHead.GetValueOnGameThread();
	HoldTune.Pin = CVarHoldPin.GetValueOnGameThread();
	HoldTune.LyingBreath = CVarHoldLyingBreath.GetValueOnGameThread();
}

bool FVisexAnimProxy::Evaluate_WithRoot(FPoseContext& Output, FAnimNode_Base* InRootNode)
{
	EvaluateAnimationNode_WithRoot(Output, InRootNode);
	if (InRootNode == GetRootNode())
	{
		Catwalk(Output.Pose);
		Alive(Output.Pose);
		Toes(Output.Pose);
		if (bHasProfile)
		{
			Reshape(Output.Pose);
		}
		Contact(Output.Pose); // hands onto the surface, measured on the reshaped body
		Limits(Output.Pose);  // nothing bends past human range of motion
		SteadyHead(Output.Pose); // after the limits, which clamp the spine but not the neck's counter-turn
	}
	return true;
}

namespace
{
	// mesh space: the mannequin faces +Y, X is to her side, Z is up
	struct FShaper
	{
		FCompactPose& Pose;
		const FBoneContainer& Bones;

		FCompactPoseBoneIndex Find(const TCHAR* Name) const
		{
			const int32 Mesh = Bones.GetPoseBoneIndexForBoneName(FName(Name));
			return Mesh == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Mesh));
		}

		FQuat RefRotation(FCompactPoseBoneIndex Index) const
		{
			FTransform T = Bones.GetRefPoseTransform(Index);
			for (FCompactPoseBoneIndex P = Bones.GetParentBoneIndex(Index); P.IsValid(); P = Bones.GetParentBoneIndex(P))
			{
				T = T * Bones.GetRefPoseTransform(P);
			}
			return T.GetRotation();
		}

		/** Body terms to bone-local scale. Limbs: X runs along the bone (length), Y and Z
		 *  get the girth. Torso: each local axis takes the term of the body axis it's
		 *  closest to in the reference pose (side = width, front = depth, up = length). */
		FVector LocalScale(FCompactPoseBoneIndex Index, float Length, float Width, float Depth, bool bLimb) const
		{
			if (bLimb)
			{
				return FVector(Length, Width, Width);
			}
			const FQuat R = RefRotation(Index);
			const FVector Axes[3] = {R.GetAxisX(), R.GetAxisY(), R.GetAxisZ()};
			const FVector Body[3] = {FVector::XAxisVector, FVector::YAxisVector, FVector::ZAxisVector};
			const float Terms[3] = {Width, Depth, Length};
			FVector Out(1.f);
			for (int32 a = 0; a < 3; ++a)
			{
				int32 Best = 0;
				for (int32 b = 1; b < 3; ++b)
				{
					if (FMath::Abs(Axes[a] | Body[b]) > FMath::Abs(Axes[a] | Body[Best])) { Best = b; }
				}
				Out[a] = Terms[Best];
			}
			return Out;
		}

		/** Scale one segment; its direct children (except twist bones, which belong to
		 *  the segment) get the inverse, so the change stays in this region. Children's
		 *  offsets still scale, so lengths and widths carry through to joint positions. */
		void Segment(const TCHAR* Name, float Length, float Width, float Depth, bool bLimb = false)
		{
			const FCompactPoseBoneIndex Index = Find(Name);
			if (!Index.IsValid() || (Length == 1.f && Width == 1.f && Depth == 1.f))
			{
				return;
			}
			const FVector S = LocalScale(Index, Length, Width, Depth, bLimb);
			Pose[Index].SetScale3D(Pose[Index].GetScale3D() * S);
			for (const FCompactPoseBoneIndex Child : Pose.ForEachBoneIndex())
			{
				if (Bones.GetParentBoneIndex(Child) == Index
					&& !Bones.GetReferenceSkeleton().GetBoneName(Bones.MakeMeshPoseIndex(Child).GetInt()).ToString().Contains(TEXT("twist")))
				{
					Pose[Child].SetScale3D(Pose[Child].GetScale3D() / S);
				}
			}
		}

		void Uniform(const TCHAR* Name, float U)
		{
			const FCompactPoseBoneIndex Index = Find(Name);
			if (Index.IsValid() && U != 1.f)
			{
				Pose[Index].SetScale3D(Pose[Index].GetScale3D() * U);
			}
		}

		FTransform Component(FCompactPoseBoneIndex Index) const
		{
			FTransform T = Pose[Index];
			for (FCompactPoseBoneIndex P = Bones.GetParentBoneIndex(Index); P.IsValid(); P = Bones.GetParentBoneIndex(P))
			{
				T = T * Pose[P];
			}
			return T;
		}

		FQuat ParentRotation(FCompactPoseBoneIndex Index) const
		{
			const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Index);
			return Parent.IsValid() ? Component(Parent).GetRotation() : FQuat::Identity;
		}

		/** Turn a bone by a mesh-space rotation, about its own joint. */
		void RotateCS(FCompactPoseBoneIndex Index, const FQuat& Delta)
		{
			const FQuat Parent = ParentRotation(Index);
			Pose[Index].SetRotation((Parent.Inverse() * Delta * Parent * Pose[Index].GetRotation()).GetNormalized());
		}

		void TranslateCS(FCompactPoseBoneIndex Index, const FVector& Delta)
		{
			Pose[Index].AddToTranslation(ParentRotation(Index).UnrotateVector(Delta));
		}

		/** Two-bone reach: bend the elbow for the distance, aim the upper arm, then give the
		 *  hand back its component-space rotation. Weight blends toward the target. */
		void Reach(FCompactPoseBoneIndex Arm, FCompactPoseBoneIndex Fore, FCompactPoseBoneIndex Hand, const FVector& Want, const FQuat& KeepRotation, float Weight)
		{
			const FVector A = Component(Arm).GetLocation(), B = Component(Fore).GetLocation(), C = Component(Hand).GetLocation();
			const FVector Target = FMath::Lerp(C, Want, Weight);
			const float La = (B - A).Size(), Lb = (C - B).Size();
			const float Dist = FMath::Clamp((Target - A).Size(), FMath::Abs(La - Lb) + 0.1f, La + Lb - 0.1f);
			const FVector N = ((B - A) ^ (C - B)).GetSafeNormal();
			if (!N.IsNearlyZero() && La > 1.f && Lb > 1.f)
			{
				const float Now = FMath::Acos(FMath::Clamp(((A - B).GetSafeNormal() | (C - B).GetSafeNormal()), -1.f, 1.f));
				const float Need = FMath::Acos(FMath::Clamp((La * La + Lb * Lb - Dist * Dist) / (2.f * La * Lb), -1.f, 1.f));
				RotateCS(Fore, FQuat(N, Now - Need));
			}
			const FVector C2 = Component(Hand).GetLocation();
			RotateCS(Arm, FQuat::FindBetweenNormals((C2 - A).GetSafeNormal(), (Target - A).GetSafeNormal()));
			const FQuat Now = Component(Hand).GetRotation();
			RotateCS(Hand, FQuat::Slerp(FQuat::Identity, KeepRotation * Now.Inverse(), Weight));
		}

		void Offset(const TCHAR* Name, float K)
		{
			const FCompactPoseBoneIndex Index = Find(Name);
			if (Index.IsValid() && K != 1.f)
			{
				Pose[Index].SetTranslation(Pose[Index].GetTranslation() * K);
			}
		}
	};
}

void FVisexAnimProxy::Reshape(FCompactPose& Pose) const
{
	FShaper S{Pose, Pose.GetBoneContainer()};
	const FVisexBodyProfile& P = Profile;
	const TCHAR* Sides[] = {TEXT("_l"), TEXT("_r")};
	auto Both = [&](const TCHAR* Base, TFunctionRef<void(const TCHAR*)> Fn)
	{
		for (const TCHAR* Side : Sides)
		{
			Fn(*FString::Printf(TEXT("%s%s"), Base, Side));
		}
	};

	// torso: depth moves less than width, so the body stays elegant at the extremes
	const float Chest = P.Get("Chest"), Waist = P.Get("Waist"), Hips = P.Get("Hips");
	S.Segment(TEXT("spine_04"), 1.f, Chest, 1.f + (Chest - 1.f) * 0.6f);
	S.Segment(TEXT("spine_05"), 1.f, Chest, 1.f + (Chest - 1.f) * 0.6f);
	S.Segment(TEXT("spine_01"), 1.f, Waist, 1.f + (Waist - 1.f) * 0.7f);
	S.Segment(TEXT("spine_02"), 1.f, Waist, 1.f + (Waist - 1.f) * 0.7f);
	S.Segment(TEXT("pelvis"), 1.f, Hips, 1.f + (Hips - 1.f) * 0.5f);

	// neck and head
	const float Neck = P.Get("Neck");
	S.Segment(TEXT("neck_01"), Neck, 1.f, 1.f, true);
	S.Segment(TEXT("neck_02"), Neck, 1.f, 1.f, true);
	S.Uniform(TEXT("head"), P.Get("Head"));

	// shoulders: the arms sit further out along the collarbone
	const float Shoulders = P.Get("Shoulders");
	Both(TEXT("upperarm"), [&](const TCHAR* B) { S.Offset(B, Shoulders); });

	// limbs: length along the bone, girth around it
	const float Legs = P.Get("Legs"), Thighs = P.Get("Thighs"), Calves = P.Get("Calves");
	const float ArmLength = P.Get("ArmLength"), Arms = P.Get("Arms");
	Both(TEXT("thigh"), [&](const TCHAR* B) { S.Segment(B, Legs, Thighs, Thighs, true); });
	Both(TEXT("calf"), [&](const TCHAR* B) { S.Segment(B, Legs, Calves, Calves, true); });
	Both(TEXT("upperarm"), [&](const TCHAR* B) { S.Segment(B, ArmLength, Arms, Arms, true); });
	Both(TEXT("lowerarm"), [&](const TCHAR* B) { S.Segment(B, ArmLength, Arms, Arms, true); });
	Both(TEXT("hand"), [&](const TCHAR* B) { S.Uniform(B, P.Get("Hands")); });
	Both(TEXT("foot"), [&](const TCHAR* B) { S.Uniform(B, P.Get("Feet")); });
}

/**
 * The catwalk layer. The step rhythm is read from the pose itself (which foot is ahead,
 * which is in the air), so it stays in sync with any walk clip, blend or play rate.
 * Rotations are built as "turn this direction into that one", which keeps the signs
 * readable: mesh space is X to her left, Y forward, Z up.
 */
void FVisexAnimProxy::Catwalk(FCompactPose& Pose)
{
	Gait = FMath::FInterpTo(Gait, GaitTarget, Dt, 4.f);
	bClipHead = false;
	if (Gait < 0.005f)
	{
		Step = Lift = 0.f;
		return;
	}

	FShaper S{Pose, Pose.GetBoneContainer()};
	const FCompactPoseBoneIndex Pelvis = S.Find(TEXT("pelvis")), Spine1 = S.Find(TEXT("spine_01")), Spine2 = S.Find(TEXT("spine_02")),
		Chest = S.Find(TEXT("spine_03")), Neck = S.Find(TEXT("neck_01")),
		ThighL = S.Find(TEXT("thigh_l")), ThighR = S.Find(TEXT("thigh_r")), FootL = S.Find(TEXT("foot_l")), FootR = S.Find(TEXT("foot_r"));
	for (const FCompactPoseBoneIndex Index : {Pelvis, Spine1, Spine2, Chest, Neck, ThighL, ThighR, FootL, FootR})
	{
		if (!Index.IsValid())
		{
			return;
		}
	}

	const FVector Up = FVector::ZAxisVector, Fwd = FVector::YAxisVector;
	const FVector Left(S.Component(ThighL).GetLocation().X >= S.Component(ThighR).GetLocation().X ? 1.f : -1.f, 0.f, 0.f);
	CatwalkLeft = Left;
	if (const FCompactPoseBoneIndex ClipHead = S.Find(TEXT("head")); ClipHead.IsValid())
	{
		const FTransform T = S.Component(ClipHead);
		ClipHeadAt = T.GetLocation();
		ClipHeadRot = T.GetRotation();
		bClipHead = true;
	}

	const FVector Feet = S.Component(FootL).GetLocation() - S.Component(FootR).GetLocation();
	Step = FMath::FInterpTo(Step, FMath::Clamp((Feet | Fwd) / 30.f, -1.f, 1.f), Dt, 12.f);
	Lift = FMath::FInterpTo(Lift, FMath::Clamp(Feet.Z / 8.f, -1.f, 1.f), Dt, 12.f);

	auto Turn = [](const FVector& From, const FVector& Toward, float Degrees)
	{
		const float A = FMath::DegreesToRadians(Degrees);
		return FQuat::FindBetweenNormals(From, From * FMath::Cos(A) + Toward * FMath::Sin(A));
	};

	// hips: the swing side drops, the leading side turns forward, the pelvis moves over the standing leg
	const float Roll = Tune.HipRoll * Lift * Gait, Yaw = Tune.HipYaw * Step * Gait;
	const FQuat Hip = Turn(Left, Fwd, Yaw) * Turn(Left, -Up, Roll);
	S.RotateCS(Pelvis, Hip);
	S.TranslateCS(Pelvis, -Left * (Tune.HipShift * Lift * Gait));

	// the legs keep their own line; the lower spine takes the hip motion back out in two steps
	S.RotateCS(ThighL, Hip.Inverse());
	S.RotateCS(ThighR, Hip.Inverse());
	const FQuat Half = FQuat::Slerp(FQuat::Identity, Hip.Inverse(), 0.5f);
	S.RotateCS(Spine1, Half);
	S.RotateCS(Spine2, Half);

	// chest counters the hips a few frames late (overlap); the head stays level and forward
	ChestStep = Tune.ChestLag > 0.f ? FMath::FInterpTo(ChestStep, Step, Dt, Tune.ChestLag) : Step;
	ChestLift = Tune.ChestLag > 0.f ? FMath::FInterpTo(ChestLift, Lift, Dt, Tune.ChestLag) : Lift;
	const float ChestYaw = Tune.HipYaw * ChestStep * Gait, ChestRoll = Tune.HipRoll * ChestLift * Gait;
	const FQuat Counter = Turn(Left, -Fwd, ChestYaw * Tune.Shoulders) * Turn(Left, Up, ChestRoll * Tune.ShoulderRoll);
	// spread over three vertebrae: on one it reached 16 deg of twist, past what a segment can do (9)
	const FQuat Third = FQuat::Slerp(FQuat::Identity, Counter, 1.f / 3.f);
	S.RotateCS(Spine2, Third);
	S.RotateCS(Chest, Third);
	S.RotateCS(S.Find(TEXT("spine_04")), Third);
	S.RotateCS(Neck, FQuat::Slerp(FQuat::Identity, Counter.Inverse(), Tune.Head));

	// proud: the spine carries the chest a little back, the neck brings the gaze level again
	const FQuat Back = Turn(Up, -Fwd, Tune.Posture * 0.5f * Gait);
	S.RotateCS(Spine2, Back);
	S.RotateCS(Chest, Back);
	S.RotateCS(Neck, (Back * Back).Inverse());

	// smaller, looser arms: the fore-aft angle of each upper arm moves toward hanging
	for (const TCHAR* Side : {TEXT("l"), TEXT("r")})
	{
		const FCompactPoseBoneIndex Arm = S.Find(*FString::Printf(TEXT("upperarm_%s"), Side));
		const FCompactPoseBoneIndex Hand = S.Find(*FString::Printf(TEXT("hand_%s"), Side));
		if (!Arm.IsValid() || !Hand.IsValid() || Tune.ArmSwing >= 1.f)
		{
			continue;
		}
		const FVector Dir = (S.Component(Hand).GetLocation() - S.Component(Arm).GetLocation()).GetSafeNormal();
		const float Along = FMath::Atan2(Dir | Fwd, -(Dir | Up));
		const float Keep = FMath::Lerp(1.f, Tune.ArmSwing, Gait);
		const float Ang = Along * Keep, Side2 = Dir | Left;
		const float Planar = FMath::Sqrt(FMath::Max(1.f - Side2 * Side2, 0.f));
		const FVector Want = (Fwd * FMath::Sin(Ang) - Up * FMath::Cos(Ang)) * Planar + Left * Side2;
		S.RotateCS(Arm, FQuat::FindBetweenNormals(Dir, Want.GetSafeNormal()));
	}

	// feet toward one line: each leg angles inward from the hip, the foot stays flat
	const FQuat InL = Turn(-Up, -Left, Tune.Line * Gait), InR = Turn(-Up, Left, Tune.Line * Gait);
	S.RotateCS(ThighL, InL);
	S.RotateCS(FootL, InL.Inverse());
	S.RotateCS(ThighR, InR);
	S.RotateCS(FootR, InR.Inverse());

	// the swinging thigh clears the standing one: the line and the hip roll bring the thighs
	// together as the legs pass, so the swinging thigh turns out a little, eased in over the
	// swing and out again at the landing, never more than 8 degrees. Only the thighs are
	// judged: real legs brush at the shins and nobody sees it, while a knee that flicks out
	// at the crossing is what everybody sees.
	const FCompactPoseBoneIndex Thighs[2] = {ThighL, ThighR}, Calves[2] = {S.Find(TEXT("calf_l")), S.Find(TEXT("calf_r"))};
	for (int32 i = 0; i < 2; ++i)
	{
		const int32 o = 1 - i;
		float Want = 0.f;
		const float Swing = i == 0 ? Lift : -Lift; // how far this foot is the one in the air
		if (Tune.Clear > 0.f && Calves[i].IsValid() && Calves[o].IsValid() && Swing > 0.1f)
		{
			const FVector A0 = S.Component(Thighs[i]).GetLocation(), A1 = S.Component(Calves[i]).GetLocation();
			const FVector B0 = S.Component(Thighs[o]).GetLocation(), B1 = S.Component(Calves[o]).GetLocation();
			FVector P, Q;
			FMath::SegmentDistToSegmentSafe(A0, A1, B0, B1, P, Q);
			const float Gap = float((P - Q).Size()), Lever = FMath::Max(float((P - A0).Size()), 15.f);
			Want = FMath::Min(FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp((Tune.Clear - Gap) / Lever, 0.f, 0.5f))), 8.f);
			Want *= FMath::SmoothStep(0.1f, 0.4f, Swing);
		}
		KneeOut[i] = FMath::FInterpTo(KneeOut[i], Want, Dt, Want > KneeOut[i] ? 10.f : 6.f);
		if (KneeOut[i] > 0.02f && Calves[i].IsValid())
		{
			const FVector Dir = (S.Component(Calves[i]).GetLocation() - S.Component(Thighs[i]).GetLocation()).GetSafeNormal();
			const FVector Side = i == 0 ? Left : -Left;
			const FVector Out = (Side - Dir * (Side | Dir)).GetSafeNormal();
			const FQuat Q = Turn(Dir, Out, KneeOut[i] * Gait);
			S.RotateCS(Thighs[i], Q);
			S.RotateCS(Calves[i], FQuat::Slerp(FQuat::Identity, Q.Inverse(), 0.5f)); // the shin follows half-way
		}
	}
}

/**
 * The moving hold. A held pose is one frame; this keeps her alive in it without moving
 * anything that touches the ground: only the chest (breath), the collarbones, a slow
 * drift of the upper spine and a small drift of the head. The pelvis and legs are left
 * alone, so floor contacts and foot IK hold. Two drift sines at unrelated periods, so
 * it never reads as a loop.
 */
void FVisexAnimProxy::Alive(FCompactPose& Pose)
{
	// fades in over about a second once the pose is struck, out quickly when she moves
	Hold = FMath::FInterpTo(Hold, HoldTarget, Dt, HoldTarget > Hold ? 1.5f : 6.f);
	if (Hold < 0.005f)
	{
		HoldClock = 0.f;
		return;
	}
	HoldClock += Dt;

	FShaper S{Pose, Pose.GetBoneContainer()};
	const FCompactPoseBoneIndex Spine1 = S.Find(TEXT("spine_01")), Spine3 = S.Find(TEXT("spine_03")), Spine4 = S.Find(TEXT("spine_04")),
		Neck = S.Find(TEXT("neck_01")), Head = S.Find(TEXT("head")), ThighL = S.Find(TEXT("thigh_l")), ThighR = S.Find(TEXT("thigh_r")),
		ClavL = S.Find(TEXT("clavicle_l")), ClavR = S.Find(TEXT("clavicle_r"));
	for (const FCompactPoseBoneIndex Index : {Spine1, Spine3, Spine4, Neck, Head, ThighL, ThighR})
	{
		if (!Index.IsValid())
		{
			return;
		}
	}
	auto Turn = [](const FVector& From, const FVector& Toward, float Degrees)
	{
		const float A = FMath::DegreesToRadians(Degrees);
		return FQuat::FindBetweenNormals(From, From * FMath::Cos(A) + Toward * FMath::Sin(A));
	};
	// her own frame, read from the pose, so it works lying down too: Up along the spine, Left across the hips
	const FVector Left = (S.Component(ThighL).GetLocation() - S.Component(ThighR).GetLocation()).GetSafeNormal();
	const FVector Along = (S.Component(Neck).GetLocation() - S.Component(Spine1).GetLocation()).GetSafeNormal();
	if (Left.IsNearlyZero() || Along.IsNearlyZero())
	{
		return;
	}
	const FVector Up = (Along - Left * (Along | Left)).GetSafeNormal(), Front = (Left ^ Up).GetSafeNormal();

	// hands resting on the floor or the body are pinned: remember where they are now
	struct FPin { FCompactPoseBoneIndex Arm{INDEX_NONE}, Fore{INDEX_NONE}, Hand{INDEX_NONE}; FTransform Was; float W = 0.f; };
	FPin Pins[2];
	const TCHAR* Sides[2] = {TEXT("l"), TEXT("r")};
	for (int32 i = 0; i < 2; ++i)
	{
		FPin& P = Pins[i];
		P.Arm = S.Find(*FString::Printf(TEXT("upperarm_%s"), Sides[i]));
		P.Fore = S.Find(*FString::Printf(TEXT("lowerarm_%s"), Sides[i]));
		P.Hand = S.Find(*FString::Printf(TEXT("hand_%s"), Sides[i]));
		if (P.Arm.IsValid() && P.Fore.IsValid() && P.Hand.IsValid())
		{
			P.Was = S.Component(P.Hand);
			P.W = 1.f - FMath::SmoothStep(HoldTune.Pin * 0.5f, HoldTune.Pin, (float)P.Was.GetLocation().Z);
		}
	}

	const float T = HoldClock, Tau = 2.f * PI;
	// breath: in is quicker than out, so the curve leans early (a little second harmonic)
	const float Ph = Tau * T / HoldTune.BreathPeriod;
	const float In = (0.5f - 0.5f * FMath::Cos(Ph) + 0.12f * FMath::Sin(2.f * Ph)) * Hold;
	// lying down, the body lets go and the breath shows more (the trunk's Up is then horizontal)
	const float Depth = FMath::Lerp(HoldTune.LyingBreath, 1.f, FMath::Abs(Up.Z));
	const FQuat Open = Turn(Up, -Front, HoldTune.Breath * Depth * In);
	S.RotateCS(Spine3, FQuat::Slerp(FQuat::Identity, Open, 0.5f));
	S.RotateCS(Spine4, FQuat::Slerp(FQuat::Identity, Open, 0.5f));
	S.RotateCS(Neck, Open.Inverse());
	if (ClavL.IsValid() && ClavR.IsValid())
	{
		S.RotateCS(ClavL, Turn(Left, Up, HoldTune.Collar * In));
		S.RotateCS(ClavR, Turn(-Left, Up, HoldTune.Collar * In));
	}

	// drift: the upper body leans and turns a little, slowly, the head mostly keeps its aim
	const float W = Tau * T / HoldTune.SwayPeriod;
	const float Side = (0.7f * FMath::Sin(W) + 0.3f * FMath::Sin(W * 2.37f + 1.1f)) * HoldTune.Sway * Hold;
	const float Twist = (0.6f * FMath::Sin(W * 0.71f + 2.0f) + 0.4f * FMath::Sin(W * 1.63f + 0.4f)) * HoldTune.Sway * Hold;
	const FQuat Drift = Turn(Up, Left, Side) * Turn(Front, Left, Twist * 0.7f);
	S.RotateCS(Spine1, Drift);
	S.RotateCS(Neck, FQuat::Slerp(FQuat::Identity, Drift.Inverse(), 0.7f));

	// the head's own small life: a slow turn and tilt, unrelated to the drift
	const float HeadTurn = (0.6f * FMath::Sin(Tau * T / 9.3f + 0.7f) + 0.4f * FMath::Sin(Tau * T / 4.1f + 2.3f)) * HoldTune.Head * Hold;
	const float HeadTilt = (0.7f * FMath::Sin(Tau * T / 6.7f + 1.9f) + 0.3f * FMath::Sin(Tau * T / 3.3f)) * HoldTune.Head * 0.6f * Hold;
	S.RotateCS(Head, Turn(Front, Left, HeadTurn) * Turn(Up, Left, HeadTilt));

	// put pinned hands back where they were
	for (const FPin& P : Pins)
	{
		if (P.W > 0.f)
		{
			S.Reach(P.Arm, P.Fore, P.Hand, P.Was.GetLocation(), P.Was.GetRotation(), P.W);
		}
	}
}

/**
 * Hands on surfaces. The pose wheel's probe measures, on her skinned mesh, how far each
 * hand's skin is above (or inside) whatever is under it, and asks for this move when the
 * hand is meant to rest there: into the floor or the column becomes on it, a hair above
 * becomes on it. The arm reaches, the wrist keeps its angle. Runs after the reshape.
 */
void FVisexAnimProxy::Contact(FCompactPose& Pose)
{
	if (HandFix[0].IsNearlyZero(0.05) && HandFix[1].IsNearlyZero(0.05))
	{
		return;
	}
	FShaper S{Pose, Pose.GetBoneContainer()};
	const TCHAR* Sides[2] = {TEXT("l"), TEXT("r")};
	for (int32 i = 0; i < 2; ++i)
	{
		if (HandFix[i].IsNearlyZero(0.05))
		{
			continue;
		}
		const FCompactPoseBoneIndex Arm = S.Find(*FString::Printf(TEXT("upperarm_%s"), Sides[i]));
		const FCompactPoseBoneIndex Fore = S.Find(*FString::Printf(TEXT("lowerarm_%s"), Sides[i]));
		const FCompactPoseBoneIndex Hand = S.Find(*FString::Printf(TEXT("hand_%s"), Sides[i]));
		if (Arm.IsValid() && Fore.IsValid() && Hand.IsValid())
		{
			const FTransform Was = S.Component(Hand);
			S.Reach(Arm, Fore, Hand, Was.GetLocation() + HandFix[i], Was.GetRotation(), 1.f);
		}
	}
}

/**
 * Human joint limits, applied last so no pose, reach, IK or layer can bend her past them.
 * Each joint's turn away from the reference pose is split into twist about the bone (X) and a
 * swing, and the swing into bend (about the local Z) and side (about Y). Each of the three is
 * held inside its own asymmetric range with a soft shoulder over the last 10 degrees, so a pose
 * that pushes too far settles instead of snapping. The ranges are the clinical goniometry
 * norms (AAOS, as tabulated by Norkin & White and Physiopedia); the spine's totals are spread
 * over its bones. visex.Limits 0 switches it off; LimitExcess / LimitReport on the instance
 * say what asked to go past.
 */
namespace
{
	float SoftMax(float X, float Max, float S = 10.f)
	{
		return X <= Max - S ? X : Max - S + S * (1.f - FMath::Exp(-(X - (Max - S)) / S));
	}
	float SoftClamp(float X, float Min, float Max)
	{
		return -SoftMax(-SoftMax(X, Max), -Min);
	}
	struct FJointLimit
	{
		const TCHAR* Bone;
		float BendMin, BendMax;   // about the local Z: + and - as the table says per joint
		float SideMin, SideMax;   // about the local Y
		float TwistMin, TwistMax; // about the bone's own axis, X
	};
	// the reference pose already bends these; limits are on the joint, so they shift by that much
	constexpr float KNEE_REF = 5.f, ELBOW_REF = 39.f; // measured on SK_Mannequin's reference pose
	// Signs were measured on this skeleton (a +15 degree local turn, where the limb's end went):
	//   thigh  -Z flexion / +Z extension, +Y adduction / -Y abduction, +X inward / -X outward rotation
	//   calf   +Z flexion, Y valgus-varus, +X inward / -X outward tibial rotation
	//   foot   +Z plantar / -Z dorsiflexion, +Y toe in / -Y toe out, X inversion-eversion
	//   upperarm +Z flexion / -Z extension, +Y adduction / -Y abduction, +X outward / -X inward
	//   lowerarm +Z flexion, Y carrying angle, X forearm turn (shared with the hand)
	//   hand   +Z extension / -Z flexion, +Y radial / -Y ulnar deviation, X forearm turn
	//   neck, spine +Z flexion / -Z extension, Y side bend, X turn
	// Left and right bones mirror, so one table serves both sides.
	const FJointLimit JointLimits[] = {
		// Clinical norms (Physiopedia, Norkin & White, AAOS) on anatomical axes, widened where the
		// mannequin's bone axes aren't those: the 0.5-99.5% envelope of 157 mocap clips on this
		// skeleton (Tools scratch envelope.py, 2026-11-10) plus a few degrees. Real people made
		// those clips, so a number the mocap reaches is a number the joint can do on this rig.
		// hip: flexion 120, extension 30, abduction 45, adduction 30 (45 with the hip flexed, as in crossed legs),
		// rotation 45 each way (60 here: the mermaid sit folds both legs to one side, a flexible model's range)
		{TEXT("thigh_l"), -120.f, 30.f, -45.f, 45.f, -60.f, 60.f}, {TEXT("thigh_r"), -120.f, 30.f, -45.f, 45.f, -60.f, 60.f},
		// knee: flexion 135, hyperextension 5; varus-valgus 5 (mocap 9), tibial rotation 30 out / 10 in (mocap 16)
		{TEXT("calf_l"), -KNEE_REF - 5.f, 135.f - KNEE_REF, -12.f, 5.f, -30.f, 20.f}, {TEXT("calf_r"), -KNEE_REF - 5.f, 135.f - KNEE_REF, -12.f, 5.f, -30.f, 20.f},
		// ankle: plantar 50, dorsi 20 (mocap 28), forefoot in 20 / out 10 (mocap 13), inversion 35 / eversion 15 (mocap 22)
		{TEXT("foot_l"), -30.f, 50.f, -15.f, 25.f, -25.f, 38.f}, {TEXT("foot_r"), -30.f, 50.f, -15.f, 25.f, -25.f, 38.f},
		// shoulder: flexion 180, extension 60, adduction 45, abduction 180, rotation out 90 / in 70 (mocap 75)
		{TEXT("upperarm_l"), -60.f, 180.f, -180.f, 45.f, -80.f, 90.f}, {TEXT("upperarm_r"), -60.f, 180.f, -180.f, 45.f, -80.f, 90.f},
		// elbow: flexion 150, hyperextension 5; the bone's side axis isn't the carrying angle (mocap -27..17); forearm turn 80 each way
		{TEXT("lowerarm_l"), -ELBOW_REF - 5.f, 150.f - ELBOW_REF, -30.f, 20.f, -80.f, 80.f}, {TEXT("lowerarm_r"), -ELBOW_REF - 5.f, 150.f - ELBOW_REF, -30.f, 20.f, -80.f, 80.f},
		// wrist: flexion 80, extension 70 (mocap 74); deviation reads wide on this axis (mocap -39..51); the hand carries the
		// forearm's 160 deg of turn, from 35 of supination to 135 of pronation off the reference (mocap -31..134)
		{TEXT("hand_l"), -80.f, 75.f, -40.f, 50.f, -35.f, 135.f}, {TEXT("hand_r"), -80.f, 75.f, -40.f, 50.f, -35.f, 135.f},
		// neck: flexion 50, extension 60, side 45, rotation 80, over two bones (mocap neck_01 bends to 37)
		{TEXT("neck_01"), -30.f, 40.f, -25.f, 25.f, -40.f, 40.f}, {TEXT("head"), -30.f, 25.f, -23.f, 23.f, -40.f, 40.f},
		// spine: flexion 80, extension 35, side 35, rotation 45 over five bones; the mocap puts most of it low
		{TEXT("spine_01"), -15.f, 30.f, -12.f, 12.f, -13.f, 13.f}, {TEXT("spine_02"), -8.f, 20.f, -10.f, 10.f, -13.f, 13.f},
		{TEXT("spine_03"), -8.f, 20.f, -10.f, 10.f, -13.f, 13.f}, {TEXT("spine_04"), -8.f, 20.f, -10.f, 10.f, -13.f, 13.f},
		{TEXT("spine_05"), -6.f, 12.f, -6.f, 6.f, -10.f, 10.f},
	};
}

void FVisexAnimProxy::Limits(FCompactPose& Pose)
{
	LimitWorst = 0.f;
	LimitWorstBone = NAME_None;
	LimitReportBuf.Reset();
	if (!bLimits)
	{
		return;
	}
	const FBoneContainer& Bones = Pose.GetBoneContainer();
	for (const FJointLimit& J : JointLimits)
	{
		const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(J.Bone);
		if (MeshIndex == INDEX_NONE)
		{
			continue;
		}
		const FCompactPoseBoneIndex Index = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
		if (!Index.IsValid())
		{
			continue;
		}
		FTransform& T = Pose[Index];
		const FQuat Ref = Bones.GetRefPoseTransform(Index).GetRotation();
		FQuat Delta = Ref.Inverse() * T.GetRotation();
		Delta.Normalize();
		if (Delta.W < 0.f)
		{
			Delta = FQuat(-Delta.X, -Delta.Y, -Delta.Z, -Delta.W);
		}
		FQuat Swing, Twist;
		Delta.ToSwingTwist(FVector::XAxisVector, Swing, Twist);
		// The wrist hinges in the forearm's turned frame: on this rig the hand bone carries the
		// pronation, so its bend and side are read after the twist, not before (a hand pronated
		// 100 deg and extended 60 otherwise reads as 100 of 'side').
		const bool bHand = FCString::Strcmp(J.Bone, TEXT("hand_l")) == 0 || FCString::Strcmp(J.Bone, TEXT("hand_r")) == 0;
		if (bHand)
		{
			Swing = Twist.Inverse() * Delta;
			Swing.Normalize();
		}
		float TwistDeg = FMath::RadiansToDegrees(2.f * FMath::Atan2(Twist.X, Twist.W));
		if (TwistDeg > 180.f) { TwistDeg -= 360.f; } else if (TwistDeg < -180.f) { TwistDeg += 360.f; }
		if (Swing.W < 0.f)
		{
			Swing = FQuat(-Swing.X, -Swing.Y, -Swing.Z, -Swing.W);
		}
		FVector Axis; float Rad;
		Swing.ToAxisAndAngle(Axis, Rad);
		const float SwingDeg = FMath::RadiansToDegrees(Rad);
		float Bend = SwingDeg * float(Axis.Z), Side = SwingDeg * float(Axis.Y);
		const float NewBend = SoftClamp(Bend, J.BendMin, J.BendMax);
		const float NewSide = SoftClamp(Side, J.SideMin, J.SideMax);
		const float NewTwist = SoftClamp(TwistDeg, J.TwistMin, J.TwistMax);
		LimitReportBuf += FString::Printf(TEXT("%s %.0f/%.0f/%.0f "), J.Bone, Bend, Side, TwistDeg);
		const float Excess = FMath::Max3(FMath::Abs(Bend - NewBend), FMath::Abs(Side - NewSide), FMath::Abs(TwistDeg - NewTwist));
		if (Excess > LimitWorst)
		{
			LimitWorst = Excess;
			LimitWorstBone = J.Bone;
		}
		if (Excess < 0.02f)
		{
			continue;
		}
		const float NewAng = FMath::Sqrt(NewBend * NewBend + NewSide * NewSide);
		const FQuat NewSwing = NewAng > 0.01f ? FQuat(FVector(0.f, NewSide, NewBend) / NewAng, FMath::DegreesToRadians(NewAng)) : FQuat::Identity;
		const FQuat NewTwistQ(FVector::XAxisVector, FMath::DegreesToRadians(NewTwist));
		T.SetRotation(Ref * (bHand ? NewTwistQ * NewSwing : NewSwing * NewTwistQ));
	}
}

/**
 * The head rides the walk, it doesn't wobble on it. The catwalk drops the hips and tilts the
 * shoulders against them; the joint limits then clamp part of that tilt in the spine, but not
 * the neck's counter-turn, so the head was thrown side to side ~12 cm a stride (the walk clip's
 * own head moves ~2). Last of all, with the body settled: the neck bends to bring the head back
 * onto the clip's own line (sideways only), then the head takes back the clip's own aim.
 */
void FVisexAnimProxy::SteadyHead(FCompactPose& Pose)
{
	if (!bClipHead || Gait < 0.005f)
	{
		return;
	}
	FShaper S{Pose, Pose.GetBoneContainer()};
	const FCompactPoseBoneIndex Neck1 = S.Find(TEXT("neck_01")), Neck2 = S.Find(TEXT("neck_02")), Head = S.Find(TEXT("head"));
	if (!Neck1.IsValid() || !Head.IsValid())
	{
		return;
	}

	// sideways: each neck bone takes its share of the way back, at most 8 degrees a bone
	const FCompactPoseBoneIndex Necks[2] = {Neck1, Neck2};
	const int32 NumNecks = Neck2.IsValid() ? 2 : 1;
	const double Start = S.Component(Head).GetLocation() | CatwalkLeft;
	const double Line = FMath::Lerp(Start, ClipHeadAt | CatwalkLeft, double(FMath::Clamp(Tune.HeadLine, 0.f, 1.f) * Gait));
	for (int32 n = 0; n < NumNecks; ++n)
	{
		const FVector Pivot = S.Component(Necks[n]).GetLocation(), At = S.Component(Head).GetLocation();
		const float Off = float((At | CatwalkLeft) - Line) / float(NumNecks - n);
		const FVector From = At - Pivot;
		const float Len = float(From.Size());
		if (Len < 1.f || FMath::Abs(Off) < 0.01f)
		{
			continue;
		}
		const FVector To = From - CatwalkLeft * FMath::Clamp(Off, -Len * 0.14f, Len * 0.14f); // ~8 degrees
		S.RotateCS(Necks[n], FQuat::FindBetweenVectors(From, To));
	}

	// aim: the head looks where the clip's head looks
	const FQuat Now = S.Component(Head).GetRotation();
	S.RotateCS(Head, FQuat::Slerp(FQuat::Identity, ClipHeadRot * Now.Inverse(), FMath::Clamp(Tune.Head, 0.f, 1.f) * Gait));
}

void FVisexAnimProxy::PostUpdate(UAnimInstance* InAnimInstance) const
{
	FAnimInstanceProxy::PostUpdate(InAnimInstance);
	if (UVisexAnimInstance* Visex = Cast<UVisexAnimInstance>(InAnimInstance))
	{
		Visex->LimitExcess = LimitWorst;
		Visex->LimitBone = LimitWorstBone;
		Visex->LimitReport = LimitReportBuf;
	}
}

/**
 * Toes at push-off. The walk and run clips bend only the ball, so the five toes moved as one
 * block. Real toes don't: as the heel rises the knuckles extend, the big toe's most (it carries
 * the push), while the pads stay on the ground and the small toes grip; the toes fan a little.
 * Once the foot is up they stay extended a moment, then settle. A toe counts as on the ground
 * when its middle joint is within a few cm of the mesh origin (her floor). Driven per foot by how far the clip bends the toes up from the
 * metatarsals past their rest, so it keeps time with any clip, blend or play rate. Quick in,
 * slower out, so the toes settle a beat after the foot leaves the ground. Standing, posing and
 * the menu's toe play are untouched (the layer only runs while she moves).
 */
void FVisexAnimProxy::Toes(FCompactPose& Pose)
{
	ToesOn = FMath::FInterpTo(ToesOn, ToesTarget, Dt, 4.f);
	if (ToesOn < 0.005f)
	{
		ToesPush[0] = ToesPush[1] = 0.f;
		return;
	}

	FShaper S{Pose, Pose.GetBoneContainer()};
	const FBoneContainer& Bones = Pose.GetBoneContainer();
	auto RefComponent = [&Bones](FCompactPoseBoneIndex Index)
	{
		FTransform T = Bones.GetRefPoseTransform(Index);
		for (FCompactPoseBoneIndex P = Bones.GetParentBoneIndex(Index); P.IsValid(); P = Bones.GetParentBoneIndex(P))
		{
			T = T * Bones.GetRefPoseTransform(P);
		}
		return T;
	};
	// a direction turned toward another, by degrees: the sign reads as written
	auto Turn = [](const FVector& From, const FVector& Toward, float Degrees)
	{
		const float A = FMath::DegreesToRadians(Degrees);
		return FQuat::FindBetweenNormals(From, (From * FMath::Cos(A) + Toward * FMath::Sin(A)).GetSafeNormal());
	};
	auto Across = [](const FVector& V, const FVector& Dir) { return (V - Dir * (V | Dir)).GetSafeNormal(); };

	const FCompactPoseBoneIndex ThighL = S.Find(TEXT("thigh_l")), ThighR = S.Find(TEXT("thigh_r"));
	if (!ThighL.IsValid() || !ThighR.IsValid())
	{
		return;
	}
	const FVector Left(S.Component(ThighL).GetLocation().X >= S.Component(ThighR).GetLocation().X ? 1.f : -1.f, 0.f, 0.f);

	static const TCHAR* Names[5] = {TEXT("bigtoe"), TEXT("indextoe"), TEXT("middletoe"), TEXT("ringtoe"), TEXT("littletoe")};
	static const float Small[5] = {0.f, 1.f, 0.9f, 0.8f, 0.7f};  // each small toe's share of Lift and Grip
	static const float Fan[5] = {-0.3f, 0.f, 0.35f, 0.7f, 1.f};  // share of Spread, + = toward the outside of the foot
	static const TCHAR* Sides[2] = {TEXT("l"), TEXT("r")};

	for (int32 Side = 0; Side < 2; ++Side)
	{
		auto Bone = [&S, Side](const TCHAR* Name) { return S.Find(*FString::Printf(TEXT("%s_%s"), Name, Sides[Side])); };
		const FCompactPoseBoneIndex Foot = Bone(TEXT("foot")), Ball = Bone(TEXT("ball")), Mid1 = Bone(TEXT("middletoe_01")), Mid2 = Bone(TEXT("middletoe_02"));
		if (!Foot.IsValid() || !Ball.IsValid() || !Mid1.IsValid() || !Mid2.IsValid())
		{
			continue;
		}

		// how far the toes are bent up from the metatarsals (foot -> ball), now and at rest
		const FTransform RF = RefComponent(Foot);
		const FVector UpInFoot = RF.GetRotation().UnrotateVector(FVector::ZAxisVector);
		auto Bend = [&](const FTransform& F, const FTransform& B, const FTransform& T1, const FTransform& T2)
		{
			const FVector Meta = (B.GetLocation() - F.GetLocation()).GetSafeNormal();
			const FVector Toe = (T2.GetLocation() - T1.GetLocation()).GetSafeNormal();
			const FVector Up = Across(F.GetRotation().RotateVector(UpInFoot), Meta);
			return FMath::RadiansToDegrees(FMath::Atan2(Toe | Up, Toe | Meta));
		};
		const float Now = Bend(S.Component(Foot), S.Component(Ball), S.Component(Mid1), S.Component(Mid2));
		const float Rest = Bend(RF, RefComponent(Ball), RefComponent(Mid1), RefComponent(Mid2));
		const float Want = FMath::Clamp(Now - Rest, 0.f, ToesTune.Full * 1.4f);
		ToesPush[Side] = FMath::FInterpTo(ToesPush[Side], Want, Dt, Want > ToesPush[Side] ? 14.f : 6.f);

		const float A = ToesPush[Side] * ToesOn;
		if (A < 0.05f)
		{
			continue;
		}
		const float Frac = FMath::Clamp(A / ToesTune.Full, 0.f, 1.f);
		const FVector FootUp = S.Component(Foot).GetRotation().RotateVector(UpInFoot);
		const FVector Out = Side == 0 ? Left : -Left;

		for (int32 k = 0; k < 5; ++k)
		{
			const FCompactPoseBoneIndex B1 = Bone(*FString::Printf(TEXT("%s_01"), Names[k])), B2 = Bone(*FString::Printf(TEXT("%s_02"), Names[k]));
			if (!B1.IsValid() || !B2.IsValid())
			{
				continue;
			}
			const float Ground = 1.f - FMath::SmoothStep(1.5f, 4.f, float(S.Component(B2).GetLocation().Z));
			const float Knuckle = k == 0 ? ToesTune.Big * A : ToesTune.Lift * Small[k] * A;
			const float Tip = -(Knuckle * ToesTune.Plant + (k == 0 ? 0.f : ToesTune.Grip * Small[k] * A)) * Ground;

			// knuckle: up, and fanned out a little
			const FVector Dir = (S.Component(B2).GetLocation() - S.Component(B1).GetLocation()).GetSafeNormal();
			S.RotateCS(B1, Turn(Dir, Across(Out, Dir), ToesTune.Spread * Fan[k] * Frac) * Turn(Dir, Across(FootUp, Dir), Knuckle));

			// middle joint: on the ground, the pad back down (and the small toes grip); its direction is the knuckle's, turned with it
			if (Tip != 0.f)
			{
				const FVector Dir2 = (S.Component(B2).GetLocation() - S.Component(B1).GetLocation()).GetSafeNormal();
				S.RotateCS(B2, Turn(Dir2, Across(FootUp, Dir2), Tip));
			}
		}
	}
}
