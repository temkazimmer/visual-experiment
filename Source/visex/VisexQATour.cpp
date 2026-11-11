#include "VisexQATour.h"
#include "VisexMenuDirector.h"
#include "VisexPhotoMode.h"
#include "VisexBodyEditor.h"
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
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "RHI.h"
#include "RenderCore.h"
#include "GameFramework/GameUserSettings.h"
#include "VisexSettings.h"
#include "VisexMenuLayout.h"
#include "Engine/GameViewportClient.h"
#include "UnrealEngine.h"
#include "GameFramework/PlayerController.h"
#include "AssetRegistry/AssetRegistryModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogVisexQA, Log, All);

// Shipping writes no log, so every QA line also goes to Saved/QA/report.txt
static void QAWrite(const FString& Line)
{
	UE_LOG(LogVisexQA, Display, TEXT("%s"), *Line);
	FFileHelper::SaveStringToFile(Line + LINE_TERMINATOR, *(FPaths::ProjectSavedDir() / TEXT("QA/report.txt")),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
}
#define QA_LOG(Fmt, ...) QAWrite(FString::Printf(Fmt, ##__VA_ARGS__))

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
	QA_LOG(TEXT("VisexQA: shot %s"), *File);
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
			Move ? (Move->IsFalling() ? TEXT("FALLING") : Move->IsMovingOnGround() ? TEXT("on ground") : *FString::Printf(TEXT("mode %d"), int32(Move->MovementMode.GetValue()))) : TEXT("no movement"));
		if (const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
		{
			const FVector V = Cam->GetCameraLocation();
			Body += FString::Printf(TEXT(", view at %.0f %.0f %.0f"), V.X, V.Y, V.Z);
		}
		// why she would fall: what the movement thinks is under her, what a trace finds, and her collision
		if (Move)
		{
			const FFindFloorResult& F = Move->CurrentFloor;
			Body += FString::Printf(TEXT(", floor %s %s dist %.1f"), F.bBlockingHit ? TEXT("hit") : TEXT("none"),
				F.HitResult.GetComponent() ? *F.HitResult.GetComponent()->GetName() : TEXT("-"), F.FloorDist);
			Body += FString::Printf(TEXT(", vel z %.0f, rootmotion %d, montage-rm %d"), Move->Velocity.Z,
				Move->HasAnimRootMotion() ? 1 : 0, C->IsPlayingRootMotion() ? 1 : 0);
		}
		FHitResult Down;
		FCollisionQueryParams Q(TEXT("QAFloor"), true, C);
		if (GetWorld()->LineTraceSingleByChannel(Down, L + FVector(0, 0, 500), L - FVector(0, 0, 100000), ECC_Visibility, Q))
		{
			Body += FString::Printf(TEXT(", trace %s at z %.0f"), Down.GetComponent() ? *Down.GetComponent()->GetName() : TEXT("?"), Down.ImpactPoint.Z);
		}
		else
		{
			Body += TEXT(", trace NOTHING below");
		}
		if (const UCapsuleComponent* Cap = C->GetCapsuleComponent())
		{
			Body += FString::Printf(TEXT(", capsule coll %d vs WorldStatic %d"), int32(Cap->GetCollisionEnabled()),
				int32(Cap->GetCollisionResponseToChannel(ECC_WorldStatic)));
		}
		// every skinned part, the cloth skirt (UChaosClothComponent, "OutfitCloth") included
		TArray<USkinnedMeshComponent*> Meshes;
		C->GetComponents<USkinnedMeshComponent>(Meshes);
		for (const USkinnedMeshComponent* Part : Meshes)
		{
			const USkeletalMeshComponent* M = Cast<USkeletalMeshComponent>(Part);
			const UAnimInstance* A = M ? M->GetAnimInstance() : nullptr;
			Body += FString::Printf(TEXT(" | %s: %s%s%s"), *Part->GetName(),
				Part->GetSkinnedAsset() ? *Part->GetSkinnedAsset()->GetName() : TEXT("NO MESH"),
				Part->IsVisible() ? TEXT("") : TEXT(" (hidden)"),
				A && A->GetCurrentActiveMontage() ? *FString::Printf(TEXT(", montage %s"), *A->GetCurrentActiveMontage()->GetName()) : TEXT(""));
		}
	}
	if (FrameN > 0)
	{
		Body += FString::Printf(TEXT(" || frame %.1f ms avg, %.1f worst | game %.1f, render %.1f, rhi %.1f, gpu %.1f ms (%d frames)"),
			FrameSum / FrameN, FrameMax, GameSum / FrameN, DrawSum / FrameN, RhiSum / FrameN, GpuSum / FrameN, FrameN);
	}
	FrameSum = FrameMax = GpuSum = GameSum = DrawSum = RhiSum = 0.0;
	FrameN = 0;
	QA_LOG(TEXT("VisexQA: %s: %s"), *Where, *Body);
}

