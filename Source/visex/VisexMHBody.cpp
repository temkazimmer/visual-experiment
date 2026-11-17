#include "VisexMHBody.h"
#include "VisexBodyComponent.h"
#include "VisexShellComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "VisexBodyProfile.h"
#include "Misc/PackageName.h"
#include "Misc/ConfigCacheIni.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimationRuntime.h"
#include "VisexAnimInstance.h"
#include "Components/LODSyncComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "TwoBoneIK.h"
#include "Components/CapsuleComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogVisexMH, Log, All);

static TAutoConsoleVariable<int32> CVarVisexMHBody(
	TEXT("visex.Body.MetaHuman"), 1,
	TEXT("1 (default since Ship 1): she is the assembled MetaHuman (body, face, grooms) following the driver's pose, in the Silversuit. 0: the old body, untouched, with the whole wardrobe."));

static TAutoConsoleVariable<int32> CVarVisexMHAnyOutfit(
	TEXT("visex.Body.MetaHuman.AnyOutfit"), 0,
	TEXT("Checks only. 1: the MetaHuman wears whatever the profile says, nothing included (an outfit with no cut on it shows as nothing). 0: only outfits cut on it, else the Silversuit."));

// the MetaHuman could not be loaded or spawned: the game is on the old body until the cvar is cycled
static bool GVisexMHFailed = false;

static TAutoConsoleVariable<int32> CVarVisexMHCorrectives(
	TEXT("visex.Body.MetaHuman.Correctives"), 1,
	TEXT("0: the MetaHuman body's post-process AnimBP (RigLogic twist and corrective joints) is switched off, to compare."));

static TAutoConsoleVariable<int32> CVarVisexMHLOD(
	TEXT("visex.Body.MetaHuman.LOD"), -1,
	TEXT("Debug: holds the MetaHuman's LOD Sync at this LOD (0-7); -1 gives it back to the view."));

static TAutoConsoleVariable<int32> CVarVisexMHFootPin(
	TEXT("visex.Body.MetaHuman.FootPin"), 1,
	TEXT("1: each MetaHuman sole is pinned to the driver's sole (two-bone leg solve, no ground trace). 0: rotations only."));

static TAutoConsoleVariable<int32> CVarVisexMHReshape(
	TEXT("visex.Body.MetaHuman.Reshape"), 1,
	TEXT("1: the Body page's bone-scale sliders and skin tone reach the MetaHuman body (each over its default). 0: it stays as built, to compare."));

static TAutoConsoleVariable<int32> CVarVisexMHMorphs(
	TEXT("visex.Body.MetaHuman.Morphs"), 1,
	TEXT("1: the Body page's shape sliders and the jiggle drive the MetaHuman body's morph targets. 0: the body as built, to compare."));

static TAutoConsoleVariable<int32> CVarVisexMHFlex(
	TEXT("visex.Body.MetaHuman.Flex"), 1,
	TEXT("1: the flex layer tenses the MetaHuman body's muscle bellies by joint angle, load and push-off (the Flex* morph targets). 0: off, to compare."));

static TAutoConsoleVariable<float> CVarVisexMHFlexScale(
	TEXT("visex.Body.MetaHuman.FlexScale"), 1.f,
	TEXT("Strength of the flex layer: every flex morph's weight is multiplied by this (1 as built, 0 none, 2 twice the belly height)."));

static TAutoConsoleVariable<FString> CVarVisexMHFlexTest(
	TEXT("visex.Body.MetaHuman.FlexTest"), TEXT(""),
	TEXT("Debug: \"1\" holds every flex morph at that weight whatever the pose (the full-flex checks); \"Quad=1,BicepsL=0.5\" holds those; \"log\" prints the drivers once a second; \"off\": the drivers alone."));

// "bone:roll,pitch,yaw;bone:..." in each bone's own axes, added on top of the followed pose
static TAutoConsoleVariable<FString> CVarVisexMHTestPose(
	TEXT("visex.Body.MetaHuman.TestPose"), TEXT(""),
	TEXT("Debug: extra local rotations on MetaHuman bones, e.g. \"calf_l:0,0,-120;upperarm_r:0,80,0\"; \"ref;...\" poses the reference pose instead of the followed one. Switches the foot pin off."));

// "scale,up,forward,right": hair-asset cm to MetaHuman head cm, then cm along the head bone's X, Y, Z
static TAutoConsoleVariable<FString> CVarVisexMHHairFit(
	TEXT("visex.Body.MetaHuman.HairFit"), TEXT(""),
	TEXT("Debug: overrides the worn hairstyle's fit on the MetaHuman head, \"scale,x,y,z\" in the head bone's axes (cm). Empty: the style's own fit."));

static TAutoConsoleVariable<FString> CVarVisexMHHairStyle(
	TEXT("visex.Body.MetaHuman.HairStyle"), TEXT(""),
	TEXT("Debug: the MetaHuman head wears this hairstyle (Long, Elf, Short) whatever the Hair tab says. Empty: the Hair tab's."));

static TAutoConsoleVariable<int32> CVarVisexMHHair(
	TEXT("visex.Body.MetaHuman.Hair"), 1,
	TEXT("1: the MetaHuman head wears its own hair, the groom assembled into the Blueprint (Tema, stage 5). 0: our card hairstyle from the Hair tab, fitted to this skull (stage 4). 2: no hair at all, to measure what the hair costs."));

static TAutoConsoleVariable<int32> CVarVisexMHHairRest(
	TEXT("visex.Body.MetaHuman.HairRest"), 1,
	TEXT("1: with her head under knee height (the floor poses) the groom's simulation is off (r.HairStrands.Simulation 0) and the hair keeps its combed shape instead of falling through her arms into the sand. 0: always simulated."));
static TAutoConsoleVariable<FString> CVarVisexMHFace(
	TEXT("visex.Body.MetaHuman.Face"), TEXT(""),
	TEXT("Debug: face rig curves on the MetaHuman face, \"name=value,name=value\" (e.g. CTRL_expressions_eyeBlinkL=1,CTRL_expressions_eyeBlinkR=1). Empty or \"off\": the face's own animation."));

namespace
{
	/** A hairstyle's fit on the MetaHuman skull: "scale,up,front,side" (hair-asset cm to MetaHuman
	 *  head cm, then cm along the head bone's X, Y, Z). The one place the numbers live is
	 *  Config/DefaultGame.ini, [VisexMHHairFit], a line per style, written by Tools/mh_hair_fit.py
	 *  (least squares of the scalp clearance; run it again after every new head). No line: no fit. */
	bool ParseHairFit(const FString& Text, float& Scale, FVector& Offset)
	{
		TArray<FString> Parts;
		if (Text.ParseIntoArray(Parts, TEXT(",")) != 4)
		{
			return false;
		}
		Scale = FMath::Clamp(FCString::Atof(*Parts[0]), 0.5f, 1.5f);
		Offset = FVector(FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]), FCString::Atof(*Parts[3]));
		return true;
	}

	const TCHAR* FigureClassPath = TEXT("/Game/Character/MH/Built/MHC_Her/BP_MHC_Her.BP_MHC_Her_C");
	// the MetaHuman was shaped to her at Height 166 (Tools/mh_create.py), at component scale 1
	const float BuiltHeight = 166.f;

	void RefComponentSpace(const FReferenceSkeleton& Ref, TArray<FTransform>& Out)
	{
		const TArray<FTransform>& Local = Ref.GetRefBonePose();
		Out.SetNum(Local.Num());
		for (int32 I = 0; I < Local.Num(); ++I)
		{
			const int32 P = Ref.GetParentIndex(I);
			Out[I] = P == INDEX_NONE ? Local[I] : Local[I] * Out[P];
		}
	}
}

/** Follows the driver's pose; see UVisexMHBodyAnimInstance. Built and fed on the game thread, evaluated on a worker. */
struct FVisexMHBodyProxy : public FAnimInstanceProxy
{
	explicit FVisexMHBodyProxy(UAnimInstance* In) : FAnimInstanceProxy(In) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

	// Absolute: the driver's component-space rotation as it is (the skeletons agree on bone axes).
	// Offset: the same, times the bones' reference-pose difference (our added toe bones: other axes).
	// LocalDelta: the driver's change from its reference pose, in the parent's space (metacarpals:
	// the hand's rest shape is the MetaHuman's own).
	enum class EMode : uint8 { None, Absolute, Offset, LocalDelta };
	struct FRow
	{
		int32 Src = INDEX_NONE, SrcParent = INDEX_NONE;
		EMode Mode = EMode::None;
		bool bPosition = false;
		FQuat Q = FQuat::Identity;
		FQuat Extra = FQuat::Identity;   // the test pose
		bool bExtra = false;
	};
	struct FLeg
	{
		int32 Thigh = INDEX_NONE, Calf = INDEX_NONE, Foot = INDEX_NONE;            // our mesh
		int32 SrcThigh = INDEX_NONE, SrcCalf = INDEX_NONE, SrcFoot = INDEX_NONE;   // the driver's
		FVector SoleOffset = FVector::ZeroVector;   // in the foot bone's axes
		FVector ToeLocal = FVector(0.f, 1.f, 0.f);  // toward the ball, in the foot bone's axes
		float Length = 0.f;
		bool IsValid() const { return Thigh != INDEX_NONE && Calf != INDEX_NONE && Foot != INDEX_NONE && SrcThigh != INDEX_NONE && SrcCalf != INDEX_NONE && SrcFoot != INDEX_NONE; }
	};

	TArray<FRow> Rows;              // by our mesh's bone index
	TArray<FTransform> SourceCS;    // the driver's pose this frame
	FLeg Legs[2];
	int32 Pelvis = INDEX_NONE;
	float Scale = 1.f, Feet = 1.f;
	FVisexMHShape Shape;            // the Body page's bone scales, over their defaults
	TArray<FVector> BoneScale;      // by our mesh's bone index: local scale
	TArray<float> BoneReach;        // ... and what its offset from the parent is multiplied by
	TArray<TPair<int32, FVector>> Fixes;   // see UVisexMHBodyAnimInstance::ScaleFixes
	float PelvisBase = 0.f;         // pelvis lift before the live leg-length term
	bool bPin = true, bTest = false;
	bool bRef = false;              // test pose on the reference pose, the driver ignored
	FString TestPose;
	TWeakObjectPtr<const USkeletalMesh> BuiltSource, BuiltTarget;

	void Build(const USkeletalMesh* SourceMesh, const USkeletalMesh* TargetMesh, UVisexMHBodyAnimInstance* Inst);
	void ParseTestPose(const USkeletalMesh* TargetMesh);
	static float SoleZ(const USkeletalMesh* Mesh, const FVector& Foot, const FVector& Ball);
};

