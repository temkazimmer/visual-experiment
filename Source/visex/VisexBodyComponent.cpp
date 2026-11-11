#include "VisexBodyComponent.h"

#include "VisexAnimInstance.h"
#include "VisexShellComponent.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"

// The skirt's cloth sim is off by default: skinned by her pose it reads cleanest (a free hem
// springs back to its reference-pose shape and leaves a ridge, Docs/CLOTHES.md) and it costs
// nothing, where the sim cost 50-70 ms of game thread in a packaged build.
static TAutoConsoleVariable<bool> CVarVisexClothSim(
	TEXT("visex.ClothSim"), false,
	TEXT("Simulate outfit cloth (the skirt). Off: the cloth piece is skinned to her pose. Takes effect on the next outfit change."));

UVisexBodyComponent::UVisexBodyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = false;
}

USkeletalMeshComponent* UVisexBodyComponent::Body() const
{
	const ACharacter* Char = Cast<ACharacter>(GetOwner());
	return Char ? Char->GetMesh() : nullptr;
}

void UVisexBodyComponent::OnRegister()
{
	Super::OnRegister();
	Apply();
}

void UVisexBodyComponent::BeginPlay()
{
	Super::BeginPlay();
	if (USkeletalMeshComponent* Mesh = Body())
	{
		Mesh->SetReceivesDecals(false);    // no sand footprints on her (shells, outfit, hair, cloth too)
	}
	if (bLoadOnBeginPlay)
	{
		Load();
	}
	Apply();
}

void UVisexBodyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	// the anim instance can be rebuilt (mesh or class change); keep it fed
	PushToAnim();
	if (bJiggle)
	{
		TickJiggle(DeltaTime);
	}
	TickCorrectives();
	TickCloth(DeltaTime);
}

void UVisexBodyComponent::TickCorrectives()
{
	USkeletalMeshComponent* Mesh = Body();
	if (!Mesh || Mesh->GetBoneIndex(TEXT("upperarm_l")) == INDEX_NONE)
	{
		return;
	}
	// measured as in the sculpt: the upper arm against the torso's down axis (pelvis to neck)
	const FVector TorsoUp = (Mesh->GetBoneLocation(TEXT("neck_01"), EBoneSpaces::ComponentSpace)
		- Mesh->GetBoneLocation(TEXT("pelvis"), EBoneSpaces::ComponentSpace)).GetSafeNormal();
	static const FName Upper[2] = {TEXT("upperarm_l"), TEXT("upperarm_r")};
	static const FName Lower[2] = {TEXT("lowerarm_l"), TEXT("lowerarm_r")};
	static const FName Morph[2] = {TEXT("ArmUp_l"), TEXT("ArmUp_r")};
	for (int32 i = 0; i < 2; ++i)
	{
		const FVector Arm = (Mesh->GetBoneLocation(Lower[i], EBoneSpaces::ComponentSpace)
			- Mesh->GetBoneLocation(Upper[i], EBoneSpaces::ComponentSpace)).GetSafeNormal();
		const float Rise = 180.f - FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Arm, TorsoUp), -1.f, 1.f)));
		const float T = FMath::Clamp((Rise - ArmUpStart) / FMath::Max(ArmUpFull - ArmUpStart, 1.f), 0.f, 1.f);
		Mesh->SetMorphTarget(Morph[i], FMath::SmoothStep(0.f, 1.f, T), false);
	}
	// hip flexion (sitting, kneeling, lying): the thigh against the same axis, 0 = standing
	static const FName Thigh[2] = {TEXT("thigh_l"), TEXT("thigh_r")};
	static const FName Calf[2] = {TEXT("calf_l"), TEXT("calf_r")};
	static const FName ThighMorph[2] = {TEXT("ThighUp_l"), TEXT("ThighUp_r")};
	for (int32 i = 0; i < 2; ++i)
	{
		const FVector Leg = (Mesh->GetBoneLocation(Calf[i], EBoneSpaces::ComponentSpace)
			- Mesh->GetBoneLocation(Thigh[i], EBoneSpaces::ComponentSpace)).GetSafeNormal();
		const float Flex = 180.f - FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Leg, TorsoUp), -1.f, 1.f)));
		const float T = FMath::Clamp((Flex - ThighUpStart) / FMath::Max(ThighUpFull - ThighUpStart, 1.f), 0.f, 1.f);
		Mesh->SetMorphTarget(ThighMorph[i], FMath::SmoothStep(0.f, 1.f, T), false);
	}
}