void AVisexQATour::ViewCheck(const FString& Where) const
{
	FVector2D Size(0.f);
	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
	{
		Viewport->GetViewportSize(Size);
	}
	const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Cam)
	{
		QA_LOG(TEXT("VisexQA: %s: no camera"), *Where);
		return;
	}
	const FMinimalViewInfo& View = Cam->GetCameraCacheView();
	QA_LOG(TEXT("VisexQA: %s: window %.0fx%.0f (%.3f), view %s, fov %.1f, %s"), *Where, Size.X, Size.Y,
		Size.Y > 0.f ? Size.X / Size.Y : 0.f, *GetNameSafe(Cam->GetViewTarget()), View.FOV,
		View.bConstrainAspectRatio ? *FString::Printf(TEXT("LETTERBOXED to %.3f"), View.AspectRatio) : TEXT("fills the window"));
}

// her feet on screen: heel (foot bone), ball and a toe tip a little past the ball, each as a
// fraction of the window. Inside 0..1 on both axes means in frame (the menu camera, 620fd62)
static void FeetCheck(ACharacter* C, const FString& Where)
{
	APlayerController* PC = C ? Cast<APlayerController>(UGameplayStatics::GetPlayerController(C, 0)) : nullptr;
	FVector2D Size(0.f);
	if (UGameViewportClient* Viewport = C ? C->GetWorld()->GetGameViewport() : nullptr) { Viewport->GetViewportSize(Size); }
	if (!PC || !C->GetMesh() || Size.X <= 0.f || Size.Y <= 0.f) { QA_LOG(TEXT("VisexQA: %s feet: no view"), *Where); return; }
	FString Line;
	bool bAll = true;
	for (const TCHAR* Side : {TEXT("l"), TEXT("r")})
	{
		const FVector Foot = C->GetMesh()->GetSocketLocation(*FString::Printf(TEXT("foot_%s"), Side));
		const FVector Ball = C->GetMesh()->GetSocketLocation(*FString::Printf(TEXT("ball_%s"), Side));
		const FVector Tip = Ball + (Ball - Foot).GetSafeNormal2D() * 7.f;
		for (const TPair<const TCHAR*, FVector>& P : {TPair<const TCHAR*, FVector>(TEXT("heel"), Foot), TPair<const TCHAR*, FVector>(TEXT("ball"), Ball), TPair<const TCHAR*, FVector>(TEXT("toe"), Tip)})
		{
			FVector2D S;
			const bool bOn = PC->ProjectWorldLocationToScreen(P.Value, S) && S.X >= 0.f && S.Y >= 0.f && S.X <= Size.X && S.Y <= Size.Y;
			bAll &= bOn;
			Line += FString::Printf(TEXT(" %s_%s %.2f,%.2f%s"), P.Key, Side, S.X / Size.X, S.Y / Size.Y, bOn ? TEXT("") : TEXT(" OUT"));
		}
	}
	QA_LOG(TEXT("VisexQA: %s feet %s:%s"), *Where, bAll ? TEXT("IN FRAME") : TEXT("CUT"), *Line);
}

// the lower foot's ball: the planted one while she walks
static FVector LowerBall(ACharacter* C)
{
	if (!C || !C->GetMesh()) { return FVector::ZeroVector; }
	const FVector L = C->GetMesh()->GetSocketLocation(TEXT("ball_l")), R = C->GetMesh()->GetSocketLocation(TEXT("ball_r"));
	return L.Z < R.Z ? L : R;
}