float FVisexMHBodyProxy::SoleZ(const USkeletalMesh* Mesh, const FVector& Foot, const FVector& Ball)
{
	// the lowest reference-pose vertex under the foot. The body does not stand on its origin (its
	// soles sit ~2.4 cm over it), and it parks ~30 collapsed vertices on the origin itself.
	const FSkeletalMeshRenderData* RD = Mesh ? Mesh->GetResourceForRendering() : nullptr;
	float Low = 1e9f;
	if (RD && RD->LODRenderData.Num() > 0)
	{
		const FPositionVertexBuffer& PB = RD->LODRenderData[0].StaticVertexBuffers.PositionVertexBuffer;
		if (PB.GetNumVertices() > 0 && PB.GetVertexData())
		{
			const FVector Mid = (Foot + Ball) * 0.5f;
			for (uint32 I = 0; I < PB.GetNumVertices(); ++I)
			{
				const FVector V(PB.VertexPosition(I));
				if (V.Z < Foot.Z && V.Z < Low && FVector::Dist2D(V, Mid) < 12.f && V.Size2D() > 3.f)
				{
					Low = V.Z;
				}
			}
		}
	}
	if (Low > 1e8f)
	{
		// no vertex data on the CPU (a build with r.FreeSkeletalMeshBuffers 1; the default keeps them,
		// cooked too): the ball joint rides 1.15 cm over the sole on the body as assembled (the feet
		// meter's "sole reference (MetaHuman body)" line; 2.15 was the Creator preview's and 1 cm off)
		Low = Ball.Z - 1.15f;
		UE_LOG(LogVisexMH, Warning, TEXT("VisexMH: no CPU vertices for %s, sole taken %.2f cm under the ball joint"), *GetNameSafe(Mesh), 1.15f);
	}
	return Low;
}

void FVisexMHBodyProxy::ParseTestPose(const USkeletalMesh* TargetMesh)
{
	for (FRow& R : Rows) { R.bExtra = false; R.Extra = FQuat::Identity; }
	bTest = false;
	bRef = false;
	TArray<FString> Parts;
	TestPose.ParseIntoArray(Parts, TEXT(";"));
	for (const FString& Part : Parts)
	{
		FString Bone, Angles;
		TArray<FString> A;
		if (Part.TrimStartAndEnd() == TEXT("ref")) { bRef = bTest = true; continue; }
		if (!Part.Split(TEXT(":"), &Bone, &Angles) || Angles.ParseIntoArray(A, TEXT(",")) != 3) { continue; }
		const int32 I = TargetMesh->GetRefSkeleton().FindBoneIndex(FName(*Bone.TrimStartAndEnd()));
		if (!Rows.IsValidIndex(I)) { continue; }
		Rows[I].Extra = FRotator(FCString::Atof(*A[1]), FCString::Atof(*A[2]), FCString::Atof(*A[0])).Quaternion();
		Rows[I].bExtra = true;
		bTest = true;
	}
}

void FVisexMHBodyProxy::Build(const USkeletalMesh* SourceMesh, const USkeletalMesh* TargetMesh, UVisexMHBodyAnimInstance* Inst)
{
	const FReferenceSkeleton& SR = SourceMesh->GetRefSkeleton();
	const FReferenceSkeleton& TR = TargetMesh->GetRefSkeleton();
	TArray<FTransform> SCS, TCS;
	RefComponentSpace(SR, SCS);
	RefComponentSpace(TR, TCS);
	Rows.Reset();
	Rows.SetNum(TR.GetNum());
	Pelvis = TR.FindBoneIndex(TEXT("pelvis"));
	int32 Count[4] = {0, 0, 0, 0};
	for (int32 I = 0; I < TR.GetNum(); ++I)
	{
		const FName Name = TR.GetBoneName(I);
		const FString S = Name.ToString();
		const int32 Src = SR.FindBoneIndex(Name);
		// twist joints belong to the post-process AnimBP (RigLogic), like every corrective
		if (Src == INDEX_NONE || S.Contains(TEXT("twist")))
		{
			continue;
		}
		FRow& R = Rows[I];
		R.Src = Src;
		R.SrcParent = SR.GetParentIndex(Src);
		R.bPosition = I == 0 || I == Pelvis;
		const FQuat SQ = SCS[Src].GetRotation(), TQ = TCS[I].GetRotation();
		if (S.Contains(TEXT("metacarpal")) && R.SrcParent != INDEX_NONE)
		{
			R.Mode = EMode::LocalDelta;
			R.Q = TR.GetRefBonePose()[I].GetRotation() * SR.GetRefBonePose()[Src].GetRotation().Inverse();
		}
		else if (FMath::RadiansToDegrees(SQ.AngularDistance(TQ)) > 45.f)
		{
			R.Mode = EMode::Offset;
			R.Q = SQ.Inverse() * TQ;
		}
		else
		{
			R.Mode = EMode::Absolute;
		}
		++Count[int32(R.Mode)];
	}
	// The Body page's bone scales: the driver's Reshape (UVisexAnimInstance) on the same bones, each
	// term over the default the MetaHuman was built at. A segment's scale sits on its bone and the
	// next followed bone gets the inverse, so the change stays in the segment and carries through to
	// the joints below as an offset. Twist and corrective joints are left alone on purpose: RigLogic
	// writes their whole local transform every frame (translation, rotation and scale, in the
	// parent's space), so anything set on them is lost at LOD 0-1 and kept past it. Inheriting is the
	// one behaviour that holds at every LOD: they sit and scale with the segment they belong to.
	BoneScale.Init(FVector::OneVector, TR.GetNum());
	BoneReach.Init(1.f, TR.GetNum());
	// Scale passes down by axis index, not by direction. Nearly every corrective joint has its
	// segment's axes (or Y and Z swapped, which a limb's girth does not notice); the four under
	// spine_05 (clavicle_pec, spine_04_latissimus: the bust and the lats) have X and Z swapped, so the
	// ribcage's width reached them as height. Those get the terms moved to the axes they belong on,
	// after RigLogic (Fixes). A joint more than 30 degrees off every axis (upperarm_in) is left as it is.
	Fixes.Reset();
	const auto Segment = [this, &TR, &TCS](int32 I, const FVector& S)
	{
		if (I == INDEX_NONE || S.Equals(FVector::OneVector, 1e-5f)) { return; }
		BoneScale[I] *= S;
		for (int32 C = I + 1; C < TR.GetNum(); ++C)
		{
			if (TR.GetParentIndex(C) != I) { continue; }
			if (Rows[C].Mode != EMode::None) { BoneScale[C] /= S; continue; }
			const FQuat Rel = TCS[I].GetRotation().Inverse() * TCS[C].GetRotation();
			const FVector Axes[3] = {Rel.GetAxisX(), Rel.GetAxisY(), Rel.GetAxisZ()};
			FVector Fix(1.f);
			bool bNear = true;
			for (int32 A = 0; A < 3; ++A)
			{
				int32 Best = 0;
				for (int32 B = 1; B < 3; ++B) { if (FMath::Abs(Axes[A][B]) > FMath::Abs(Axes[A][Best])) { Best = B; } }
				bNear &= FMath::Abs(Axes[A][Best]) > 0.866f;
				Fix[A] = S[Best] / S[A];
			}
			if (bNear && !Fix.Equals(FVector::OneVector, 1e-4f)) { Fixes.Emplace(C, Fix); }
		}
	};
	// torso: each local axis takes the term of the body axis it is closest to (X side, Y front, Z up)
	const auto Torso = [&](const TCHAR* Bone, float Width, float Depth)
	{
		const int32 I = TR.FindBoneIndex(Bone);
		if (I == INDEX_NONE) { return; }
		const FQuat R = TCS[I].GetRotation();
		const FVector Axes[3] = {R.GetAxisX(), R.GetAxisY(), R.GetAxisZ()};
		const float Terms[3] = {Width, Depth, 1.f};
		FVector S(1.f);
		for (int32 A = 0; A < 3; ++A)
		{
			int32 Best = 0;
			for (int32 B = 1; B < 3; ++B) { if (FMath::Abs(Axes[A][B]) > FMath::Abs(Axes[A][Best])) { Best = B; } }
			S[A] = Terms[Best];
		}
		Segment(I, S);
	};
	const float* V = Shape.V;
	Torso(TEXT("spine_04"), V[FVisexMHShape::ChestW], V[FVisexMHShape::ChestD]);
	Torso(TEXT("spine_05"), V[FVisexMHShape::ChestW], V[FVisexMHShape::ChestD]);
	Torso(TEXT("spine_01"), V[FVisexMHShape::WaistW], V[FVisexMHShape::WaistD]);
	Torso(TEXT("spine_02"), V[FVisexMHShape::WaistW], V[FVisexMHShape::WaistD]);
	Torso(TEXT("pelvis"), V[FVisexMHShape::HipsW], V[FVisexMHShape::HipsD]);
	// neck: longer by moving neck_02 and the head along it, with no scale. The Face mesh hangs ~60
	// neck joints of its own on neck_01 and neck_02 with their axes turned a quarter (X is not the
	// neck's length), and bone scale passes down by axis index: a length scale here would thicken
	// the Face's neck sideways and leave the Body's neck long, opening the seam between the two.
	for (const TCHAR* Bone : {TEXT("neck_02"), TEXT("head")})
	{
		if (const int32 I = TR.FindBoneIndex(Bone); I != INDEX_NONE) { BoneReach[I] = V[FVisexMHShape::Neck]; }
	}
	if (const int32 Head = TR.FindBoneIndex(TEXT("head")); Head != INDEX_NONE)
	{
		BoneScale[Head] *= V[FVisexMHShape::Head];   // uniform: the Face's joints under it take it whole
	}
	FVector ThighS(1.f), CalfS(1.f);
	for (const TCHAR* Side : {TEXT("_l"), TEXT("_r")})
	{
		const auto Bone = [&TR, Side](const TCHAR* Base) { return TR.FindBoneIndex(FName(*(FString(Base) + Side))); };
		// shoulders: the collarbone longer along itself (X), so the joints riding it spread with it;
		// the upper arm ends up where the driver's Offset puts it (its offset is the collarbone's X)
		Segment(Bone(TEXT("clavicle")), FVector(V[FVisexMHShape::Shoulders], 1.f, 1.f));
		// limbs: X runs along the bone (length), Y and Z take the girth
		ThighS = FVector(V[FVisexMHShape::Legs], V[FVisexMHShape::Thighs], V[FVisexMHShape::Thighs]);
		CalfS = FVector(V[FVisexMHShape::Legs], V[FVisexMHShape::Calves], V[FVisexMHShape::Calves]);
		const FVector ArmS(V[FVisexMHShape::ArmLength], V[FVisexMHShape::Arms], V[FVisexMHShape::Arms]);
		Segment(Bone(TEXT("thigh")), ThighS);
		Segment(Bone(TEXT("calf")), CalfS);
		Segment(Bone(TEXT("upperarm")), ArmS);
		Segment(Bone(TEXT("lowerarm")), ArmS);
		// hands and feet whole, fingers and toes with them
		if (const int32 I = Bone(TEXT("hand")); I != INDEX_NONE) { BoneScale[I] *= V[FVisexMHShape::Hands]; }
		if (const int32 I = Bone(TEXT("foot")); I != INDEX_NONE) { BoneScale[I] *= V[FVisexMHShape::Feet]; }
	}
	// legs: where each sole sits against its foot bone on both bodies (the MetaHuman's ankle is
	// higher and its foot longer), as one fixed offset in the foot bone's axes
	const float FootS = V[FVisexMHShape::Feet];
	float AnkleM = 0.f, AnkleD = 0.f, BallM = 0.f;
	static const TCHAR* Sides[2] = {TEXT("l"), TEXT("r")};
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const auto Name = [Side](const TCHAR* Bone) { return FName(*FString::Printf(TEXT("%s_%s"), Bone, Sides[Side])); };
		FLeg& L = Legs[Side];
		L = FLeg();
		L.Thigh = TR.FindBoneIndex(Name(TEXT("thigh")));
		L.Calf = TR.FindBoneIndex(Name(TEXT("calf")));
		L.Foot = TR.FindBoneIndex(Name(TEXT("foot")));
		L.SrcThigh = SR.FindBoneIndex(Name(TEXT("thigh")));
		L.SrcCalf = SR.FindBoneIndex(Name(TEXT("calf")));
		L.SrcFoot = SR.FindBoneIndex(Name(TEXT("foot")));
		const int32 TBall = TR.FindBoneIndex(Name(TEXT("ball"))), SBall = SR.FindBoneIndex(Name(TEXT("ball")));
		if (!L.IsValid() || TBall == INDEX_NONE || SBall == INDEX_NONE)
		{
			L = FLeg();
			continue;
		}
		const FVector TF = TCS[L.Foot].GetTranslation(), TB = TCS[TBall].GetTranslation();
		const FVector SF = SCS[L.SrcFoot].GetTranslation(), SB = SCS[SBall].GetTranslation();
		const float Sole = SoleZ(TargetMesh, TF, TB);
		// our foot is scaled whole about its ankle (the Feet slider over its default), like the driver's
		AnkleM = (TF.Z - Sole) * FootS;
		BallM = (TB.Z - Sole) * FootS;
		AnkleD = SF.Z * Scale * Feet;   // the driver's soles are on its origin
		const FQuat TQi = TCS[L.Foot].GetRotation().Inverse(), SQi = SCS[L.SrcFoot].GetRotation().Inverse();
		FVector Bm = (TB - TF) * FootS, Bd = (SB - SF) * Scale * Feet;
		Bm.Z = -AnkleM;
		Bd.Z = -AnkleD;
		const FVector Hm = TQi.RotateVector(FVector(0.f, 0.f, -AnkleM)), Hd = SQi.RotateVector(FVector(0.f, 0.f, -AnkleD));
		// halfway between matching under the ankle and under the ball: fixed, so it adds no slide
		L.SoleOffset = ((Hd - Hm) + (SQi.RotateVector(Bd) - TQi.RotateVector(Bm))) * 0.5f;
		L.ToeLocal = TQi.RotateVector(TB - TF).GetSafeNormal();
		// thigh plus calf as they are drawn: the offsets to knee and ankle under the segments' scales
		L.Length = float((TR.GetRefBonePose()[L.Calf].GetTranslation() * ThighS).Size() + (TR.GetRefBonePose()[L.Foot].GetTranslation() * CalfS).Size());
	}
	// the pelvis: lifted by the ankle difference, less what the hip joints already sit higher
	PelvisBase = 0.f;
	if (Legs[0].IsValid() && Pelvis != INDEX_NONE && Rows[Pelvis].Src != INDEX_NONE)
	{
		const float HipDropD = (SCS[Rows[Pelvis].Src].GetTranslation().Z - SCS[Legs[0].SrcThigh].GetTranslation().Z) * Scale;
		const float HipDropM = TCS[Pelvis].GetTranslation().Z - TCS[Legs[0].Thigh].GetTranslation().Z;
		PelvisBase = (AnkleM - AnkleD) - (HipDropD - HipDropM);
	}
	Inst->HeelOverSole = AnkleM;
	Inst->BallOverSole = BallM;
	BuiltSource = SourceMesh;
	BuiltTarget = TargetMesh;
	ParseTestPose(TargetMesh);
	UE_LOG(LogVisexMH, Display, TEXT("VisexMH: following %s with %s: %d bones as they are, %d by reference offset, %d by local change, of %d; ankle over sole %.2f (driver %.2f), ball %.2f, leg %.2f, pelvis base %.2f, sole offset L %s"),
		*SourceMesh->GetName(), *TargetMesh->GetName(), Count[1], Count[2], Count[3], TR.GetNum(), AnkleM, AnkleD, BallM, Legs[0].Length, PelvisBase, *Legs[0].SoleOffset.ToCompactString());
}

void FVisexMHBodyProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	SourceCS.Reset();
	UVisexMHBodyAnimInstance* Inst = Cast<UVisexMHBodyAnimInstance>(InAnimInstance);
	const USkeletalMeshComponent* Src = Inst ? Inst->Source.Get() : nullptr;
	const USkeletalMeshComponent* Me = Inst ? Inst->GetSkelMeshComponent() : nullptr;
	const USkeletalMesh* SourceMesh = Src ? Src->GetSkeletalMeshAsset() : nullptr;
	const USkeletalMesh* TargetMesh = Me ? Me->GetSkeletalMeshAsset() : nullptr;
	if (!SourceMesh || !TargetMesh)
	{
		return;
	}
	if (BuiltSource != SourceMesh || BuiltTarget != TargetMesh || Scale != Inst->SourceScale || Feet != Inst->FeetScale || Shape != Inst->Shape)
	{
		Scale = Inst->SourceScale;
		Feet = Inst->FeetScale;
		Shape = Inst->Shape;
		Build(SourceMesh, TargetMesh, Inst);
		Inst->ScaleFixes = Fixes;
	}
	const FString Test = CVarVisexMHTestPose.GetValueOnGameThread();
	if (Test != TestPose)
	{
		TestPose = Test;
		ParseTestPose(TargetMesh);
	}
	bPin = CVarVisexMHFootPin.GetValueOnGameThread() != 0 && !bTest;
	// the driver ticks first (a tick prerequisite), so this is this frame's finished pose
	const TArray<FTransform>& Pose = Src->GetComponentSpaceTransforms();
	if (Pose.Num() == SourceMesh->GetRefSkeleton().GetNum())
	{
		SourceCS = Pose;
	}
}

bool FVisexMHBodyProxy::Evaluate(FPoseContext& Output)
{
	Output.ResetToRefPose();
	if (SourceCS.Num() == 0 || Rows.Num() == 0)
	{
		return true;
	}
	FCompactPose& Pose = Output.Pose;
	const FBoneContainer& RB = Pose.GetBoneContainer();
	TArray<FTransform> CS;
	CS.SetNumUninitialized(Pose.GetNumBones());
	// her legs are a touch shorter or longer than the driver's as it stands now (its leg sliders)
	float PelvisLift = PelvisBase;
	if (Legs[0].IsValid())
	{
		const FLeg& L = Legs[0];
		const float Driver = (FVector::Dist(SourceCS[L.SrcThigh].GetTranslation(), SourceCS[L.SrcCalf].GetTranslation())
			+ FVector::Dist(SourceCS[L.SrcCalf].GetTranslation(), SourceCS[L.SrcFoot].GetTranslation())) * Scale;
		PelvisLift += L.Length - Driver;
	}
	for (const FCompactPoseBoneIndex I : Pose.ForEachBoneIndex())
	{
		const int32 M = RB.MakeMeshPoseIndex(I).GetInt();
		const FTransform ParentCS = I.IsRootBone() ? FTransform::Identity : CS[RB.GetParentBoneIndex(I).GetInt()];
		FTransform Local = Pose[I];
		if (BoneScale.IsValidIndex(M))
		{
			// the Body page's reshape, on the way down from the root: each bone's component space is
			// its local transform times its parent's, so offsets take the scales above them
			Local.SetScale3D(BoneScale[M]);
			Local.SetTranslation(Local.GetTranslation() * BoneReach[M]);
		}
		if (Rows.IsValidIndex(M))
		{
			const FRow& R = Rows[M];
			if (R.Mode != EMode::None && !bRef && SourceCS.IsValidIndex(R.Src))
			{
				const FTransform& S = SourceCS[R.Src];
				if (R.Mode == EMode::LocalDelta && SourceCS.IsValidIndex(R.SrcParent))
				{
					Local.SetRotation(R.Q * (SourceCS[R.SrcParent].GetRotation().Inverse() * S.GetRotation()));
				}
				else
				{
					const FQuat Q = R.Mode == EMode::Offset ? S.GetRotation() * R.Q : S.GetRotation();
					Local.SetRotation(ParentCS.GetRotation().Inverse() * Q);
				}
				if (R.bPosition)
				{
					FVector At = S.GetTranslation() * Scale;
					At.Z += M == Pelvis ? PelvisLift : 0.f;
					Local.SetTranslation(ParentCS.InverseTransformPosition(At));
				}
			}
			if (R.bExtra)
			{
				Local.SetRotation(Local.GetRotation() * R.Extra);
			}
			Local.NormalizeRotation();
		}
		Pose[I] = Local;
		CS[I.GetInt()] = Local * ParentCS;
	}
	// The foot pin: the copied rotations leave each ankle within a centimetre of where its sole
	// would match the driver's. Close that with a two-bone solve toward the driver's foot. It reads
	// no ground: the driver's Control Rig stays the one ground-IK owner, this only repeats its answer.
	for (const FLeg& L : Legs)
	{
		if (!bPin || !L.IsValid())
		{
			continue;
		}
		const FCompactPoseBoneIndex T = RB.MakeCompactPoseIndex(FMeshPoseBoneIndex(L.Thigh));
		const FCompactPoseBoneIndex C = RB.MakeCompactPoseIndex(FMeshPoseBoneIndex(L.Calf));
		const FCompactPoseBoneIndex F = RB.MakeCompactPoseIndex(FMeshPoseBoneIndex(L.Foot));
		if (T.GetInt() == INDEX_NONE || C.GetInt() == INDEX_NONE || F.GetInt() == INDEX_NONE || T.IsRootBone())
		{
			continue;
		}
		FTransform Thigh = CS[T.GetInt()], Calf = CS[C.GetInt()], Foot = CS[F.GetInt()];
		const FQuat FootRot = Foot.GetRotation();
		const FVector Target = SourceCS[L.SrcFoot].GetTranslation() * Scale + FootRot.RotateVector(L.SoleOffset);
		// keep the knee where it points; a straight leg has no direction, so lean on the toes'
		const FVector Hip = Thigh.GetTranslation(), Knee = Calf.GetTranslation();
		const FVector Axis = (Foot.GetTranslation() - Hip).GetSafeNormal();
		const FVector Bend = ((Knee - Hip) - Axis * FVector::DotProduct(Knee - Hip, Axis) + FootRot.RotateVector(L.ToeLocal) * 0.5f).GetSafeNormal();
		AnimationCore::SolveTwoBoneIK(Thigh, Calf, Foot, Knee + Bend * 100.f, Target, true, 1.0, 1.03);
		Foot.SetRotation(FootRot);
		Pose[T] = Thigh.GetRelativeTransform(CS[RB.GetParentBoneIndex(T).GetInt()]);
		Pose[C] = Calf.GetRelativeTransform(Thigh);
		Pose[F] = Foot.GetRelativeTransform(Calf);
		Pose[T].NormalizeRotation();
		Pose[C].NormalizeRotation();
		Pose[F].NormalizeRotation();
	}
	return true;
}

