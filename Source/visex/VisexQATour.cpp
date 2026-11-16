#include "VisexQATour.h"
#include "VisexMenuDirector.h"
#include "VisexLaunchCover.h"
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
#include "VisexMHBody.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "GameFramework/PlayerController.h"

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

	// photo mode, through every motion and pose
	Add(0.5f, [this, D] { Report(TEXT("before photo")); D->OnItemChosen(AVisexMenuDirector::ItemPhoto); });
	// when does she start to drop: just after choosing, through the stand-up, as the camera takes over
	float Since = 0.f;
	for (const float Wait : {0.1f, 0.3f, 0.4f, 0.7f, 1.5f})
	{
		Since += Wait;
		Add(Wait, [this, Since] { Report(FString::Printf(TEXT("photo +%.1fs"), Since)); });
	}
	// she pushed off the column and walks slowly in place: two shots 1 s apart, and how far she went
	TSharedRef<FVector> WalkFrom = MakeShared<FVector>(FVector::ZeroVector);
	Add(1.f, [this, WalkFrom]
	{
		Report(TEXT("photo open walk")); Shot(TEXT("photo_open_walk"));
		*WalkFrom = Her() ? Her()->GetActorLocation() : FVector::ZeroVector;
	});
	Add(1.f, [this, WalkFrom]
	{
		const float Moved = Her() ? FVector::Dist2D(Her()->GetActorLocation(), *WalkFrom) : -1.f;
		QA_LOG(TEXT("VisexQA: photo open walk moved %.1f cm in 1 s (%s)"), Moved, Moved >= 0.f && Moved < 3.f ? TEXT("PASS, in place") : TEXT("MOVES"));
		Report(TEXT("photo open walk +1s")); Shot(TEXT("photo_open_walk_1s"));
	});
	Add(4.f, [this]
	{
		AVisexPhotoMode* P = Photo();
		QA_LOG(TEXT("VisexQA: photo mode %s"), P && P->IsOpen() ? TEXT("open") : TEXT("DID NOT OPEN"));
		if (!P || !P->IsOpen()) { return; }
		for (int32 I = 0; I < 60; ++I)
		{
			P->SetMotion(I);
			if (P->GetMotion() != I) { QA_LOG(TEXT("VisexQA: %d motions"), I); break; }
		}
		P->SetMotion(0);
	});
	for (int32 I = 0; I < 40; ++I)
	{
		// switch, then shoot once the pose has settled (2.5 s caught them mid-flow in 0333/0351)
		Add(0.5f, [this, I]
		{
			AVisexPhotoMode* P = Photo();
			if (!P || !P->IsOpen()) { return; }
			P->SetMotion(I);
			if (P->GetMotion() != I) { Next = AfterPoses; }   // past the last motion
		});
		Add(4.5f, [this, I]
		{
			Report(FString::Printf(TEXT("photo motion %d"), I));
			Shot(FString::Printf(TEXT("photo_%02d"), I));
		});
	}
	AfterPoses = Steps.Num();
	// a full GC, then walk and run again: Shipping 0240 crashed here once GC freed the loop clips
	Add(1.f, [] { CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true); QA_LOG(TEXT("VisexQA: forced GC")); });
	Add(1.f, [this] { if (AVisexPhotoMode* P = Photo()) { P->SetMotion(1); } });
	Add(2.f, [this] { if (AVisexPhotoMode* P = Photo()) { P->SetMotion(2); } Report(TEXT("photo after GC")); });
	// hair turnaround: idle, 8 angles 45 deg apart, full body on the orbit camera, then head and
	// shoulders on the free camera. No keypresses, so it runs on any keyboard layout (QA 0281)
	Add(1.f, [this] { if (AVisexPhotoMode* P = Photo()) { P->SetMotion(0); P->SetHudVisible(false); } });
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

