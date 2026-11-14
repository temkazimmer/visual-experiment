#include "VisexAnimInstance.h"

#include "Animation/AnimNodeBase.h"
#include "BonePose.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "visex.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"

// Catwalk tuning. Live in the console (also during Play): e.g. `visex.Catwalk.HipRoll 9`.
// off: it lifted the lean (5.5/6.6 cm sole gap); the mesh drops by LegDelta instead (VisexBodyComponent)
// and the plant pass, last of all, puts the soles on the sand
static TAutoConsoleVariable<int32> CVarKeepFeet(TEXT("visex.Body.KeepFeet"), 0, TEXT("Old Legs/Feet reshape pass that kept her feet where the foot IK planted them: 0 off (default), 1 on"));
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
static TAutoConsoleVariable<float> CVarPlant(TEXT("visex.Plant"), 1.f, TEXT("Plant pass, last of all: planted soles onto the visible sand. 0 off, 1 on"));
static TAutoConsoleVariable<float> CVarPlantClear(TEXT("visex.Plant.Clear"), 0.2f, TEXT("Centimetres the plant pass leaves between her lowest sole vertex and the sand"));
static TAutoConsoleVariable<float> CVarPin(TEXT("visex.Pin"), 1.f, TEXT("Pin pass: 0 off; 1 a foot the graph holds still stays put under the layers (catwalk, reshape, limits); 2 also a planted heel or ball keeps its spot in the world"));
static TAutoConsoleVariable<float> CVarPinStill(TEXT("visex.Pin.Still"), 12.f, TEXT("Pin: the graph's heel or ball slower than this (cm/s) is planted"));
static TAutoConsoleVariable<float> CVarPinMoving(TEXT("visex.Pin.Moving"), 40.f, TEXT("Pin: both faster than this (cm/s) is a swing"));
static TAutoConsoleVariable<int32> CVarPinLog(TEXT("visex.Pin.Log"), 0, TEXT("1 logs each stance: how far the graph's and the final heel or ball slid"));
static TAutoConsoleVariable<float> CVarPinMax(TEXT("visex.Pin.Max"), 6.f, TEXT("Pin 2: the most (cm) a planted heel or ball is held off where the graph puts it"));
static TAutoConsoleVariable<int32> CVarPlantLog(TEXT("visex.Plant.Log"), 0, TEXT("1 logs the plant pass about twice a second"));
static TAutoConsoleVariable<int32> CVarReshape(TEXT("visex.Reshape"), 1, TEXT("Body reshape layer (bone lengths, KeepFeet): 0 off, for measuring"));
static TAutoConsoleVariable<int32> CVarSteadyHead(TEXT("visex.SteadyHead"), 1, TEXT("SteadyHead layer: 0 off, for measuring"));
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
	PinOn = FMath::Clamp(CVarPin.GetValueOnGameThread(), 0.f, 2.f);
	PlantGround(InAnimInstance);
}