void UVisexMHBodyAnimInstance::SetSource(USkeletalMeshComponent* InSource, float InSourceScale, float InFeetScale)
{
	Source = InSource;
	SourceScale = InSourceScale;
	FeetScale = InFeetScale;
}

FAnimInstanceProxy* UVisexMHBodyAnimInstance::CreateAnimInstanceProxy()
{
	return new FVisexMHBodyProxy(this);
}

UVisexMHBodyComponent::UVisexMHBodyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UVisexMHBodyComponent::Ensure(AActor* Owner)
{
	const UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!World || !World->IsGameWorld() || !Cast<ACharacter>(Owner) || Owner->FindComponentByClass<UVisexMHBodyComponent>())
	{
		return;
	}
	UVisexMHBodyComponent* C = NewObject<UVisexMHBodyComponent>(Owner, TEXT("MHBody"), RF_Transient);
	C->RegisterComponent();
}

USkeletalMeshComponent* UVisexMHBodyComponent::ShownBody(const ACharacter* Her)
{
	const UVisexMHBodyComponent* C = Her ? Her->FindComponentByClass<UVisexMHBodyComponent>() : nullptr;
	return C && C->bShown ? C->BodyMesh.Get() : nullptr;
}

USkeletalMeshComponent* UVisexMHBodyComponent::ShownFace(const ACharacter* Her)
{
	const UVisexMHBodyComponent* C = Her ? Her->FindComponentByClass<UVisexMHBodyComponent>() : nullptr;
	return C && C->bShown ? C->FaceMesh.Get() : nullptr;
}

USkeletalMeshComponent* UVisexMHBodyComponent::ShownGarment(const ACharacter* Her)
{
	const UVisexMHBodyComponent* C = Her ? Her->FindComponentByClass<UVisexMHBodyComponent>() : nullptr;
	return C && C->bShown ? C->Garment.Get() : nullptr;
}

USkeletalMeshComponent* UVisexMHBodyComponent::FeetMesh(const ACharacter* Her)
{
	USkeletalMeshComponent* Shown = ShownBody(Her);
	return Shown ? Shown : (Her ? Her->GetMesh() : nullptr);
}

bool UVisexMHBodyComponent::On()
{
	return CVarVisexMHBody.GetValueOnGameThread() != 0 && !GVisexMHFailed;
}

bool UVisexMHBodyComponent::Wears(FName Outfit)
{
	if (!On() || CVarVisexMHAnyOutfit.GetValueOnGameThread() != 0)
	{
		return true;
	}
	if (Outfit.IsNone() || Outfit == TEXT("None"))
	{
		return false;
	}
	// a cut exists: asked once per outfit (the wardrobe asks every frame)
	static TMap<FName, bool> Cut;
	if (const bool* Known = Cut.Find(Outfit))
	{
		return *Known;
	}
	return Cut.Add(Outfit, FPackageName::DoesPackageExist(TEXT("/Game/Character/MH/Outfits/SKM_") + Outfit.ToString() + TEXT("_MH")));
}

FName UVisexMHBodyComponent::Worn(FName Outfit)
{
	return Wears(Outfit) ? Outfit : FName(TEXT("Silversuit"));
}

bool UVisexMHBodyComponent::HairTab()
{
	return !On() || CVarVisexMHHair.GetValueOnGameThread() == 0;
}

bool UVisexMHBodyComponent::SoleHeights(const ACharacter* Her, float& OutHeel, float& OutBall)
{
	const USkeletalMeshComponent* Shown = ShownBody(Her);
	const UVisexMHBodyAnimInstance* Inst = Shown ? Cast<UVisexMHBodyAnimInstance>(Shown->GetAnimInstance()) : nullptr;
	if (!Inst || Inst->HeelOverSole <= 0.f)
	{
		return false;
	}
	const float S = Shown->GetComponentScale().Z;
	OutHeel = Inst->HeelOverSole * S;
	OutBall = Inst->BallOverSole * S;
	return true;
}

void UVisexMHBodyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const bool bWant = CVarVisexMHBody.GetValueOnGameThread() != 0;
	if (!bWant)
	{
		bFailed = false;
		GVisexMHFailed = false;
	}
	if (bWant != bShown && !bFailed)
	{
		bWant ? Show() : Hide();
	}
	if (bShown)
	{
		// an outfit put on later is a new component: keep the driver's layers hidden
		HideDriver(true);
		ApplyOutfit();
		ApplyDebug();
		ApplyHair();   // after ApplyDebug: it may have bound a re-made Body
		ApplyHairRest(true);
		ApplyFace();
		ApplyShape();
		ApplyMorphs();
		ApplyFlex(DeltaTime);
		ApplySkin();   // after ApplyOutfit: it puts other skin materials in the slots
	}
}

void UVisexMHBodyComponent::ApplyShape()
{
	const ACharacter* Her = Cast<ACharacter>(GetOwner());
	const UVisexBodyComponent* Body = Her ? Her->FindComponentByClass<UVisexBodyComponent>() : nullptr;
	UVisexMHBodyAnimInstance* Inst = BodyMesh ? Cast<UVisexMHBodyAnimInstance>(BodyMesh->GetAnimInstance()) : nullptr;
	if (!Body || !Inst)
	{
		return;
	}
	FVisexMHShape S;
	if (CVarVisexMHReshape.GetValueOnGameThread() != 0)
	{
		// a slider over its default; Part is how much of the slider a torso depth term takes (Reshape)
		const auto Rel = [Body](const TCHAR* Name, float Part = 1.f)
		{
			const FVisexBodySlider* Row = FVisexBodyProfile::Find(Name);
			const float Built = 1.f + ((Row ? Row->Default : 1.f) - 1.f) * Part;
			return (1.f + (Body->GetValue(Name) - 1.f) * Part) / FMath::Max(Built, 0.01f);
		};
		S.V[FVisexMHShape::Head] = Rel(TEXT("Head"));
		S.V[FVisexMHShape::Neck] = Rel(TEXT("Neck"));
		S.V[FVisexMHShape::Shoulders] = Rel(TEXT("Shoulders"));
		S.V[FVisexMHShape::ChestW] = Rel(TEXT("Chest"));
		S.V[FVisexMHShape::ChestD] = Rel(TEXT("Chest"), 0.6f);
		S.V[FVisexMHShape::WaistW] = Rel(TEXT("Waist"));
		S.V[FVisexMHShape::WaistD] = Rel(TEXT("Waist"), 0.7f);
		S.V[FVisexMHShape::HipsW] = Rel(TEXT("Hips"));
		S.V[FVisexMHShape::HipsD] = Rel(TEXT("Hips"), 0.5f);
		S.V[FVisexMHShape::Legs] = Rel(TEXT("Legs"));
		S.V[FVisexMHShape::Thighs] = Rel(TEXT("Thighs"));
		S.V[FVisexMHShape::Calves] = Rel(TEXT("Calves"));
		S.V[FVisexMHShape::ArmLength] = Rel(TEXT("ArmLength"));
		S.V[FVisexMHShape::Arms] = Rel(TEXT("Arms"));
		S.V[FVisexMHShape::Hands] = Rel(TEXT("Hands"));
		S.V[FVisexMHShape::Feet] = Rel(TEXT("Feet"));
	}
	Inst->Shape = S;
	Inst->FeetScale = Body->GetValue(TEXT("Feet"));   // the driver's, as it is: its sole sits under a foot of that size
	if (FixBound != BodyMesh)
	{
		// once per Body component: it goes with the component when the figure is re-made
		BodyMesh->RegisterOnBoneTransformsFinalizedDelegate(FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateUObject(this, &UVisexMHBodyComponent::FixScales));
		FixBound = BodyMesh;
		FixWritten.Reset();
	}
}

void UVisexMHBodyComponent::ApplyMorphs()
{
	const ACharacter* Her = Cast<ACharacter>(GetOwner());
	const UVisexBodyComponent* Body = Her ? Her->FindComponentByClass<UVisexBodyComponent>() : nullptr;
	const USkeletalMeshComponent* Driver = Her ? Her->GetMesh() : nullptr;
	if (!Body || !Driver || !BodyMesh)
	{
		return;
	}
	const bool bOn = CVarVisexMHMorphs.GetValueOnGameThread() != 0;
	const auto Set = [this, bOn](const TCHAR* Name, float Weight) { BodyMesh->SetMorphTarget(Name, bOn ? Weight : 0.f, false); };
	const auto Built = [](const TCHAR* Name) { const FVisexBodySlider* Row = FVisexBodyProfile::Find(Name); return Row ? Row->Default : 0.f; };
	const auto Rel = [Body, Built](const TCHAR* Name) { return Body->GetValue(Name) - Built(Name); };
	// the shape: she was built at the default sliders, so a morph's weight is the slider's distance
	// from its default (a smaller bust is a negative weight), in the driver's own units
	Set(TEXT("Bust"), Rel(TEXT("Bust")));
	Set(TEXT("BustLift"), Rel(TEXT("BustLift")));
	// spacing scales with how much bust there is, as on the driver (it carves against the full bust)
	Set(TEXT("BustApart"), Rel(TEXT("BustApart")) * 1.2f * FMath::Min(Body->GetValue(TEXT("Bust")) / 0.7f, 1.2f));
	Set(TEXT("Glutes"), Rel(TEXT("Glutes")));
	Set(TEXT("Fat"), Rel(TEXT("Fat")));
	Set(TEXT("ThighGap"), Rel(TEXT("ThighGap")));
	float Lines, Pack, Lines0, Pack0;
	FVisexBodyProfile::AbsWeights(Body->GetValue(TEXT("Abs")), Body->GetValue(TEXT("Fat")), Lines, Pack);
	FVisexBodyProfile::AbsWeights(Built(TEXT("Abs")), Built(TEXT("Fat")), Lines0, Pack0);
	Set(TEXT("AbsLines"), Lines - Lines0);
	Set(TEXT("AbsPack"), Pack - Pack0);
	// the jiggle: the driver's springs already run (UVisexBodyComponent::TickJiggle writes the weights
	// on its mesh every frame). Its bust bounce carries the standing bust height too, which is the
	// BustLift morph here: take that part out and keep the swing.
	const float Lift = Body->GetValue(TEXT("BustLift")) * Body->BustLiftRange;
	static const TCHAR* Bounce[4] = {TEXT("BustBounceL"), TEXT("BustBounceR"), TEXT("GluteBounceL"), TEXT("GluteBounceR")};
	for (int32 I = 0; I < 4; ++I)
	{
		const FName Up(*(FString(Bounce[I]) + TEXT("_Up"))), Down(*(FString(Bounce[I]) + TEXT("_Down")));
		const float Swing = Driver->GetMorphTarget(Up) - Driver->GetMorphTarget(Down) - (I < 2 ? Lift : 0.f);
		BodyMesh->SetMorphTarget(Up, bOn ? FMath::Max(0.f, Swing) : 0.f, false);
		BodyMesh->SetMorphTarget(Down, bOn ? FMath::Max(0.f, -Swing) : 0.f, false);
	}
	static const TCHAR* Sway[8] = {TEXT("BustSwayL_Out"), TEXT("BustSwayL_In"), TEXT("BustSwayL_Fwd"), TEXT("BustSwayL_Back"),
		TEXT("BustSwayR_Out"), TEXT("BustSwayR_In"), TEXT("BustSwayR_Fwd"), TEXT("BustSwayR_Back")};
	for (const TCHAR* Name : Sway)
	{
		Set(Name, Driver->GetMorphTarget(Name));
	}
}