void UVisexBodyComponent::PushToAnim() const
{
	if (USkeletalMeshComponent* Mesh = Body())
	{
		if (UVisexAnimInstance* Anim = Cast<UVisexAnimInstance>(Mesh->GetAnimInstance()))
		{
			Anim->BodyProfile = Profile;
		}
	}
}

void UVisexBodyComponent::Apply()
{
	ACharacter* Char = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Mesh = Body();
	if (!Char || !Mesh)
	{
		return;
	}

	// height: the whole figure
	const float S = Profile.Get("Height") / ReferenceHeight;

	// she grows from the soles: the reshape scales the hip-to-ankle line by Legs and the foot
	// (ankle to sole) by Feet, so the soles drop that far below the root. Lift the mesh and grow
	// the capsule by exactly that drop, measured on the reference pose (soles at Z 0).
	float LegDelta = 0.f;
	if (const USkeletalMesh* Asset = Mesh->GetSkeletalMeshAsset())
	{
		const FReferenceSkeleton& Ref = Asset->GetRefSkeleton();
		const auto RefZ = [&Ref](const TCHAR* Bone)
		{
			FTransform T = FTransform::Identity;
			for (int32 i = Ref.FindBoneIndex(Bone); i != INDEX_NONE; i = Ref.GetParentIndex(i))
			{
				T = T * Ref.GetRefBonePose()[i];
			}
			return float(T.GetLocation().Z);
		};
		if (Ref.FindBoneIndex(TEXT("thigh_l")) != INDEX_NONE && Ref.FindBoneIndex(TEXT("foot_l")) != INDEX_NONE)
		{
			const float Ankle = RefZ(TEXT("foot_l"));               // 8.1 cm on Quinn
			const float HipToAnkle = RefZ(TEXT("thigh_l")) - Ankle; // 87.3 cm
			LegDelta = S * ((Profile.Get("Legs") - 1.f) * HipToAnkle + (Profile.Get("Feet") - 1.f) * Ankle);
		}
	}

	Mesh->SetRelativeScale3D(FVector(S));
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, ReferenceMeshZ * S + LegDelta * 0.5f));
	if (UCapsuleComponent* Capsule = Char->GetCapsuleComponent())
	{
		// the capsule grows about its middle: lift her by the change so its bottom stays on the
		// ground and she grows up from her soles (live, while a slider moves)
		const float OldHalf = Capsule->GetUnscaledCapsuleHalfHeight();
		const float NewHalf = ReferenceCapsule.X * S + LegDelta * 0.5f;
		Capsule->SetCapsuleSize(ReferenceCapsule.Y * S, NewHalf);
		const UCharacterMovementComponent* Move = Char->GetCharacterMovement();
		if (HasBegunPlay() && GetWorld() && GetWorld()->IsGameWorld() && Move && !Move->IsFalling()
			&& !FMath::IsNearlyEqual(OldHalf, NewHalf, 0.01f))
		{
			Char->AddActorWorldOffset(FVector(0.f, 0.f, (NewHalf - OldHalf) * Capsule->GetComponentScale().Z), false, nullptr, ETeleportType::TeleportPhysics);
		}
	}

	// shape morphs: 0 = the base body, 1 = the full sculpt (Tools/blender_skin.py)
	Mesh->SetMorphTarget(TEXT("BustUp"), Profile.Get("Bust"), false);
	Mesh->SetMorphTarget(TEXT("GlutesUp"), Profile.Get("Glutes"), false);
	// the middle (Tools/blender_torso.py)
	float Lines, Pack;
	FVisexBodyProfile::AbsWeights(Profile.Get("Abs"), Profile.Get("Fat"), Lines, Pack);
	Mesh->SetMorphTarget(TEXT("Fat"), Profile.Get("Fat"), false);
	Mesh->SetMorphTarget(TEXT("AbsLines"), Lines, false);
	Mesh->SetMorphTarget(TEXT("AbsPack"), Pack, false);
	Mesh->SetMorphTarget(TEXT("ThighGap"), Profile.Get("ThighGap"), false);
	// spacing is sculpted against the full bust: scale it by how much bust she has, so the
	// cleavage it carves never digs into a flat chest
	Mesh->SetMorphTarget(TEXT("BustApart"), Profile.Get("BustApart") * 1.2f * FMath::Min(Profile.Get("Bust") / 0.7f, 1.2f), false);
	// bust height rides the bounce morphs: a standing offset the jiggle adds to
	for (const TCHAR* Side : {TEXT("L"), TEXT("R")})
	{
		const float Lift = Profile.Get("BustLift") * BustLiftRange;
		Mesh->SetMorphTarget(FName(FString::Printf(TEXT("BustBounce%s_Up"), Side)), FMath::Max(0.f, Lift), false);
		Mesh->SetMorphTarget(FName(FString::Printf(TEXT("BustBounce%s_Down"), Side)), FMath::Max(0.f, -Lift), false);
	}

	ApplySkin();
	ApplyHair();
	ApplyOutfit();
	PushToAnim();
}