void AVisexQATour::BuildFeet()
{
	auto Add = [this](float Wait, TFunction<void()> Run) { Steps.Add({Wait, MoveTemp(Run)}); };
	AVisexMenuDirector* D = Director;
	auto Editor = [this]() -> AVisexBodyEditor* { for (TActorIterator<AVisexBodyEditor> It(GetWorld()); It; ++It) { return *It; } return nullptr; };
	ApplyCvarArg();   // -visexcvar=name=value, as in the gaps tour (the film with the MetaHuman body on)
	// one move: the phase starts with Run, four frames of her feet 0.6 s apart, then it settles
	auto Move = [this, &Add](const FString& Name, float Settle, TFunction<void()> Run, float First = 0.6f, float Gap = 0.6f)
	{
		Add(0.5f, [this, Name, Run] { Feet.SetPhase(Name); QA_LOG(TEXT("VisexFeet: phase %s"), *Name); Run(); });
		for (int32 I = 0; I < 4; ++I)
		{
			Add(I ? Gap : First, [this, Name, I] { Feet.Shoot(GetWorld(), Her(), Name.Replace(TEXT(" "), TEXT("_")), I); });
		}
		Add(Settle, [] {});
	};
	// the clock holds through her walk in (Tick), so this is the menu with her on her spot
	Move(TEXT("standing"), 2.f, [] {});
	// her walk out, start to stop: the lean-up hands over at 0.6 s, she stops by ~1.9 s
	Move(TEXT("body out"), 3.f, [D] { D->OpenBody(); }, 0.75f, 0.4f);
	Move(TEXT("body back"), 3.f, [Editor] { if (AVisexBodyEditor* E = Editor()) { E->Close(); } });
	// -visexfilm (run with -benchmark -fps=60): every other frame shot, the arrival to here
	Add(0.1f, [this] { bFilm = false; });
	Move(TEXT("wardrobe out"), 2.5f, [D] { D->OpenWardrobe(); });
	Move(TEXT("wardrobe back"), 3.f, [Editor] { if (AVisexBodyEditor* E = Editor()) { E->Close(); } });
	Move(TEXT("photo enter"), 3.f, [D] { D->OnItemChosen(AVisexMenuDirector::ItemPhoto); });
	Move(TEXT("photo back"), 3.f, [this] { if (AVisexPhotoMode* P = Photo()) { P->Close(); } });
	Add(0.5f, [this]
	{
		Feet.SetPhase(TEXT("done"));
		QA_LOG(TEXT("VisexFeet: %d frames in %.1f s, %.1f fps"), FeetFrames, FPlatformTime::Seconds() - FeetT0, FeetFrames / FMath::Max(0.1, FPlatformTime::Seconds() - FeetT0));
		QA_LOG(TEXT("VisexQA: done"));
		UKismetSystemLibrary::QuitGame(this, UGameplayStatics::GetPlayerController(this, 0), EQuitPreference::Quit, false);
	});
}

void AVisexQATour::HiShot(const FString& What)
{
	FString Shots;
	UGameViewportClient* View = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (FParse::Value(FCommandLine::Get(), TEXT("visexshots="), Shots) && View)
	{
		const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("QA") / FString::Printf(TEXT("%s_%s.png"), *Shots, *What.Replace(TEXT(" "), TEXT("_"))));
		View->ConsoleCommand(FString::Printf(TEXT("HighResShot 3840x2160 filename=\"%s\""), *File));
		QA_LOG(TEXT("VisexQA: 4K shot %s"), *File);
		CloseShots(Shots, What);
	}
}

void AVisexQATour::CloseShots(const FString& Shots, const FString& What)
{
	// -visexclose=<What>:<bone>:<x,y,z>:<fov>[u][:<tx,ty,tz>][+...]: with the 4K frame of <What> (its name
	// or its ending, "Crown" for photo_03_Crown), a 4K scene capture looking at a bone of the body on
	// screen from x forward, y to her right, z up of it (cm), the target moved by t. No post grade,
	// unless the field of view carries a "g" (17g).
	FString All;
	ACharacter* C = Her();
	if (!FParse::Value(FCommandLine::Get(), TEXT("visexclose="), All, false) || !C)
	{
		return;
	}
	TArray<FString> Specs;
	All.ParseIntoArray(Specs, TEXT("+"));
	int32 N = 0;
	for (const FString& Spec : Specs)
	{
		TArray<FString> F;
		Spec.ParseIntoArray(F, TEXT(":"));
		const auto Vec = [](const FString& Text)
		{
			TArray<FString> P;
			Text.ParseIntoArray(P, TEXT(","));
			return P.Num() == 3 ? FVector(FCString::Atof(*P[0]), FCString::Atof(*P[1]), FCString::Atof(*P[2])) : FVector::ZeroVector;
		};
		if (F.Num() < 4 || !What.EndsWith(F[0]))
		{
			continue;
		}
		const USkeletalMeshComponent* Shown = UVisexMHBodyComponent::ShownBody(C);
		const USkeletalMeshComponent* Mesh = Shown ? Shown : C->GetMesh();
		const auto Frame = [C](const FVector& V) { return C->GetActorForwardVector() * V.X + C->GetActorRightVector() * V.Y + FVector(0, 0, V.Z); };
		const FVector Target = Mesh->GetSocketLocation(FName(*F[1])) + Frame(F.Num() > 4 ? Vec(F[4]) : FVector::ZeroVector);
		const FVector From = Mesh->GetSocketLocation(FName(*F[1])) + Frame(Vec(F[2]));
		if (!CloseCapture)
		{
			CloseCapture = NewObject<USceneCaptureComponent2D>(this, NAME_None, RF_Transient);
			CloseCapture->TextureTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, 3840, 2160, RTF_RGBA8_SRGB);
			CloseCapture->CaptureSource = SCS_FinalToneCurveHDR;
			CloseCapture->bCaptureEveryFrame = false;
			CloseCapture->bCaptureOnMovement = false;
			CloseCapture->bAlwaysPersistRenderingState = true;
			CloseCapture->RegisterComponent();
		}
		CloseCapture->FOVAngle = FCString::Atof(*F[3]);
		// "17g": with the level's grade (the final LDR colour); without, the tone curve only
		CloseCapture->CaptureSource = F[3].Contains(TEXT("g")) ? SCS_FinalColorLDR : SCS_FinalToneCurveHDR;
		// "25u": unlit, base colours only (skin against hair reads in any light)
		CloseCapture->ShowFlags.SetLighting(!F[3].EndsWith(TEXT("u")));
		CloseCapture->SetWorldLocationAndRotation(From, (Target - From).Rotation());
		for (int32 I = 0; I < 4; ++I)
		{
			CloseCapture->CaptureScene();   // a few, so the temporal passes settle
		}
		const FString Name = FString::Printf(TEXT("%s_%s_close%d_%s.png"), *Shots, *What.Replace(TEXT(" "), TEXT("_")), N++, *F[1]);
		UKismetRenderingLibrary::ExportRenderTarget(this, CloseCapture->TextureTarget, FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("QA")), Name);
		QA_LOG(TEXT("VisexQA: close shot %s from %s at %s"), *Name, *From.ToCompactString(), *Target.ToCompactString());
	}
}