void UVisexMHBodyComponent::FixScales()
{
	const UVisexMHBodyAnimInstance* Inst = IsValid(BodyMesh) ? Cast<UVisexMHBodyAnimInstance>(BodyMesh->GetAnimInstance()) : nullptr;
	if (!Inst || Inst->ScaleFixes.Num() == 0)
	{
		return;
	}
	// The bones are final and nothing has read them yet this frame (the garment's Leader Pose and
	// the renderer take them at the end of the frame, the Face copies them when it ticks after the
	// Body). A scale only: these joints have no children, so no position hangs on them.
	TArray<FTransform>& CS = const_cast<TArray<FTransform>&>(BodyMesh->GetComponentSpaceTransforms());
	FixWritten.SetNum(Inst->ScaleFixes.Num());
	for (int32 K = 0; K < Inst->ScaleFixes.Num(); ++K)
	{
		const TPair<int32, FVector>& Fix = Inst->ScaleFixes[K];
		if (!CS.IsValidIndex(Fix.Key))
		{
			continue;
		}
		const FVector Now = CS[Fix.Key].GetScale3D();
		if (Now == FixWritten[K])
		{
			continue;   // finalized again without a new pose: already done
		}
		FixWritten[K] = Now * Fix.Value;
		CS[Fix.Key].SetScale3D(FixWritten[K]);
	}
}

void UVisexMHBodyComponent::ApplySkin()
{
	const ACharacter* Her = Cast<ACharacter>(GetOwner());
	const UVisexBodyComponent* Body = Her ? Her->FindComponentByClass<UVisexBodyComponent>() : nullptr;
	if (!Body || !IsValid(BodyMesh))
	{
		return;
	}
	// The assembled skin material (body, every face LOD, and the copies worn under outfits: all one
	// material, M_skin_unified_baked or its masked copy) multiplies its baked base colour by this
	// parameter, and its instances have the switch for it on. The baked colour is her default tone
	// (stage 6), so the multiplier is the slider's colour over the default's: 1 at the default, and
	// the same relative range as the android skin's tone ramp. Nothing in the assets is changed.
	static const FName Param(TEXT("Basecolor Global Multiply Post-Bake"));
	const FVisexBodySlider* Row = FVisexBodyProfile::Find(TEXT("Skin"));
	const float Built = Row ? Row->Default : 0.25f;
	const float Tone = CVarVisexMHReshape.GetValueOnGameThread() != 0 ? Body->GetValue(TEXT("Skin")) : Built;
	const FLinearColor A = FVisexBodyProfile::SkinColor(Tone), B = FVisexBodyProfile::SkinColor(Built);
	const FLinearColor Tint(A.R / FMath::Max(B.R, 1e-3f), A.G / FMath::Max(B.G, 1e-3f), A.B / FMath::Max(B.B, 1e-3f), 1.f);
	int32 K = 0;
	for (USkeletalMeshComponent* Mesh : {BodyMesh.Get(), FaceMesh.Get()})
	{
		for (int32 Slot = 0; IsValid(Mesh) && Slot < Mesh->GetNumMaterials(); ++Slot, ++K)
		{
			UMaterialInterface* Mat = Mesh->GetMaterial(Slot);
			if (!ToneSeen.IsValidIndex(K)) { ToneSeen.SetNum(K + 1); }
			if (ToneSeen[K] == Mat && Tone == LastTone)
			{
				continue;   // looked at, and nothing moved
			}
			UMaterialInstanceDynamic* Dyn = Cast<UMaterialInstanceDynamic>(Mat);
			FLinearColor Was;
			if (!Dyn && Mat && Mat->GetVectorParameterValue(FHashedMaterialParameterInfo(Param), Was))
			{
				Dyn = Mesh->CreateDynamicMaterialInstance(Slot, Mat);   // skin: an outfit's or the body's own
				ToneMaterials.Add(Dyn);
			}
			if (Dyn && ToneMaterials.Contains(Dyn))
			{
				Dyn->SetVectorParameterValue(Param, Tint);
			}
			ToneSeen[K] = Mesh->GetMaterial(Slot);
		}
	}
	LastTone = Tone;
}

void UVisexMHBodyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Hide();
	Super::EndPlay(EndPlayReason);
}

void UVisexMHBodyComponent::Show()
{
	ACharacter* Her = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Driver = Her ? Her->GetMesh() : nullptr;
	// by path, loaded on first use: with the cvar at 0 none of the MetaHuman's 800 MB is touched
	UClass* Class = Driver ? LoadClass<AActor>(nullptr, FigureClassPath) : nullptr;
	FActorSpawnParameters Params;
	Params.Owner = Her;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Figure = Class ? GetWorld()->SpawnActor<AActor>(Class, Driver->GetComponentTransform(), Params) : nullptr;
	if (!Figure)
	{
		bFailed = true;
		GVisexMHFailed = true;   // the wardrobe and the Hair tab go back to the old body's
		UE_LOG(LogVisexMH, Warning, TEXT("VisexMH: cannot show the MetaHuman body (%s)"), Class ? TEXT("spawn failed") : FigureClassPath);
		return;
	}
	Figure->SetActorEnableCollision(false);
	Figure->AttachToComponent(Driver, FAttachmentTransformRules::KeepRelativeTransform);
	// on the driver's origin (the floor under her). The driver is a 177.5 cm mesh scaled to her
	// height; the MetaHuman was built at her height, so it undoes that scale.
	const UVisexBodyComponent* Body = Her->FindComponentByClass<UVisexBodyComponent>();
	const float Reference = Body ? Body->ReferenceHeight : 177.5f;
	Figure->SetActorRelativeTransform(FTransform(FQuat::Identity, FVector::ZeroVector, FVector(Reference / BuiltHeight)));
	Bind();
	bShown = true;
	LastCorrectives = -1;
	UE_LOG(LogVisexMH, Display, TEXT("VisexMH: showing %s on %s (body %s, face %s, scale %.4f)"), *Figure->GetName(), *Her->GetName(),
		*GetNameSafe(BodyMesh), *GetNameSafe(FaceMesh), Reference / BuiltHeight);
}

void UVisexMHBodyComponent::Bind()
{
	ACharacter* Her = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Driver = Her ? Her->GetMesh() : nullptr;
	if (!Driver || !Figure)
	{
		return;
	}
	const UVisexBodyComponent* Body = Her->FindComponentByClass<UVisexBodyComponent>();
	const float Reference = Body ? Body->ReferenceHeight : 177.5f;
	const FLightingChannels& Ch = Driver->LightingChannels;
	ClearOutfit();   // it hangs on the Body that is being replaced
	ClearHair();
	ToneMaterials.Reset();   // the tone is put on the new figure's materials
	ToneSeen.Reset();
	LastTone = -1.f;
	GroomMode = -1;   // a new figure: look at its hair again
	bFaceDriven = false;   // and so does its face
	FaceCurves.Reset();
	FaceOwnClass = nullptr;
	BodyMesh = nullptr;
	FaceMesh = nullptr;
	TInlineComponentArray<UPrimitiveComponent*> Prims(Figure);
	for (UPrimitiveComponent* Prim : Prims)
	{
		// spawned meshes come up blocking: she would push herself off her own spawn
		Prim->SetCollisionProfileName(TEXT("NoCollision"));
		Prim->SetGenerateOverlapEvents(false);
		Prim->SetReceivesDecals(false);
		Prim->SetLightingChannels(Ch.bChannel0, Ch.bChannel1, Ch.bChannel2);
		if (USkeletalMeshComponent* Skel = Cast<USkeletalMeshComponent>(Prim))
		{
			Skel->SetBoundsScale(2.f);
			Skel->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			BodyMesh = Skel->GetFName() == TEXT("Body") ? Skel : BodyMesh.Get();
			FaceMesh = Skel->GetFName() == TEXT("Face") ? Skel : FaceMesh.Get();
		}
	}
	if (BodyMesh)
	{
		BodyMesh->SetAnimInstanceClass(UVisexMHBodyAnimInstance::StaticClass());
		if (UVisexMHBodyAnimInstance* Inst = Cast<UVisexMHBodyAnimInstance>(BodyMesh->GetAnimInstance()))
		{
			Inst->SetSource(Driver, BuiltHeight / Reference, Body ? Body->GetValue(TEXT("Feet")) : 1.f);
		}
		// after the driver's pose is final; the face copies the body's pose in its own post-process
		BodyMesh->AddTickPrerequisiteComponent(Driver);
	}
	LastCorrectives = -1;
	LastLOD = -2;
}

void UVisexMHBodyComponent::Hide()
{
	ClearOutfit();
	ClearHair();
	GroomMode = -1;   // the next figure says again which hair it wears
	bFaceDriven = false;   // and so does its face
	FaceCurves.Reset();
	FaceOwnClass = nullptr;
	if (Figure)
	{
		Figure->Destroy();
	}
	Figure = nullptr;
	BodyMesh = nullptr;
	FaceMesh = nullptr;
	HideDriver(false);
	bShown = false;
	ApplyHairRest(false);   // the simulation back on: the switch is the engine's, not this body's
}

void UVisexMHBodyComponent::HideDriver(bool bHide)
{
	const ACharacter* Her = Cast<ACharacter>(GetOwner());
	if (!bHide)
	{
		for (const TWeakObjectPtr<USceneComponent>& Weak : Hidden)
		{
			if (USceneComponent* C = Weak.Get())
			{
				C->SetHiddenInGame(false);
			}
		}
		Hidden.Reset();
		return;
	}
	// the skin and outfits (shells), the cloth piece and the hair cards; her own mesh never draws
	TInlineComponentArray<USkinnedMeshComponent*> Skins(GetOwner());
	for (USkinnedMeshComponent* C : Skins)
	{
		if (Her && C != Her->GetMesh() && !C->bHiddenInGame)
		{
			C->SetHiddenInGame(true);
			Hidden.Add(C);
		}
	}
}