void UVisexBodyComponent::ApplyHair()
{
	// game worlds only, like the outfit: a runtime component, never saved with the level
	USkeletalMeshComponent* Mesh = Body();
	const UWorld* World = GetWorld();
	if ((HairMesh && WornHair == Profile.Hair) || !Mesh || !World || !World->IsGameWorld() || !GetOwner())
	{
		return;
	}
	const FVisexHairStyle& Style = FVisexBodyProfile::HairStyles()[FVisexBodyProfile::HairIndex(Profile.Hair)];
	USkeletalMesh* Asset = LoadObject<USkeletalMesh>(nullptr, Style.Mesh);
	UClass* Anim = LoadClass<UAnimInstance>(nullptr, Style.Anim);
	if (!Asset || !Anim)
	{
		return;
	}
	WornHair = Profile.Hair;
	if (HairMesh)
	{
		// another style: same component, new cards and their own physics
		HairMesh->SetSkeletalMeshAsset(Asset);
		HairMesh->SetAnimInstanceClass(Anim);
		return;
	}
	// on the body mesh: the hair's AnimBP copies the pose from its attached parent, then
	// Kawaii Physics swings the locks. Its own anim class, so no leader pose.
	HairMesh = NewObject<USkeletalMeshComponent>(GetOwner(), TEXT("Hair"), RF_Transient);
	HairMesh->SetupAttachment(Mesh);
	HairMesh->SetCollisionProfileName(TEXT("NoCollision"));
	HairMesh->SetGenerateOverlapEvents(false);
	HairMesh->SetSkeletalMeshAsset(Asset);
	HairMesh->SetAnimInstanceClass(Anim);
	HairMesh->SetCastShadow(true);
	HairMesh->SetReceivesDecals(false);
	const FLightingChannels& Ch = Mesh->LightingChannels;
	HairMesh->SetLightingChannels(Ch.bChannel0, Ch.bChannel1, Ch.bChannel2);
	HairMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	HairMesh->RegisterComponent();
}

void UVisexBodyComponent::ApplySkin()
{
	// game worlds only: a dynamic material set in the editor would land in the saved level
	if (!GetWorld() || !GetWorld()->IsGameWorld())
	{
		return;
	}
	// the skin is the Shell (the body itself is hidden); outfits are shells too, skip them
	if (!SkinMaterial)
	{
		TArray<UVisexShellComponent*> Shells;
		GetOwner()->GetComponents(Shells);
		for (UVisexShellComponent* Shell : Shells)
		{
			if (Shell != OutfitMesh && Shell->GetFName() == TEXT("Shell"))
			{
				SkinMaterial = Shell->CreateDynamicMaterialInstance(0);
				break;
			}
		}
	}
	if (SkinMaterial)
	{
		SkinMaterial->SetVectorParameterValue(TEXT("SkinColor"), FVisexBodyProfile::SkinColor(Profile.Get("Skin")));
	}
}

void UVisexBodyComponent::SetValue(FName Name, float Value)
{
	Profile.Set(Name, Value);
	Apply();
}

void UVisexBodyComponent::ResetValue(FName Name)
{
	Profile.Values.Remove(Name);
	Apply();
}

void UVisexBodyComponent::SetOutfit(FName Outfit)
{
	// a locked costume can be looked at in the wardrobe, not worn
	const int32 I = FVisexBodyProfile::Outfits().IndexOfByPredicate([Outfit](const FVisexOutfit& O) { return O.Name == Outfit; });
	if (I != INDEX_NONE && FVisexBodyProfile::Outfits()[I].bLocked)
	{
		return;
	}
	Profile.Outfit = Outfit;
	ApplyOutfit();
}

void UVisexBodyComponent::SetHair(FName Hair)
{
	Profile.Hair = Hair;
	ApplyHair();
}

