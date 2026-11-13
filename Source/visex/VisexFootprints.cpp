#include "VisexFootprints.h"

#include "visex.h"
#include "VisexAnimInstance.h"

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
	if (Mesh && Material && !Pawn->IsHidden())
	{
		Feet(Pawn, Mesh);
	}
	// mounds stay where they formed: they push out, hold, then settle flat and go
	for (int32 i = Mounds.Num() - 1; i >= 0; --i)
	{
		UStaticMeshComponent* M = Mounds[i];
		MoundAge[i] += DeltaSeconds;
		if (!M || MoundAge[i] >= MoundLife)
		{
			if (M) { M->DestroyComponent(); }
			Mounds.RemoveAt(i);
			MoundAge.RemoveAt(i);
			continue;
		}
		if (M == Held[0].Get() || M == Held[1].Get())
		{
			MoundAge[i] = FMath::Min(MoundAge[i], 0.35f * MoundLife);   // a standing foot keeps its sand pushed out
		}
		const float T = MoundAge[i];
		const float H = FMath::SmoothStep(0.f, MoundGrow, T) * (1.f - FMath::SmoothStep(0.35f * MoundLife, MoundLife, T));
		M->SetRelativeScale3D(FVector(1.f, 1.f, FMath::Max(H, 0.01f)));
	}
}

void AVisexFootprints::Feet(ACharacter* Pawn, USkeletalMeshComponent* Mesh)
{
	static const FName FootBones[2] = { TEXT("foot_l"), TEXT("foot_r") };
	static const FName Balls[2] = { TEXT("ball_l"), TEXT("ball_r") };
	FCollisionQueryParams Q(TEXT("Footprints"), true, Pawn);
	// the instance that runs the plant pass (on the body mesh, or a layer under it)
	const UVisexAnimInstance* Plant = Cast<UVisexAnimInstance>(Mesh->GetAnimInstance());
	if (!Plant)
	{
		TArray<USkeletalMeshComponent*> Skins;
		Pawn->GetComponents(Skins);
		for (const USkeletalMeshComponent* K : Skins)
		{
			if ((Plant = Cast<UVisexAnimInstance>(K->GetAnimInstance())) != nullptr) { break; }
		}
	}
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
		// a mound once per contact: the plant pass has this sole down, on the sand
		if (bMounds && MoundMesh && Plant)
		{
			const float Gap = (f == 0 ? Plant->SoleGapL : Plant->SoleGapR) * float(Plant->GetSkelMeshComponent()->GetComponentScale().Z);
			const float Down = f == 0 ? Plant->PlantedL : Plant->PlantedR;
			if (Gap > MoundLift)
			{
				bMounded[f] = false;
				Held[f] = nullptr;
			}
			else if (!bMounded[f] && bOnSand && Gap <= MoundGap && Down >= MoundPlanted)
			{
				FVector Fwd = Ball - Ankle;
				Fwd.Z = 0;
				if (!Fwd.Normalize()) { Fwd = Pawn->GetActorForwardVector(); }
				const FVector Mid = FVector(Ankle.X, Ankle.Y, Hit.ImpactPoint.Z) + Fwd * SoleOffset;
				FHitResult Under;
				if (GetWorld()->LineTraceSingleByChannel(Under, Mid + FVector(0, 0, 40), Mid - FVector(0, 0, 60), VisexFootTraceChannel, Q))
				{
					// a re-plant inside the same stance keeps the sand already pushed out there
					UStaticMeshComponent* Near = nullptr;
					for (UStaticMeshComponent* M : Mounds)
					{
						if (M && FVector::DistSquared(M->GetComponentLocation(), Under.ImpactPoint) < 10.f * 10.f) { Near = M; }
					}
					if (!Near)
					{
						Mound(Under.ImpactPoint, Under.ImpactNormal, Fwd);
					}
					bMounded[f] = true;
					Held[f] = Near ? Near : (Mounds.Num() ? Mounds.Last().Get() : nullptr);
				}
			}
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

void AVisexFootprints::Mound(const FVector& Where, const FVector& Up, const FVector& Forward)
{
	if (Mounds.Num() >= FMath::Max(1, MoundMax))
	{
		if (Mounds[0]) { Mounds[0]->DestroyComponent(); }
		Mounds.RemoveAt(0);
		MoundAge.RemoveAt(0);
	}
	UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(this);
	M->SetStaticMesh(MoundMesh);
	if (MoundMaterial) { M->SetMaterial(0, MoundMaterial); }
	// never collides: nothing plants on it, nothing traces it, FootTrace included
	M->SetCollisionProfileName(TEXT("NoCollision"));
	M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M->SetGenerateOverlapEvents(false);
	// a centimetre of sand: no shadow or distance-field darkening of its own, the sand's shading only
	M->SetCastShadow(false);
	M->bAffectDistanceFieldLighting = false;
	M->bAffectDynamicIndirectLighting = false;
	M->SetMobility(EComponentMobility::Movable);
	M->SetupAttachment(RootComponent);
	M->SetUsingAbsoluteLocation(true);
	M->SetUsingAbsoluteRotation(true);
	M->SetUsingAbsoluteScale(true);
	M->SetWorldLocationAndRotation(Where - Up * MoundSink, FRotationMatrix::MakeFromZX(Up, Forward).Rotator());
	M->SetRelativeScale3D(FVector(1.f, 1.f, 0.01f));
	M->RegisterComponent();
	Mounds.Add(M);
	MoundAge.Add(0.f);
	if (bDebug)
	{
		UE_LOG(LogTemp, Log, TEXT("mound at %s (%d alive)"), *Where.ToString(), Mounds.Num());
	}
}
