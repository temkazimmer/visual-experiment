#include "VisexIntroRig.h"

#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "VisexMenuDirector.h"
#include "VisexBodyComponent.h"
#include "Components/WidgetComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogVisexIntro, Log, All);

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

}

void AVisexIntroRig::SpawnHer()
{
	// on the first tick, not in BeginPlay: a sequence spawnable begins play at the origin and
	// only then takes its keyed transform
	bSpawned = true;
	UWorld* World = GetWorld();
	UClass* Cls = CharacterClass.LoadSynchronous();
	if (!Cls)
	{
		UE_LOG(LogVisexIntro, Warning, TEXT("Intro rig: no character class"));
		return;
	}
	const ACharacter* Def = Cls->GetDefaultObject<ACharacter>();
	const float Half = Def->GetCapsuleComponent() ? Def->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() : 90.f;
	const FVector Loc = GetActorLocation() + FVector(0.f, 0.f, Half + 2.f);
	const FRotator Rot(0.f, GetActorRotation().Yaw, 0.f);
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Her = World->SpawnActor<ACharacter>(Cls, Loc, Rot, P);
	UE_LOG(LogVisexIntro, Log, TEXT("Intro rig: spawned %s at %s"), *GetNameSafe(Her), *Loc.ToString());
	if (!Her)
	{
		return;
	}
	if (USkeletalMeshComponent* Mesh = Her->GetMesh())
	{
		MeshBaseZ = Mesh->GetRelativeLocation().Z;
	}
	if (!Outfit.IsNone())
	{
		if (UVisexBodyComponent* Body = Her->FindComponentByClass<UVisexBodyComponent>())
		{
			Body->SetOutfit(Outfit);   // Tema: the film wears the Silversuit
		}
	}
	// her own AI controller until she takes the player (TakePlayer, when she starts walking)
	Her->SpawnDefaultController();
	if (AController* C = Her->GetController())
	{
		C->SetControlRotation(Rot);
	}
}

void AVisexIntroRig::TakePlayer()
{
	// the sand footprints and mounds follow player 0's pawn: whoever walks on camera takes it,
	// and the one before gets an AI controller back so she keeps walking out of the shot
	bTookPlayer = true;
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !Her)
	{
		return;
	}
	APawn* Before = PC->GetPawn();
	const FRotator Rot = Her->GetActorRotation();
	if (AController* Old = Her->GetController())
	{
		Old->UnPossess();
		Old->Destroy();
	}
	// the sequence's camera cut owns the view; possessing would hand it to her boom camera
	PC->bAutoManageActiveCameraTarget = false;
	PC->Possess(Her);
	PC->SetControlRotation(Rot);
	if (Before && Before != Her)
	{
		Before->SpawnDefaultController();
	}
}

void AVisexIntroRig::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bSpawned && GetWorld() && GetWorld()->IsGameWorld() && !GetActorLocation().IsNearlyZero(1.f))
	{
		SpawnHer();
	}
	if (Her)
	{
		Her->SetActorHiddenInGame(Show < 0.5f);
		if (USkeletalMeshComponent* Mesh = Her->GetMesh())
		{
			FVector R = Mesh->GetRelativeLocation();
			R.Z = MeshBaseZ - SinkCm;
			Mesh->SetRelativeLocation(R);
		}
	}
	if (Her && !bTookPlayer && (Player >= 0.5f || (bAutoPlayer && Walk > 0.001f && Show >= 0.5f)))
	{
		TakePlayer();
	}
	if (bMenuShown)
	{
		// the film's menu shot has no panel: its build number would never match the live one;
		// the live menu brings the panel in after the cut
		for (TActorIterator<AVisexMenuDirector> It(GetWorld()); It; ++It)
		{
			UWidgetComponent* Panel = bHideMenuPanel ? It->FindComponentByClass<UWidgetComponent>() : nullptr;
			if (Panel)
			{
				Panel->SetVisibility(false);
			}
			// the launch cover holds the handheld drift at its start pose while it cross-fades:
			// the film's menu shot ends on exactly that frame
			It->HoldDrift();
		}
	}
	if (Her && bTookPlayer && !bMenuShown && Menu >= 0.5f)
	{
		bMenuShown = true;
		for (TActorIterator<AVisexMenuDirector> It(GetWorld()); It; ++It)
		{
			It->SetActorTickEnabled(true);   // parked at BeginPlay
			It->ShowMenu();
		}
	}
	if (Her && Walk > 0.001f)
	{
		// forced: sequence playback ignores the player's move input (cinematic mode)
		Her->AddMovementInput(GetActorForwardVector(), FMath::Clamp(Walk, 0.f, 1.f), true);
	}
	// her real path, for framing the camera keys (Tools/intro_sequence.py)
	LogClock += DeltaSeconds;
	if (Her && LogClock >= 0.04f)
	{
		LogClock = 0.f;
		// the balls of her feet along her heading (cm ahead of her) and their height: her stride phase,
		// to match the live arrival's frame 0
		const FVector Fwd = Her->GetActorForwardVector();
		const FVector P = Her->GetActorLocation();
		const FVector BL = Her->GetMesh()->GetSocketLocation(TEXT("ball_l"));
		const FVector BR = Her->GetMesh()->GetSocketLocation(TEXT("ball_r"));
		UE_LOG(LogVisexIntro, Log, TEXT("Intro path t=%.3f pos=%s speed=%.0f ballL=%.1f/%.1f ballR=%.1f/%.1f"), GetWorld()->GetTimeSeconds(),
			*P.ToCompactString(), Her->GetVelocity().Size2D(), FVector::DotProduct(BL - P, Fwd), BL.Z, FVector::DotProduct(BR - P, Fwd), BR.Z);
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