void AVisexQATour::ApplyCvarArg()
{
	// -visexoutfit=None (or an outfit's name): worn for this run, in memory only
	FString Wear;
	ACharacter* Worn = Her();
	UVisexBodyComponent* WornBody = Worn ? Worn->FindComponentByClass<UVisexBodyComponent>() : nullptr;
	if (WornBody && FParse::Value(FCommandLine::Get(), TEXT("visexoutfit="), Wear))
	{
		WornBody->Profile.Outfit = Wear == TEXT("None") ? FName(NAME_None) : FName(*Wear);
		WornBody->Apply();
		QA_LOG(TEXT("VisexQA: outfit for this run: %s"), *Wear);
	}
	FString Setting, More, Var, Value;
	// several: -visexcvar=a=1+b=Elf (the first is the one the report names)
	FString First;
	if (FParse::Value(FCommandLine::Get(), TEXT("visexcvar="), Setting) && Setting.Split(TEXT("+"), &First, &More))
	{
		Setting = First;
		TArray<FString> Rest;
		More.ParseIntoArray(Rest, TEXT("+"));
		for (const FString& One : Rest)
		{
			IConsoleVariable* V = One.Split(TEXT("="), &Var, &Value) ? IConsoleManager::Get().FindConsoleVariable(*Var) : nullptr;
			if (V) { V->Set(*Value, ECVF_SetByConsole); }
			QA_LOG(TEXT("VisexGaps: also setting %s, reads %s"), *One, V ? *V->GetString() : TEXT("NO SUCH CVAR"));
		}
	}
	if (!Setting.IsEmpty() && Setting.Split(TEXT("="), &Var, &Value))
	{
		IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(*Var);
		if (V) { V->Set(*Value, ECVF_SetByConsole); }
		// the body reads visex.Body.KeepFeet when it places the mesh
		if (ACharacter* C = Her())
		{
			if (UVisexBodyComponent* B = C->FindComponentByClass<UVisexBodyComponent>()) { B->Apply(); }
		}
		QA_LOG(TEXT("VisexGaps: setting %s, reads %s"), *Setting, V ? *V->GetString() : TEXT("NO SUCH CVAR"));
	}
	else
	{
		QA_LOG(TEXT("VisexGaps: setting default"));
	}
	ApplyBodyArg();
}

void AVisexQATour::BuildGaps()
{
	auto Add = [this](float Wait, TFunction<void()> Run) { Steps.Add({Wait, MoveTemp(Run)}); };
	AVisexMenuDirector* D = Director;
	ApplyCvarArg();
	// -visexshots=<name>: 4K frames of the game view on the way (lean, three of the walk, idle),
	// for a same-camera pair with a cvar off and on. Each costs a hitch, so only when asked.
	auto Hi = [this](const TCHAR* What) { HiShot(What); };
	// the clock holds through her walk in: six seconds into the lean, two seconds measured
	Add(6.f, [this] { Feet.SetPhase(TEXT("lean")); });
	// Settings: she climbs onto the drum and perches; eight seconds to settle, three measured
	Add(2.f, [this, D, Hi] { Hi(TEXT("lean")); Feet.SetPhase(TEXT("sitdown")); D->OpenSettings(); });
	Add(8.f, [this] { Feet.SetPhase(TEXT("sit")); });
	Add(3.f, [this, D] { Feet.SetPhase(TEXT("standup")); D->CloseSettings(); });
	Add(8.f, [this, D] { Feet.SetPhase(TEXT("start")); D->OnItemChosen(AVisexMenuDirector::ItemStart); });
	// Start: she stands up, walks off and stops on the open sand
	Add(3.f, [Hi] { Hi(TEXT("walk1")); });
	Add(1.f, [Hi] { Hi(TEXT("walk2")); });
	Add(1.f, [Hi] { Hi(TEXT("walk3")); });
	Add(9.f, [this] { Feet.SetPhase(TEXT("idle")); });
	Add(2.f, [this, Hi]
	{
		Hi(TEXT("idle"));
		Feet.SetPhase(TEXT("done"));
		QA_LOG(TEXT("VisexQA: done"));
		UKismetSystemLibrary::QuitGame(this, UGameplayStatics::GetPlayerController(this, 0), EQuitPreference::Quit, false);
	});
}