void UVisexBodyComponent::ApplyOutfit()
{
	USkeletalMeshComponent* Mesh = Body();
	const UWorld* World = GetWorld();
	if (!Mesh || !World || !World->IsGameWorld()
		|| (WornOutfit == Profile.Outfit && (OutfitMesh || Profile.Outfit.IsNone())))
	{
		return;
	}
	const FVisexOutfit& O = FVisexBodyProfile::Outfits()[FVisexBodyProfile::OutfitIndex(Profile.Outfit)];
	USkeletalMesh* Asset = O.Mesh.IsEmpty() ? nullptr : LoadObject<USkeletalMesh>(nullptr, *O.Mesh);
	if (Asset && !OutfitMesh && GetOwner())
	{
		// at runtime only: the outfit follows the body's pose and morphs (Leader Pose)
		OutfitMesh = NewObject<UVisexShellComponent>(GetOwner(), TEXT("Outfit"), RF_Transient);
		OutfitMesh->SetupAttachment(Mesh);
		OutfitMesh->RegisterComponent();
	}
	if (OutfitMesh)
	{
		OutfitMesh->SetSkeletalMeshAsset(Asset);
		OutfitMesh->SetVisibility(Asset != nullptr);
		OutfitMesh->SetLeaderPoseComponent(Mesh, true);
		// clothes shadow her skin and the ground (shells don't by default), and take the same
		// lights as the body: the Character page lights her on their own lighting channel
		OutfitMesh->SetCastShadow(true);
		const FLightingChannels& Ch = Mesh->LightingChannels;
		OutfitMesh->SetLightingChannels(Ch.bChannel0, Ch.bChannel1, Ch.bChannel2);
	}
	// the cloth piece: made for a row that names one, destroyed on any other outfit or None
	UChaosClothAsset* Cloth = O.Cloth.IsEmpty() ? nullptr : LoadObject<UChaosClothAsset>(nullptr, *O.Cloth);
	if (!Cloth && ClothMesh)
	{
		ClothMesh->DestroyComponent();
		ClothMesh = nullptr;
	}
	if (Cloth && GetOwner())
	{
		if (!ClothMesh)
		{
			ClothMesh = NewObject<UChaosClothComponent>(GetOwner(), TEXT("OutfitCloth"), RF_Transient);
			ClothMesh->SetupAttachment(Mesh);
			ClothMesh->SetEnableSimulation(CVarVisexClothSim.GetValueOnGameThread());   // off: no tick, skinned only
			ClothMesh->RegisterComponent();
		}
		ClothMesh->SetEnableSimulation(CVarVisexClothSim.GetValueOnGameThread());
		ClothMesh->SetAsset(Cloth);
		ClothMesh->SetLeaderPoseComponent(Mesh, true);
		ClothMesh->SetCastShadow(true);
		ClothMesh->SetReceivesDecals(false);
		const FLightingChannels& Ch = Mesh->LightingChannels;
		ClothMesh->SetLightingChannels(Ch.bChannel0, Ch.bChannel1, Ch.bChannel2);
		ClothMesh->ForceNextUpdateTeleportAndReset();
	}
	WornOutfit = Profile.Outfit;
}

void UVisexBodyComponent::TickCloth(float DeltaTime)
{
	USkeletalMeshComponent* Mesh = Body();
	if (!ClothMesh || !ClothMesh->IsSimulationEnabled() || !Mesh || Mesh->GetBoneIndex(TEXT("thigh_l")) == INDEX_NONE)
	{
		return;
	}
	// a pose snap moves the legs further in one frame than walking or posing can: start the
	// sim over from the skinned pose (actor teleports are caught by the component's own thresholds).
	// Not on a slow frame: a restart puts the hem where the skinned pose has it, which can be
	// inside a thigh, and the collider may then push a panel out on the wrong side of the leg.
	static const FName Bones[3] = {TEXT("pelvis"), TEXT("calf_l"), TEXT("calf_r")};
	const float Scale = FMath::Max(Mesh->GetComponentScale().Z, KINDA_SMALL_NUMBER);
	bool bSnap = false;
	for (int32 i = 0; i < 3; ++i)
	{
		const FVector P = Mesh->GetBoneLocation(Bones[i], EBoneSpaces::ComponentSpace);
		bSnap |= FVector::Dist(P, ClothLast[i]) / Scale > 25.f;   // body cm in one frame
		ClothLast[i] = P;
	}
	if (bSnap)
	{
		ClothMesh->ForceNextUpdateTeleportAndReset();
	}
}

