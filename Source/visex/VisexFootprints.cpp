#include "VisexFootprints.h"

#include "Components/DecalComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AVisexFootprints::AVisexFootprints()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;   // after the pose, foot IK included
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Game/VisEx/M_Footprint.M_Footprint"));
	if (Mat.Succeeded())
	{
		Material = Mat.Object;
	}
}

void AVisexFootprints::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// older prints sink back toward flat sand
	for (int32 i = Prints.Num() - 1; i >= 0; --i)
	{
		FPrint& P = Prints[i];
		if (!P.MID.IsValid() || !P.Decal.IsValid())
		{
			Prints.RemoveAt(i);
			continue;
		}
		P.Fade = FMath::FInterpConstantTo(P.Fade, P.Target, DeltaSeconds, FadeRate);
		P.MID->SetScalarParameterValue(TEXT("Fade"), P.Fade);
		if (P.Target <= 0.f && P.Fade <= 0.f)
		{
			P.Decal->DestroyComponent();
			Prints.RemoveAt(i);
		}
	}

	ACharacter* Pawn = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	USkeletalMeshComponent* Mesh = Pawn ? Pawn->GetMesh() : nullptr;
	if (!Mesh || !Material || !Pawn->IsPlayerControlled())
	{
		return;
	}
	static const FName Feet[2] = { TEXT("foot_l"), TEXT("foot_r") };
	static const FName Balls[2] = { TEXT("ball_l"), TEXT("ball_r") };
	FCollisionQueryParams Q(TEXT("Footprints"), false, Pawn);
	for (int32 f = 0; f < 2; ++f)
	{
		if (Mesh->GetBoneIndex(Feet[f]) == INDEX_NONE)
		{
			continue;
		}
		const FVector Ankle = Mesh->GetSocketLocation(Feet[f]);
		const bool bBall = Mesh->GetBoneIndex(Balls[f]) != INDEX_NONE;
		const FVector Ball = bBall ? Mesh->GetSocketLocation(Balls[f]) : Ankle + Pawn->GetActorForwardVector() * 14.f;
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Ankle + FVector(0, 0, 20), Ankle - FVector(0, 0, 80), ECC_Visibility, Q))
		{
			continue;
		}
		const float Height = Ankle.Z - Hit.ImpactPoint.Z;
		if (bDebug)
		{
			UE_LOG(LogTemp, Log, TEXT("foot %d height %.1f planted %d on %s"), f, Height, bPlanted[f], *GetNameSafe(Hit.GetActor()));
		}
		if (bPlanted[f])
		{
			bPlanted[f] = Height < PlantHeight + LiftMargin;
			continue;
		}
		if (Height < PlantHeight)
		{
			bPlanted[f] = true;
			if (Hit.GetActor() && Hit.GetActor()->ActorHasTag(TEXT("Sand")))
			{
				FVector Fwd = Ball - Ankle;
				Fwd.Z = 0;
				if (!Fwd.Normalize())
				{
					Fwd = Pawn->GetActorForwardVector();
				}
				// the print's middle: between heel and toe, on the ground
				const FVector Mid = FVector(0.5f * (Ankle.X + Ball.X), 0.5f * (Ankle.Y + Ball.Y), Hit.ImpactPoint.Z) - Fwd * 3.f;
				Stamp(Mid, Fwd, f == 0);
			}
		}
	}
}

void AVisexFootprints::Stamp(const FVector& Where, const FVector& Forward, bool bLeft)
{
	// projection axis (X) straight down; the decal's Z is the foot's length, Y its width
	const FRotator Rot = FRotationMatrix::MakeFromXZ(FVector::DownVector, Forward).Rotator();
	UDecalComponent* D = UGameplayStatics::SpawnDecalAtLocation(this, Material, FVector(25.f, HalfSize.Y, HalfSize.X), Where, Rot, 0.f);
	if (!D)
	{
		return;
	}
	D->SetRelativeScale3D(FVector(1.f, bLeft ? -1.f : 1.f, 1.f));    // mirror for the left foot
	D->SetFadeScreenSize(0.002f);
	FPrint P;
	P.Decal = D;
	P.MID = D->CreateDynamicMaterialInstance();
	P.MID->SetScalarParameterValue(TEXT("Fade"), 1.f);    // pressed at once
	P.Fade = 1.f;
	P.Target = 1.f;
	Prints.Add(P);
	// each step back is shallower: the newest at full depth, the Keep-th gone
	const int32 N = Prints.Num();
	for (int32 i = 0; i < N; ++i)
	{
		const int32 Age = N - 1 - i;
		Prints[i].Target = FMath::Max(0.f, 1.f - float(Age) / float(Keep));
	}
	if (bDebug)
	{
		UE_LOG(LogTemp, Log, TEXT("footprint %s at %s"), bLeft ? TEXT("L") : TEXT("R"), *Where.ToString());
	}
}