void AVisexQATour::BuildSpikes()
{
	auto Add = [this](float Wait, TFunction<void()> Run) { Steps.Add({Wait, MoveTemp(Run)}); };
	AVisexMenuDirector* D = Director;
	ApplyCvarArg();
	Spikes.Log = [](const FString& Line) { QA_LOG(TEXT("%s"), *Line); };
	auto Phase = [this](const FString& Name) { Spikes.SetPhase(Name); };
	auto Height = [this](float Cm)
	{
		ACharacter* C = Her();
		if (UVisexBodyComponent* B = C ? C->FindComponentByClass<UVisexBodyComponent>() : nullptr) { B->SetValue(TEXT("Height"), Cm); }
	};
	Phase(TEXT("arrival walk"));
	// the clock holds through her walk in; then every screen she has a pose on
	Add(4.f, [Phase] { Phase(TEXT("menu lean")); });
	Add(3.f, [Phase, D] { Phase(TEXT("settings sit down")); D->OpenSettings(); });
	Add(8.f, [Phase] { Phase(TEXT("settings sit")); });
	Add(3.f, [this, Phase, D] { HiShot(TEXT("settings_sit")); Phase(TEXT("settings stand up")); D->CloseSettings(); });
	Add(8.f, [Phase, D] { Phase(TEXT("photo enter")); D->OnItemChosen(AVisexMenuDirector::ItemPhoto); });
	Add(4.f, [] {});
	for (int32 I = 0; I < 40; ++I)
	{
		Add(0.5f, [this, Phase, I]
		{
			AVisexPhotoMode* P = Photo();
			if (!P || !P->IsOpen()) { Next = AfterPoses; return; }
			P->SetMotion(I);
			if (P->GetMotion() != I) { Next = AfterPoses; return; }   // past the last motion
			Phase(FString::Printf(TEXT("photo %02d %s"), I, *P->GetMotionName(I)));
		});
		// -visexshots=<name>: a 4K frame of each settled pose, to look at what the numbers say
		Add(4.5f, [this, I] { if (AVisexPhotoMode* P = Photo()) { HiShot(FString::Printf(TEXT("photo_%02d_%s"), I, *P->GetMotionName(I))); } });
	}
	AfterPoses = Steps.Num();
	Add(0.5f, [this, Phase] { Phase(TEXT("photo close")); if (AVisexPhotoMode* P = Photo()) { P->Close(); } });
	Add(4.f, [Phase, D] { Phase(TEXT("start walk")); D->OnItemChosen(AVisexMenuDirector::ItemStart); });
	Add(14.f, [Phase] { Phase(TEXT("proud idle")); });
	Add(3.f, [Phase, Height] { Phase(TEXT("height 145")); Height(145.f); });
	Add(3.f, [Phase, Height] { Phase(TEXT("height 185")); Height(185.f); });
	Add(3.f, [this, Phase, Height]
	{
		Height(166.f);
		Phase(TEXT("done"));
		static const TCHAR* Names[FVisexMHSpikes::Count] = {TEXT("body"), TEXT("face"), TEXT("garment")};
		for (int32 I = 0; I < FVisexMHSpikes::Count; ++I)
		{
			const FVisexSpike& S = Spikes.Total[I];
			QA_LOG(TEXT("VisexSpikes: TOTAL %s: %s, stretch %.2fx on %s (%d over %.0fx), creases %.2fx on %s (%d over), grew %.2f cm at most on %s"), Names[I],
				!S.bValid ? TEXT("NOT MEASURED") : S.Fails() ? TEXT("FAIL") : TEXT("pass"), S.Ratio, *S.Bone.ToString(), S.Over, FVisexMHSpikes::Limit,
				S.Crease, *S.CreaseBone.ToString(), S.CreaseOver, S.Grow, *S.GrowBone.ToString());
		}
		const FVisexPoke& K = Spikes.TotalPoke;
		QA_LOG(TEXT("VisexSpikes: TOTAL poke: %s, where skin is drawn %d fabric vertices under it at most, deepest %.2f cm on %s; under clipped skin %d, deepest %.2f cm on %s"),
			!K.bValid ? TEXT("NOT MEASURED") : K.Fails() ? TEXT("FAIL") : TEXT("pass"), K.EdgeUnder, K.EdgeDepth, *K.EdgeBone.ToString(), K.Under, K.Depth, *K.Bone.ToString());
		QA_LOG(TEXT("VisexQA: done"));
		UKismetSystemLibrary::QuitGame(this, UGameplayStatics::GetPlayerController(this, 0), EQuitPreference::Quit, false);
	});
}

void AVisexQATour::ApplyBodyArg()
{
	FString All;
	ACharacter* C = Her();
	UVisexBodyComponent* B = C ? C->FindComponentByClass<UVisexBodyComponent>() : nullptr;
	if (!B || !FParse::Value(FCommandLine::Get(), TEXT("visexbody="), All, false))
	{
		return;
	}
	TArray<FString> Parts;
	All.ParseIntoArray(Parts, TEXT("+"));
	for (const FString& One : Parts)
	{
		FString Name, Value;
		if (One.Split(TEXT("="), &Name, &Value)) { B->Profile.Set(FName(*Name), FCString::Atof(*Value)); }
	}
	B->Apply();   // in memory only: nothing here saves the profile
	QA_LOG(TEXT("VisexQA: body values for this run: %s"), *All);
}

