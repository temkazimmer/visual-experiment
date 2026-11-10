#include "VisexAnimInstance.h"

#include "Animation/AnimNodeBase.h"
#include "BonePose.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"

// Catwalk tuning. Live in the console (also during Play): e.g. `visex.Catwalk.HipRoll 9`.
static TAutoConsoleVariable<float> CVarCatwalk(TEXT("visex.Catwalk"), 1.f, TEXT("Catwalk layer on the walk: 0 off, 1 full"));
static TAutoConsoleVariable<float> CVarCatwalkHipRoll(TEXT("visex.Catwalk.HipRoll"), 6.f, TEXT("Degrees the swing-side hip drops"));
static TAutoConsoleVariable<float> CVarCatwalkHipYaw(TEXT("visex.Catwalk.HipYaw"), 7.f, TEXT("Degrees the hip turns forward with the leading leg"));
static TAutoConsoleVariable<float> CVarCatwalkHipShift(TEXT("visex.Catwalk.HipShift"), 2.f, TEXT("Centimetres the pelvis moves over the standing leg"));
static TAutoConsoleVariable<float> CVarCatwalkLine(TEXT("visex.Catwalk.Line"), 4.5f, TEXT("Degrees each leg angles inward, so the feet land near one line"));
static TAutoConsoleVariable<float> CVarCatwalkShoulders(TEXT("visex.Catwalk.Shoulders"), 1.7f, TEXT("How much the chest counters the hips (1 = equal and opposite)"));
static TAutoConsoleVariable<float> CVarCatwalkHead(TEXT("visex.Catwalk.Head"), 0.85f, TEXT("How steady the head stays against the chest's motion (0..1)"));
// from the Girl Walk&Run struts (Docs/Research, learning log 2026-11-10): spine 4 deg back, chest
// 4-5 frames behind the hips, arms ~40 cm of swing, head within ~1 cm of its line
static TAutoConsoleVariable<float> CVarCatwalkPosture(TEXT("visex.Catwalk.Posture"), 11.f, TEXT("Degrees the spine leans back while walking, head kept level (proud)"));
static TAutoConsoleVariable<float> CVarCatwalkChestLag(TEXT("visex.Catwalk.ChestLag"), 7.f, TEXT("How quickly the chest's counter-turn follows the hips (lower = later; 0 = no lag)"));
static TAutoConsoleVariable<float> CVarCatwalkArmSwing(TEXT("visex.Catwalk.ArmSwing"), 0.6f, TEXT("Share of the clip's fore-aft arm swing kept (0..1)"));
static TAutoConsoleVariable<float> CVarCatwalkHeadLine(TEXT("visex.Catwalk.HeadLine"), 0.9f, TEXT("How much the head's side-to-side sway is taken out (0..1)"));
static TAutoConsoleVariable<float> CVarCatwalkSprint(TEXT("visex.Catwalk.Sprint"), 0.35f, TEXT("Share of the layer kept when sprinting (0..1)"));

// Moving hold, from the Female Everyday idles (learning log 2026-11-10). Live: `visex.Hold.Breath 2`.
static TAutoConsoleVariable<float> CVarHold(TEXT("visex.Hold"), 1.f, TEXT("Moving-hold layer on held poses: 0 off, 1 full"));
static TAutoConsoleVariable<float> CVarHoldBreath(TEXT("visex.Hold.Breath"), 2.4f, TEXT("Degrees the chest opens back on each breath in"));
static TAutoConsoleVariable<float> CVarHoldBreathPeriod(TEXT("visex.Hold.BreathPeriod"), 4.f, TEXT("Seconds per breath"));
static TAutoConsoleVariable<float> CVarHoldCollar(TEXT("visex.Hold.Collar"), 4.f, TEXT("Degrees the collarbones lift on each breath in"));
static TAutoConsoleVariable<float> CVarHoldSway(TEXT("visex.Hold.Sway"), 1.2f, TEXT("Degrees the upper body drifts side to side and turns, slowly"));
static TAutoConsoleVariable<float> CVarHoldSwayPeriod(TEXT("visex.Hold.SwayPeriod"), 7.5f, TEXT("Seconds per drift"));
static TAutoConsoleVariable<float> CVarHoldLyingBreath(TEXT("visex.Hold.LyingBreath"), 1.6f, TEXT("Breath depth lying down, relative to upright (Lie Down Dozing: chest 3.7 cm vs 1-2 cm standing)"));
static TAutoConsoleVariable<float> CVarHoldPin(TEXT("visex.Hold.Pin"), 30.f, TEXT("A hand lower than this (cm above her feet) is resting on something: it stays put while the body breathes"));
static TAutoConsoleVariable<float> CVarHoldHead(TEXT("visex.Hold.Head"), 1.5f, TEXT("Degrees the head drifts on its own (turn and tilt)"));

void FVisexAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	if (const UVisexAnimInstance* Inst = Cast<UVisexAnimInstance>(InAnimInstance))
	{
		Profile = Inst->BodyProfile;
		bHasProfile = Inst->bReshapeBody;
		PhotoGait = Inst->PhotoGait;
		HoldTarget = FMath::Clamp(Inst->HoldAlpha, 0.f, 1.f) * CVarHold.GetValueOnGameThread();
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
		if (bHasProfile)
		{
			Reshape(Output.Pose);
		}
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
	const FQuat Counter = Turn(Left, -Fwd, ChestYaw * Tune.Shoulders) * Turn(Left, Up, ChestRoll * Tune.Shoulders * 0.5f);
	S.RotateCS(Chest, Counter);
	S.RotateCS(Neck, FQuat::Slerp(FQuat::Identity, Counter.Inverse(), Tune.Head));

	// proud: the spine carries the chest a little back, the neck brings the gaze level again
	const FQuat Back = Turn(Up, -Fwd, Tune.Posture * 0.5f * Gait);
	S.RotateCS(Spine2, Back);
	S.RotateCS(Chest, Back);
	S.RotateCS(Neck, (Back * Back).Inverse());

	// the head on a steady line: its sideways drift from its own slow average is leaned out
	const FCompactPoseBoneIndex Head = S.Find(TEXT("head"));
	if (Head.IsValid() && Tune.HeadLine > 0.f)
	{
		const FVector HeadAt = S.Component(Head).GetLocation(), From = S.Component(Spine1).GetLocation();
		HeadSide = FMath::FInterpTo(HeadSide, HeadAt | Left, Dt, 1.5f);
		const float Off = (HeadAt | Left) - HeadSide, Height = FMath::Max((HeadAt - From) | Up, 30.f);
		S.RotateCS(Spine1, Turn(Up, -Left, FMath::RadiansToDegrees(FMath::Atan2(Off, Height)) * Tune.HeadLine * Gait));
	}

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

	// put pinned hands back: bend the elbow to the right reach, aim the arm, restore the wrist
	for (const FPin& P : Pins)
	{
		if (P.W <= 0.f)
		{
			continue;
		}
		const FVector A = S.Component(P.Arm).GetLocation(), B = S.Component(P.Fore).GetLocation(), C = S.Component(P.Hand).GetLocation();
		const FVector Target = FMath::Lerp(C, P.Was.GetLocation(), P.W);
		const float La = (B - A).Size(), Lb = (C - B).Size(), Want = FMath::Clamp((Target - A).Size(), FMath::Abs(La - Lb) + 0.1f, La + Lb - 0.1f);
		const FVector N = ((B - A) ^ (C - B)).GetSafeNormal();
		if (!N.IsNearlyZero())
		{
			// the inner elbow angle now and the one that gives the wanted reach (law of cosines)
			const float Now = FMath::Acos(FMath::Clamp(((A - B).GetSafeNormal() | (C - B).GetSafeNormal()), -1.f, 1.f));
			const float Need = FMath::Acos(FMath::Clamp((La * La + Lb * Lb - Want * Want) / (2.f * La * Lb), -1.f, 1.f));
			S.RotateCS(P.Fore, FQuat(N, Now - Need));
		}
		const FVector C2 = S.Component(P.Hand).GetLocation();
		S.RotateCS(P.Arm, FQuat::FindBetweenNormals((C2 - A).GetSafeNormal(), (Target - A).GetSafeNormal()));
		const FQuat Now = S.Component(P.Hand).GetRotation();
		S.RotateCS(P.Hand, FQuat::Slerp(FQuat::Identity, P.Was.GetRotation() * Now.Inverse(), P.W));
	}
}