void UVisexMHBodyComponent::ApplyDebug()
{	// a Blueprint actor re-runs its construction when a tool edits one of its components (the editor
	// does, a script can): the Body is then a new component with no anim class. Take it up again.
	if (Figure && (!IsValid(BodyMesh) || BodyMesh->GetOwner() != Figure || BodyMesh->GetAnimClass() != UVisexMHBodyAnimInstance::StaticClass()))
	{
		UE_LOG(LogVisexMH, Display, TEXT("VisexMH: the figure's components were re-made, binding again"));
		Bind();
	}
	// a garment has one LOD and is skinned to the corrective joints, which the body stops evaluating
	// past LOD 1: while one is worn the figure is held at LOD 0
	const int32 Debug = CVarVisexMHLOD.GetValueOnGameThread();
	// the shape and jiggle morphs are on body LOD 0 only (Tools/mh_morphs.py): while they are live she is held there too
	const USkeletalMesh* BodyAsset = BodyMesh ? BodyMesh->GetSkeletalMeshAsset() : nullptr;
	const bool bMorphs = CVarVisexMHMorphs.GetValueOnGameThread() != 0 && BodyAsset && BodyAsset->GetMorphTargets().Num() > 0;
	const int32 LOD = Debug >= 0 ? Debug : (Garment || bMorphs) ? 0 : -1;
	if (Figure && LOD != LastLOD)
	{
		if (ULODSyncComponent* Sync = Figure->FindComponentByClass<ULODSyncComponent>())
		{
			Sync->ForcedLOD = LOD;
		}
		LastLOD = LOD;
	}
	// the mesh re-creates its anim instance now and then (a rebuilt asset, a re-register): a new one
	// has no driver and would stand her in the reference pose
	const ACharacter* Her = Cast<ACharacter>(GetOwner());
	UVisexMHBodyAnimInstance* Inst = BodyMesh ? Cast<UVisexMHBodyAnimInstance>(BodyMesh->GetAnimInstance()) : nullptr;
	if (Her && Inst && Inst->Source != Her->GetMesh())
	{
		const UVisexBodyComponent* Body = Her->FindComponentByClass<UVisexBodyComponent>();
		const float Reference = Body ? Body->ReferenceHeight : 177.5f;
		Inst->SetSource(Her->GetMesh(), BuiltHeight / Reference, Body ? Body->GetValue(TEXT("Feet")) : 1.f);
		UE_LOG(LogVisexMH, Display, TEXT("VisexMH: the body's anim instance was re-made, following again"));
	}

	const int32 Want = CVarVisexMHCorrectives.GetValueOnGameThread() != 0 ? 1 : 0;
	if (BodyMesh && Want != LastCorrectives)
	{
		BodyMesh->SetDisablePostProcessBlueprint(Want == 0);
		LastCorrectives = Want;
	}
}

void UVisexMHBodyComponent::ClearOutfit()
{
	if (Garment)
	{
		Garment->DestroyComponent();
	}
	Garment = nullptr;
	if (IsValid(BodyMesh) && BareSkin)
	{
		BodyMesh->SetMaterial(0, BareSkin);
	}
	BareSkin = nullptr;
	if (IsValid(FaceMesh) && FaceSkin)
	{
		FaceMesh->SetMaterial(0, FaceSkin);
	}
	FaceSkin = nullptr;
	bOutfitApplied = false;
}

void UVisexMHBodyComponent::ApplyOutfit()
{
	const ACharacter* Her = Cast<ACharacter>(GetOwner());
	const UVisexBodyComponent* Body = Her ? Her->FindComponentByClass<UVisexBodyComponent>() : nullptr;
	// never bare and never an outfit without a cut here: the Silversuit instead (Ship 1)
	const FName Want = Worn(Body ? Body->Profile.Outfit : FName(NAME_None));
	if (!IsValid(BodyMesh) || !Figure)
	{
		return;
	}
	if (bOutfitApplied && Want == GarmentOutfit && (!Garment || Garment->LeaderPoseComponent.Get() == BodyMesh))
	{
		return;
	}
	ClearOutfit();
	GarmentOutfit = Want;
	bOutfitApplied = true;
	if (Want.IsNone())
	{
		return;
	}
	// by name, loaded on first wear: SKM_<Outfit>_MH and MI_Body_<Outfit> from Tools/mh_outfit.py
	const auto Load = [&Want](const TCHAR* Prefix, const TCHAR* Suffix, UClass* Class) -> UObject*
	{
		const FString Name = Prefix + Want.ToString() + Suffix;
		const FString Package = TEXT("/Game/Character/MH/Outfits/") + Name;
		return FPackageName::DoesPackageExist(Package) ? StaticLoadObject(Class, nullptr, *(Package + TEXT(".") + Name)) : nullptr;
	};
	USkeletalMesh* Mesh = Cast<USkeletalMesh>(Load(TEXT("SKM_"), TEXT("_MH"), USkeletalMesh::StaticClass()));
	if (!Mesh)
	{
		UE_LOG(LogVisexMH, Display, TEXT("VisexMH: outfit %s has no cut on the MetaHuman body yet, she wears nothing"), *Want.ToString());
		return;
	}
	UVisexShellComponent* Shell = NewObject<UVisexShellComponent>(Figure, NAME_None, RF_Transient);
	Shell->SetSkeletalMesh(Mesh);
	Shell->SetupAttachment(BodyMesh);
	Shell->SetCastShadow(true);              // the skin under it is clipped away: the fabric is her shadow there
	Shell->SetBoundsScale(2.f);
	const FLightingChannels& Ch = BodyMesh->LightingChannels;
	Shell->SetLightingChannels(Ch.bChannel0, Ch.bChannel1, Ch.bChannel2);
	Shell->RegisterComponent();              // follows the Body's final bones from here (Leader Pose)
	Garment = Shell;
	if (UMaterialInterface* Under = Cast<UMaterialInterface>(Load(TEXT("MI_Body_"), TEXT(""), UMaterialInterface::StaticClass())))
	{
		BareSkin = BodyMesh->GetMaterial(0);
		BodyMesh->SetMaterial(0, Under);
	}
	// the head mesh's chest part carries the baked top's shoulder straps: under an outfit, the copy
	// of its LOD 0 skin with them painted out (Tools/mh_bare_skin.py); the figure is held at LOD 0
	const TCHAR* HeadBare = TEXT("/Game/Character/MH/Outfits/MI_Head_Bare");
	UMaterialInterface* Head = FaceMesh && FPackageName::DoesPackageExist(HeadBare) ? LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Character/MH/Outfits/MI_Head_Bare.MI_Head_Bare")) : nullptr;
	if (Head)
	{
		FaceSkin = FaceMesh->GetMaterial(0);
		FaceMesh->SetMaterial(0, Head);
	}
	UE_LOG(LogVisexMH, Display, TEXT("VisexMH: wearing %s (%s, skin under it %s)"), *Want.ToString(), *Mesh->GetName(), BareSkin ? TEXT("clipped") : TEXT("left as it is"));
}

void UVisexMHHairComponent::Refit()
{
	const USkeletalMeshComponent* Parent = Cast<USkeletalMeshComponent>(GetAttachParent());
	if (!Parent)
	{
		return;
	}
	// a card vertex p (head space, hair-asset cm) belongs at Head * (FitScale * p + FitOffset); the
	// AnimBP's copy puts it at Head * p in this component, so the component carries the rest. A
	// uniform scale commutes with the head's rotation, so this is exact in any head pose.
	const FTransform Head = Parent->GetSocketTransform(TEXT("head"), RTS_Component);
	SetRelativeTransform(FTransform(FQuat::Identity,
		Head.GetLocation() * (1.f - FitScale) + Head.GetRotation().RotateVector(FitOffset), FVector(FitScale)));
}

void UVisexMHHairComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Refit();   // the attach parent (the Body) is a tick prerequisite: its head is this frame's
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UVisexMHBodyComponent::ApplyHairRest(bool bFigureShown)
{
	const ACharacter* Her = Cast<ACharacter>(GetOwner());
	const USkeletalMeshComponent* Driver = Her ? Her->GetMesh() : nullptr;
	bool bRest = false;
	if (bFigureShown && Driver && CVarVisexMHHairRest.GetValueOnGameThread() != 0 && CVarVisexMHHair.GetValueOnGameThread() == 1)
	{
		// her head over the ground she is on: 23 cm in FeetUp and LegLift, 77 sitting on the sand
		// (Mermaid), 140 and more standing. Off under 50, on again over 62, so a breath cannot flip it
		const float Up = Driver->GetSocketLocation(TEXT("head")).Z - (Her->GetActorLocation().Z - Her->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		bRest = bHairResting ? Up < 62.f : Up < 50.f;
	}
	IConsoleVariable* Sim = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HairStrands.Simulation"));
	if (bRest != bHairResting)
	{
		bHairResting = bRest;
		HairRestIn = 0;
		if (bRest && Figure)
		{
			// switched off as it is, the strands stay where the fall has thrown them: the simulation is put
			// back to the combed shape first (the groom's own ResetSimulation, by name: no module dependency)
			// and stopped a few frames later, once the solver has taken the reset
			TInlineComponentArray<UPrimitiveComponent*> Prims(Figure);
			for (UPrimitiveComponent* Prim : Prims)
			{
				if (UFunction* Reset = Prim->FindFunction(TEXT("ResetSimulation"))) { Prim->ProcessEvent(Reset, nullptr); }
			}
			HairRestIn = 4;
		}
		else if (Sim)
		{
			Sim->Set(1, ECVF_SetByCode);
		}
		UE_LOG(LogVisexMH, Display, TEXT("VisexMH: hair simulation %s"), bRest ? TEXT("off, her head is near the ground") : TEXT("on"));
	}
	if (HairRestIn > 0 && --HairRestIn == 0 && Sim)
	{
		Sim->Set(0, ECVF_SetByCode);
	}
}

void UVisexMHBodyComponent::ClearHair()
{
	if (Hair)
	{
		Hair->DestroyComponent();
	}
	Hair = nullptr;
	if (IsValid(PresetHair))
	{
		PresetHair->SetHiddenInGame(false);
	}
	PresetHair = nullptr;
	HairStyle = NAME_None;
	LastHairFit = TEXT("?");   // the next style logs its fit
	bHairApplied = false;
}

void UVisexMHBodyComponent::ApplyHair()
{
	const ACharacter* Her = Cast<ACharacter>(GetOwner());
	const UVisexBodyComponent* Body = Her ? Her->FindComponentByClass<UVisexBodyComponent>() : nullptr;
	if (!IsValid(BodyMesh) || !Figure || !Body)
	{
		return;
	}
	// MetaHuman hair by default: the Blueprint's own groom stays and no card hair is made
	const int32 Mode = CVarVisexMHHair.GetValueOnGameThread();
	if (Mode != 0)
	{
		if (GroomMode != Mode)
		{
			ClearHair();   // gives the groom back if the cards had hidden it
			GroomMode = Mode;
			FString Groom = TEXT("no Hair groom on the figure");
			TInlineComponentArray<UPrimitiveComponent*> Prims(Figure);
			for (UPrimitiveComponent* Prim : Prims)
			{
				if (Prim->GetFName() == TEXT("Hair") && !Prim->IsA<USkeletalMeshComponent>())
				{
					Prim->SetHiddenInGame(Mode == 2);   // 2: bald, to time a frame without the hair
					PresetHair = Mode == 2 ? Prim : nullptr;   // ClearHair shows it again
					Groom = FString::Printf(TEXT("%s (%s)"), *Prim->GetClass()->GetName(), Mode == 2 ? TEXT("hidden") : TEXT("shown"));
				}
			}
			UE_LOG(LogVisexMH, Display, TEXT("VisexMH: hair = the MetaHuman's own groom: %s; visex.Body.MetaHuman.Hair 0 wears the card hairstyle instead, 2 none"), *Groom);
		}
		return;
	}
	if (GroomMode != 0)
	{
		ClearHair();
		GroomMode = 0;
	}
	const FString Forced = CVarVisexMHHairStyle.GetValueOnGameThread();
	const FVisexHairStyle& Style = FVisexBodyProfile::HairStyles()[FVisexBodyProfile::HairIndex(Forced.IsEmpty() ? Body->Profile.Hair : FName(*Forced))];
	if (!bHairApplied || Style.Name != HairStyle || (Hair && Hair->GetAttachParent() != BodyMesh))
	{
		ClearHair();
		HairStyle = Style.Name;
		bHairApplied = true;
		// the style's own cards and physics AnimBP (the same assets today's head wears)
		// the cards refitted to this skull if they exist (Tools/mh_hair.py: same bones, the cap and the
		// roots moved onto the new scalp), else the style's own, which the fit alone seats to ~0.4 cm
		const FString Own = FPackageName::ObjectPathToPackageName(FString(Style.Mesh));
		const FString Name = FPackageName::GetShortName(Own) + TEXT("_MH");
		const FString Refit = TEXT("/Game/Character/MH/Hair/") + Name;
		USkeletalMesh* Asset = FPackageName::DoesPackageExist(Refit) ? LoadObject<USkeletalMesh>(nullptr, *(Refit + TEXT(".") + Name)) : nullptr;
		Asset = Asset ? Asset : LoadObject<USkeletalMesh>(nullptr, Style.Mesh);
		UClass* Anim = LoadClass<UAnimInstance>(nullptr, Style.Anim);
		if (!Asset || !Anim)
		{
			UE_LOG(LogVisexMH, Warning, TEXT("VisexMH: hairstyle %s did not load, the preset's groom stays"), *Style.Name.ToString());
			return;
		}
		Hair = NewObject<UVisexMHHairComponent>(Figure, TEXT("HerHair"), RF_Transient);
		Hair->SetupAttachment(BodyMesh);   // the AnimBP copies its attached parent's pose
		Hair->SetCollisionProfileName(TEXT("NoCollision"));
		Hair->SetGenerateOverlapEvents(false);
		Hair->SetSkeletalMeshAsset(Asset);
		Hair->SetAnimInstanceClass(Anim);
		Hair->SetCastShadow(true);
		Hair->SetReceivesDecals(false);
		Hair->SetBoundsScale(2.f);
		const FLightingChannels& Ch = BodyMesh->LightingChannels;
		Hair->SetLightingChannels(Ch.bChannel0, Ch.bChannel1, Ch.bChannel2);
		Hair->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Hair->RegisterComponent();
		// the preset's hair groom goes; brows, lashes and peach fuzz are grooms too and stay
		TInlineComponentArray<UPrimitiveComponent*> Prims(Figure);
		for (UPrimitiveComponent* Prim : Prims)
		{
			if (Prim->GetFName() == TEXT("Hair") && !Prim->IsA<USkeletalMeshComponent>())
			{
				Prim->SetHiddenInGame(true);
				PresetHair = Prim;
			}
		}
		UE_LOG(LogVisexMH, Display, TEXT("VisexMH: hair %s on the MetaHuman head (%s, preset groom %s)"), *Style.Name.ToString(), *Asset->GetName(), PresetHair ? TEXT("hidden") : TEXT("not found"));
	}
	if (!Hair)
	{
		return;
	}
	// the fit: the style's own line in DefaultGame.ini, or the debug cvar's while it is set
	float Scale = 1.f;
	FVector Offset = FVector::ZeroVector;
	FString Own;
	const bool bOwn = GConfig->GetString(TEXT("VisexMHHairFit"), *HairStyle.ToString(), Own, GGameIni) && ParseHairFit(Own, Scale, Offset);
	const FString Debug = CVarVisexMHHairFit.GetValueOnGameThread();
	ParseHairFit(Debug, Scale, Offset);
	if (Debug != LastHairFit)
	{
		LastHairFit = Debug;
		UE_LOG(LogVisexMH, Display, TEXT("VisexMH: hair fit %s scale %.3f offset %s (%s)"), *HairStyle.ToString(), Scale, *Offset.ToString(),
			!Debug.IsEmpty() ? TEXT("visex.Body.MetaHuman.HairFit") : bOwn ? TEXT("DefaultGame.ini") : TEXT("NO LINE in [VisexMHHairFit]"));
	}
	Hair->FitScale = Scale;
	Hair->FitOffset = Offset;
}

/** The reference pose with the face rig curves set; the Face's post-process AnimBP does the rest. */
struct FVisexMHFaceProxy : public FAnimInstanceProxy
{
	explicit FVisexMHFaceProxy(UAnimInstance* In) : FAnimInstanceProxy(In) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override
	{
		FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
		if (const UVisexMHFaceAnimInstance* Inst = Cast<UVisexMHFaceAnimInstance>(InAnimInstance))
		{
			Curves = Inst->Curves;
		}
	}

	virtual bool Evaluate(FPoseContext& Output) override
	{
		Output.ResetToRefPose();
		for (const TPair<FName, float>& Curve : Curves)
		{
			Output.Curve.Set(Curve.Key, Curve.Value);
		}
		return true;
	}

	TArray<TPair<FName, float>> Curves;
};

FAnimInstanceProxy* UVisexMHFaceAnimInstance::CreateAnimInstanceProxy()
{
	return new FVisexMHFaceProxy(this);
}

void UVisexMHBodyComponent::ApplyFace()
{
	if (!IsValid(FaceMesh))
	{
		return;
	}
	FString Want = CVarVisexMHFace.GetValueOnGameThread();
	if (Want == TEXT("off") || Want == TEXT("\"\""))
	{
		Want.Reset();   // the console cannot set an empty value: "off" gives the face back
	}
	if (Want == FaceCurves && (bFaceDriven || Want.IsEmpty()))
	{
		return;
	}
	FaceCurves = Want;
	if (Want.IsEmpty())
	{
		// Zero what was driven first: with no anim class of its own the Face would keep the last
		// curves (a smile stayed on). If the Blueprint gave the Face a class, that comes back.
		if (UVisexMHFaceAnimInstance* Driven = Cast<UVisexMHFaceAnimInstance>(FaceMesh->GetAnimInstance()))
		{
			for (TPair<FName, float>& Curve : Driven->Curves)
			{
				Curve.Value = 0.f;
			}
		}
		if (FaceOwnClass)
		{
			FaceMesh->SetAnimInstanceClass(FaceOwnClass);
			bFaceDriven = false;
		}
		UE_LOG(LogTemp, Display, TEXT("VisexMH: face curves off"));
		return;
	}
	if (!bFaceDriven)
	{
		FaceOwnClass = FaceMesh->GetAnimClass();
		FaceMesh->SetAnimInstanceClass(UVisexMHFaceAnimInstance::StaticClass());
		bFaceDriven = true;
	}
	UVisexMHFaceAnimInstance* Inst = Cast<UVisexMHFaceAnimInstance>(FaceMesh->GetAnimInstance());
	if (!Inst)
	{
		return;
	}
	Inst->Curves.Reset();
	TArray<FString> Pairs;
	Want.ParseIntoArray(Pairs, TEXT(","));
	for (const FString& Pair : Pairs)
	{
		FString Name, Value;
		if (Pair.Split(TEXT("="), &Name, &Value))
		{
			Inst->Curves.Emplace(FName(*Name.TrimStartAndEnd()), FCString::Atof(*Value));
		}
	}
	UE_LOG(LogTemp, Display, TEXT("VisexMH: face curves = %s (%d set; the Face's own class was %s)"), *Want, Inst->Curves.Num(), *GetNameSafe(FaceOwnClass));
}

// ---- the flex layer (stage 8) -------------------------------------------------------------------------
namespace
{
	// the muscles of Tools/mh_flex_build.py: a left and a right of each, then the abs
	enum EVisexFlex { FxBiceps, FxTriceps, FxForearm, FxDeltoid, FxQuad, FxHam, FxCalf, FxGlute, FxOblique, FxLat, FxPairs };
	const TCHAR* const FlexNames[FxPairs] = {TEXT("Biceps"), TEXT("Triceps"), TEXT("Forearm"), TEXT("Deltoid"), TEXT("Quad"),
		TEXT("Ham"), TEXT("Calf"), TEXT("Glute"), TEXT("Oblique"), TEXT("Lat")};
	const int32 FxAbs = FxPairs * 2;

	FString FlexMorph(int32 I)
	{
		return I == FxAbs ? FString(TEXT("FlexAbs")) : FString(TEXT("Flex")) + FlexNames[I / 2] + (I % 2 ? TEXT("R") : TEXT("L"));
	}

	/** 0 up to A, 1 from B, eased between. */
	float Ramp(float X, float A, float B) { return FMath::SmoothStep(A, B, X); }

	/** Degrees between two directions. */
	float Between(const FVector& A, const FVector& B)
	{
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(float(A.GetSafeNormal() | B.GetSafeNormal()), -1.f, 1.f)));
	}
}