void AVisexQATour::ApplySetup(const FBodySetup& Setup)
{
	ACharacter* C = Her();
	UVisexBodyComponent* B = C ? C->FindComponentByClass<UVisexBodyComponent>() : nullptr;
	if (!B)
	{
		return;
	}
	for (const FVisexBodySlider& Row : FVisexBodyProfile::Sliders()) { B->Profile.Set(Row.Name, Row.Default); }
	for (const TPair<FName, float>& V : Setup.Values) { B->Profile.Set(V.Key, V.Value); }
	B->Apply();
}

void AVisexQATour::PortraitShot(const FString& What, bool bHead, const FString& Frame)
{
	FString Shots;
	ACharacter* C = Her();
	if (!C || !FParse::Value(FCommandLine::Get(), TEXT("visexshots="), Shots))
	{
		return;
	}
	const USkeletalMeshComponent* Shown = UVisexMHBodyComponent::ShownBody(C);
	const USkeletalMeshComponent* Mesh = Shown ? Shown : C->GetMesh();
	if (!PortraitCapture)
	{
		PortraitCapture = NewObject<USceneCaptureComponent2D>(this, NAME_None, RF_Transient);
		PortraitCapture->TextureTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, 1280, 2160, RTF_RGBA8_SRGB);
		PortraitCapture->CaptureSource = SCS_FinalColorLDR;   // with the level's grade
		PortraitCapture->bCaptureEveryFrame = false;
		PortraitCapture->bCaptureOnMovement = false;
		PortraitCapture->bAlwaysPersistRenderingState = true;
		PortraitCapture->RegisterComponent();
	}
	// the same camera for every setup: in front of her, anchored to the floor under her (the driver's
	// mesh origin) and the pelvis' place over it, never to a bone a slider moves
	const FVector Fwd = C->GetActorForwardVector();
	const FVector Pelvis = C->GetMesh()->GetSocketLocation(TEXT("pelvis"));
	FVector Target(Pelvis.X, Pelvis.Y, C->GetMesh()->GetComponentLocation().Z + 92.f);
	float Distance = 470.f;
	PortraitCapture->FOVAngle = 15.f;
	FVector From = Target + Fwd * Distance;
	if (bHead)
	{
		// head and shoulders, from where her face points (the head bone's Y), a little to one side:
		// she looks about in the lean, and a camera on the actor's forward meets the back of her hair
		Target = Mesh->GetSocketLocation(TEXT("head")) + FVector(0.f, 0.f, -4.f);
		const FVector Face = C->GetMesh()->GetSocketQuaternion(TEXT("head")).GetAxisY().GetSafeNormal2D();
		From = Target + Face.RotateAngleAxis(18.f, FVector::UpVector) * 170.f + FVector(0.f, 0.f, 6.f);
		PortraitCapture->FOVAngle = 13.f;
	}
	// "torso" / "side" / "back" (-visexframe=body+torso+side+back): the middle of her, bust to thigh, from
	// her front right, in profile from her right, or from behind her right, for the shape sliders.
	// One camera per pose and framing.
	if (!bHead && Frame != TEXT("body"))
	{
		Target.Z = C->GetMesh()->GetComponentLocation().Z + 108.f;
		From = Target + Fwd.RotateAngleAxis(Frame == TEXT("side") ? 88.f : Frame == TEXT("back") ? 152.f : 38.f, FVector::UpVector) * 470.f;
		PortraitCapture->FOVAngle = 10.5f;
	}
	// the full figure: one camera per pose, where the pose's first setup (the default body) put it
	const FString Key = What.Left(What.Find(TEXT("_"))) + TEXT("_") + Frame;
	if (bHead)
	{
	}
	else if (const TPair<FVector, FVector>* Kept = PortraitCameras.Find(Key))
	{
		From = Kept->Key;
		Target = Kept->Value;
	}
	else
	{
		PortraitCameras.Add(Key, TPair<FVector, FVector>(From, Target));
	}
	PortraitCapture->SetWorldLocationAndRotation(From, (Target - From).Rotation());
	for (int32 I = 0; I < 4; ++I)
	{
		PortraitCapture->CaptureScene();   // a few, so the temporal passes settle
	}
	const FString Name = FString::Printf(TEXT("%s_%s_%s.png"), *Shots, *What.Replace(TEXT(" "), TEXT("_")), bHead ? TEXT("head") : *Frame);
	UKismetRenderingLibrary::ExportRenderTarget(this, PortraitCapture->TextureTarget, FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("QA")), Name);
}