bool FVisexAnimProxy::Evaluate_WithRoot(FPoseContext& Output, FAnimNode_Base* InRootNode)
{
	EvaluateAnimationNode_WithRoot(Output, InRootNode);
	if (InRootNode == GetRootNode())
	{
		PinFrom(Output.Pose);
		Catwalk(Output.Pose);
		Alive(Output.Pose);
		Toes(Output.Pose);
		if (bHasProfile && CVarReshape.GetValueOnAnyThread() != 0)
		{
			Reshape(Output.Pose);
		}
		auto FootZ = [&Output]()
		{
			const FBoneContainer& B = Output.Pose.GetBoneContainer();
			const int32 M = B.GetPoseBoneIndexForBoneName(FName(TEXT("foot_l")));
			if (M == INDEX_NONE) { return 0.f; }
			FCompactPoseBoneIndex I = B.MakeCompactPoseIndex(FMeshPoseBoneIndex(M));
			FTransform T = Output.Pose[I];
			for (FCompactPoseBoneIndex P = B.GetParentBoneIndex(I); P.IsValid(); P = B.GetParentBoneIndex(P)) { T = T * Output.Pose[P]; }
			return float(T.GetLocation().Z);
		};
		const bool bTrace = CVarKeepFeet.GetValueOnAnyThread() > 2;
		const float Z0 = bTrace ? FootZ() : 0.f;
		Contact(Output.Pose); // hands onto the surface, measured on the reshaped body
		const float Z1 = bTrace ? FootZ() : 0.f;
		Limits(Output.Pose);  // nothing bends past human range of motion
		const float Z2 = bTrace ? FootZ() : 0.f;
		if (CVarSteadyHead.GetValueOnAnyThread() != 0) { SteadyHead(Output.Pose); } // after the limits, which clamp the spine but not the neck's counter-turn
		Pin(Output.Pose);     // the planted feet back where the graph keeps them
		Plant(Output.Pose);   // last: the soles onto the sand
		static int32 Every = 0;
		if (bTrace && (Every++ % 60) == 0)
		{
			UE_LOG(LogTemp, Log, TEXT("KeepFeet trace: foot_l Z after reshape %.2f, contact %.2f, limits %.2f, head %.2f"), Z0, Z1, Z2, FootZ());
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

		/** Component space the way the engine fills it, root down (child * parent's component
		 *  space): with the reshape's non-uniform scales, composing child-first put the feet up
		 *  to 5 cm from where she is drawn. */
		FTransform Component(FCompactPoseBoneIndex Index) const
		{
			TArray<FCompactPoseBoneIndex, TInlineAllocator<32>> Chain;
			for (FCompactPoseBoneIndex P = Index; P.IsValid(); P = Bones.GetParentBoneIndex(P))
			{
				Chain.Add(P);
			}
			FTransform T = FTransform::Identity;
			for (int32 i = Chain.Num() - 1; i >= 0; --i)
			{
				T = Pose[Chain[i]] * T;
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

namespace
{
	/** A leg as the graph left it, before the reshape: its foot, and how far it reached. */
	struct FLegWas
	{
		FTransform Foot;
		float Reach = 1.f;   // hip-to-ankle over thigh + calf
	};

	float LegLength(const FShaper& S, const FCompactPoseBoneIndex (&Leg)[3])
	{
		const FVector A = S.Component(Leg[0]).GetLocation(), B = S.Component(Leg[1]).GetLocation(), C = S.Component(Leg[2]).GetLocation();
		return FMath::Max(1.f, float((B - A).Size() + (C - B).Size()));
	}

	/** Legs and Feet reshape after the graph's foot IK (Foot Placement, Leg IK), so shorter legs
	 *  lifted her planted soles off the sand by the change (1.7 cm on the default body). Instead
	 *  the feet stay where the graph put them: each ankle moves by the foot's own scale about the
	 *  sole point carrying her weight (heel or ball), the pelvis moves along the legs so each
	 *  reaches as far as it did (straight down for a standing leg), and the legs reach back. Her
	 *  mesh origin sits on the floor (VisexBodyComponent). */
	void KeepFeet(FShaper& S, const FCompactPoseBoneIndex (&Leg)[2][3], const FLegWas (&Was)[2], float Feet)
	{
		const FCompactPoseBoneIndex Pelvis = S.Find(TEXT("pelvis"));
		const FCompactPoseBoneIndex Balls[2] = {S.Find(TEXT("ball_l")), S.Find(TEXT("ball_r"))};
		for (int32 F = 0; F < 2; ++F)
		{
			if (!Pelvis.IsValid() || !Leg[F][0].IsValid() || !Leg[F][1].IsValid() || !Leg[F][2].IsValid() || !Balls[F].IsValid()) { return; }
		}
		auto RefZ = [&S](FCompactPoseBoneIndex I)
		{
			FTransform T = S.Bones.GetRefPoseTransform(I);
			for (FCompactPoseBoneIndex P = S.Bones.GetParentBoneIndex(I); P.IsValid(); P = S.Bones.GetParentBoneIndex(P)) { T = T * S.Bones.GetRefPoseTransform(P); }
			return float(T.GetLocation().Z);
		};
		const float Ankle = RefZ(Leg[0][2]), Ball = RefZ(Balls[0]);
		FVector Target[2];
		for (int32 F = 0; F < 2; ++F)
		{
			const FVector AnkleWas = Was[F].Foot.GetLocation();
			const FVector BallWas = Was[F].Foot.TransformPosition(S.Pose[Balls[F]].GetTranslation());
			const FVector Heel = AnkleWas - FVector(0.f, 0.f, Ankle), Toe = BallWas - FVector(0.f, 0.f, Ball);
			const FVector Pivot = Heel.Z <= Toe.Z ? Heel : Toe;
			Target[F] = AnkleWas + (1.f - Feet) * (Pivot - AnkleWas);
		}
		// the pelvis along the legs, the straighter leg (the one carrying her) counting most
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			FVector Move = FVector::ZeroVector;
			float Weights = 0.f;
			for (int32 F = 0; F < 2; ++F)
			{
				const FVector Hip = S.Component(Leg[F][0]).GetLocation();
				const float Dist = float((Target[F] - Hip).Size());
				const float W = FMath::Pow(FMath::Clamp(Was[F].Reach, 0.05f, 1.f), 8.f);
				Move += W * (Target[F] - Hip).GetSafeNormal() * (Dist - Was[F].Reach * LegLength(S, Leg[F]));
				Weights += W;
			}
			S.TranslateCS(Pelvis, Move / FMath::Max(Weights, 1e-4f));
		}
		for (int32 F = 0; F < 2; ++F)
		{
			S.Reach(Leg[F][0], Leg[F][1], Leg[F][2], Target[F], Was[F].Foot.GetRotation(), 1.f);
		}
		static int32 Every = 0;
		if (CVarKeepFeet.GetValueOnAnyThread() > 1 && (Every++ % 60) == 0)
		{
			UE_LOG(LogTemp, Log, TEXT("KeepFeet: feet %.3f; L ankle target %s now %s (reach %.3f), R target %s now %s (reach %.3f); pelvis %s"), Feet,
				*Target[0].ToCompactString(), *S.Component(Leg[0][2]).GetLocation().ToCompactString(), Was[0].Reach,
				*Target[1].ToCompactString(), *S.Component(Leg[1][2]).GetLocation().ToCompactString(), Was[1].Reach, *S.Component(Pelvis).GetLocation().ToCompactString());
		}
	}
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

	// the feet as the graph left them (Foot Placement and Leg IK planted them on the ground,
	// before this reshape): Legs and Feet must not lift them off it (KeepFeet, below)
	const float Legs = P.Get("Legs"), Feet = P.Get("Feet");
	const bool bKeepFeet = CVarKeepFeet.GetValueOnAnyThread() != 0 && (Legs != 1.f || Feet != 1.f);
	FLegWas LegsWere[2];
	const FCompactPoseBoneIndex LegBones[2][3] = {{S.Find(TEXT("thigh_l")), S.Find(TEXT("calf_l")), S.Find(TEXT("foot_l"))},
		{S.Find(TEXT("thigh_r")), S.Find(TEXT("calf_r")), S.Find(TEXT("foot_r"))}};
	for (int32 F = 0; F < 2; ++F)
	{
		if (bKeepFeet && LegBones[F][0].IsValid() && LegBones[F][1].IsValid() && LegBones[F][2].IsValid())
		{
			LegsWere[F].Foot = S.Component(LegBones[F][2]);
			LegsWere[F].Reach = float((LegsWere[F].Foot.GetLocation() - S.Component(LegBones[F][0]).GetLocation()).Size()) / LegLength(S, LegBones[F]);
		}
	}

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
	const float Thighs = P.Get("Thighs"), Calves = P.Get("Calves");
	const float ArmLength = P.Get("ArmLength"), Arms = P.Get("Arms");
	Both(TEXT("thigh"), [&](const TCHAR* B) { S.Segment(B, Legs, Thighs, Thighs, true); });
	Both(TEXT("calf"), [&](const TCHAR* B) { S.Segment(B, Legs, Calves, Calves, true); });
	Both(TEXT("upperarm"), [&](const TCHAR* B) { S.Segment(B, ArmLength, Arms, Arms, true); });
	Both(TEXT("lowerarm"), [&](const TCHAR* B) { S.Segment(B, ArmLength, Arms, Arms, true); });
	Both(TEXT("hand"), [&](const TCHAR* B) { S.Uniform(B, P.Get("Hands")); });
	Both(TEXT("foot"), [&](const TCHAR* B) { S.Uniform(B, Feet); });
	if (bKeepFeet)
	{
		KeepFeet(S, LegBones, LegsWere, Feet);
	}
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
		Visex->SoleGapL = OutGap[0];
		Visex->SoleGapR = OutGap[1];
		Visex->PlantedL = OutPlant[0];
		Visex->PlantedR = OutPlant[1];
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

/**
 * The plant pass, after every other layer: each planted foot's lowest sole point goes onto the
 * visible sand (FootTrace) with a two-bone leg solve, the pelvis dropping only when a leg can't
 * reach. It owns the vertical only, so Foot Placement's planting and the walk's own feet stay.
 */
void FVisexAnimProxy::PlantGround(UAnimInstance* Instance)
{
	const UVisexAnimInstance* Inst = Cast<UVisexAnimInstance>(Instance);
	PlantTarget = Inst ? FMath::Clamp(Inst->PlantAlpha, 0.f, 1.f) * CVarPlant.GetValueOnGameThread() : 0.f;
	const USkeletalMeshComponent* Mesh = Instance->GetSkelMeshComponent();
	const ACharacter* Her = Cast<ACharacter>(Instance->TryGetPawnOwner());
	bGround[0] = bGround[1] = false;
	if (PlantTarget < 0.005f || !Mesh || !Mesh->GetWorld()) { return; }
	MeasureSole(Mesh);
	if (CVarPlantLog.GetValueOnGameThread() != 0 && (GFrameCounter % 30) == 1)
	{
		UE_LOG(LogTemp, Log, TEXT("Plant mesh: frame %llu foot_l Z %.2f foot_r Z %.2f, %s"), GFrameCounter, Mesh->GetSocketTransform(TEXT("foot_l"), RTS_Component).GetLocation().Z,
			Mesh->GetSocketTransform(TEXT("foot_r"), RTS_Component).GetLocation().Z, *Mesh->GetName());
	}
	const UCharacterMovementComponent* Move = Her ? Her->GetCharacterMovement() : nullptr;
	PlantMoving = Her ? FMath::SmoothStep(10.f, 60.f, float(Her->GetVelocity().Size2D())) : 0.f;
	if (Move && Move->IsFalling()) { PlantTarget = 0.f; return; }
	// straight down at each foot's lowest sole point (last frame), onto the visible sand only
	const FTransform X = Mesh->GetComponentTransform();
	FCollisionQueryParams Q(TEXT("VisexPlant"), true, Mesh->GetOwner());
	static const FName Bones[2] = {TEXT("foot_l"), TEXT("foot_r")};
	for (int32 F = 0; F < 2; ++F)
	{
		const FVector At = SoleAt[F].IsZero() ? Mesh->GetSocketLocation(Bones[F]) : X.TransformPosition(SoleAt[F]);
		FHitResult Hit;
		if (Mesh->GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, 40.f), At - FVector(0.f, 0.f, 80.f), VisexFootTraceChannel, Q))
		{
			bGround[F] = true;
			GroundZ[F] = float(X.InverseTransformPosition(Hit.ImpactPoint).Z);
		}
	}
}

void FVisexAnimProxy::Plant(FCompactPose& Pose)
{
	if (PlantTarget < 0.005f)
	{
		PlantFix[0] = PlantFix[1] = PelvisFix = 0.f;
		OutGap[0] = OutGap[1] = 1e4f;
		OutPlant[0] = OutPlant[1] = 0.f;
		return;
	}
	FShaper S{Pose, Pose.GetBoneContainer()};
	const FCompactPoseBoneIndex Pelvis = S.Find(TEXT("pelvis"));
	const FCompactPoseBoneIndex Leg[2][3] = {{S.Find(TEXT("thigh_l")), S.Find(TEXT("calf_l")), S.Find(TEXT("foot_l"))},
		{S.Find(TEXT("thigh_r")), S.Find(TEXT("calf_r")), S.Find(TEXT("foot_r"))}};
	if (!Pelvis.IsValid()) { return; }
	// each foot's lowest sole point: its foot bone as the graph and the layers left it, plus the
	// lowest skinned sole vertex's depth under that bone last frame (MeasureSole)
	float Want[2] = {0.f, 0.f}, Before[2] = {1e4f, 1e4f};
	for (int32 F = 0; F < 2; ++F)
	{
		OutPlant[F] = 0.f;
		if (!bSole[F] || !bGround[F] || !Leg[F][0].IsValid() || !Leg[F][1].IsValid() || !Leg[F][2].IsValid())
		{
			if (CVarPlantLog.GetValueOnAnyThread() != 0 && (GFrameCounter % 30) == 0) { UE_LOG(LogTemp, Log, TEXT("Plant %d: skipped, sole %d ground %d"), F, bSole[F], bGround[F]); }
			continue;
		}
		const FVector Low = S.Component(Leg[F][2]).GetLocation() + FVector(0.f, 0.f, SoleUnder[F]);
		// planted: standing, any foot within reach of the sand; walking, only a foot that is down
		const float Gap = float(Low.Z) - GroundZ[F] - CVarPlantClear.GetValueOnAnyThread();   // the sole rests just on the sand
		const float Stand = 1.f - FMath::SmoothStep(6.f, 12.f, Gap), Walk = 1.f - FMath::SmoothStep(1.f, 4.f, Gap);
		const float W = (Gap < 0.f ? 1.f : FMath::Lerp(Stand, Walk, PlantMoving)) * PlantTarget * (1.f - Hold);
		Want[F] = -Gap * W;
		Before[F] = Gap + CVarPlantClear.GetValueOnAnyThread();
		OutPlant[F] = W;
		if (CVarPlantLog.GetValueOnAnyThread() != 0 && (GFrameCounter % 30) == 0)
		{
			UE_LOG(LogTemp, Log, TEXT("Plant %d: target %.2f hold %.2f moving %.2f sole under %.2f, low %s ground %.2f gap %.2f w %.2f fix %.2f pelvis %.2f"),
				F, PlantTarget, Hold, PlantMoving, SoleUnder[F], *Low.ToCompactString(), GroundZ[F], Gap, W, PlantFix[F], PelvisFix);
		}
	}
	// eased, so nothing pops; then the ankles' targets, and how far the pelvis must drop for a leg to reach
	const float Ease = Dt > 0.f ? FMath::Min(1.f, Dt * 20.f) : 0.f;
	FVector Target[2];
	FQuat Keep[2];
	float Drop = 0.f;
	for (int32 F = 0; F < 2; ++F)
	{
		PlantFix[F] = FMath::Lerp(PlantFix[F], Want[F], Ease);
		OutGap[F] = Before[F] < 1e3f ? Before[F] + PlantFix[F] : 1e4f;   // where the sole ends up
		if (!Leg[F][2].IsValid()) { continue; }
		const FTransform Foot = S.Component(Leg[F][2]);
		Target[F] = Foot.GetLocation() + FVector(0.f, 0.f, PlantFix[F]);
		Keep[F] = Foot.GetRotation();
		const FVector Hip = S.Component(Leg[F][0]).GetLocation();
		const float Reach = 0.995f * (float((S.Component(Leg[F][1]).GetLocation() - Hip).Size()) + float((Foot.GetLocation() - S.Component(Leg[F][1]).GetLocation()).Size()));
		const float R = float(FVector::Dist2D(Hip, Target[F])), Up = float(Hip.Z - Target[F].Z);
		if (R < Reach && FMath::Abs(PlantFix[F]) > 0.01f) { Drop = FMath::Max(Drop, Up - FMath::Sqrt(Reach * Reach - R * R)); }
	}
	PelvisFix = FMath::Lerp(PelvisFix, Drop, Ease);
	if (PelvisFix > 0.01f) { S.TranslateCS(Pelvis, FVector(0.f, 0.f, -PelvisFix)); }
	for (int32 F = 0; F < 2; ++F)
	{
		if (Leg[F][0].IsValid() && Leg[F][1].IsValid() && Leg[F][2].IsValid() && (FMath::Abs(PlantFix[F]) > 0.01f || PelvisFix > 0.01f))
		{
			S.Reach(Leg[F][0], Leg[F][1], Leg[F][2], Target[F], Keep[F], 1.f);
		}
		if (CVarPlantLog.GetValueOnAnyThread() != 0 && (GFrameCounter % 30) == 0 && Leg[F][2].IsValid())
		{
			UE_LOG(LogTemp, Log, TEXT("Plant %d out: frame %llu foot Z %.2f target Z %.2f"), F, GFrameCounter, S.Component(Leg[F][2]).GetLocation().Z, Target[F].Z);
		}
	}
}


void FVisexAnimProxy::MeasureSole(const USkeletalMeshComponent* Mesh)
{
	// the feet meter's sole (VisexFootMeter): the lowest skinned vertex below the foot bone,
	// within 16 cm of the heel-ball middle, in component space
	bSole[0] = bSole[1] = false;
	FSkeletalMeshRenderData* RD = Mesh->GetSkeletalMeshRenderData();
	if (!RD || RD->LODRenderData.Num() == 0) { return; }
	const FSkeletalMeshLODRenderData& LOD = RD->LODRenderData[0];
	if (LOD.StaticVertexBuffers.PositionVertexBuffer.GetNumVertices() == 0 || !LOD.StaticVertexBuffers.PositionVertexBuffer.GetVertexData()) { return; }
	TArray<FMatrix44f> RefToLocal;
	Mesh->CacheRefToLocalMatrices(RefToLocal);
	TArray<FVector3f> P;
	USkinnedMeshComponent::ComputeSkinnedPositions(const_cast<USkeletalMeshComponent*>(Mesh), P, RefToLocal, LOD, LOD.SkinWeightVertexBuffer);
	static const FName Heels[2] = {TEXT("foot_l"), TEXT("foot_r")}, Balls[2] = {TEXT("ball_l"), TEXT("ball_r")};
	const float Near = 16.f / FMath::Max(0.1f, float(Mesh->GetComponentScale().Z));
	for (int32 F = 0; F < 2; ++F)
	{
		const FVector H = Mesh->GetSocketTransform(Heels[F], RTS_Component).GetLocation(), B = Mesh->GetSocketTransform(Balls[F], RTS_Component).GetLocation();
		const FVector Mid = (H + B) * 0.5f;
		FVector Low(0.f, 0.f, 1e9);
		for (const FVector3f& V3 : P)
		{
			const FVector V(V3);
			if (V.Z < Low.Z && V.Z < H.Z && FVector::Dist2D(V, Mid) < Near) { Low = V; }
		}
		if (Low.Z < 1e8) { bSole[F] = true; SoleUnder[F] = float(Low.Z - H.Z); SoleAt[F] = Low; }
	}
}


/**
 * The pin. The graph's feet come in planted (the clip, or Foot Placement's lock); the layers
 * after it (catwalk hips and leg line, reshape, limits) move the legs, and with them a foot that
 * should stay put. Pin 1: the layers' change to each foot (offset and turn, world) is frozen
 * while the graph's foot stands still, so the leg reaches for the graph's foot plus that change.
 * Pin 2 also holds the contact point itself: a planted heel or ball keeps its world spot even
 * when the graph slides it (root motion against the capsule, the director's curves), the foot
 * lock ALS uses, by the plant pass's ground. In the swing it all follows the layers again.
 */
void FVisexAnimProxy::PreEvaluateAnimation(UAnimInstance* InAnimInstance)
{
	FAnimInstanceProxy::PreEvaluateAnimation(InAnimInstance);
	if (const USkeletalMeshComponent* Mesh = InAnimInstance ? InAnimInstance->GetSkelMeshComponent() : nullptr) { EvalX = Mesh->GetComponentTransform(); }
}

void FVisexAnimProxy::PinFrom(const FCompactPose& Pose)
{
	bPinFrom = false;
	FShaper S{const_cast<FCompactPose&>(Pose), Pose.GetBoneContainer()};
	const FCompactPoseBoneIndex Foot[2] = {S.Find(TEXT("foot_l")), S.Find(TEXT("foot_r"))};
	const FCompactPoseBoneIndex Ball[2] = {S.Find(TEXT("ball_l")), S.Find(TEXT("ball_r"))};
	if (!Foot[0].IsValid() || !Foot[1].IsValid() || !Ball[0].IsValid() || !Ball[1].IsValid()) { return; }
	if (PinRefZ[0] < 0.f)
	{
		const FBoneContainer& Bones = Pose.GetBoneContainer();
		auto RefZ = [&Bones](FCompactPoseBoneIndex I) { FTransform T = Bones.GetRefPoseTransform(I); for (FCompactPoseBoneIndex P = Bones.GetParentBoneIndex(I); P.IsValid(); P = Bones.GetParentBoneIndex(P)) { T = T * Bones.GetRefPoseTransform(P); } return float(T.GetLocation().Z); };
		PinRefZ[0] = RefZ(Foot[0]);
		PinRefZ[1] = RefZ(Ball[0]);
	}
	const FTransform& X = EvalX;
	const float Still = CVarPinStill.GetValueOnAnyThread(), Moving = FMath::Max(CVarPinMoving.GetValueOnAnyThread(), Still + 1.f);
	for (int32 F = 0; F < 2; ++F)
	{
		PinGraph[F] = S.Component(Foot[F]);
		PinBall[F] = S.Component(Ball[F]).GetLocation();
		const FVector H = X.TransformPosition(PinGraph[F].GetLocation()), B = X.TransformPosition(PinBall[F]);
		float W = 0.f;
		if (bPinLast && Dt > 1e-4f)
		{
			PinSpeed[F][0] = float((H - PinLastHeel[F]).Size()) / Dt;
			PinSpeed[F][1] = float((B - PinLastBall[F]).Size()) / Dt;
			W = 1.f - FMath::SmoothStep(Still, Moving, FMath::Min(PinSpeed[F][0], PinSpeed[F][1]));
			if (FMath::Min(PinSpeed[F][0], PinSpeed[F][1]) > 2000.f) { W = 0.f; }   // a teleport or a cut
		}
		PinW[F] = W;
		PinLastHeel[F] = H;
		PinLastBall[F] = B;
	}
	bPinLast = true;
	bPinFrom = true;
}

void FVisexAnimProxy::Pin(FCompactPose& Pose)
{
	if (!bPinFrom) { return; }
	FShaper S{Pose, Pose.GetBoneContainer()};
	const FCompactPoseBoneIndex Leg[2][3] = {{S.Find(TEXT("thigh_l")), S.Find(TEXT("calf_l")), S.Find(TEXT("foot_l"))},
		{S.Find(TEXT("thigh_r")), S.Find(TEXT("calf_r")), S.Find(TEXT("foot_r"))}};
	const FQuat Xq = EvalX.GetRotation();
	const int32 Mode = PinOn > 1.5f ? 2 : PinOn > 0.5f ? 1 : 0;
	const float Still = CVarPinStill.GetValueOnAnyThread(), Moving = FMath::Max(CVarPinMoving.GetValueOnAnyThread(), Still + 1.f);
	auto FollowOf = [this](float W) { return W <= 0.f ? 1.f : 1.f - FMath::Pow(FMath::Clamp(W, 0.f, 1.f), FMath::Max(Dt * 60.f, 0.01f)); };
	for (int32 F = 0; F < 2; ++F)
	{
		if (!Leg[F][0].IsValid() || !Leg[F][1].IsValid() || !Leg[F][2].IsValid()) { continue; }
		const FTransform Post = S.Component(Leg[F][2]);
		const FVector Move = Xq.RotateVector(Post.GetLocation() - PinGraph[F].GetLocation());
		const FQuat Turn = Xq * (Post.GetRotation() * PinGraph[F].GetRotation().Inverse()) * Xq.Inverse();
		// pin 1: the layers' change, frozen while the graph's foot stands still
		const float Follow = Mode == 0 ? 1.f : FollowOf(PinW[F]);
		PinDelta[F] = FMath::Lerp(PinDelta[F], Move, Follow);
		PinTurn[F] = FQuat::Slerp(PinTurn[F], Turn, Follow).GetNormalized();
		FVector Want = Post.GetLocation();
		FQuat WantRot = Post.GetRotation();
		if (Follow < 1.f)
		{
			Want = PinGraph[F].GetLocation() + Xq.UnrotateVector(PinDelta[F]);
			WantRot = ((Xq.Inverse() * PinTurn[F] * Xq) * PinGraph[F].GetRotation()).GetNormalized();
		}
		// the heel (ankle) and ball where the foot would end up, world; how planted each is in the graph
		const FVector BallAt = Want + WantRot.RotateVector(PinGraph[F].GetRotation().UnrotateVector(PinBall[F] - PinGraph[F].GetLocation()));
		const FVector P[2] = {EvalX.TransformPosition(Want), EvalX.TransformPosition(BallAt)};
		const FVector G[2] = {EvalX.TransformPosition(PinGraph[F].GetLocation()), EvalX.TransformPosition(PinBall[F])};
		const float Z[2] = {float(PinGraph[F].GetLocation().Z), float(PinBall[F].Z)};
		float W[2];
		for (int32 I = 0; I < 2; ++I)
		{
			const float Height = bGround[F] ? Z[I] - PinRefZ[I] - GroundZ[F] : 0.f;
			W[I] = (1.f - FMath::SmoothStep(1.f, 3.f, Height)) * (1.f - FMath::SmoothStep(Still, Moving, PinSpeed[F][I]));
			if (PinW[F] <= 0.f) { W[I] = 0.f; }   // a cut
		}
		if (Mode == 2)
		{
			FVector Corr = FVector::ZeroVector;
			for (int32 I = 0; I < 2; ++I)
			{
				PinAnchor[F][I] = FMath::Lerp(PinAnchor[F][I], P[I], FollowOf(W[I]));
				Corr += (PinAnchor[F][I] - P[I]) * W[I];
			}
			Corr /= FMath::Max(W[0] + W[1], 1.f);
			Corr.Z = 0.f;
			Corr = Corr.GetClampedToMaxSize(CVarPinMax.GetValueOnAnyThread());
			Want += Xq.UnrotateVector(Corr);
		}
		else
		{
			PinAnchor[F][0] = P[0];
			PinAnchor[F][1] = P[1];
		}
		if (Mode > 0 && (FVector::Dist(Want, Post.GetLocation()) > 0.005f || WantRot.AngularDistance(Post.GetRotation()) > 1e-4f))
		{
			S.Reach(Leg[F][0], Leg[F][1], Leg[F][2], Want, WantRot, 1.f);
		}
		if (CVarPinLog.GetValueOnAnyThread() == 0) { continue; }
		// the log: each heel or ball's slide while it is down, the graph's and the final one's
		const FCompactPoseBoneIndex BallBone = S.Find(F ? TEXT("ball_r") : TEXT("ball_l"));
		const FVector O[2] = {EvalX.TransformPosition(S.Component(Leg[F][2]).GetLocation()), BallBone.IsValid() ? EvalX.TransformPosition(S.Component(BallBone).GetLocation()) : P[1]};
		bool bAny = false;
		for (int32 I = 0; I < 2; ++I)
		{
			if (W[I] > 0.5f)
			{
				if (!bPinDown[F][I]) { bPinDown[F][I] = true; PinGraphStart[F][I] = G[I]; PinOutStart[F][I] = O[I]; }
				PinGraphDrift[F] = FMath::Max(PinGraphDrift[F], float(FVector::Dist2D(G[I], PinGraphStart[F][I])));
				PinOutDrift[F] = FMath::Max(PinOutDrift[F], float(FVector::Dist2D(O[I], PinOutStart[F][I])));
				bAny = true;
			}
			else { bPinDown[F][I] = false; }
		}
		if (bAny) { PinStance[F] += Dt; continue; }
		if (PinStance[F] > 0.15f)
		{
			UE_LOG(LogTemp, Display, TEXT("VisexPin: %s stance %.2f s, pin %d: graph slid %.2f cm, final %.2f cm"), F ? TEXT("R") : TEXT("L"), PinStance[F], Mode, PinGraphDrift[F], PinOutDrift[F]);
		}
		PinStance[F] = PinGraphDrift[F] = PinOutDrift[F] = 0.f;
	}
}