void UVisexMHBodyComponent::ApplyFlex(float DeltaTime)
{
	const ACharacter* Her = Cast<ACharacter>(GetOwner());
	const USkeletalMesh* Asset = BodyMesh ? BodyMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Her || !Asset)
	{
		return;
	}
	static TArray<FName> Morphs;
	if (Morphs.Num() == 0)
	{
		for (int32 I = 0; I < FlexCount; ++I) { Morphs.Add(FName(*FlexMorph(I))); }
	}
	const bool bOn = CVarVisexMHFlex.GetValueOnGameThread() != 0 && CVarVisexMHMorphs.GetValueOnGameThread() != 0;
	if (!bOn)
	{
		// off is off at once: the same frame can be compared with and without
		for (int32 I = 0; I < FlexCount; ++I)
		{
			FlexNow[I] = 0.f;
			BodyMesh->SetMorphTarget(Morphs[I], 0.f, false);
		}
		return;
	}
	if (FlexRefMesh.Get() != Asset)
	{
		const FReferenceSkeleton& Ref = Asset->GetRefSkeleton();
		const auto RefAt = [&Ref](const TCHAR* Bone)
		{
			const int32 I = Ref.FindBoneIndex(Bone);
			return I == INDEX_NONE ? FTransform::Identity : FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, I);
		};
		FlexRefQ[0] = RefAt(TEXT("pelvis")).GetRotation();
		FlexRefQ[1] = RefAt(TEXT("spine_05")).GetRotation();
		FlexRefQ[2] = RefAt(TEXT("head")).GetRotation();
		FlexRefAnkle[0] = Between(RefAt(TEXT("foot_l")).GetLocation() - RefAt(TEXT("calf_l")).GetLocation(), RefAt(TEXT("ball_l")).GetLocation() - RefAt(TEXT("foot_l")).GetLocation());
		FlexRefAnkle[1] = Between(RefAt(TEXT("foot_r")).GetLocation() - RefAt(TEXT("calf_r")).GetLocation(), RefAt(TEXT("ball_r")).GetLocation() - RefAt(TEXT("foot_r")).GetLocation());
		FlexRefMesh = Asset;
	}
	// the body's own last pose, in component space: X is her left, Y her front, Z up
	const auto At = [this](const FString& Bone) { return BodyMesh->GetSocketTransform(FName(*Bone), RTS_Component); };
	const auto Pos = [&At](const FString& Bone) { return At(Bone).GetLocation(); };
	const FVector Up = FVector::UpVector, Front(0.f, 1.f, 0.f);
	const FQuat PelvisD = At(TEXT("pelvis")).GetRotation() * FlexRefQ[0].Inverse();
	const FQuat ChestD = At(TEXT("spine_05")).GetRotation() * FlexRefQ[1].Inverse();
	// the chest against the pelvis, in the reference axes
	const FQuat Trunk = PelvisD.Inverse() * ChestD;
	const float Twist = FMath::RadiansToDegrees(Trunk.GetTwistAngle(Up));          // + : her chest turned to her right
	const FVector Lean = Trunk.RotateVector(Up);
	const float BendFront = FMath::RadiansToDegrees(FMath::Atan2(Lean.Y, Lean.Z));   // + : bent forward
	const float BendLeft = FMath::RadiansToDegrees(FMath::Atan2(Lean.X, Lean.Z));    // + : bent to her left
	const FVector PelvisUp = PelvisD.RotateVector(Up), PelvisFront = PelvisD.RotateVector(Front), ChestUp = ChestD.RotateVector(Up);
	const FVector Pelvis = Pos(TEXT("pelvis"));

	float Elbow[2], Wrist[2], Raise[2], Prop[2], HandBack[2], Knee[2], HipBack[2], Plantar[2], Down[2], Load[2], Push[2];
	const FVector Velocity = BodyMesh->GetComponentTransform().InverseTransformVectorNoScale(Her->GetVelocity());
	const float Speed = float(Velocity.Size2D());
	// how far her chest leans back from upright, degrees (she rests her weight on what is behind her)
	const float BackTilt = FMath::RadiansToDegrees(FMath::Atan2(float(-ChestUp.Y), float(ChestUp.Z)));
	FVector Ankle[2], Ball[2];
	float LegLength = 0.f;
	for (int32 S = 0; S < 2; ++S)
	{
		const FString E = S ? TEXT("_r") : TEXT("_l");
		const FVector Shoulder = Pos(TEXT("upperarm") + E), El = Pos(TEXT("lowerarm") + E), Hand = Pos(TEXT("hand") + E), Knuckle = Pos(TEXT("middle_01") + E);
		Elbow[S] = Between(El - Shoulder, Hand - El);
		Wrist[S] = Between(Hand - El, Knuckle - Hand);
		Raise[S] = Between(El - Shoulder, -ChestUp);
		// an arm she props herself on: the hand bent back on something, or her chest leaning back onto
		// her arms (the menu lean: the chest tilts 10-12 degrees back, hands on the stone beside her, the
		// wrists only 20-30 degrees bent); the arm not folded, the hand below the shoulder, and she is
		// not walking. The lean is a rest, not a push-up: 0.85.
		HandBack[S] = float(-((Hand - Shoulder) | ChestD.RotateVector(Front)));
		Prop[S] = FMath::Max(Ramp(Wrist[S], 30.f, 55.f), 0.85f * Ramp(BackTilt, 4.f, 10.f))
			* (1.f - Ramp(Elbow[S], 60.f, 100.f)) * Ramp(float(Shoulder.Z - Hand.Z), 10.f, 25.f) * (1.f - Ramp(Speed, 15.f, 45.f));
		const FVector Hip = Pos(TEXT("thigh") + E), Kn = Pos(TEXT("calf") + E);
		Ankle[S] = Pos(TEXT("foot") + E);
		Ball[S] = Pos(TEXT("ball") + E);
		Knee[S] = Between(Kn - Hip, Ankle[S] - Kn);
		const FVector Thigh = (Kn - Hip).GetSafeNormal();
		HipBack[S] = FMath::Max(0.f, -FMath::RadiansToDegrees(FMath::Atan2(float(Thigh | PelvisFront), float(-(Thigh | PelvisUp)))));
		Plantar[S] = FlexRefAnkle[S] - Between(Ankle[S] - Kn, Ball[S] - Ankle[S]);
		LegLength += 0.5f * float(FVector::Dist(Hip, Kn) + FVector::Dist(Kn, Ankle[S]));
	}
	// which leg carries her: the driver's plant state and where the pelvis stands over the feet. A foot
	// is down when the driver says it is planted or its ball is as low as the other's; the weight goes
	// to the foot the pelvis is over; a sitting or lying body puts none on its legs.
	const UVisexAnimInstance* Anim = Cast<UVisexAnimInstance>(Her->GetMesh() ? Her->GetMesh()->GetAnimInstance() : nullptr);
	const float Floor = float(FMath::Min(Ball[0].Z, Ball[1].Z));
	Down[0] = FMath::Max(1.f - Ramp(float(Ball[0].Z) - Floor, 2.f, 6.f), Anim ? Anim->PlantedL : 0.f);
	Down[1] = FMath::Max(1.f - Ramp(float(Ball[1].Z) - Floor, 2.f, 6.f), Anim ? Anim->PlantedR : 0.f);
	const FVector2D FootL(Ankle[0]), Across = FVector2D(Ankle[1]) - FootL;
	const float Apart = float(Across.Size());
	const float Along = Apart > 1.f ? float(((FVector2D(Pelvis) - FootL) | Across) / (Apart * Apart)) : 0.5f;   // 0 over her left foot, 1 over her right
	const float OverRight = FMath::Lerp(0.5f, Ramp(Along, 0.2f, 0.8f), Ramp(Apart, 6.f, 16.f));
	const float Raw[2] = {(1.f - OverRight) * Down[0], OverRight * Down[1]};
	const float Standing = Ramp((float(Pelvis.Z) - Floor) / FMath::Max(LegLength, 1.f), 0.72f, 0.95f);
	const FVector Heading = Velocity.GetSafeNormal2D();
	for (int32 S = 0; S < 2; ++S)
	{
		Load[S] = Raw[0] + Raw[1] > 0.05f ? Raw[S] / (Raw[0] + Raw[1]) * Standing : 0.f;
		// the walk's push-off: the foot behind her, still down, while she moves
		Push[S] = Ramp(Speed, 30.f, 110.f) * Ramp(float((Pelvis - Ankle[S]) | Heading), 4.f, 22.f) * Down[S];
	}

	float Want[FlexCount] = {};
	for (int32 S = 0; S < 2; ++S)
	{
		const float Tense = Ramp(Load[S], 0.3f, 0.8f);
		const auto Set = [&Want, S](int32 Muscle, float Value) { Want[Muscle * 2 + S] = FMath::Clamp(Value, 0.f, 1.f); };
		Set(FxBiceps, Ramp(Elbow[S], 40.f, 125.f));
		Set(FxTriceps, Prop[S]);
		Set(FxForearm, FMath::Max3(0.55f * Ramp(Elbow[S], 50.f, 125.f), 0.8f * Prop[S], 0.6f * Ramp(Wrist[S], 30.f, 65.f)));
		Set(FxDeltoid, FMath::Max(0.9f * Ramp(Raise[S], 45.f, 110.f), 0.7f * Prop[S]));
		Set(FxLat, FMath::Max(0.8f * Prop[S], 0.7f * Ramp(Raise[S], 115.f, 165.f)));
		Set(FxQuad, Tense * (0.75f + 0.25f * Ramp(Knee[S], 15.f, 60.f)));
		Set(FxHam, FMath::Max(0.8f * Ramp(Knee[S], 45.f, 120.f), 0.35f * Ramp(HipBack[S], 5.f, 30.f)) + 0.35f * Push[S]);
		Set(FxCalf, FMath::Max(Ramp(Plantar[S], 8.f, 35.f), 0.6f * Tense) + 0.4f * Push[S]);
		Set(FxGlute, FMath::Max(0.8f * Ramp(HipBack[S], 6.f, 30.f), 0.6f * Tense) + 0.3f * Push[S]);
		// the oblique on the side she bends to, and the one across from the way she turns
		Set(FxOblique, 0.8f * Ramp(S ? -Twist : Twist, 8.f, 35.f) + 0.7f * Ramp(S ? -BendLeft : BendLeft, 6.f, 25.f));
	}
	Want[FxAbs] = FMath::Clamp(0.85f * Ramp(BendFront, 8.f, 35.f) + 0.5f * Ramp(FMath::Abs(Twist), 10.f, 40.f), 0.f, 1.f);

	// visex.Body.MetaHuman.FlexTest: "1" every morph at that weight, "Quad=1,BicepsL=0.5" those, "log" the drivers
	const FString Test = CVarVisexMHFlexTest.GetValueOnGameThread();
	bool bLog = false;
	TArray<FString> Tokens;
	Test.ParseIntoArray(Tokens, TEXT(","));
	for (const FString& Token : Tokens)
	{
		FString Name, Value;
		if (Token == TEXT("log")) { bLog = true; }
		else if (Token.Split(TEXT("="), &Name, &Value))
		{
			for (int32 I = 0; I < FlexCount; ++I)
			{
				if (FlexMorph(I).Mid(4).StartsWith(Name)) { Want[I] = FCString::Atof(*Value); }
			}
		}
		else if (Token.IsNumeric())
		{
			for (float& W : Want) { W = FCString::Atof(*Token); }
		}
	}
	const float Scale = FMath::Max(0.f, CVarVisexMHFlexScale.GetValueOnGameThread());
	for (int32 I = 0; I < FlexCount; ++I)
	{
		// tensing is quicker than letting go
		FlexNow[I] = FMath::FInterpTo(FlexNow[I], Want[I], DeltaTime, Want[I] > FlexNow[I] ? 7.f : 4.f);
		BodyMesh->SetMorphTarget(Morphs[I], FlexNow[I] * Scale, false);
	}
	FlexLogClock += DeltaTime;
	if (bLog && FlexLogClock >= 1.f)
	{
		FlexLogClock = 0.f;
		FString Line;
		for (int32 I = 0; I < FlexCount; ++I) { Line += FString::Printf(TEXT(" %s %.2f"), *FlexMorph(I).Mid(4), FlexNow[I]); }
		UE_LOG(LogVisexMH, Display, TEXT("VisexFlex: elbow %.0f %.0f wrist %.0f %.0f raise %.0f %.0f hand back %.0f %.0f tilt %.0f prop %.2f %.2f | knee %.0f %.0f hip back %.0f %.0f plantar %.0f %.0f down %.2f %.2f load %.2f %.2f push %.2f %.2f standing %.2f speed %.0f | twist %.0f bend front %.0f left %.0f |%s"),
			Elbow[0], Elbow[1], Wrist[0], Wrist[1], Raise[0], Raise[1], HandBack[0], HandBack[1], BackTilt, Prop[0], Prop[1], Knee[0], Knee[1], HipBack[0], HipBack[1], Plantar[0], Plantar[1],
			Down[0], Down[1], Load[0], Load[1], Push[0], Push[1], Standing, Speed, Twist, BendFront, BendLeft, *Line);
	}
}