void AVisexQATour::BuildSliders()
{
	auto Add = [this](float Wait, TFunction<void()> Run) { Steps.Add({Wait, MoveTemp(Run)}); };
	AVisexMenuDirector* D = Director;
	ApplyCvarArg();
	Spikes.Log = [](const FString& Line) { QA_LOG(TEXT("%s"), *Line); };
	Feet.Log = [](const FString& Line) { QA_LOG(TEXT("%s"), *Line); };
	// every bone-scale slider alone at both ends, the height's ends, the skin tone's steps, and
	// two bodies with many sliders pushed together
	Setups.Reset();
	const auto V = [](const TCHAR* Name, float Value) { return TPair<FName, float>(FName(Name), Value); };
	Setups.Add({TEXT("default"), {}});
	// -visexsetups=Bust+Fat+Hips: only these sliders at both ends (and the default body), for the
	// shape sliders or a short re-check; "shape" is the seven morph sliders
	FString Only;
	TArray<FString> Names = {TEXT("Head"), TEXT("Neck"), TEXT("Shoulders"), TEXT("Chest"), TEXT("Waist"), TEXT("Hips"), TEXT("Legs"),
		TEXT("Thighs"), TEXT("Calves"), TEXT("ArmLength"), TEXT("Arms"), TEXT("Hands"), TEXT("Feet"), TEXT("Height")};
	const bool bOnly = FParse::Value(FCommandLine::Get(), TEXT("visexsetups="), Only, false);
	if (bOnly)
	{
		Only.ReplaceInline(TEXT("shape"), TEXT("Bust+BustLift+BustApart+Glutes+Abs+Fat+ThighGap"));
		Only.ParseIntoArray(Names, TEXT("+"));
	}
	for (const FString& Name : Names)
	{
		if (const FVisexBodySlider* Row = FVisexBodyProfile::Find(FName(*Name)))
		{
			Setups.Add({Name + TEXT("_min"), {V(*Name, Row->Min)}});
			Setups.Add({Name + TEXT("_max"), {V(*Name, Row->Max)}});
		}
	}
	for (const float Tone : {0.f, 0.5f, 0.75f, 1.f})
	{
		if (bOnly) { break; }
		Setups.Add({FString::Printf(TEXT("Skin_%03d"), FMath::RoundToInt(Tone * 100.f)), {V(TEXT("Skin"), Tone)}});
	}
	if (!bOnly)
	{
	Setups.Add({TEXT("small"), {V(TEXT("Height"), 150.f), V(TEXT("Head"), 1.0f), V(TEXT("Neck"), 0.84f), V(TEXT("Shoulders"), 0.82f), V(TEXT("Chest"), 0.90f),
		V(TEXT("Waist"), 0.74f), V(TEXT("Hips"), 0.88f), V(TEXT("Legs"), 0.92f), V(TEXT("Thighs"), 0.90f), V(TEXT("Calves"), 0.90f),
		V(TEXT("ArmLength"), 0.93f), V(TEXT("Arms"), 0.84f), V(TEXT("Hands"), 0.85f), V(TEXT("Feet"), 0.87f)}});
	Setups.Add({TEXT("tall"), {V(TEXT("Height"), 183.f), V(TEXT("Head"), 1.08f), V(TEXT("Neck"), 1.10f), V(TEXT("Shoulders"), 1.15f), V(TEXT("Chest"), 1.15f),
		V(TEXT("Waist"), 1.05f), V(TEXT("Hips"), 1.15f), V(TEXT("Legs"), 1.10f), V(TEXT("Thighs"), 1.35f), V(TEXT("Calves"), 1.22f),
		V(TEXT("ArmLength"), 1.06f), V(TEXT("Arms"), 1.22f), V(TEXT("Hands"), 1.10f), V(TEXT("Feet"), 1.10f)}});
	}
	// one pose: each setup is put on, left to settle (her contact passes, the capsule), measured
	// for a second under its own phase name, then reported and shot
	const auto Pass = [this, Add](const FString& Pose)
	{
		for (int32 I = 0; I < Setups.Num(); ++I)
		{
			Add(0.3f, [this, I] { Spikes.SetPhase(TEXT("settle")); Feet.SetPhase(TEXT("settle")); ApplySetup(Setups[I]); });
			Add(1.6f, [this, I, Pose] { const FString Name = Pose + TEXT(" ") + Setups[I].Name; Spikes.SetPhase(Name); Feet.SetPhase(Name); });
			Add(1.0f, [this, I, Pose] { SetupReport(Pose, Setups[I]); });
		}
		Add(0.3f, [this] { Spikes.SetPhase(TEXT("settle")); Feet.SetPhase(TEXT("settle")); ApplySetup(Setups[0]); });
	};
	const auto Motion = [this](const TCHAR* Name)
	{
		AVisexPhotoMode* P = Photo();
		for (int32 I = 0; P && P->IsOpen() && I < 40; ++I)
		{
			if (P->GetMotionName(I) == Name) { P->SetMotion(I); QA_LOG(TEXT("VisexSliders: photo motion %s"), Name); return; }
		}
		QA_LOG(TEXT("VisexSliders: NO photo motion %s"), Name);
	};
	Spikes.SetPhase(TEXT("arrival walk"));
	// the clock holds through her walk in
	Add(4.f, [] {});
	Pass(TEXT("lean"));
	Add(1.f, [D] { D->OnItemChosen(AVisexMenuDirector::ItemPhoto); });
	Add(5.f, [Motion] { Motion(TEXT("LegLift")); });
	Add(5.f, [] {});
	Pass(TEXT("leglift"));
	Add(0.5f, [Motion] { Motion(TEXT("Walk")); });
	Add(4.f, [] {});
	Pass(TEXT("walk"));
	if (bOnly)
	{
		// a subset run also stands her still in the open (the photo Idle), where a shape can be judged:
		// the lean hides her seat and the walk never shows two setups in the same step
		Add(0.5f, [Motion] { Motion(TEXT("Idle")); });
		Add(4.f, [] {});
		Pass(TEXT("idle"));
	}
	Add(0.5f, [this]
	{
		// her own profile back, as it was loaded (nothing was saved on the way)
		ACharacter* C = Her();
		if (UVisexBodyComponent* B = C ? C->FindComponentByClass<UVisexBodyComponent>() : nullptr) { B->Load(); B->Apply(); }
		Spikes.SetPhase(TEXT("done"));
		Feet.SetPhase(TEXT("done"));
		QA_LOG(TEXT("VisexQA: done"));
		UKismetSystemLibrary::QuitGame(this, UGameplayStatics::GetPlayerController(this, 0), EQuitPreference::Quit, false);
	});
}

