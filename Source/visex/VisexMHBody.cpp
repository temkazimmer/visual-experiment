#include "VisexMHBody.h"
#include "VisexBodyComponent.h"
#include "VisexShellComponent.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Animation/AnimInstanceProxy.h"
#include "Components/LODSyncComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "TwoBoneIK.h"

DEFINE_LOG_CATEGORY_STATIC(LogVisexMH, Log, All);

static TAutoConsoleVariable<int32> CVarVisexMHBody(
	TEXT("visex.Body.MetaHuman"), 0,
	TEXT("1: draw her as the assembled MetaHuman (body, face, grooms) following today's character's pose. 0: today's body, untouched."));

static TAutoConsoleVariable<int32> CVarVisexMHCorrectives(
	TEXT("visex.Body.MetaHuman.Correctives"), 1,
	TEXT("0: the MetaHuman body's post-process AnimBP (RigLogic twist and corrective joints) is switched off, to compare."));

static TAutoConsoleVariable<int32> CVarVisexMHLOD(
	TEXT("visex.Body.MetaHuman.LOD"), -1,
	TEXT("Debug: holds the MetaHuman's LOD Sync at this LOD (0-7); -1 gives it back to the view."));

static TAutoConsoleVariable<int32> CVarVisexMHFootPin(
	TEXT("visex.Body.MetaHuman.FootPin"), 1,
	TEXT("1: each MetaHuman sole is pinned to the driver's sole (two-bone leg solve, no ground trace). 0: rotations only."));

// "bone:roll,pitch,yaw;bone:..." in each bone's own axes, added on top of the followed pose
static TAutoConsoleVariable<FString> CVarVisexMHTestPose(
	TEXT("visex.Body.MetaHuman.TestPose"), TEXT(""),
	TEXT("Debug: extra local rotations on MetaHuman bones, e.g. \"calf_l:0,0,-120;upperarm_r:0,80,0\"; \"ref;...\" poses the reference pose instead of the followed one. Switches the foot pin off."));

namespace
{
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
		// no vertex data on the CPU (a cooked build): the ball joint rides 2.15 cm over the sole
		Low = Ball.Z - 2.15f;
		UE_LOG(LogVisexMH, Warning, TEXT("VisexMH: no CPU vertices for %s, sole taken %.2f cm under the ball joint"), *GetNameSafe(Mesh), 2.15f);
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
	// legs: where each sole sits against its foot bone on both bodies (the MetaHuman's ankle is
	// higher and its foot longer), as one fixed offset in the foot bone's axes
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
		AnkleM = TF.Z - Sole;
		BallM = TB.Z - Sole;
		AnkleD = SF.Z * Scale * Feet;   // the driver's soles are on its origin
		const FQuat TQi = TCS[L.Foot].GetRotation().Inverse(), SQi = SCS[L.SrcFoot].GetRotation().Inverse();
		FVector Bm = TB - TF, Bd = (SB - SF) * Scale * Feet;
		Bm.Z = -AnkleM;
		Bd.Z = -AnkleD;
		const FVector Hm = TQi.RotateVector(FVector(0.f, 0.f, -AnkleM)), Hd = SQi.RotateVector(FVector(0.f, 0.f, -AnkleD));
		// halfway between matching under the ankle and under the ball: fixed, so it adds no slide
		L.SoleOffset = ((Hd - Hm) + (SQi.RotateVector(Bd) - TQi.RotateVector(Bm))) * 0.5f;
		L.ToeLocal = TQi.RotateVector(TB - TF).GetSafeNormal();
		L.Length = FVector::Dist(TCS[L.Thigh].GetTranslation(), TCS[L.Calf].GetTranslation()) + FVector::Dist(TCS[L.Calf].GetTranslation(), TF);
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
	if (BuiltSource != SourceMesh || BuiltTarget != TargetMesh || Scale != Inst->SourceScale || Feet != Inst->FeetScale)
	{
		Scale = Inst->SourceScale;
		Feet = Inst->FeetScale;
		Build(SourceMesh, TargetMesh, Inst);
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
	}
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
	if (Figure)
	{
		Figure->Destroy();
	}
	Figure = nullptr;
	BodyMesh = nullptr;
	FaceMesh = nullptr;
	HideDriver(false);
	bShown = false;
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
	const int32 LOD = Debug >= 0 ? Debug : Garment ? 0 : -1;
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
	const FName Want = Body ? Body->Profile.Outfit : FName(NAME_None);
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
