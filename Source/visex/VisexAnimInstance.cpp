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
static TAutoConsoleVariable<float> CVarCatwalkShoulders(TEXT("visex.Catwalk.Shoulders"), 0.7f, TEXT("How much the chest counters the hips (0..1)"));
static TAutoConsoleVariable<float> CVarCatwalkHead(TEXT("visex.Catwalk.Head"), 0.85f, TEXT("How steady the head stays against the chest's motion (0..1)"));
static TAutoConsoleVariable<float> CVarCatwalkSprint(TEXT("visex.Catwalk.Sprint"), 0.35f, TEXT("Share of the layer kept when sprinting (0..1)"));

void FVisexAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	if (const UVisexAnimInstance* Inst = Cast<UVisexAnimInstance>(InAnimInstance))
	{
		Profile = Inst->BodyProfile;
		bHasProfile = Inst->bReshapeBody;
		PhotoGait = Inst->PhotoGait;
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
}

bool FVisexAnimProxy::Evaluate_WithRoot(FPoseContext& Output, FAnimNode_Base* InRootNode)
{
	EvaluateAnimationNode_WithRoot(Output, InRootNode);
	if (InRootNode == GetRootNode())
	{
		Catwalk(Output.Pose);
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

	// chest counters the hips; the head stays level and forward
	const FQuat Counter = Turn(Left, -Fwd, Yaw * Tune.Shoulders) * Turn(Left, Up, Roll * Tune.Shoulders * 0.5f);
	S.RotateCS(Chest, Counter);
	S.RotateCS(Neck, FQuat::Slerp(FQuat::Identity, Counter.Inverse(), Tune.Head));

	// feet toward one line: each leg angles inward from the hip, the foot stays flat
	const FQuat InL = Turn(-Up, -Left, Tune.Line * Gait), InR = Turn(-Up, Left, Tune.Line * Gait);
	S.RotateCS(ThighL, InL);
	S.RotateCS(FootL, InL.Inverse());
	S.RotateCS(ThighR, InR);
	S.RotateCS(FootR, InR.Inverse());
}