// logs how far the planted foot went in the gap, against how far she did
static void FootSlide(ACharacter* C, const FString& Where, const FVector& Foot0, const FVector& Body0, float Gap)
{
	if (!C || !C->GetMesh()) { return; }
	const FVector L = C->GetMesh()->GetSocketLocation(TEXT("ball_l")), R = C->GetMesh()->GetSocketLocation(TEXT("ball_r"));
	const float Foot = FMath::Min(FVector::Dist2D(L, Foot0), FVector::Dist2D(R, Foot0));
	const float Body = FVector::Dist2D(C->GetActorLocation(), Body0);
	QA_LOG(TEXT("VisexQA: %s foot slide: planted foot moved %.1f cm while she moved %.1f cm in %.2f s (%s)"), *Where, Foot, Body, Gap,
		Body < 3.f ? TEXT("she is still") : Foot < FMath::Max(3.f, Body * 0.2f) ? TEXT("PASS, planted") : TEXT("SLIDES"));
}

// the poses in photo mode's order (VisexPhotoMode::BuildMotions): flows between poses are not poses
static TArray<FString> PoseNames()
{
	TArray<FAssetData> Found;
	FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().GetAssetsByPath(TEXT("/Game/Characters/Poses"), Found, true);
	Found.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.LexicalLess(B.AssetName); });
	TArray<FString> Names;
	for (const FAssetData& Asset : Found)
	{
		FString Name = Asset.AssetName.ToString();
		if (Name.EndsWith(TEXT("_In")) || Name.EndsWith(TEXT("_Out")) || Name.Contains(TEXT("_To_")) || !Asset.IsInstanceOf(UAnimSequence::StaticClass())) { continue; }
		Name.RemoveFromStart(TEXT("A_Pose_"));
		Names.Add(Name);
	}
	return Names;
}

void AVisexQATour::Build()
{
	auto Add = [this](float Wait, TFunction<void()> Run) { Steps.Add({Wait, MoveTemp(Run)}); };
	AVisexMenuDirector* D = Director;

	Add(6.f, [this] { Report(TEXT("menu")); Shot(TEXT("menu")); });
	// her feet with the toes in the menu frame (620fd62): a check on the bones, and a shot to crop
	Add(1.f, [this] { FeetCheck(Her(), TEXT("menu 16:9")); Shot(TEXT("menu_feet")); });
	Add(0.5f, [D] { D->StartGame(); });
	Add(4.f, [this] { Report(TEXT("play")); Shot(TEXT("play")); WalkFor = 3.f; });
	Add(3.5f, [this] { Report(TEXT("walked")); Shot(TEXT("walked")); });

	// what each quality level costs here, standing in play (applied, not saved)
	for (int32 Level = 0; Level <= 4; ++Level)
	{
		Add(0.5f, [this, Level]
		{
			if (UGameUserSettings* S = UGameUserSettings::GetGameUserSettings())
			{
				S->SetOverallScalabilityLevel(Level);
				FVisexSettings::ApplyBetweenFrames();
			}
			FrameSum = FrameMax = GpuSum = 0.0;
			FrameN = 0;
		});
		Add(1.f, [this] { FrameSum = FrameMax = GpuSum = GameSum = DrawSum = RhiSum = 0.0; FrameN = 0; });   // let it settle
		Add(3.f, [this, Level] { Report(FString::Printf(TEXT("quality %d"), Level)); });
	}
	// from Epic, each category alone at Ultra: what every Cinematic feature costs
	struct FCat { const TCHAR* Name; void (UGameUserSettings::*Set)(int32); };
	static const FCat Cats[] = {
		{TEXT("shadows"), &UGameUserSettings::SetShadowQuality},
		{TEXT("lighting"), &UGameUserSettings::SetGlobalIlluminationQuality},
		{TEXT("reflections"), &UGameUserSettings::SetReflectionQuality},
		{TEXT("effects"), &UGameUserSettings::SetVisualEffectQuality},
		{TEXT("post"), &UGameUserSettings::SetPostProcessingQuality},
		{TEXT("antialiasing"), &UGameUserSettings::SetAntiAliasingQuality},
		{TEXT("shading"), &UGameUserSettings::SetShadingQuality},
		{TEXT("viewdistance"), &UGameUserSettings::SetViewDistanceQuality},
		{TEXT("foliage"), &UGameUserSettings::SetFoliageQuality},
		{TEXT("textures"), &UGameUserSettings::SetTextureQuality},
	};
	for (const FCat& Cat : Cats)
	{
		Add(0.5f, [this, Cat]
		{
			if (UGameUserSettings* S = UGameUserSettings::GetGameUserSettings())
			{
				S->SetOverallScalabilityLevel(3);
				(S->*Cat.Set)(4);
			}
			FVisexSettings::ApplyBetweenFrames();
		});
		Add(1.f, [this] { FrameSum = FrameMax = GpuSum = GameSum = DrawSum = RhiSum = 0.0; FrameN = 0; });
		Add(2.5f, [this, Cat] { Report(FString::Printf(TEXT("epic + ultra %s"), Cat.Name)); });
	}
	Add(0.5f, []
	{
		if (UGameUserSettings* S = UGameUserSettings::GetGameUserSettings()) { S->SetOverallScalabilityLevel(4); }
		FVisexSettings::ApplyBetweenFrames();
	});
	Add(0.5f, [D] { D->ToggleMenu(); });
	// GC while the lean-down plays: the idle clip waiting in its timer must survive (0298 crash)
	Add(0.3f, [] { CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true); });
	Add(4.7f, [this] { Report(TEXT("menu again")); Shot(TEXT("menu_again")); });

