#include "VisexQATour.h"
#include "VisexMenuDirector.h"
#include "VisexPhotoMode.h"
#include "VisexBodyComponent.h"
#include "VisexBodyProfile.h"
#include "VisexShellComponent.h"
#include "EngineUtils.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogVisexQA, Log, All);

AVisexQATour::AVisexQATour()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

ACharacter* AVisexQATour::Her() const
{
	return Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
}

AVisexPhotoMode* AVisexQATour::Photo() const
{
	for (TActorIterator<AVisexPhotoMode> It(GetWorld()); It; ++It) { return *It; }
	return nullptr;
}

void AVisexQATour::Shot(const FString& Name)
{
	const FString File = FPaths::ProjectSavedDir() / TEXT("QA") / FString::Printf(TEXT("%02d_%s.png"), ++ShotCount, *Name);
	FScreenshotRequest::RequestScreenshot(File, true, false);
	UE_LOG(LogVisexQA, Display, TEXT("VisexQA: shot %s"), *File);
}

void AVisexQATour::Report(const FString& Where) const
{
	ACharacter* C = Her();
	FString Body = TEXT("no pawn");
	if (C)
	{
		const FVector L = C->GetActorLocation();
		const UCharacterMovementComponent* Move = C->GetCharacterMovement();
		Body = FString::Printf(TEXT("at %.0f %.0f %.0f, %s"), L.X, L.Y, L.Z,
			Move ? (Move->IsFalling() ? TEXT("FALLING") : Move->IsMovingOnGround() ? TEXT("on ground") : TEXT("other")) : TEXT("no movement"));
		TArray<USkeletalMeshComponent*> Meshes;
		C->GetComponents<USkeletalMeshComponent>(Meshes);
		for (const USkeletalMeshComponent* M : Meshes)
		{
			const UAnimInstance* A = M->GetAnimInstance();
			Body += FString::Printf(TEXT(" | %s: %s%s%s"), *M->GetName(),
				M->GetSkeletalMeshAsset() ? *M->GetSkeletalMeshAsset()->GetName() : TEXT("NO MESH"),
				M->IsVisible() ? TEXT("") : TEXT(" (hidden)"),
				A && A->GetCurrentActiveMontage() ? *FString::Printf(TEXT(", montage %s"), *A->GetCurrentActiveMontage()->GetName()) : TEXT(""));
		}
	}
	UE_LOG(LogVisexQA, Display, TEXT("VisexQA: %s: %s"), *Where, *Body);
}

void AVisexQATour::Build()
{
	auto Add = [this](float Wait, TFunction<void()> Run) { Steps.Add({Wait, MoveTemp(Run)}); };
	AVisexMenuDirector* D = Director;

	Add(6.f, [this] { Report(TEXT("menu")); Shot(TEXT("menu")); });
	Add(0.5f, [D] { D->StartGame(); });
	Add(4.f, [this] { Report(TEXT("play")); Shot(TEXT("play")); WalkFor = 3.f; });
	Add(3.5f, [this] { Report(TEXT("walked")); Shot(TEXT("walked")); });
	Add(0.5f, [D] { D->ToggleMenu(); });
	Add(5.f, [this] { Report(TEXT("menu again")); Shot(TEXT("menu_again")); });

	// each outfit on her, seen from the menu camera
	for (const FVisexOutfit& O : FVisexBodyProfile::Outfits())
	{
		const FName Name = O.Name;
		Add(0.5f, [this, Name]
		{
			if (ACharacter* C = Her())
			{
				if (UVisexBodyComponent* B = C->FindComponentByClass<UVisexBodyComponent>()) { B->SetOutfit(Name); }
			}
		});
		Add(2.f, [this, Name] { Report(FString::Printf(TEXT("outfit %s"), *Name.ToString())); Shot(FString::Printf(TEXT("outfit_%s"), *Name.ToString())); });
	}
	Add(0.5f, [this]
	{
		if (ACharacter* C = Her())
		{
			if (UVisexBodyComponent* B = C->FindComponentByClass<UVisexBodyComponent>()) { B->SetOutfit(FVisexBodyProfile().Outfit); }
		}
	});

	// photo mode, through every motion and pose
	Add(0.5f, [D] { D->OnItemChosen(2); });
	Add(6.f, [this]
	{
		AVisexPhotoMode* P = Photo();
		UE_LOG(LogVisexQA, Display, TEXT("VisexQA: photo mode %s"), P && P->IsOpen() ? TEXT("open") : TEXT("DID NOT OPEN"));
		if (!P || !P->IsOpen()) { return; }
		for (int32 I = 0; I < 60; ++I)
		{
			P->SetMotion(I);
			if (P->GetMotion() != I) { UE_LOG(LogVisexQA, Display, TEXT("VisexQA: %d motions"), I); break; }
		}
		P->SetMotion(0);
	});
	for (int32 I = 0; I < 40; ++I)
	{
		Add(I == 0 ? 0.5f : 2.5f, [this, I]
		{
			AVisexPhotoMode* P = Photo();
			if (!P || !P->IsOpen()) { return; }
			P->SetMotion(I);
			if (P->GetMotion() != I) { Next = AfterPoses; return; }   // past the last motion
			Report(FString::Printf(TEXT("photo motion %d"), I));
			Shot(FString::Printf(TEXT("photo_%02d"), I));
		});
	}
	AfterPoses = Steps.Num();
	Add(1.f, [this] { if (AVisexPhotoMode* P = Photo()) { P->Close(); } });
	Add(5.f, [this] { Report(TEXT("after photo")); Shot(TEXT("after_photo")); });
	Add(1.f, [this]
	{
		UE_LOG(LogVisexQA, Display, TEXT("VisexQA: done"));
		UKismetSystemLibrary::QuitGame(this, UGameplayStatics::GetPlayerController(this, 0), EQuitPreference::Quit, false);
	});
}

void AVisexQATour::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Director) { return; }
	if (!bBuilt) { Build(); bBuilt = true; }
	if (WalkFor > 0.f)
	{
		WalkFor -= DeltaSeconds;
		if (ACharacter* C = Her())
		{
			const FRotator Yaw(0.f, UGameplayStatics::GetPlayerCameraManager(this, 0)->GetCameraRotation().Yaw, 0.f);
			C->AddMovementInput(Yaw.Vector(), 1.f);
		}
	}
	Clock += DeltaSeconds;
	if (Steps.IsValidIndex(Next) && Clock >= Steps[Next].Wait)
	{
		Clock = 0.f;
		Steps[Next++].Run();
	}
}
