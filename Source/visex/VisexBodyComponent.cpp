#include "VisexBodyComponent.h"

#include "VisexAnimInstance.h"
#include "VisexShellComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"

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

	// leg length lowers the feet below the root; lift the mesh and grow the capsule
	float LegDelta = 0.f;
	if (const USkeletalMesh* Asset = Mesh->GetSkeletalMeshAsset())
	{
		const FReferenceSkeleton& Ref = Asset->GetRefSkeleton();
		const int32 Calf = Ref.FindBoneIndex(TEXT("calf_l"));
		const int32 Foot = Ref.FindBoneIndex(TEXT("foot_l"));
		if (Calf != INDEX_NONE && Foot != INDEX_NONE)
		{
			const float LegLength = Ref.GetRefBonePose()[Calf].GetTranslation().Size()
				+ Ref.GetRefBonePose()[Foot].GetTranslation().Size();
			LegDelta = LegLength * S * (Profile.Get("Legs") - 1.f);
		}
	}

	Mesh->SetRelativeScale3D(FVector(S));
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, ReferenceMeshZ * S + LegDelta * 0.5f));
	if (UCapsuleComponent* Capsule = Char->GetCapsuleComponent())
	{
		Capsule->SetCapsuleSize(ReferenceCapsule.Y * S, ReferenceCapsule.X * S + LegDelta * 0.5f);
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
	ApplyOutfit();
	PushToAnim();
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
	Profile.Outfit = Outfit;
	ApplyOutfit();
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
	WornOutfit = Profile.Outfit;
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
		// the tissue is left behind by the bone's move, then pulled back by a damped spring
		const float BoneVelocity = (Z - J.LastZ) / DeltaTime;
		J.LastZ = Z;
		const float Spread = bBust ? BustDetune : GluteDetune;
		const float Detune = (i % 2 == 0) ? 1.f - Spread : 1.f + Spread;
		const float W = 2.f * PI * (bBust ? BustFrequency : GluteFrequency) * Detune;
		const float Zeta = bBust ? BustDamping : GluteDamping;
		const int32 Steps = FMath::Clamp(FMath::CeilToInt(DeltaTime * W * 2.f), 1, 8);
		const float H = DeltaTime / Steps;
		for (int32 s = 0; s < Steps; ++s)
		{
			// Offset = tissue - bone; its acceleration = spring + damping - the bone's acceleration,
			// which we feed as the change in relative velocity
			const float Rel = J.Velocity - BoneVelocity;
			const float Acc = -W * W * J.Offset - 2.f * Zeta * W * Rel;
			J.Velocity += Acc * H;
			J.Offset += (J.Velocity - BoneVelocity) * H;
		}
		const float Travel = bBust ? BustTravel : GluteTravel;
		J.Offset = FMath::Clamp(J.Offset, -2.5f * Travel, 2.5f * Travel);
		// bigger is heavier: a small bust barely moves
		const float Size = Profile.Get(bBust ? FName("Bust") : FName("Glutes"));
		const float Raw = J.Offset / Travel * JiggleStrength * (0.25f + Size);
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