#if !WITH_EDITOR
	// the layout editor's SAVE AS DEFAULT must reach the player's own ini on disk (packaged, it
	// used to stay in memory: a saved Game.ini drops sections it doesn't list). Saves the layout
	// as it is, checks the file, then puts the file back the way it was
	Add(0.5f, [D]
	{
		static const TCHAR* Section = TEXT("VisexMenuLayout");
		FString Before;
		const bool bHad = GConfig->GetString(Section, TEXT("Layout"), Before, GGameUserSettingsIni);
		const FString Where = FVisexMenuLayout::Save(D->GetLayout());
		QA_LOG(TEXT("VisexQA: layout save %s: %s"), FVisexMenuLayout::IsSavedOnDisk() ? TEXT("PASS, on disk") : TEXT("FAIL, not in the file"), *Where);
		if (bHad) { GConfig->SetString(Section, TEXT("Layout"), *Before, GGameUserSettingsIni); }
		else { GConfig->RemoveKeyFromSection(Section, TEXT("Layout"), GGameUserSettingsIni); }
		GConfig->Flush(false, GGameUserSettingsIni);
	});
#endif

	// a 4:3 window: the menu, the Start move and its landing fill it, no letterbox (the menu's
	// cine camera used to box to 16:9 and the bars snapped off as the move landed)
	{
		struct FRes { int32 X = 0, Y = 0; EWindowMode::Type Mode = EWindowMode::Windowed; };
		TSharedRef<FRes> Was = MakeShared<FRes>();
		const float Orbit = D->BlendToPlayTime;
		const float Landing = D->LandingBlendTime;
		Add(0.5f, [Was]
		{
			Was->X = GSystemResolution.ResX;
			Was->Y = GSystemResolution.ResY;
			Was->Mode = GSystemResolution.WindowMode;
			FSystemResolution::RequestResolutionChange(1440, 1080, EWindowMode::Windowed);
		});
		Add(3.f, [this] { ViewCheck(TEXT("4:3 menu")); FeetCheck(Her(), TEXT("menu 4:3")); Shot(TEXT("menu_4x3")); });
		Add(0.1f, [D] { D->StartGame(); });
		Add(Orbit * 0.5f, [this] { ViewCheck(TEXT("4:3 mid-move")); Shot(TEXT("start_4x3_mid")); });
		Add(Orbit * 0.5f + Landing * 0.5f, [this] { ViewCheck(TEXT("4:3 landing")); Shot(TEXT("start_4x3_landing")); });
		Add(Landing + 0.5f, [this] { ViewCheck(TEXT("4:3 play")); Shot(TEXT("play_4x3")); });
		Add(0.5f, [D] { D->ToggleMenu(); });
		Add(3.f, [Was] { FSystemResolution::RequestResolutionChange(Was->X, Was->Y, Was->Mode); });
		Add(3.f, [this] { ViewCheck(TEXT("menu, window back")); Report(TEXT("menu after 4:3")); });
	}

	// the Settings page: open, both tabs, step a value there and back, close
	Add(0.5f, [D] { D->OpenSettings(); });
	Add(1.5f, [this] { Report(TEXT("settings")); Shot(TEXT("settings")); });
	Add(0.5f, [D] { D->SettingsTab(1); D->SettingsStep(1); });
	Add(1.5f, [this] { Report(TEXT("settings visuals")); Shot(TEXT("settings_visuals")); });
	Add(0.5f, [D] { D->SettingsStep(-1); D->SettingsTab(1); D->CloseSettings(); });

	// each outfit on her, seen from the menu camera
	for (const FVisexOutfit& O : FVisexBodyProfile::Outfits())
	{
		if (O.bLocked) { continue; }   // not wearable: the wardrobe stop below shows them
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

	// the Body page's three tabs, then the wardrobe and a locked costume's window
	auto Editor = [this]() -> AVisexBodyEditor* { for (TActorIterator<AVisexBodyEditor> It(GetWorld()); It; ++It) { return *It; } return nullptr; };
	Add(0.5f, [D] { D->OpenBody(); });
	Add(4.f, [this, Editor] { AVisexBodyEditor* E = Editor(); QA_LOG(TEXT("VisexQA: body page %s"), E && E->IsOpen() ? TEXT("open") : TEXT("DID NOT OPEN")); Shot(TEXT("body_tab_body")); });
	Add(0.2f, [Editor] { if (AVisexBodyEditor* E = Editor()) { E->SwitchTab(1); } });
	Add(2.f, [this] { Shot(TEXT("body_tab_face")); });
	Add(0.2f, [Editor] { if (AVisexBodyEditor* E = Editor()) { E->SwitchTab(1); } });
	Add(2.f, [this] { Shot(TEXT("body_tab_hair")); });
	Add(0.2f, [Editor] { if (AVisexBodyEditor* E = Editor()) { E->Close(); } });
	Add(4.f, [this] { Report(TEXT("after body page")); });
	Add(0.5f, [D] { D->OpenWardrobe(); });
	Add(4.f, [this, Editor] { AVisexBodyEditor* E = Editor(); QA_LOG(TEXT("VisexQA: wardrobe %s"), E && E->IsOpen() && E->GetPage() == 1 ? TEXT("open") : TEXT("DID NOT OPEN")); Shot(TEXT("wardrobe")); });
	Add(0.2f, [Editor]
	{
		AVisexBodyEditor* E = Editor();
		const int32 Locked = FVisexBodyProfile::Outfits().IndexOfByPredicate([](const FVisexOutfit& O) { return O.bLocked; });
		if (E && Locked != INDEX_NONE) { E->SelectRow(Locked); E->ApplyTab(); }
	});
	Add(1.5f, [this] { Shot(TEXT("wardrobe_locked")); });
	Add(0.2f, [Editor] { if (AVisexBodyEditor* E = Editor()) { E->Close(); } });   // closes the window
	Add(1.f, [this] { Shot(TEXT("wardrobe_after_window")); });
	Add(0.2f, [Editor] { if (AVisexBodyEditor* E = Editor()) { E->Close(); } });   // leaves the page
	Add(4.f, [this] { Report(TEXT("after wardrobe")); Shot(TEXT("menu_after_wardrobe")); });

	// photo mode (7048985): opens seated against the column, then Next steps through stand-up,
	// slow walk, walk, run, dash, jump, the poses and the walk back to sit. Motion indices as in
	// VisexPhotoMode::BuildMotions: 0 sit, 1-5 slow walk..jump, then the poses, then the return
	const TArray<FString> Poses = PoseNames();
	int32 FirstPose = 6;   // not const: a captured constant draws an unused-capture warning
	const int32 ReturnAt = FirstPose + Poses.Num();
	Add(0.5f, [this, D] { Report(TEXT("before photo")); D->OnItemChosen(AVisexMenuDirector::ItemPhoto); });
	// she should sit once, straight from the menu, not stand and sit again: the montage in each report
	float Since = 0.f;
	for (const float Wait : {0.1f, 0.3f, 0.4f, 0.7f, 1.5f})
	{
		Since += Wait;
		Add(Wait, [this, Since] { Report(FString::Printf(TEXT("photo +%.1fs"), Since)); });
	}
	Add(1.f, [this, ReturnAt]
	{
		AVisexPhotoMode* P = Photo();
		QA_LOG(TEXT("VisexQA: photo mode %s, motion %d, %d poses, return at %d"), P && P->IsOpen() ? TEXT("open") : TEXT("DID NOT OPEN"),
			P ? P->GetMotion() : -1, ReturnAt - 6, ReturnAt);
		Report(TEXT("photo sit at open")); Shot(TEXT("photo_sit_open"));
	});
	// each move: shot after it has got going, then the planted foot 0.1 s later
	struct FMove { int32 Motion; const TCHAR* Name; float Wait; };
	for (const FMove& Mv : {FMove{1, TEXT("slow_walk"), 6.f}, FMove{2, TEXT("walk"), 3.f}, FMove{3, TEXT("run"), 3.f}, FMove{4, TEXT("dash"), 3.f}})
	{
		TSharedRef<TPair<FVector, FVector>> At = MakeShared<TPair<FVector, FVector>>();
		const FString Name = Mv.Name;
		const int32 Index = Mv.Motion;
		Add(0.5f, [this, Index] { if (AVisexPhotoMode* P = Photo()) { P->SetMotion(Index); } });
		Add(Mv.Wait, [this, Name, Index, At]
		{
			AVisexPhotoMode* P = Photo();
			QA_LOG(TEXT("VisexQA: photo %s is motion %d"), *Name, P ? P->GetMotion() : -1);
			Report(TEXT("photo ") + Name); Shot(TEXT("photo_") + Name);
			At->Key = LowerBall(Her());
			At->Value = Her() ? Her()->GetActorLocation() : FVector::ZeroVector;
		});
		Add(0.1f, [this, Name, At] { FootSlide(Her(), TEXT("photo ") + Name, At->Key, At->Value, 0.1f); });
		Add(0.4f, [this, At]
		{
			At->Key = LowerBall(Her());
			At->Value = Her() ? Her()->GetActorLocation() : FVector::ZeroVector;
		});
		Add(0.1f, [this, Name, At] { FootSlide(Her(), TEXT("photo ") + Name + TEXT(" again"), At->Key, At->Value, 0.1f); });
	}
	// the jump: she leaps 3.5 s into the jog. Polls every frame and shoots as she tops out
	{
		TSharedRef<float> Left = MakeShared<float>(0.f);
		Add(0.5f, [this, Left] { if (AVisexPhotoMode* P = Photo()) { P->SetMotion(5); } *Left = 9.f; });
		Add(0.5f, [this, Left]
		{
			ACharacter* C = Her();
			const UCharacterMovementComponent* Move = C ? C->GetCharacterMovement() : nullptr;
			*Left -= GetWorld()->GetDeltaSeconds();
			const bool bApex = Move && Move->IsFalling() && Move->Velocity.Z <= 0.f;
			if (!bApex && *Left > 0.f) { --Next; Clock = 1.f; return; }   // again next frame
			Report(bApex ? TEXT("photo jump apex") : TEXT("photo jump: NO LEAP in 9 s"));
			Shot(bApex ? TEXT("photo_jump_apex") : TEXT("photo_jump_none"));
		});
		Add(2.f, [this] { Report(TEXT("photo jump landed")); });
	}
	// each pose once its flow in has ended (2.5 s caught them mid-transition in 0333/0351)
	for (int32 I = 0; I < Poses.Num(); ++I)
	{
		const int32 Index = FirstPose + I;
		const FString Name = Poses[I];
		Add(0.5f, [this, Index] { if (AVisexPhotoMode* P = Photo()) { P->SetMotion(Index); } });
		Add(5.f, [this, Index, Name]
		{
			AVisexPhotoMode* P = Photo();
			Report(FString::Printf(TEXT("photo pose %s (motion %d, now %d)"), *Name, Index, P ? P->GetMotion() : -1));
			Shot(TEXT("photo_pose_") + Name);
		});
	}
	// the return: she walks back to the stone, turns and sits. Waits for her to stop, then for the sit
	{
		TSharedRef<float> Left = MakeShared<float>(0.f);
		Add(0.5f, [this, Left, ReturnAt] { if (AVisexPhotoMode* P = Photo()) { P->SetMotion(ReturnAt); } *Left = 30.f; });
		Add(2.f, [this] { Report(TEXT("photo return walk")); Shot(TEXT("photo_return_walk")); });
		Add(0.5f, [this, Left]
		{
			ACharacter* C = Her();
			*Left -= GetWorld()->GetDeltaSeconds();
			if (C && C->GetVelocity().Size2D() > 5.f && *Left > 0.f) { --Next; Clock = 1.f; return; }
			Report(*Left > 0.f ? TEXT("photo return: she stopped") : TEXT("photo return: STILL MOVING after 30 s"));
		});
		Add(5.f, [this] { Report(TEXT("photo return sit")); Shot(TEXT("photo_return_sit")); });
	}
	AfterPoses = Steps.Num();
	// a full GC, then walk and run again: Shipping 0240 crashed here once GC freed the loop clips
	Add(1.f, [] { CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true); QA_LOG(TEXT("VisexQA: forced GC")); });
	Add(1.f, [this] { if (AVisexPhotoMode* P = Photo()) { P->SetMotion(2); } });
	Add(5.f, [this] { if (AVisexPhotoMode* P = Photo()) { P->SetMotion(3); } Report(TEXT("photo after GC")); });
	// hair turnaround on a standing pose out on the plaza (seated, the stone is behind her)
	Add(2.f, [this, FirstPose, Poses] { if (AVisexPhotoMode* P = Photo()) { const int32 Peace = Poses.IndexOfByKey(TEXT("Peace")); P->SetMotion(FirstPose + FMath::Max(Peace, 0)); P->SetHudVisible(false); } });
	Add(4.f, [] {});
	for (int32 K = 0; K < 8; ++K)
	{
		Add(1.5f, [this, K]
		{
			AVisexPhotoMode* P = Photo();
			ACharacter* C = Her();
			if (!P || !P->IsOpen() || !C) { return; }
			if (P->IsFree()) { P->ToggleFree(); }
			P->SetView(C->GetActorRotation().Yaw + 45.f * K, 380.f, 120.f);   // head to toe (QA 0286: 300 cut her head)
		});
		Add(1.5f, [this, K] { Shot(FString::Printf(TEXT("turn_body_%d"), K)); });
	}
	for (int32 K = 0; K < 8; ++K)
	{
		Add(1.f, [this, K]
		{
			AVisexPhotoMode* P = Photo();
			ACharacter* C = Her();
			if (!P || !P->IsOpen() || !C || !C->GetMesh()) { return; }
			// a little in front of the head bone, so side views keep her face, not just hair on sky
			const FVector Head = C->GetMesh()->GetSocketLocation(TEXT("head")) + FVector(0.f, 0.f, -6.f)
				+ C->GetActorForwardVector() * 8.f;
			const float A = FMath::DegreesToRadians(C->GetActorRotation().Yaw + 45.f * K);
			const FVector Loc = Head + FVector(FMath::Cos(A), FMath::Sin(A), 0.12f) * 95.f;
			P->SetFreeView(Loc, (Head - Loc).Rotation(), 0.f, 50.f);
		});
		Add(1.5f, [this, K] { Shot(FString::Printf(TEXT("turn_head_%d"), K)); });
	}
	Add(0.1f, [this] { if (AVisexPhotoMode* P = Photo()) { P->SetHudVisible(true); } });
	Add(1.f, [this] { if (AVisexPhotoMode* P = Photo()) { P->Close(); } });
	Add(5.f, [this] { Report(TEXT("after photo")); Shot(TEXT("after_photo")); });
	Add(1.f, [this]
	{
		// the tour's Settings pass saved; leave this Mac as a first launch (Epic until chosen)
		GConfig->RemoveKeyFromSection(TEXT("VisEx"), TEXT("bQualityChosen"), GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
		QA_LOG(TEXT("VisexQA: done"));
		UKismetSystemLibrary::QuitGame(this, UGameplayStatics::GetPlayerController(this, 0), EQuitPreference::Quit, false);
	});
}

void AVisexQATour::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Director) { return; }
	if (!bBuilt) { Build(); bBuilt = true; }
	FrameSum += DeltaSeconds * 1000.0;
	FrameMax = FMath::Max(FrameMax, double(DeltaSeconds) * 1000.0);
	GpuSum += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0));
	GameSum += FPlatformTime::ToMilliseconds(GGameThreadTime);
	DrawSum += FPlatformTime::ToMilliseconds(GRenderThreadTime);
	RhiSum += FPlatformTime::ToMilliseconds(GRHIThreadTime);
	++FrameN;
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