void AVisexQATour::BuildClip()
{
	auto Add = [this](float Wait, TFunction<void()> Run) { Steps.Add({Wait, MoveTemp(Run)}); };
	AVisexMenuDirector* D = Director;
	ApplyCvarArg();
	Add(4.f, [] {});
	Add(1.f, [D] { D->OnItemChosen(AVisexMenuDirector::ItemPhoto); });
	Add(5.f, [this]
	{
		AVisexPhotoMode* P = Photo();
		for (int32 I = 0; P && P->IsOpen() && I < 40; ++I)
		{
			if (P->GetMotionName(I) == TEXT("Walk")) { P->SetMotion(I); QA_LOG(TEXT("VisexClip: photo motion Walk")); return; }
		}
		QA_LOG(TEXT("VisexClip: NO photo motion Walk"));
	});
	Add(4.f, [this] { ClipLeft = 150; ClipN = 0; });
	Add(6.f, [this]
	{
		ACharacter* C = Her();
		if (UVisexBodyComponent* B = C ? C->FindComponentByClass<UVisexBodyComponent>() : nullptr) { B->Load(); B->Apply(); }
		QA_LOG(TEXT("VisexQA: done"));
		UKismetSystemLibrary::QuitGame(this, UGameplayStatics::GetPlayerController(this, 0), EQuitPreference::Quit, false);
	});
}

void AVisexQATour::ClipFrame()
{
	FString Shots;
	ACharacter* C = Her();
	if (!C || !FParse::Value(FCommandLine::Get(), TEXT("visexshots="), Shots))
	{
		ClipLeft = 0;
		return;
	}
	if (!ClipCapture)
	{
		ClipCapture = NewObject<USceneCaptureComponent2D>(this, NAME_None, RF_Transient);
		ClipCapture->TextureTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, 3840, 2160, RTF_RGBA8_SRGB);
		ClipCapture->CaptureSource = SCS_FinalColorLDR;
		ClipCapture->bCaptureEveryFrame = false;
		ClipCapture->bCaptureOnMovement = false;
		ClipCapture->bAlwaysPersistRenderingState = true;
		ClipCapture->FOVAngle = 20.f;
		ClipCapture->RegisterComponent();
		// one camera for the whole clip: her torso from the front and a little to her left
		const FVector Pelvis = C->GetMesh()->GetSocketLocation(TEXT("pelvis"));
		const FVector Target(Pelvis.X, Pelvis.Y, C->GetMesh()->GetComponentLocation().Z + 122.f);
		const FVector From = Target + C->GetActorForwardVector().RotateAngleAxis(-32.f, FVector::UpVector) * 215.f + FVector(0.f, 0.f, 4.f);
		ClipCapture->SetWorldLocationAndRotation(From, (Target - From).Rotation());
		QA_LOG(TEXT("VisexClip: camera %s at %s"), *From.ToCompactString(), *Target.ToCompactString());
	}
	ClipCapture->CaptureScene();
	UKismetRenderingLibrary::ExportRenderTarget(this, ClipCapture->TextureTarget, FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("QA")),
		FString::Printf(TEXT("%s_clip_%04d.png"), *Shots, ClipN++));
	--ClipLeft;
}

