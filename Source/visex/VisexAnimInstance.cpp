#include "VisexAnimInstance.h"

#include "Animation/AnimNodeBase.h"
#include "BonePose.h"

void FVisexAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	if (const UVisexAnimInstance* Inst = Cast<UVisexAnimInstance>(InAnimInstance))
	{
		Profile = Inst->BodyProfile;
		bHasProfile = Inst->bReshapeBody;
	}
}

bool FVisexAnimProxy::Evaluate_WithRoot(FPoseContext& Output, FAnimNode_Base* InRootNode)
{
	EvaluateAnimationNode_WithRoot(Output, InRootNode);
	if (bHasProfile && InRootNode == GetRootNode())
	{
		Reshape(Output.Pose);
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
