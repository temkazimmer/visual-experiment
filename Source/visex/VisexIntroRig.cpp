#include "VisexIntroRig.h"

#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "VisexMenuDirector.h"

AVisexIntroRig::AVisexIntroRig()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	CharacterClass = TSoftClassPtr<ACharacter>(FSoftObjectPath(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C")));
}

void AVisexIntroRig::BeginPlay()
{
	Super::BeginPlay();
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	// the director polls for a pawn to put in the menu: stop it before she exists
	for (TActorIterator<AVisexMenuDirector> It(World); It; ++It)
	{
		World->GetTimerManager().ClearAllTimersForObject(*It);
		It->SetActorTickEnabled(false);
	}

	UClass* Cls = CharacterClass.LoadSynchronous();
	if (!Cls)
	{
		return;
	}
	const ACharacter* Def = Cls->GetDefaultObject<ACharacter>();
	const float Half = Def->GetCapsuleComponent() ? Def->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() : 90.f;
	const FVector Loc = GetActorLocation() + FVector(0.f, 0.f, Half + 2.f);
	const FRotator Rot(0.f, GetActorRotation().Yaw, 0.f);
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Her = World->SpawnActor<ACharacter>(Cls, Loc, Rot, P);
	if (!Her)
	{
		return;
	}
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		// the sequence's camera cut owns the view; possessing would hand it to her boom camera
		PC->bAutoManageActiveCameraTarget = false;
		PC->Possess(Her);
		PC->SetControlRotation(Rot);
	}
}

void AVisexIntroRig::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Her && Walk > 0.001f)
	{
		Her->AddMovementInput(GetActorForwardVector(), FMath::Clamp(Walk, 0.f, 1.f));
	}
}

void AVisexIntroRig::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Her)
	{
		Her->Destroy();
		Her = nullptr;
	}
	Super::EndPlay(Reason);
}