void UVisexBodyComponent::TickJiggle(float DeltaTime)
{
	USkeletalMeshComponent* Mesh = Body();
	if (!Mesh || DeltaTime <= 0.f)
	{
		return;
	}
	// each spring hangs on the bone the tissue sits on: the chest bob plus the shoulder on
	// that side (arm swing alternates them), the hip joint on that side (stride alternates)
	static const FName Chest(TEXT("spine_05"));
	static const FName Hangers[4] = {TEXT("clavicle_l"), TEXT("clavicle_r"), TEXT("thigh_l"), TEXT("thigh_r")};
	static const FName Knees[2] = {TEXT("calf_l"), TEXT("calf_r")};
	static const FName Up[4] = {TEXT("BustBounceL_Up"), TEXT("BustBounceR_Up"), TEXT("GluteBounceL_Up"), TEXT("GluteBounceR_Up")};
	static const FName Down[4] = {TEXT("BustBounceL_Down"), TEXT("BustBounceR_Down"), TEXT("GluteBounceL_Down"), TEXT("GluteBounceR_Down")};

	const float Scale = Mesh->GetComponentScale().Z;   // the springs work in body cm
	const bool bTeleport = DeltaTime > 0.1f;
	// standing still the tissue settles: extra damping fades in as she slows to a stop
	const AActor* Mover = GetOwner();
	const float Speed = Mover ? Mover->GetVelocity().Size2D() : 0.f;
	Stillness = FMath::FInterpTo(Stillness, 1.f - FMath::Clamp(Speed / 150.f, 0.f, 1.f), DeltaTime, 3.f);
	const float Settle = Stillness * StandingDamping;
	for (int32 i = 0; i < 4; ++i)
	{
		const bool bBust = i < 2;
		float Z = Mesh->GetBoneLocation(Hangers[i]).Z;
		if (bBust)
		{
			Z = FMath::Lerp(Mesh->GetBoneLocation(Chest).Z, Z, BustSideFollow);
		}
		else
		{
			// a point down that side's thigh: its own leg's swing moves it, not the shared pelvis
			Z = FMath::Lerp(Z, Mesh->GetBoneLocation(Knees[i - 2]).Z, GluteLegFollow);
		}
		Z /= FMath::Max(Scale, 0.01f);
		FVisexJiggleState& J = Jiggle[i];
		if (!J.bStarted || bTeleport)
		{
			J = FVisexJiggleState();
			J.LastZ = Z;
			J.bStarted = true;
			continue;
		}
		const float Spread = bBust ? BustDetune : GluteDetune;
		const float Detune = (i % 2 == 0) ? 1.f - Spread : 1.f + Spread;
		const float Travel = bBust ? BustTravel : GluteTravel;
		StepSpring(J, Z, DeltaTime, (bBust ? BustFrequency : GluteFrequency) * Detune, (bBust ? BustDamping : GluteDamping) + Settle,
			SoftLimit * Travel);
		// bigger is heavier: a small bust barely moves
		const float Size = Profile.Get(bBust ? FName("Bust") : FName("Glutes"));
		const float Raw = J.Offset / Travel * (bBust ? BustStrength : JiggleStrength) * (0.25f + Size);
		// soft tissue eases into its limit instead of hitting a stop, so each side rolls on
		// its own rather than both slamming the end together
		float Weight = FMath::Tanh(Raw * (bBust ? 1.f : 0.8f));
		if (bBust)
		{
			Weight += Profile.Get("BustLift") * BustLiftRange;   // the standing bust height
		}
		Mesh->SetMorphTarget(Up[i], FMath::Max(0.f, Weight), false);
		Mesh->SetMorphTarget(Down[i], FMath::Max(0.f, -Weight), false);
	}

	// the bust also swings on the chest: sideways (the gait's weight shift, a turn) and
	// forward/back (each shoulder's twist with the stride, starting and stopping), each
	// breast on its own spring, measured in her own frame
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	const FVector Left = -Owner->GetActorRightVector();
	const FVector Fwd = Owner->GetActorForwardVector();
	static const FName SwayOut[2] = {TEXT("BustSwayL_Out"), TEXT("BustSwayR_Out")};
	static const FName SwayIn[2] = {TEXT("BustSwayL_In"), TEXT("BustSwayR_In")};
	static const FName SwayFwd[2] = {TEXT("BustSwayL_Fwd"), TEXT("BustSwayR_Fwd")};
	static const FName SwayBack[2] = {TEXT("BustSwayL_Back"), TEXT("BustSwayR_Back")};
	const float Gain = BustSwayStrength * (0.25f + Profile.Get("Bust")) / FMath::Max(BustSwayTravel, 0.01f);
	for (int32 i = 0; i < 2; ++i)
	{
		const FVector P = FMath::Lerp(Mesh->GetBoneLocation(Chest), Mesh->GetBoneLocation(Hangers[i]), BustSideFollow)
			/ FMath::Max(Scale, 0.01f);
		const float Detune = (i == 0) ? 1.f - BustDetune : 1.f + BustDetune;
		FVisexJiggleState& S = Sway[i];
		FVisexJiggleState& F = Sway[i + 2];
		if (bTeleport)
		{
			S.bStarted = F.bStarted = false;
		}
		// her frame turns under the tissue: track the axis position by integrating the bone's
		// velocity along the current axes, not by projecting the world position
		const float PrevS = S.LastZ, PrevF = F.LastZ;
		const FVector Last = S.bStarted ? SwayLast[i] : P;
		SwayLast[i] = P;
		const FVector Move = P - Last;
		StepSpring(S, PrevS + FVector::DotProduct(Move, Left), DeltaTime, BustSwayFrequency * Detune, BustSwayDamping + Settle,
			SoftLimit * BustSwayTravel);
		StepSpring(F, PrevF + FVector::DotProduct(Move, Fwd), DeltaTime, BustSwayFrequency * Detune * 1.15f, BustSwayDamping + Settle,
			SoftLimit * BustSwayTravel);
		// her left breast swings out to her left; the right one out to her right
		const float Out = FMath::Tanh(S.Offset * Gain) * (i == 0 ? 1.f : -1.f);
		const float Surge = FMath::Tanh(F.Offset * Gain);
		Mesh->SetMorphTarget(SwayOut[i], FMath::Max(0.f, Out), false);
		Mesh->SetMorphTarget(SwayIn[i], FMath::Max(0.f, -Out), false);
		Mesh->SetMorphTarget(SwayFwd[i], FMath::Max(0.f, Surge), false);
		Mesh->SetMorphTarget(SwayBack[i], FMath::Max(0.f, -Surge), false);
	}
}