void AVisexQATour::SetupReport(const FString& Pose, const FBodySetup& Setup)
{
	ACharacter* C = Her();
	if (!C)
	{
		return;
	}
	const USkeletalMeshComponent* Driver = C->GetMesh();
	const USkeletalMeshComponent* Shown = UVisexMHBodyComponent::ShownBody(C);
	const UCapsuleComponent* Cap = C->GetCapsuleComponent();
	FString Joints = TEXT("no MetaHuman body");
	if (Shown)
	{
		// how far the MetaHuman's joints sit from the driver's (the driver holds the contacts)
		const auto Off = [Shown, Driver](const TCHAR* Bone) { return float(FVector::Dist(Shown->GetSocketLocation(Bone), Driver->GetSocketLocation(Bone))); };
		Joints = FString::Printf(TEXT("MetaHuman joint from the driver's, cm: hand L %.2f R %.2f, foot L %.2f R %.2f, head %.2f, pelvis %.2f"),
			Off(TEXT("hand_l")), Off(TEXT("hand_r")), Off(TEXT("foot_l")), Off(TEXT("foot_r")), Off(TEXT("head")), Off(TEXT("pelvis")));
	}
	QA_LOG(TEXT("VisexSliders: %s | %s | capsule half %.2f radius %.2f, actor Z %.2f, mesh rel Z %.2f, mesh scale %.4f | %s"), *Pose, *Setup.Name,
		Cap ? Cap->GetUnscaledCapsuleHalfHeight() : 0.f, Cap ? Cap->GetUnscaledCapsuleRadius() : 0.f, C->GetActorLocation().Z,
		Driver->GetRelativeLocation().Z, Driver->GetRelativeScale3D().Z, *Joints);
	const FString What = Pose + TEXT("_") + Setup.Name;
	FString Frames = TEXT("body+head");
	FParse::Value(FCommandLine::Get(), TEXT("visexframe="), Frames);
	TArray<FString> Each;
	Frames.ParseIntoArray(Each, TEXT("+"));
	for (const FString& Frame : Each)
	{
		PortraitShot(What, Frame == TEXT("head"), Frame);
	}
}

void AVisexQATour::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Director) { return; }
	if (!bBuilt)
	{
		const bool bGaps = FParse::Param(FCommandLine::Get(), TEXT("visexgaps"));
		bFilm = FParse::Param(FCommandLine::Get(), TEXT("visexfilm"));
		bFeet = bGaps || bFilm || FParse::Param(FCommandLine::Get(), TEXT("visexfeet"));
		FeetClock = 2e5f;   // until she first moves
		Feet.Log = [](const FString& Line) { QA_LOG(TEXT("%s"), *Line); };
		bSpikes = FParse::Param(FCommandLine::Get(), TEXT("visexspikes"));
		bSliders = FParse::Param(FCommandLine::Get(), TEXT("visexsliders"));
		FParse::Param(FCommandLine::Get(), TEXT("visexclip")) ? BuildClip() :
		bSliders ? BuildSliders() : bSpikes ? BuildSpikes() : bGaps ? BuildGaps() : bFeet ? BuildFeet() : Build();
		bBuilt = true;
	}
	if (ClipLeft > 0)
	{
		ClipFrame();
	}
	if (bFeet)
	{
		// the tour's real frame rate (the film runs at a fixed step: every frame shot)
		++FeetFrames;
		if (FeetT0 == 0.0) { FeetT0 = FPlatformTime::Seconds(); }
		if (bFilm && (FeetFrames & 1)) { FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("QA/film/f%05d.png"), FeetFrames), false, false); }
	}
	if (bFeet)
	{
		FeetClock += FeetClock < 1e5f ? DeltaSeconds : 0.f;
		Feet.Tick(Her(), DeltaSeconds);
		// the arrival, as she walks in: four frames 0.6 s apart once she moves (before that she
		// is held under the launch cover, and that is not measured)
		const ACharacter* C = Her();
		if (Next == 0 && C && C->GetVelocity().Size2D() > 20.f && FeetClock > 1e5f) { FeetClock = 0.f; Feet.SetPhase(TEXT("arrival")); }
		if (Next == 0 && FeetClock < 1e5f && ArrivalShots < 4 && FeetClock > 0.6f * ArrivalShots)
		{
			Feet.Shoot(GetWorld(), Her(), TEXT("arrival"), ArrivalShots++);
		}
	}
	if (bSpikes || bSliders)
	{
		Spikes.Tick(Her());
	}
	if (bSliders && !bFeet)
	{
		Feet.Tick(Her(), DeltaSeconds);
	}
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
	// the tour starts once the launch cover has faded into the menu (VisexLaunchCover.h)
	if (!bCoverLogged)
	{
		const UVisexLaunchCover* Cover = GetGameInstance() ? GetGameInstance()->GetSubsystem<UVisexLaunchCover>() : nullptr;
		if (Cover && !Cover->IsDone())
		{
			return;
		}
		bCoverLogged = true;
		QA_LOG(TEXT("VisexQA: launch cover held %.2f s, released: %s"), Cover ? Cover->GetHeldSeconds() : 0.f, Cover ? *Cover->GetReleaseReason() : TEXT("no cover"));
	}
	// the first menu shot also waits for her walk in to the column (visex.Menu.Arrival)
	Clock = (Next == 0 && Director->IsArriving()) ? 0.f : Clock + DeltaSeconds;
	if (Steps.IsValidIndex(Next) && Clock >= Steps[Next].Wait)
	{
		Clock = 0.f;
		Steps[Next++].Run();
	}
}
