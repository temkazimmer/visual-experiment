#include "VisexBodyComponent.h"

#include "VisexAnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

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

	// morphs: the slider's middle is the base mesh; each side has its own target
	auto Morph = [Mesh](FName Up, FName Down, float V)
	{
		Mesh->SetMorphTarget(Up, FMath::Max(0.f, (V - 0.5f) * 2.f), false);
		Mesh->SetMorphTarget(Down, FMath::Max(0.f, (0.5f - V) * 2.f), false);
	};
	Morph(TEXT("BustUp"), TEXT("BustDown"), Profile.Get("Bust"));
	Morph(TEXT("GlutesUp"), TEXT("GlutesDown"), Profile.Get("Glutes"));

	PushToAnim();
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

bool UVisexBodyComponent::Save() const
{
	UVisexBodySave* Data = Cast<UVisexBodySave>(UGameplayStatics::CreateSaveGameObject(UVisexBodySave::StaticClass()));
	Data->Profile = Profile;
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