void UVisexBodyComponent::StepSpring(FVisexJiggleState& J, float X, float DeltaTime, float Hz, float Zeta, float Limit)
{
	if (!J.bStarted)
	{
		J = FVisexJiggleState();
		J.LastZ = X;
		J.bStarted = true;
		return;
	}
	// a jump no step makes (the body reshaped, the menu placing her, a respawn) moves the
	// tissue with the bone instead of flinging it
	if (FMath::Abs(X - J.LastZ) > 20.f)
	{
		J.LastZ = X;
		return;
	}
	// the tissue is left behind by the bone's move, then pulled back by a damped spring
	const float BoneVelocity = (X - J.LastZ) / DeltaTime;
	J.LastZ = X;
	const float W = 2.f * PI * Hz;
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(DeltaTime * W * 2.f), 1, 8);
	const float H = DeltaTime / Steps;
	for (int32 s = 0; s < Steps; ++s)
	{
		// Offset = tissue - bone; its acceleration = spring + damping - the bone's acceleration,
		// which we feed as the change in relative velocity
		const float Rel = J.Velocity - BoneVelocity;
		// tissue stiffens as it stretches: the spring hardens past Limit instead of hitting a
		// stop, so a hard step eases into the end of its travel and swings back, never flat-tops
		const float Stretch = J.Offset / FMath::Max(Limit, 0.01f);
		const float Acc = -W * W * J.Offset * (1.f + Stretch * Stretch) - 2.f * Zeta * W * Rel;
		J.Velocity += Acc * H;
		J.Offset += (J.Velocity - BoneVelocity) * H;
	}
	J.Offset = FMath::Clamp(J.Offset, -4.f * Limit, 4.f * Limit);   // only a safety net
}

bool UVisexBodyComponent::Save() const
{
	return SaveProfile(Profile);
}

bool UVisexBodyComponent::SaveProfile(const FVisexBodyProfile& Saved) const
{
	UVisexBodySave* Data = Cast<UVisexBodySave>(UGameplayStatics::CreateSaveGameObject(UVisexBodySave::StaticClass()));
	Data->Profile = Saved;
	return UGameplayStatics::SaveGameToSlot(Data, SaveSlot, 0);
}

bool UVisexBodyComponent::Load()
{
	if (!UGameplayStatics::DoesSaveGameExist(SaveSlot, 0))
	{
		return false;
	}
	if (const UVisexBodySave* Data = Cast<UVisexBodySave>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0)))
	{
		Profile = Data->Profile;
		return true;
	}
	return false;
}
