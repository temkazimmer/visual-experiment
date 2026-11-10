#include "VisexFootprints.h"

#include "Components/DecalComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
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
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Mound(TEXT("/Game/VisEx/SM_SandMound.SM_SandMound"));
	if (Mound.Succeeded())
	{
		MoundMesh = Mound.Object;
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Sand(TEXT("/Game/VisEx/MI_Dunes.MI_Dunes"));
	if (Sand.Succeeded())
	{
		MoundMaterial = Sand.Object;
	}
}

void AVisexFootprints::BeginPlay()
{
	Super::BeginPlay();
	for (int32 f = 0; f < 2; ++f)
	{
		UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(this);
		M->SetStaticMesh(MoundMesh);
		if (MoundMaterial)
		{
			M->SetMaterial(0, MoundMaterial);
		}
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->SetCollisionProfileName(TEXT("NoCollision"));
		M->SetGenerateOverlapEvents(false);
		M->SetMobility(EComponentMobility::Movable);
		M->SetupAttachment(RootComponent);
		M->SetVisibility(false);
		M->RegisterComponent();
		Mounds[f] = M;
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
	// mounds settle back unless a standing foot raises them again below
	MoundTarget[0] = MoundTarget[1] = 0.f;
	if (Mesh && Material && !Pawn->IsHidden())
	{
		Feet(Pawn, Mesh);
	}
	for (int32 f = 0; f < 2; ++f)
	{
		UStaticMeshComponent* M = Mounds[f];
		if (!M)
		{
			continue;
		}
		MoundH[f] = FMath::FInterpConstantTo(MoundH[f], MoundTarget[f], DeltaSeconds, MoundRate);
		M->SetVisibility(MoundH[f] > 0.05f);
		FVector S = M->GetRelativeScale3D();
		S.Z = MoundH[f] / 100.f;
		M->SetWorldScale3D(S);
	}
}

void AVisexFootprints::Feet(ACharacter* Pawn, USkeletalMeshComponent* Mesh)
{
	static const FName FootBones[2] = { TEXT("foot_l"), TEXT("foot_r") };
	static const FName Balls[2] = { TEXT("ball_l"), TEXT("ball_r") };
	FCollisionQueryParams Q(TEXT("Footprints"), false, Pawn);
	for (int32 f = 0; f < 2; ++f)
	{
		if (Mesh->GetBoneIndex(FootBones[f]) == INDEX_NONE)
		{
			continue;
		}
		const FVector Ankle = Mesh->GetSocketLocation(FootBones[f]);
		const bool bBall = Mesh->GetBoneIndex(Balls[f]) != INDEX_NONE;
		const FVector Ball = bBall ? Mesh->GetSocketLocation(Balls[f]) : Ankle + Pawn->GetActorForwardVector() * 14.f;
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Ankle + FVector(0, 0, 20), Ankle - FVector(0, 0, 80), ECC_Visibility, Q))
		{
			continue;
		}
		const float Height = Ankle.Z - Hit.ImpactPoint.Z;
		const bool bOnSand = Hit.GetActor() && Hit.GetActor()->ActorHasTag(TEXT("Sand"));
		// standing on sand: a mound under this foot, its top MoundCover above the sole
		if (bMounds && bOnSand && Mounds[f] && Pawn->GetVelocity().Size2D() < StandSpeed && Height < PlantHeight + LiftMargin)
		{
			FVector Fwd = Ball - Ankle;
			Fwd.Z = 0;
			if (!Fwd.Normalize())
			{
				Fwd = Pawn->GetActorForwardVector();
			}
			const FVector Mid = FVector(Ankle.X, Ankle.Y, Hit.ImpactPoint.Z) + Fwd * SoleOffset;
			FHitResult Under;
			const FVector Floor = GetWorld()->LineTraceSingleByChannel(Under, Mid + FVector(0, 0, 40), Mid - FVector(0, 0, 60), ECC_Visibility, Q)
				? Under.ImpactPoint : Mid;
			const FVector Up = Under.bBlockingHit ? Under.ImpactNormal : FVector::UpVector;
			const float Gap = FMath::Max(0.f, Height - AnkleRest);
			MoundTarget[f] = FMath::Clamp(Gap + MoundCover, MoundHeightRange.X, MoundHeightRange.Y);
			if (MoundH[f] <= 0.05f || FVector::DistSquared2D(Mounds[f]->GetComponentLocation(), Floor) > 25.f * 25.f)
			{
				MoundH[f] = FMath::Min(MoundH[f], 0.05f);    // a new stance: grow from flat
			}
			// slightly sunk, so its rim meets the sand without a seam
			Mounds[f]->SetWorldLocationAndRotation(Floor - Up * 0.3f, FRotationMatrix::MakeFromZX(Up, Fwd).Rotator());
			Mounds[f]->SetWorldScale3D(FVector(MoundSize.X / 100.f, MoundSize.Y / 100.f, FMath::Max(MoundH[f], 0.05f) / 100.f));
		}
		if (bDebug)
		{
			UE_LOG(LogTemp, Log, TEXT("foot %d height %.1f planted %d on %s"), f, Height, bPlanted[f], *GetNameSafe(Hit.GetActor()));
		}
		FVector Fwd = Ball - Ankle;
		Fwd.Z = 0;
		if (!Fwd.Normalize())
		{
			Fwd = Pawn->GetActorForwardVector();
		}
		const FVector Sole = FVector(Ankle.X, Ankle.Y, Hit.ImpactPoint.Z) + Fwd * SoleOffset;
		// how fast the ankle moves over the ground: a planted foot is (nearly) still
		const float Dt = FMath::Max(GetWorld()->GetDeltaSeconds(), 1e-3f);
		const float Speed = bHaveLast[f] ? FVector::Dist2D(Ankle, LastAnkle[f]) / Dt : 0.f;
		LastAnkle[f] = Ankle;
		bHaveLast[f] = true;
		// a swinging foot moves at ~2x her speed, a planted one near 0 (a little more if
		// the walk cycle slides): plant below half her speed, lift above 1.2x
		const float Body = Pawn->GetVelocity().Size2D();
		const float PlantBelow = FMath::Max(PlantSpeed, 0.5f * Body), LiftAbove = FMath::Max(PlantSpeed * 2.f, 1.2f * Body);
		if (bPlanted[f])
		{
			bPlanted[f] = Height < PlantHeight + LiftMargin && Speed < LiftAbove;
			if (bPlanted[f] && Active[f].IsValid())
			{
				// the foot rolls from heel to flat: keep the print under it
				Active[f]->SetWorldLocationAndRotation(Sole, PrintRotation(Fwd));
			}
			else
			{
				Active[f] = nullptr;
			}
			continue;
		}
		if (Height < PlantHeight + LiftMargin && Speed < PlantBelow)
		{
			bPlanted[f] = true;
			if (bOnSand && Pawn->IsPlayerControlled())
			{
				Active[f] = Stamp(Sole, Fwd, f == 0);
			}
		}
	}
}

FRotator AVisexFootprints::PrintRotation(const FVector& Forward) const
{
	// projection axis (X) straight down; the decal's Z is the foot's length, Y its width
	return FRotationMatrix::MakeFromXZ(FVector::DownVector, Forward).Rotator();
}

UDecalComponent* AVisexFootprints::Stamp(const FVector& Where, const FVector& Forward, bool bLeft)
{
	UDecalComponent* D = UGameplayStatics::SpawnDecalAtLocation(this, Material, FVector(HalfDepth, HalfSize.Y, HalfSize.X), Where, PrintRotation(Forward), 0.f);
	if (!D)
	{
		return nullptr;
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
	return D;
}
