#include "VisexOutfitSubsystem.h"

#include "VisexBodyComponent.h"
#include "VisexShellComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<FString> CVarOutfit(
	TEXT("visex.Outfit"),
	TEXT("/Game/Character/Clothes/SKM_Outfit_Sheer"),
	TEXT("Skeletal mesh newly spawned characters wear, or none."));

static const FName OutfitName(TEXT("Outfit"));

void UVisexOutfitSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SpawnHandle = GetWorld()->AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateUObject(this, &UVisexOutfitSubsystem::OnActorSpawned));
}

void UVisexOutfitSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(SpawnHandle);
	}
	Super::Deinitialize();
}

void UVisexOutfitSubsystem::OnActorSpawned(AActor* Actor)
{
	ACharacter* Character = Cast<ACharacter>(Actor);
	// only our figure: the one with a body shape
	if (!Character || !Character->FindComponentByClass<UVisexBodyComponent>())
	{
		return;
	}
	const FString Path = CVarOutfit.GetValueOnGameThread();
	if (Path.IsEmpty() || Path.Equals(TEXT("none"), ESearchCase::IgnoreCase))
	{
		return;
	}
	if (USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *Path))
	{
		SetOutfit(Character, Mesh);
	}
}

UVisexShellComponent* UVisexOutfitSubsystem::SetOutfit(ACharacter* Character, USkeletalMesh* Outfit)
{
	if (!Character || !Character->GetMesh())
	{
		return nullptr;
	}
	UVisexShellComponent* Comp = nullptr;
	for (UActorComponent* C : Character->GetComponents())
	{
		if (C && C->GetFName() == OutfitName)
		{
			Comp = Cast<UVisexShellComponent>(C);
		}
	}
	if (!Outfit)
	{
		if (Comp)
		{
			Comp->DestroyComponent();
		}
		return nullptr;
	}
	if (!Comp)
	{
		Comp = NewObject<UVisexShellComponent>(Character, OutfitName, RF_Transient);
		Comp->SetupAttachment(Character->GetMesh());
		Comp->SetSkeletalMeshAsset(Outfit);
		Comp->RegisterComponent();
		Character->AddInstanceComponent(Comp);
	}
	else
	{
		Comp->SetSkeletalMeshAsset(Outfit);
	}
	Comp->SetCastShadow(false);
	return Comp;
}
