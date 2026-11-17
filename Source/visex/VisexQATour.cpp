#include "VisexQATour.h"
#include "VisexMenuDirector.h"
#include "VisexLaunchCover.h"
#include "VisexPhotoMode.h"
#include "VisexBodyEditor.h"
#include "VisexBodyComponent.h"
#include "VisexBodyProfile.h"
#include "VisexAnimInstance.h"
#include "visex.h"
#include "Engine/Engine.h"
#include "VisexShellComponent.h"
#include "EngineUtils.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
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
#include "Components/SpotLightComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include <unistd.h>
#include "Animation/AnimClassInterface.h"
#include "BoneControllers/AnimNode_LegIK.h"
#include "BoneControllers/AnimNode_FootPlacement.h"

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
			// which montage holds her, and where in it (the Start walk-off waits on it)
			const UAnimInstance* Anim = C->GetMesh() ? C->GetMesh()->GetAnimInstance() : nullptr;
			if (const UAnimMontage* M = Anim ? Anim->GetCurrentActiveMontage() : nullptr)
			{
				const UAnimSequenceBase* Clip = M->SlotAnimTracks.Num() && M->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() ? M->SlotAnimTracks[0].AnimTrack.AnimSegments[0].GetAnimReference().Get() : nullptr;
				Body += FString::Printf(TEXT(", montage %s at %.2f/%.2f w %.2f, rm mode %d"), Clip ? *Clip->GetName() : *M->GetName(), Anim->Montage_GetPosition(M), M->GetPlayLength(), Anim->GetSlotMontageGlobalWeight(FName(TEXT("DefaultSlot"))), int32(Anim->RootMotionMode.GetValue()));
			}
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
		if (FVisexBodyProfile::OutfitLocked(int32(&O - FVisexBodyProfile::Outfits().GetData()))) { continue; }   // not wearable: the wardrobe stop below shows them
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

void AVisexQATour::BuildLayers()
{
	// one run, every layer's share: the body walk out and back once per setting, at real speed
	auto Add = [this](float Wait, TFunction<void()> Run) { Steps.Add({Wait, MoveTemp(Run)}); };
	AVisexMenuDirector* D = Director;
	auto Editor = [this]() -> AVisexBodyEditor* { for (TActorIterator<AVisexBodyEditor> It(GetWorld()); It; ++It) { return *It; } return nullptr; };
	static const TCHAR* Vars[] = {TEXT("visex.Pin"), TEXT("visex.Catwalk"), TEXT("visex.Plant"), TEXT("visex.Toes"), TEXT("visex.Limits"), TEXT("visex.Reshape"), TEXT("visex.SteadyHead"), TEXT("visex.Hold")};
	struct FSetting { const TCHAR* Name; TArray<int32> Off; int32 Pin; const TCHAR* Extra = TEXT(""); };
	// pin 0 is the layers as they were; the layer settings are measured without the pin
	const TArray<FSetting> Settings = {
		{TEXT("all"), {}, 0}, {TEXT("pin1"), {}, 1}, {TEXT("pin2"), {}, 2}, {TEXT("nocatwalk"), {1}, 0},
		{TEXT("pin2strict"), {}, 2, TEXT("visex.Pin.Still=12;visex.Pin.Moving=40")}, {TEXT("pin2nocat"), {1}, 2}};
	if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("visex.Pin.Log"))) { V->Set(TEXT("1"), ECVF_SetByConsole); }
	Add(2.f, [this] { Feet.SetPhase(TEXT("standing")); });
	for (const FSetting& Set : Settings)
	{
		const FString Name = Set.Name;
		const TArray<int32> Off = Set.Off;
		const int32 Pin = Set.Pin;
		const FString Extra = Set.Extra;
		auto Apply = [Off, Pin, Name, Extra]
		{
			for (int32 I = 0; I < UE_ARRAY_COUNT(Vars); ++I)
			{
				if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Vars[I])) { V->Set(I == 0 ? *FString::FromInt(Pin) : Off.Contains(I) ? TEXT("0") : TEXT("1"), ECVF_SetByConsole); }
			}
			TArray<FString> Pairs;
			FString(TEXT("visex.Pin.Still=20;visex.Pin.Moving=60;") + Extra).ParseIntoArray(Pairs, TEXT(";"));
			for (const FString& Pair : Pairs)
			{
				FString K, Val;
				if (Pair.Split(TEXT("="), &K, &Val)) { if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(*K)) { V->Set(*Val, ECVF_SetByConsole); } }
			}
			QA_LOG(TEXT("VisexFeet: setting %s"), *Name);
			UE_LOG(LogTemp, Display, TEXT("VisexPin: setting %s"), *Name);
		};
		Add(0.5f, [this, Apply, Name, D] { Apply(); Feet.SetPhase(TEXT("out ") + Name); if (Name == TEXT("all") || Name == TEXT("pin2")) { Feet.ArmStance(TEXT("out_") + Name); } D->OpenBody(); });
		Add(4.5f, [] {});
		Add(0.5f, [this, Name, Editor] { Feet.SetPhase(TEXT("back ") + Name); if (Name == TEXT("all") || Name == TEXT("pin2")) { Feet.ArmStance(TEXT("back_") + Name); } if (AVisexBodyEditor* E = Editor()) { E->Close(); } });
		Add(5.f, [] {});
	}
	// the other moves with the pin, then the photo return once more without it
	auto Pin = [](const TCHAR* V) { if (IConsoleVariable* C = IConsoleManager::Get().FindConsoleVariable(TEXT("visex.Pin"))) { C->Set(V, ECVF_SetByConsole); } };
	Add(0.5f, [this, D, Pin] { Pin(TEXT("2")); Feet.SetPhase(TEXT("wardrobe out pin2")); D->OpenWardrobe(); });
	Add(4.5f, [] {});
	Add(0.5f, [this, Editor] { Feet.SetPhase(TEXT("wardrobe back pin2")); if (AVisexBodyEditor* E = Editor()) { E->Close(); } });
	Add(5.f, [] {});
	for (const TCHAR* P : {TEXT("2"), TEXT("0")})
	{
		const FString Tag = FString(TEXT(" pin")) + P;
		Add(0.5f, [this, D, Pin, P, Tag] { Pin(P); if (const ACharacter* C = Her()) { QA_LOG(TEXT("VisexFeet: before photo%s: she stands at %s yaw %.1f"), *Tag, *C->GetActorLocation().ToCompactString(), C->GetActorRotation().Yaw); } Feet.SetPhase(TEXT("photo enter") + Tag); D->OnItemChosen(AVisexMenuDirector::ItemPhoto); });
		Add(5.f, [] {});
		Add(0.5f, [this, Tag] { Feet.SetPhase(TEXT("photo back") + Tag); if (AVisexPhotoMode* Ph = Photo()) { Ph->Close(); } });
		Add(6.f, [this, Tag] { if (const ACharacter* C = Her()) { QA_LOG(TEXT("VisexFeet: photo back%s: she stands at %s yaw %.1f"), *Tag, *C->GetActorLocation().ToCompactString(), C->GetActorRotation().Yaw); } });
	}
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
		// a check may undress the MetaHuman body or try an outfit with no cut on it; the game may not
		if (IConsoleVariable* Any = IConsoleManager::Get().FindConsoleVariable(TEXT("visex.Body.MetaHuman.AnyOutfit"))) { Any->Set(1, ECVF_SetByConsole); }
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

void AVisexQATour::SpikeTotals()
{
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
		SpikeTotals();
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
	// -visexposes=lean+stand: only these passes (default: all three, and "stand" too on a subset run)
	FString Poses = bOnly ? TEXT("lean+leglift+walk+stand") : TEXT("lean+leglift+walk");
	FParse::Value(FCommandLine::Get(), TEXT("visexposes="), Poses, false);
	const auto Want = [Poses](const TCHAR* Name) { return (TEXT("+") + Poses + TEXT("+")).Contains(FString(TEXT("+")) + Name + TEXT("+")); };
	if (Want(TEXT("lean")))
	{
		Pass(TEXT("lean"));
	}
	Add(1.f, [D] { D->OnItemChosen(AVisexMenuDirector::ItemPhoto); });
	if (Want(TEXT("leglift")))
	{
		Add(5.f, [Motion] { Motion(TEXT("LegLift")); });
		Add(5.f, [] {});
		Pass(TEXT("leglift"));
	}
	if (Want(TEXT("walk")))
	{
		Add(Want(TEXT("leglift")) ? 0.5f : 5.f, [Motion] { Motion(TEXT("Walk")); });
		Add(4.f, [] {});
		Pass(TEXT("walk"));
	}
	if (Want(TEXT("stand")))
	{
		// standing still in the open, in the MetaHuman's reference pose (visex.Body.MetaHuman.TestPose
		// "ref"): the one pose where two setups can be laid side by side. The lean hides her seat, the
		// walk never shows two setups in the same step, and the photo Idle shifts her about.
		Add(Want(TEXT("leglift")) || Want(TEXT("walk")) ? 0.5f : 5.f, [Motion]
		{
			Motion(TEXT("Idle"));
			if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("visex.Body.MetaHuman.TestPose"))) { V->Set(TEXT("ref"), ECVF_SetByConsole); }
		});
		Add(4.f, [] {});
		Pass(TEXT("stand"));
		Add(0.3f, []
		{
			if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("visex.Body.MetaHuman.TestPose"))) { V->Set(TEXT("off"), ECVF_SetByConsole); }
		});
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

void AVisexQATour::BuildFlex()
{
	auto Add = [this](float Wait, TFunction<void()> Run) { Steps.Add({Wait, MoveTemp(Run)}); };
	AVisexMenuDirector* D = Director;
	ApplyCvarArg();
	const auto Set = [](const TCHAR* Var, const FString& Value)
	{
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Var)) { V->Set(*Value, ECVF_SetByConsole); }
	};
	// one pose: the flex layer off, shot; on, left to rise, shot again from the same camera
	const auto Pair = [this, Add, Set](const FString& Name)
	{
		const auto Shoot = [this](const FString& What)
		{
			FString Shots;
			if (FParse::Value(FCommandLine::Get(), TEXT("visexshots="), Shots)) { CloseShots(Shots, What); }
		};
		// her clips are held while the pair is shot (the photo Idle shifts her weight within a second):
		// the driver's pose stands still, the flex layer still eases in on real time
		const auto Hold = [this](float Rate) { if (ACharacter* C = Her()) { C->GetMesh()->GlobalAnimRateScale = Rate; } };
		Add(1.5f, [Set, Hold] { Hold(0.f); Set(TEXT("visex.Body.MetaHuman.Flex"), TEXT("0")); });
		Add(0.6f, [Shoot, Name, Set] { Shoot(TEXT("flex0_") + Name); Set(TEXT("visex.Body.MetaHuman.Flex"), TEXT("1")); });
		Add(1.6f, [Shoot, Name, Hold] { Shoot(TEXT("flex1_") + Name); Hold(1.f); });
	};
	const auto Motion = [this](const TCHAR* Name)
	{
		AVisexPhotoMode* P = Photo();
		for (int32 I = 0; P && P->IsOpen() && I < 40; ++I)
		{
			if (P->GetMotionName(I) == Name) { P->SetMotion(I); QA_LOG(TEXT("VisexFlex: photo motion %s"), Name); return; }
		}
		QA_LOG(TEXT("VisexFlex: NO photo motion %s"), Name);
	};
	// the clock holds through her walk in
	Add(4.f, [] {});
	Pair(TEXT("lean"));
	Add(1.f, [D] { D->OnItemChosen(AVisexMenuDirector::ItemPhoto); });
	Add(5.f, [Motion] { Motion(TEXT("Idle")); });
	Add(4.f, [] {});
	Pair(TEXT("idle"));
	// -visexflexposes=name=<TestPose>|name=<TestPose>: standing poses made of extra bone rotations
	FString All;
	TArray<FString> Poses;
	FParse::Value(FCommandLine::Get(), TEXT("visexflexposes="), All, false);
	All.ParseIntoArray(Poses, TEXT("|"));
	for (const FString& One : Poses)
	{
		FString Name, Pose;
		if (!One.Split(TEXT("="), &Name, &Pose)) { continue; }
		Add(0.3f, [Set, Name, Pose] { Set(TEXT("visex.Body.MetaHuman.TestPose"), Pose); QA_LOG(TEXT("VisexFlex: pose %s = %s"), *Name, *Pose); });
		Pair(Name);
	}
	Add(0.3f, [this, Set]
	{
		Set(TEXT("visex.Body.MetaHuman.TestPose"), TEXT("off"));
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
	// -visexclipcam=...,<frames>: the fifth number is the clip's length in frames
	FString Cam;
	TArray<FString> CamParts;
	if (FParse::Value(FCommandLine::Get(), TEXT("visexclipcam="), Cam, false) && Cam.ParseIntoArray(CamParts, TEXT(",")) >= 5)
	{
		ClipFrames = FMath::Clamp(FCString::Atoi(*CamParts[4]), 1, 900);
	}
	Add(4.f, [this] { ClipLeft = ClipFrames; ClipN = 0; });
	Add(ClipFrames / 30.f + 1.f, [this]
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
		ClipCapture->FOVAngle = 28.f;   // across; 16 degrees up and down: 70 cm of her at this distance, bust to seat
		ClipCapture->RegisterComponent();
		// her torso almost in profile, from her right (the side the photo spot's light rims): the bust and
		// the seat read against the dunes. The direction and the height are kept for the whole clip
		ClipOffset = C->GetActorForwardVector().RotateAngleAxis(72.f, FVector::UpVector) * 250.f + FVector(0.f, 0.f, 4.f);
		ClipZ = C->GetMesh()->GetComponentLocation().Z + 106.f - C->GetActorLocation().Z;
		const auto Numbers = [](const TCHAR* Key, TArray<float>& Out)
		{
			FString Text;
			TArray<FString> Parts;
			if (!FParse::Value(FCommandLine::Get(), Key, Text, false)) { return false; }
			Text.ParseIntoArray(Parts, TEXT(","));
			for (const FString& Part : Parts) { Out.Add(FCString::Atof(*Part)); }
			return Out.Num() >= 4;
		};
		TArray<float> Cam, Light;
		if (Numbers(TEXT("visexclipcam="), Cam))
		{
			ClipOffset = C->GetActorForwardVector().RotateAngleAxis(Cam[0], FVector::UpVector) * Cam[1];
			ClipZ = C->GetMesh()->GetComponentLocation().Z + Cam[2] - C->GetActorLocation().Z;
			ClipCapture->FOVAngle = Cam[3];
		}
		if (Numbers(TEXT("visexcliplight="), Light))
		{
			ClipLight = NewObject<USpotLightComponent>(this, NAME_None, RF_Transient);
			ClipLight->SetMobility(EComponentMobility::Movable);
			ClipLight->SetIntensityUnits(ELightUnits::Candelas);
			ClipLight->SetIntensity(Light[3]);
			ClipLight->SetLightColor(FLinearColor(1.f, 0.86f, 0.68f));   // the dust plaza's low sun, not a studio lamp
			ClipLight->SetAttenuationRadius(1200.f);
			ClipLight->SetInnerConeAngle(18.f);
			ClipLight->SetOuterConeAngle(30.f);
			ClipLight->SetCastShadows(false);
			ClipLight->RegisterComponent();
			ClipLightOffset = C->GetActorForwardVector().RotateAngleAxis(Light[0], FVector::UpVector) * Light[1]
				+ FVector(0.f, 0.f, C->GetMesh()->GetComponentLocation().Z + Light[2] - C->GetActorLocation().Z - ClipZ);
		}
	}
	// the camera travels with her capsule, up and down the dunes too, so what moves in the picture is
	// her own body (the photo Walk itself lifts and drops her torso about 9 cm a step)
	const FVector Target = C->GetActorLocation() + FVector(0.f, 0.f, ClipZ);
	ClipCapture->SetWorldLocationAndRotation(Target + ClipOffset, (-ClipOffset).Rotation());
	if (ClipLight)
	{
		ClipLight->SetWorldLocationAndRotation(Target + ClipLightOffset, (-ClipLightOffset).Rotation());
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

static FString GPinPhase = TEXT("held");

void AVisexQATour::PinPhase(const FString& Name)
{
	float S[4];
	VisexPinStats(S, true);
	QA_LOG(TEXT("VisexPin: STATS %s: hold max %.2f cm, one-frame change max %.2f cm, straightest leg %.3f, clamped %d frames"), *GPinPhase, S[0], S[1], S[2], int32(S[3]));
	GPinPhase = Name;
	Feet.SetPhase(Name);
}

void AVisexQATour::BuildPin()
{
	// the pin where the layers tour never took it: each move once per mode, pin 2 as it is, pin 2
	// gated (off in the air and turning on the spot, eased release), and no pin
	auto Add = [this](float Wait, TFunction<void()> Run) { Steps.Add({Wait, MoveTemp(Run)}); };
	AVisexMenuDirector* D = Director;
	auto Editor = [this]() -> AVisexBodyEditor* { for (TActorIterator<AVisexBodyEditor> It(GetWorld()); It; ++It) { return *It; } return nullptr; };
	auto Mode = [](const FString& M)
	{
		auto Set = [](const TCHAR* N, const TCHAR* V) { if (IConsoleVariable* C = IConsoleManager::Get().FindConsoleVariable(N)) { C->Set(V, ECVF_SetByConsole); } };
		Set(TEXT("visex.Pin"), M == TEXT("pin0") ? TEXT("0") : TEXT("2"));
		Set(TEXT("visex.Pin.Gate"), M == TEXT("gate") ? TEXT("1") : TEXT("0"));
	};
	auto Shot4K = [this](const FString& Name)
	{
		const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("QA") / FString::Printf(TEXT("pin_%s.png"), *Name));
		// through the player's console, which reaches the viewport (GEngine->Exec wrote nothing)
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0)) { PC->ConsoleCommand(FString::Printf(TEXT("HighResShot 3840x2160 filename=\"%s\""), *File)); }
		QA_LOG(TEXT("VisexQA: shot %s"), *File);
	};
	FString Only;
	FParse::Value(FCommandLine::Get(), TEXT("visexpinmodes="), Only);
	TArray<FString> Modes = {TEXT("pin2"), TEXT("gate"), TEXT("pin0")};
	if (!Only.IsEmpty()) { Only.ParseIntoArray(Modes, TEXT(",")); }
	const FString Show = Modes.Contains(TEXT("gate")) ? TEXT("gate") : Modes[0];   // the mode the sheet shows
	Add(2.f, [] {});
	for (const FString& M : Modes)
	{
		Add(0.5f, [this, Mode, M] { Mode(M); PinPhase(TEXT("lean ") + M); });
		Add(9.f, [] {});
	}
	// Settings: the jump onto the drum, the sit with a leg hanging, the way back down
	for (const FString& M : Modes)
	{
		Add(0.5f, [this, D, Mode, M] { Mode(M); PinPhase(TEXT("sitdown ") + M); D->OpenSettings(); });
		Add(3.5f, [this, M] { PinPhase(TEXT("sit ") + M); });
		Add(3.f, [this, M, Show, Shot4K] { if (M == Show) { Shot4K(TEXT("sit")); } });
		Add(2.f, [this, D, M] { PinPhase(TEXT("standup ") + M); D->CloseSettings(); });
		Add(7.f, [] {});
	}
	// the Body page: walk out, a fast turn and a slow one, stillness, sliders and rows, walk back
	for (const FString& M : Modes)
	{
		Add(0.5f, [this, D, Mode, M] { Mode(M); PinPhase(TEXT("body out ") + M); D->OpenBody(); });
		Add(5.f, [this, Editor, M] { PinPhase(TEXT("body turn fast ") + M); if (AVisexBodyEditor* E = Editor()) { E->TurnSpeed = 150.f; } });
		Add(1.2f, [this, M, Show, Shot4K] { if (M == Show) { Shot4K(TEXT("body_turn")); } });
		Add(1.2f, [this, Editor, M] { PinPhase(TEXT("body turn slow ") + M); if (AVisexBodyEditor* E = Editor()) { E->TurnSpeed = -40.f; } });
		Add(4.f, [this, Editor, M] { PinPhase(TEXT("body still ") + M); if (AVisexBodyEditor* E = Editor()) { E->TurnSpeed = 0.f; } });
		Add(3.f, [this, Editor, M]
		{
			PinPhase(TEXT("body sliders ") + M);
			if (AVisexBodyEditor* E = Editor()) { E->SelectRow(1); E->Adjust(8.f); }
		});
		Add(1.5f, [Editor] { if (AVisexBodyEditor* E = Editor()) { E->Adjust(-8.f); E->SelectRow(4); E->Adjust(8.f); } });
		Add(1.5f, [Editor] { if (AVisexBodyEditor* E = Editor()) { E->Adjust(-8.f); E->SelectRow(8); E->Adjust(6.f); } });
		Add(2.f, [Editor] { if (AVisexBodyEditor* E = Editor()) { E->Adjust(-6.f); E->SelectRow(0); } });
		Add(2.f, [this, Editor, M] { PinPhase(TEXT("body back ") + M); if (AVisexBodyEditor* E = Editor()) { E->Close(); } });
		Add(7.f, [] {});
	}
	// photo mode: the slow walk in place, the run, then poses by name (a raised or sliding foot)
	auto Motion = [this](const FString& Name)
	{
		AVisexPhotoMode* P = Photo();
		if (!P || !P->IsOpen()) { QA_LOG(TEXT("VisexPin: photo not open for %s"), *Name); return; }
		for (int32 I = 0; I < 60 && !P->GetMotionName(I).IsEmpty(); ++I)
		{
			if (P->GetMotionName(I) == Name) { P->SetMotion(I); return; }
		}
		QA_LOG(TEXT("VisexPin: no motion %s"), *Name);
	};
	for (const FString& M : Modes)
	{
		Add(0.5f, [this, D, Mode, M] { Mode(M); PinPhase(TEXT("photo walk ") + M); D->OnItemChosen(AVisexMenuDirector::ItemPhoto); });
		Add(7.f, [this, Motion, M] { PinPhase(TEXT("photo run ") + M); Motion(TEXT("Run")); });
		for (const TCHAR* Pose : {TEXT("Ledge"), TEXT("Crown"), TEXT("Mermaid"), TEXT("LegLift"), TEXT("FeetUp")})
		{
			const FString Name = Pose;
			Add(3.f, [this, Motion, M, Name] { PinPhase(TEXT("photo ") + Name + TEXT(" ") + M); Motion(Name); });
			Add(4.5f, [this, M, Name, Show, Shot4K] { if (M == Show && Name == TEXT("Mermaid")) { Shot4K(TEXT("photo_mermaid")); } });
		}
		Add(1.f, [this, M] { PinPhase(TEXT("photo back ") + M); if (AVisexPhotoMode* P = Photo()) { P->Close(); } });
		Add(8.f, [] {});
	}
	// Start, then third-person play on a scripted stick: walk, jog, run, stop, reversals, a zigzag,
	// a turn on the spot, a jump in place and one out of a run
	Add(0.5f, [this, D] { PinPhase(TEXT("start")); D->OnItemChosen(AVisexMenuDirector::ItemStart); });
	Add(14.f, [] {});
	auto Go = [this](float X, float Y) { Drive = FVector2D(X, Y); };
	for (const FString& M : Modes)
	{
		Add(0.5f, [this, Mode, M, Go] { Mode(M); PinPhase(TEXT("tp walk ") + M); Go(0.3f, 0.f); });
		Add(4.f, [this, M, Go] { PinPhase(TEXT("tp jog ") + M); Go(-0.6f, 0.f); });
		Add(3.f, [this, M, Go] { PinPhase(TEXT("tp run ") + M); Go(1.f, 0.f); });
		Add(3.f, [this, M, Go] { PinPhase(TEXT("tp stop ") + M); Go(0.f, 0.f); });
		Add(2.5f, [this, M, Go] { PinPhase(TEXT("tp reverse ") + M); Go(1.f, 0.f); });
		Add(1.5f, [Go] { Go(-1.f, 0.f); });
		Add(1.5f, [Go] { Go(0.f, 0.f); });
		Add(1.5f, [this, M, Go] { PinPhase(TEXT("tp zigzag ") + M); Go(0.5f, 0.5f); });
		for (int32 K = 0; K < 4; ++K) { Add(0.6f, [Go, K] { Go(0.5f, K & 1 ? 0.5f : -0.5f); }); }
		Add(0.6f, [Go] { Go(0.f, 0.f); });
		Add(2.f, [this, M] { PinPhase(TEXT("tp spin ") + M); Spin = 90.f; });
		Add(2.f, [this] { Spin = -200.f; });
		Add(1.f, [this] { Spin = 0.f; });
		Add(1.5f, [this, M] { PinPhase(TEXT("tp jump ") + M); if (ACharacter* C = Her()) { C->Jump(); } });
		Add(0.3f, [this] { if (ACharacter* C = Her()) { C->StopJumping(); } });
		Add(1.7f, [this, M, Go] { PinPhase(TEXT("tp runjump ") + M); Go(-1.f, 0.f); });
		Add(1.5f, [this] { if (ACharacter* C = Her()) { C->Jump(); } });
		Add(0.3f, [this] { if (ACharacter* C = Her()) { C->StopJumping(); } });
		Add(0.5f, [this, M, Show, Shot4K] { if (M == Show) { Shot4K(TEXT("landing")); } });
		Add(0.7f, [Go] { Go(0.f, 0.f); });
		Add(2.5f, [] {});
	}
	Add(0.5f, [this]
	{
		PinPhase(TEXT("done"));
		QA_LOG(TEXT("VisexFeet: %d frames in %.1f s, %.1f fps"), FeetFrames, FPlatformTime::Seconds() - FeetT0, FeetFrames / FMath::Max(0.1, FPlatformTime::Seconds() - FeetT0));
		QA_LOG(TEXT("VisexQA: done"));
		UKismetSystemLibrary::QuitGame(this, UGameplayStatics::GetPlayerController(this, 0), EQuitPreference::Quit, false);
	});
}

void AVisexQATour::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Director) { return; }
	if (!bBuilt)
	{
		const bool bGaps = FParse::Param(FCommandLine::Get(), TEXT("visexgaps"));
		bFilm = FParse::Param(FCommandLine::Get(), TEXT("visexfilm"));
		const bool bLayers = FParse::Param(FCommandLine::Get(), TEXT("visexlayers"));
		const bool bPin = FParse::Param(FCommandLine::Get(), TEXT("visexpin"));
		bJitter = FParse::Param(FCommandLine::Get(), TEXT("visexjitter")) || FParse::Param(FCommandLine::Get(), TEXT("visexjitterscan"));
		bFeet = bGaps || bFilm || bLayers || bPin || bJitter || FParse::Param(FCommandLine::Get(), TEXT("visexfeet"));
		FeetClock = 2e5f;   // until she first moves
		Feet.Log = [](const FString& Line) { QA_LOG(TEXT("%s"), *Line); };
		bSpikes = FParse::Param(FCommandLine::Get(), TEXT("visexspikes"));
		bSliders = FParse::Param(FCommandLine::Get(), TEXT("visexsliders"));
		FString ShipPass;
		bShip = FParse::Param(FCommandLine::Get(), TEXT("visexship")) || FParse::Value(FCommandLine::Get(), TEXT("visexship="), ShipPass);
		bSpikes |= bShip;
		Jitter.Log = [](const FString& Line) { QA_LOG(TEXT("%s"), *Line); };
		FParse::Param(FCommandLine::Get(), TEXT("visexclip")) ? BuildClip() :
		FParse::Param(FCommandLine::Get(), TEXT("visexflex")) ? BuildFlex() :
		FParse::Param(FCommandLine::Get(), TEXT("visexside")) || FCString::Strifind(FCommandLine::Get(), TEXT("-visexside=")) ? BuildSide() :
		bShip ? BuildShip() : bSliders ? BuildSliders() : bSpikes ? BuildSpikes() : bJitter ? BuildJitter() : bPin ? BuildPin() :
		bGaps ? BuildGaps() : bLayers ? BuildLayers() : bFeet ? BuildFeet() : Build();
		bBuilt = true;
	}
	if (ClipLeft > 0)
	{
		ClipFrame();
	}
	if (SideFilmLeft > 0)
	{
		FString Shots;
		FParse::Value(FCommandLine::Get(), TEXT("visexshots="), Shots);
		if ((SideFilmLeft-- & 1) == 0)
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("QA/%s_film/f%05d.png"), *Shots, SideFilmN++), false, false);
		}
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
	if (bShip) { ToeTick(); }
	if (bSpikes || bSliders)
	{
		Spikes.Tick(Her());
	}
	if (bSliders && !bFeet)
	{
		Feet.Tick(Her(), DeltaSeconds);
	}
	if (bJitter)
	{
		Jitter.Tick();
		JitterSpotClock += DeltaSeconds;
		if (bFilming && JitterSpotClock >= FilmFrom && JitterSpotClock < FilmFrom + FilmSecs) { Jitter.Film(GetWorld(), Her(), FilmName); }
		// an uneven frame time: every seventh frame 30 ms late, every third 8 ms
		if (bHitch) { FPlatformProcess::Sleep(FeetFrames % 7 == 0 ? 0.030f : FeetFrames % 3 == 0 ? 0.008f : 0.f); }
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
			const FRotator Yaw(0.f, WalkYaw < 1e8f ? WalkYaw : UGameplayStatics::GetPlayerCameraManager(this, 0)->GetCameraRotation().Yaw, 0.f);
			C->AddMovementInput(Yaw.Vector(), 1.f);
		}
	}
	if (ACharacter* C = Her(); C && (!Drive.IsZero() || Spin != 0.f))
	{
		const FRotator Yaw(0.f, UGameplayStatics::GetPlayerCameraManager(this, 0)->GetCameraRotation().Yaw, 0.f);
		if (!Drive.IsZero()) { C->AddMovementInput(Yaw.RotateVector(FVector(Drive.X, Drive.Y, 0.f)), 1.f); }
		else { C->SetActorRotation(C->GetActorRotation() + FRotator(0.f, Spin * DeltaSeconds, 0.f)); }
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

bool AVisexQATour::JitterPlace(const FVector& At, float Yaw)
{
	// her capsule set down on the walking ground at the spot (a sweep from above the hint)
	ACharacter* C = Her();
	const UCapsuleComponent* Cap = C ? C->GetCapsuleComponent() : nullptr;
	if (!Cap) { return false; }
	FHitResult Hit;
	FCollisionQueryParams Q(TEXT("VisexJitterPlace"), false, C);
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Cap->GetScaledCapsuleRadius(), Cap->GetScaledCapsuleHalfHeight());
	const FVector Top(At.X, At.Y, At.Z + 300.f), Bottom(At.X, At.Y, At.Z - 400.f);
	if (!GetWorld()->SweepSingleByChannel(Hit, Top, Bottom, FQuat::Identity, ECC_Pawn, Shape, Q) || Hit.bStartPenetrating)
	{
		QA_LOG(TEXT("VisexJitter: no ground for her at %s"), *At.ToCompactString());
		return false;
	}
	C->SetActorLocationAndRotation(Hit.Location + FVector(0.f, 0.f, 1.f), FRotator(0.f, Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	if (UCharacterMovementComponent* Move = C->GetCharacterMovement()) { Move->StopMovementImmediately(); }
	QA_LOG(TEXT("VisexJitter: placed at %s yaw %.0f on %s"), *Hit.Location.ToCompactString(), Yaw, Hit.GetActor() ? *Hit.GetActor()->GetActorNameOrLabel() : TEXT("none"));
	return true;
}

void AVisexQATour::JitterScan()
{
	// the ground around her on a grid, as her feet see it (FootTrace): height, the heights 12 cm
	// either side, the normal and what it is; Saved/QA/jitter_scan.csv, for choosing the spots
	ACharacter* C = Her();
	if (!C) { return; }
	FVector Centre = C->GetActorLocation();
	float Half = 1500.f, Step = 50.f;
	FString Arg;
	TArray<FString> P;
	if (FParse::Value(FCommandLine::Get(), TEXT("visexjitterscanat="), Arg) && Arg.ParseIntoArray(P, TEXT(":")) >= 4)
	{
		Centre.X = FCString::Atof(*P[0]); Centre.Y = FCString::Atof(*P[1]); Half = FCString::Atof(*P[2]); Step = FMath::Max(5.f, FCString::Atof(*P[3]));
	}
	FCollisionQueryParams Q(TEXT("VisexJitterScan"), true, C);
	auto Ground = [this, &Q, &Centre](float X, float Y, FHitResult& Hit)
	{
		return GetWorld()->LineTraceSingleByChannel(Hit, FVector(X, Y, Centre.Z + 500.f), FVector(X, Y, Centre.Z - 1200.f), VisexFootTraceChannel, Q);
	};
	TArray<FString> Csv;
	Csv.Add(TEXT("x,y,z,zxp,zxm,zyp,zym,nx,ny,nz,actor"));
	for (float X = Centre.X - Half; X <= Centre.X + Half; X += Step)
	{
		for (float Y = Centre.Y - Half; Y <= Centre.Y + Half; Y += Step)
		{
			FHitResult H, N;
			if (!Ground(X, Y, H)) { continue; }
			float Z[4];
			const float DX[4] = {12.f, -12.f, 0.f, 0.f}, DY[4] = {0.f, 0.f, 12.f, -12.f};
			for (int32 I = 0; I < 4; ++I) { Z[I] = Ground(X + DX[I], Y + DY[I], N) ? float(N.ImpactPoint.Z) : -99999.f; }
			Csv.Add(FString::Printf(TEXT("%.0f,%.0f,%.2f,%.2f,%.2f,%.2f,%.2f,%.3f,%.3f,%.3f,%s"), X, Y, H.ImpactPoint.Z, Z[0], Z[1], Z[2], Z[3],
				H.ImpactNormal.X, H.ImpactNormal.Y, H.ImpactNormal.Z, H.GetActor() ? *H.GetActor()->GetActorNameOrLabel() : TEXT("none")));
		}
	}
	const FString File = FPaths::ProjectSavedDir() / TEXT("QA/jitter_scan.csv");
	FFileHelper::SaveStringToFile(FString::Join(Csv, TEXT("\n")), *File);
	QA_LOG(TEXT("VisexJitter: scan of %d points around %s: %s"), Csv.Num() - 1, *Centre.ToCompactString(), *File);
}

namespace
{
	// The limits, from the flat spot (see Docs/Research/idle_jitter/NOTES.md): the worst frame any
	// leg track sits off its neighbours' line (mm), and the plant/pin flips per weight shift. A
	// sound flat run reads 1.3 to 3.7 mm (feet and pelvis under 1.6); the locked-knee bug read 55
	// on the flat and up to 230 on a slope
	float GJitterLimitMm = 5.0f;
	float GJitterLimitFlips = 2.f;
	float GJitterLimitSquash = 1.f;   // percent a thigh or calf may change length (the reshape layer breathes a little)
	// name:x:y:z:yaw, in Lvl_Dust; z is a hint, her capsule is swept onto the ground there
	// where the Start walk-off leaves her (a 3 degree slope), a flat step tread, dune slopes of 13
	// and 25 degrees across her stance and one facing up it, and astride a 20 cm step edge
	const TCHAR* GJitterSpots = TEXT("start;flat:9425:-2267:10:0;dune13x:8025:-1317:225:90;dune25x:9225:-1567:100:0;dune25up:9225:-1567:100:90;stepedge:9425:-2160:20:0");
}

/** The Anim Blueprint's foot IK nodes as they run (Foot Placement is tuned from code), for the notes. */
static void JitterDumpIK(const ACharacter* C)
{
	UAnimInstance* Anim = C && C->GetMesh() ? C->GetMesh()->GetAnimInstance() : nullptr;
	IAnimClassInterface* Class = Anim ? IAnimClassInterface::GetFromClass(Anim->GetClass()) : nullptr;
	if (!Class) { return; }
	for (const FStructProperty* Node : Class->GetAnimNodeProperties())
	{
		const FName Type = Node->Struct->GetFName();
		if (Type == TEXT("AnimNode_LegIK"))
		{
			const FAnimNode_LegIK& N = *Node->ContainerPtrToValuePtr<FAnimNode_LegIK>(Anim);
			QA_LOG(TEXT("VisexJitter: LegIK alpha %.2f, reach precision %.3f, max iterations %d, soft %.2f / %.2f, %d legs"), N.Alpha, N.ReachPrecision, N.MaxIterations, N.SoftPercentLength, N.SoftAlpha, N.LegsDefinition.Num());
			for (const FAnimLegIKDefinition& L : N.LegsDefinition)
			{
				QA_LOG(TEXT("VisexJitter: LegIK leg %s <- %s: %d bones, min angle %.1f, forward axis %d, hinge axis %d, rotation limit %d, knee twist correction %d"), *L.FKFootBone.BoneName.ToString(), *L.IKFootBone.BoneName.ToString(),
					L.NumBonesInLimb, L.MinRotationAngle, int32(L.FootBoneForwardAxis.GetValue()), int32(L.HingeRotationAxis.GetValue()), L.bEnableRotationLimit, L.bEnableKneeTwistCorrection);
			}
		}
		else if (Type == TEXT("AnimNode_FootPlacement"))
		{
			const FAnimNode_FootPlacement& N = *Node->ContainerPtrToValuePtr<FAnimNode_FootPlacement>(Anim);
			QA_LOG(TEXT("VisexJitter: FootPlacement alpha %.2f, plant speed %.0f, unalign %.0f, dist %.1f, lock %d, pelvis max %.0f stiffness %.0f, foot stiffness %.0f, sweep %.1f"), N.Alpha, N.PlantSettings.SpeedThreshold,
				N.PlantSettings.UnalignmentSpeedThreshold, N.PlantSettings.DistanceToGround, int32(N.PlantSettings.LockType), N.PelvisSettings.MaxOffset, N.PelvisSettings.LinearStiffness,
				N.InterpolationSettings.FloorLinearStiffness, N.TraceSettings.SweepRadius);
		}
		else if (Type.ToString().Contains(TEXT("IK")) || Type == TEXT("AnimNode_CopyBone") || Type.ToString().Contains(TEXT("Warp")))
		{
			QA_LOG(TEXT("VisexJitter: graph node %s"), *Type.ToString());
		}
	}
}

void AVisexQATour::BuildJitter()
{
	auto Add = [this](float Wait, TFunction<void()> Run) { Steps.Add({Wait, MoveTemp(Run)}); };
	AVisexMenuDirector* D = Director;
	FString Cvars;
	if (FParse::Value(FCommandLine::Get(), TEXT("visexcvars="), Cvars, false))
	{
		TArray<FString> Each;
		Cvars.ParseIntoArray(Each, TEXT(","));
		for (const FString& E : Each)
		{
			FString Var, Value;
			if (!E.Split(TEXT("="), &Var, &Value)) { continue; }
			IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(*Var);
			if (V) { V->Set(*Value, ECVF_SetByConsole); }
			QA_LOG(TEXT("VisexJitter: setting %s, reads %s"), *E, V ? *V->GetString() : TEXT("NO SUCH CVAR"));
		}
	}
	bHitch = FParse::Param(FCommandLine::Get(), TEXT("visexjitterhitch"));
	FParse::Value(FCommandLine::Get(), TEXT("visexjitterfilm="), FilmSpot);
	FParse::Value(FCommandLine::Get(), TEXT("visexjitterfilmfrom="), FilmFrom);
	FilmSecs = 20.f;
	FParse::Value(FCommandLine::Get(), TEXT("visexjitterfilmsecs="), FilmSecs);
	// an uneven frame time reads a little more for the same motion (flat spot: 4.4 mm against 1.9 to 3.8)
	if (bHitch) { GJitterLimitMm = 8.f; }
	FParse::Value(FCommandLine::Get(), TEXT("visexjitterlimit="), GJitterLimitMm);
	float Secs = 40.f, Settle = 4.f;
	FParse::Value(FCommandLine::Get(), TEXT("visexjittersecs="), Secs);
	FString Spots = GJitterSpots;
	FParse::Value(FCommandLine::Get(), TEXT("visexjitterspots="), Spots, false);
	QA_LOG(TEXT("VisexJitter: %.0f s per spot, hitch %d, film '%s' from %.1f s for %.1f s, limit %.2f mm, spots %s"), Secs, bHitch, *FilmSpot, FilmFrom, FilmSecs, GJitterLimitMm, *Spots);
	// Start: she stands up, walks off and stops on the open sand
	Add(4.f, [this, D] { Feet.SetPhase(TEXT("start")); D->OnItemChosen(AVisexMenuDirector::ItemStart); });
	Add(14.f, [] {});
	Add(0.2f, [this] { JitterDumpIK(Her()); });
	if (FParse::Param(FCommandLine::Get(), TEXT("visexjitterscan")))
	{
		Add(0.5f, [this] { JitterScan(); });
	}
	else
	{
		TArray<FString> Each;
		Spots.ParseIntoArray(Each, TEXT(";"));
		if (Each.Num() == 0) { Each.Add(TEXT("here")); }
		for (const FString& E : Each)
		{
			TArray<FString> P;
			E.ParseIntoArray(P, TEXT(":"));
			const FString Name = P[0];
			Add(0.5f, [this, P, Name]
			{
				if (P.Num() >= 5) { JitterPlace(FVector(FCString::Atof(*P[1]), FCString::Atof(*P[2]), FCString::Atof(*P[3])), FCString::Atof(*P[4])); }
				Feet.SetPhase(TEXT("settle ") + Name);
			});
			Add(Settle, [this, Name]
			{
				Feet.SetPhase(Name);
				Jitter.Begin(Her(), Name);
				JitterSpotClock = 0.f;
				bFilming = Name == FilmSpot;
				FilmName = Name;
				if (const ACharacter* C = Her()) { QA_LOG(TEXT("VisexJitter: %s: she stands at %s yaw %.0f"), *Name, *C->GetActorLocation().ToCompactString(), C->GetActorRotation().Yaw); }
			});
			Add(Secs, [this] { Jitter.End(); bFilming = false; });
		}
	}
	Add(0.5f, [this]
	{
		Feet.SetPhase(TEXT("done"));
		// the table, and the verdict against the flat-ground limits
		bool bFail = false;
		QA_LOG(TEXT("VisexJitter: TABLE spot | fps | shifts | worst frame mm (track) | rms mm | reversals/s | foot / knee / pelvis worst mm | flips | bone length change %% | verdict"));
		for (const FVisexJitterMeter::FResult& R : Jitter.Results())
		{
			const bool bBad = R.Max > GJitterLimitMm || R.PlantFlips > GJitterLimitFlips * FMath::Max(1, R.Shifts) || R.Squash > GJitterLimitSquash;
			bFail |= bBad;
			QA_LOG(TEXT("VisexJitter: TABLE %s | %.0f | %d | %.2f (%s) | %.3f | %.1f | %.2f / %.2f / %.2f | %d | %.1f | %s"), *R.Spot, R.Fps, R.Shifts, R.Max, *R.WorstTrack, R.Rms, R.Flips,
				R.FootMax, R.KneeMax, R.PelvisMax, R.PlantFlips, R.Squash, bBad ? TEXT("JITTER") : TEXT("ok"));
			if (R.MHKneeMax >= 0.f)
			{
				// the body on screen follows the driver: its own bones must read as steady
				const bool bMHBad = FMath::Max3(R.MHFootMax, R.MHKneeMax, R.MHPelvisMax) > GJitterLimitMm || R.MHSquash > GJitterLimitSquash;
				bFail |= bMHBad;
				QA_LOG(TEXT("VisexJitter: TABLE %s MetaHuman | foot / knee / pelvis worst mm %.2f / %.2f / %.2f | bone length change %.1f | %s"), *R.Spot,
					R.MHFootMax, R.MHKneeMax, R.MHPelvisMax, R.MHSquash, bMHBad ? TEXT("JITTER") : TEXT("ok"));
			}
		}
		QA_LOG(TEXT("VisexJitter: %s (limit %.2f mm off-line in any frame, %.0f plant/pin flips per weight shift); %d frames filmed"), bFail ? TEXT("FAIL") : TEXT("PASS"), GJitterLimitMm, GJitterLimitFlips, Jitter.FilmFrames);
		QA_LOG(TEXT("VisexQA: done"));
		// the Mac platform layer drops RequestExitWithStatus's code, so a failed run leaves at once
		// with its own (the report is already on disk)
		if (bFail) { if (GLog) { GLog->Flush(); } _exit(3); }
		else { UKismetSystemLibrary::QuitGame(this, UGameplayStatics::GetPlayerController(this, 0), EQuitPreference::Quit, false); }
	});
}

void AVisexQATour::ToePhase(const FString& Name)
{
	if (bToeRef)
	{
		QA_LOG(TEXT("VisexShip: toes, %s: big toe turned against the foot by at most, driver L %.1f R %.1f deg, MetaHuman L %.1f R %.1f deg"),
			*ToeName, ToeMax[0][0], ToeMax[0][1], ToeMax[1][0], ToeMax[1][1]);
	}
	ToeName = Name;
	bToeRef = false;
	for (int32 B = 0; B < 2; ++B) { for (int32 S = 0; S < 2; ++S) { ToeMax[B][S] = 0.f; ToeHave[B][S] = false; } }
}

void AVisexQATour::ToeTick()
{
	const ACharacter* C = Her();
	USkeletalMeshComponent* Meshes[2] = {C ? C->GetMesh() : nullptr, UVisexMHBodyComponent::ShownBody(C)};
	static const FName Foot[2] = {TEXT("foot_l"), TEXT("foot_r")}, Toe[2] = {TEXT("bigtoe_01_l"), TEXT("bigtoe_01_r")};
	if (!Meshes[0]) { return; }
	for (int32 B = 0; B < 2; ++B)
	{
		for (int32 S = 0; S < 2; ++S)
		{
			if (!Meshes[B] || Meshes[B]->GetBoneIndex(Toe[S]) == INDEX_NONE) { continue; }
			const FQuat Rel = Meshes[B]->GetBoneQuaternion(Foot[S]).Inverse() * Meshes[B]->GetBoneQuaternion(Toe[S]);
			if (!ToeHave[B][S]) { ToeRef[B][S] = Rel; ToeHave[B][S] = true; continue; }
			FQuat Turn = ToeRef[B][S].Inverse() * Rel;
			Turn.Normalize();
			ToeMax[B][S] = FMath::Max(ToeMax[B][S], FMath::RadiansToDegrees(2.f * FMath::Acos(FMath::Min(1.f, FMath::Abs(float(Turn.W))))));
		}
	}
	bToeRef = true;
}

void AVisexQATour::BuildShip()
{
	auto Add = [this](float Wait, TFunction<void()> Run) { Steps.Add({Wait, MoveTemp(Run)}); };
	AVisexMenuDirector* D = Director;
	ApplyCvarArg();
	FString Pass;
	FParse::Value(FCommandLine::Get(), TEXT("visexship="), Pass);
	Spikes.Log = [](const FString& Line) { QA_LOG(TEXT("%s"), *Line); };
	auto Phase = [this](const FString& Name) { Spikes.SetPhase(Name); ToePhase(Name); };
	auto Editor = [this]() -> AVisexBodyEditor* { for (TActorIterator<AVisexBodyEditor> It(GetWorld()); It; ++It) { return *It; } return nullptr; };
	// what she is and wears, the values the first pass applies, and the settings as the pages show them
	auto State = [this](const FString& Where)
	{
		const ACharacter* C = Her();
		const UVisexBodyComponent* B = C ? C->FindComponentByClass<UVisexBodyComponent>() : nullptr;
		const USkeletalMeshComponent* G = UVisexMHBodyComponent::ShownGarment(C);
		auto Join = [](const TArray<FText>& Rows) { FString Out; for (const FText& T : Rows) { Out += T.ToString() + TEXT(" | "); } return Out; };
		QA_LOG(TEXT("VisexShip: %s: MetaHuman body %s, profile outfit %s, garment %s; Legs %.4f Skin %.4f Bust %.3f Height %.1f; settings %s visuals %s"), *Where,
			UVisexMHBodyComponent::ShownBody(C) ? TEXT("shown") : TEXT("NOT SHOWN"), B ? *B->Profile.Outfit.ToString() : TEXT("?"),
			G && G->GetSkeletalMeshAsset() ? *G->GetSkeletalMeshAsset()->GetName() : TEXT("NONE"),
			B ? B->GetValue(TEXT("Legs")) : 0.f, B ? B->GetValue(TEXT("Skin")) : 0.f, B ? B->GetValue(TEXT("Bust")) : 0.f, B ? B->GetValue(TEXT("Height")) : 0.f,
			*Join(FVisexSettings::Values()), *Join(FVisexSettings::VisualValues()));
	};
	auto Quit = [this, D] { QA_LOG(TEXT("VisexQA: done")); D->OnItemChosen(AVisexMenuDirector::ItemQuit); };
	Phase(TEXT("arrival"));
	if (Pass == TEXT("2"))
	{
		Add(7.f, [this, State] { State(TEXT("relaunch")); Shot(TEXT("ship_relaunch")); });
		Add(1.f, [this, Phase, Quit] { Phase(TEXT("done")); SpikeTotals(); Quit(); });
		return;
	}
	Add(7.f, [this, Phase, State] { Phase(TEXT("lean")); State(TEXT("lean")); Shot(TEXT("ship_lean")); });
	static const TCHAR* Beats[3] = {TEXT("beat glance"), TEXT("beat shift"), TEXT("beat toes")};
	for (int32 I = 0; I < 3; ++I)
	{
		Add(I == 0 ? 3.f : 6.f, [Phase, D, I] { Phase(Beats[I]); D->PlayIdleBeat(I); });
		Add(2.5f, [this, I] { Shot(FString::Printf(TEXT("ship_beat_%d"), I)); });
	}
	// the Body page: each slider dragged the way the stick does it (the editor's Adjust, a step every 50 ms)
	Add(6.f, [Phase, D] { Phase(TEXT("body page opens")); D->OpenBody(); });
	Add(4.f, [this, Editor] { AVisexBodyEditor* E = Editor(); QA_LOG(TEXT("VisexShip: body page %s"), E && E->IsOpen() ? TEXT("open") : TEXT("DID NOT OPEN")); Shot(TEXT("ship_body")); });
	auto Row = [](const TCHAR* Name)
	{
		const TArray<int32>& Rows = FVisexBodyProfile::SliderRows(0);
		return Rows.IndexOfByPredicate([Name](int32 I) { return FVisexBodyProfile::Sliders()[I].Name == Name; });
	};
	auto Drag = [this, Add, Editor](const FString& Name, int32 Count, int32 Dir)
	{
		for (int32 K = 0; K < Count; ++K) { Add(0.05f, [Editor, Dir] { if (AVisexBodyEditor* E = Editor()) { E->Adjust(float(Dir)); } }); }
		Add(0.4f, [this, Name, Dir] { Shot(FString::Printf(TEXT("ship_drag_%s_%s"), *Name, Dir > 0 ? TEXT("up") : TEXT("down"))); });
	};
	for (const TCHAR* Name : {TEXT("Height"), TEXT("Hips"), TEXT("Legs"), TEXT("Bust"), TEXT("Glutes"), TEXT("Feet"), TEXT("Skin")})
	{
		Add(0.3f, [Phase, Editor, Row, Name] { Phase(FString::Printf(TEXT("drag %s"), Name)); if (AVisexBodyEditor* E = Editor()) { E->SelectRow(Row(Name)); } });
		Add(1.f, [] {});
		Drag(Name, 40, 1);    // to the slider's maximum
		Drag(Name, 40, -1);   // and down to its minimum
		Add(0.1f, [Editor] { if (AVisexBodyEditor* E = Editor()) { E->ResetSelected(); } });
	}
	// one change kept: longer legs and a darker skin, applied (this writes VisexBody.sav)
	Add(0.3f, [Phase, Editor, Row] { Phase(TEXT("apply")); if (AVisexBodyEditor* E = Editor()) { E->SelectRow(Row(TEXT("Legs"))); E->Adjust(6.f); E->SelectRow(Row(TEXT("Skin"))); E->Adjust(8.f); E->ApplyTab(); } });
	Add(1.f, [this, State] { State(TEXT("applied")); Shot(TEXT("ship_applied")); });
	// FACE, then the tab after it: BODY again while the Hair tab is left out
	Add(0.3f, [Phase, Editor] { Phase(TEXT("drag Head")); if (AVisexBodyEditor* E = Editor()) { E->SwitchTab(1); E->SelectRow(0); QA_LOG(TEXT("VisexShip: tab after one switch: %d"), E->GetTab()); } });
	Add(1.f, [] {});
	Drag(TEXT("Head"), 40, 1);
	Drag(TEXT("Head"), 40, -1);
	Add(0.1f, [Editor] { if (AVisexBodyEditor* E = Editor()) { E->ResetSelected(); E->SwitchTab(1); QA_LOG(TEXT("VisexShip: tab after two switches: %d (0 = BODY: no Hair tab; 2 = HAIR)"), E->GetTab()); } });
	Add(1.f, [this] { Shot(TEXT("ship_tabs")); });
	Add(0.2f, [Editor] { if (AVisexBodyEditor* E = Editor()) { E->Close(); } });
	// the wardrobe: onto every row, CROSS on "Nothing" and on the first costume that is not hers
	Add(5.f, [Phase, D] { Phase(TEXT("wardrobe")); D->OpenWardrobe(); });
	Add(4.f, [this] { Shot(TEXT("ship_wardrobe")); });
	for (int32 I = 0; I < FVisexBodyProfile::Outfits().Num(); ++I)
	{
		Add(0.2f, [Editor, I] { if (AVisexBodyEditor* E = Editor()) { E->SelectRow(I); if (I < 2) { E->ApplyTab(); } } });
		Add(0.9f, [this, State, Editor, I]
		{
			State(FString::Printf(TEXT("wardrobe row %s (%s)"), *FVisexBodyProfile::Outfits()[I].Name.ToString(), FVisexBodyProfile::OutfitLocked(I) ? TEXT("locked") : TEXT("wearable")));
			if (I < 2) { Shot(FString::Printf(TEXT("ship_wardrobe_row%d"), I)); }
		});
		if (I < 2) { Add(0.2f, [Editor] { if (AVisexBodyEditor* E = Editor()) { E->Close(); } }); }   // closes the window
	}
	Add(0.3f, [this] { Shot(TEXT("ship_wardrobe_end")); });
	Add(0.2f, [Editor] { if (AVisexBodyEditor* E = Editor()) { E->Close(); } });
	// Settings: she climbs onto the drum; each Visuals row stepped once and back (the filters)
	auto Visuals = [this, Add](const TCHAR* Where)
	{
		for (int32 R = 0; R + 1 < FVisexSettings::NumVisualRows(); ++R)
		{
			Add(0.3f, [this, R] { FVisexSettings::AdjustVisual(GetWorld(), R, 1); });
			Add(1.5f, [this, R, Where] { Shot(FString::Printf(TEXT("ship_%s_visual%d"), Where, R)); });
			Add(0.2f, [this, R] { FVisexSettings::AdjustVisual(GetWorld(), R, -1); });   // a step later: the shot is taken at the frame's end
		}
	};
	Add(5.f, [Phase, D] { Phase(TEXT("settings sit down")); D->OpenSettings(); });
	Add(9.f, [this, Phase, D] { Phase(TEXT("settings sit")); Shot(TEXT("ship_settings")); D->SettingsTab(1); });
	Visuals(TEXT("sit"));
	// one setting left changed and saved, for the relaunch to find: the fog a step lighter (a step
	// denser would sit at the end of its range, and the photo pass below steps every row up and back)
	Add(0.3f, [D] { D->SettingsStep(-1); });
	Add(1.f, [this, Phase, State, D] { State(TEXT("settings changed")); Phase(TEXT("settings stand up")); D->CloseSettings(); });
	// photo mode: the motions are the spike tour's; here each Visuals row over the photo Idle and a pose
	Add(9.f, [Phase, D] { Phase(TEXT("photo")); D->OnItemChosen(AVisexMenuDirector::ItemPhoto); });
	Add(6.f, [this] { Shot(TEXT("ship_photo")); });
	Visuals(TEXT("photo"));
	Add(0.3f, [this, Phase] { Phase(TEXT("photo pose")); if (AVisexPhotoMode* P = Photo()) { P->SetMotion(5); } });
	Add(4.f, [this] { Shot(TEXT("ship_photo_pose")); if (AVisexPhotoMode* P = Photo()) { P->Close(); } });
	// Start: the camera move, her walk-off, the idle, then driven forward
	Add(6.f, [Phase, D] { Phase(TEXT("start")); D->OnItemChosen(AVisexMenuDirector::ItemStart); });
	Add(4.f, [this] { Shot(TEXT("ship_start")); });
	Add(10.f, [this, Phase, State] { Phase(TEXT("idle after start")); State(TEXT("play")); Shot(TEXT("ship_idle")); });
	Add(3.f, [this, Phase] { Phase(TEXT("walk")); WalkFor = 3.f; });
	Add(3.5f, [this, Phase] { Phase(TEXT("stopped")); Shot(TEXT("ship_walked")); });
	Add(2.f, [this, Phase, State, Quit] { State(TEXT("end")); Phase(TEXT("done")); SpikeTotals(); Quit(); });
}

void AVisexQATour::BuildSide()
{
	auto Add = [this](float Wait, TFunction<void()> Run) { Steps.Add({Wait, MoveTemp(Run)}); };
	AVisexMenuDirector* D = Director;
	ApplyCvarArg();
	FString Pass;
	FParse::Value(FCommandLine::Get(), TEXT("visexside="), Pass);
	auto Editor = [this]() -> AVisexBodyEditor* { for (TActorIterator<AVisexBodyEditor> It(GetWorld()); It; ++It) { return *It; } return nullptr; };
	auto Quit = [this]
	{
		QA_LOG(TEXT("VisexQA: done"));
		UKismetSystemLibrary::QuitGame(this, UGameplayStatics::GetPlayerController(this, 0), EQuitPreference::Quit, false);
	};
	auto State = [this](const TCHAR* Where)
	{
		const ACharacter* C = Her();
		const USkeletalMeshComponent* G = UVisexMHBodyComponent::ShownGarment(C);
		QA_LOG(TEXT("VisexSide: %s: MetaHuman body %s, garment %s, at %s"), Where, UVisexMHBodyComponent::ShownBody(C) ? TEXT("shown") : TEXT("not shown"),
			G && G->GetSkeletalMeshAsset() ? *G->GetSkeletalMeshAsset()->GetName() : TEXT("none"), C ? *C->GetActorLocation().ToCompactString() : TEXT("?"));
	};
	if (Pass == TEXT("film"))
	{
		// the walk-off and the idle it ends in, the third-person camera: 14 s, every second frame of a 60 Hz step
		Add(7.f, [D, State] { State(TEXT("film")); D->OnItemChosen(AVisexMenuDirector::ItemStart); });
		Add(1.f, [this] { SideFilmLeft = 14 * 60; SideFilmN = 0; });
		Add(15.f, Quit);
		return;
	}
	// a shot and the change after it are never in one step: the frame is taken at the frame's end
	Add(7.f, [this, State] { State(TEXT("menu")); HiShot(TEXT("01_menu")); });
	Add(1.f, [D] { D->OpenBody(); });
	Add(5.f, [this] { HiShot(TEXT("02_body")); });
	Add(0.6f, [Editor] { if (AVisexBodyEditor* E = Editor()) { E->SwitchTab(1); } });
	Add(3.f, [this] { HiShot(TEXT("03_face")); });
	Add(0.6f, [Editor] { if (AVisexBodyEditor* E = Editor()) { E->Close(); } });
	Add(5.f, [D] { D->OpenWardrobe(); });
	Add(5.f, [this] { HiShot(TEXT("04_wardrobe")); });
	Add(0.6f, [Editor] { if (AVisexBodyEditor* E = Editor()) { E->Close(); } });
	Add(5.f, [D] { D->OpenSettings(); });
	Add(9.f, [this] { HiShot(TEXT("05_settings")); });
	Add(0.6f, [D] { D->CloseSettings(); });
	Add(9.f, [D] { D->OnItemChosen(AVisexMenuDirector::ItemPhoto); });
	Add(6.f, [this] { if (AVisexPhotoMode* P = Photo()) { P->SetHudVisible(false); } });
	auto Pose = [this](const TCHAR* Name)
	{
		AVisexPhotoMode* P = Photo();
		for (int32 I = 0; P && P->IsOpen() && I < 60; ++I)
		{
			if (P->GetMotionName(I) == Name) { P->SetMotion(I); return; }
		}
		QA_LOG(TEXT("VisexSide: NO photo motion %s"), Name);
	};
	for (const TCHAR* Name : {TEXT("Bookend"), TEXT("Mermaid"), TEXT("LegLift"), TEXT("FeetUp")})
	{
		Add(0.6f, [Pose, Name] { Pose(Name); });
		Add(5.f, [this, Name]
		{
			const ACharacter* C = Her();
			if (C && C->GetMesh()) { QA_LOG(TEXT("VisexSide: %s: her head %.0f cm over the ground"), Name, C->GetMesh()->GetSocketLocation(TEXT("head")).Z - C->GetActorLocation().Z + C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()); }
			HiShot(FString::Printf(TEXT("06_photo_%s"), Name));
		});
	}
	// close on the photo Idle, the free camera placed off the driver's bones (the same camera on both bodies):
	// <bone>, the camera's place from it (forward, right, up of her, cm), the lens
	struct FClose { const TCHAR* Name; FName Bone; FVector From; FVector Lift; float Focal; };
	static const FClose Closes[] = {
		{TEXT("07_portrait"), TEXT("head"), FVector(112.f, 46.f, 4.f), FVector(4.f, 0.f, 0.f), 85.f},
		{TEXT("08_hand_r"), TEXT("hand_r"), FVector(95.f, 50.f, 10.f), FVector(0.f, 0.f, -8.f), 85.f},
		{TEXT("09_hand_l"), TEXT("hand_l"), FVector(95.f, -50.f, 10.f), FVector(0.f, 0.f, -8.f), 85.f},
		{TEXT("10_feet"), TEXT("foot_r"), FVector(150.f, 45.f, 38.f), FVector(6.f, -10.f, -4.f), 70.f},
	};
	Add(0.6f, [Pose] { Pose(TEXT("Idle")); });
	for (const FClose& Close : Closes)
	{
		Add(3.f, [this, Close]
		{
			AVisexPhotoMode* P = Photo();
			ACharacter* C = Her();
			if (!P || !P->IsOpen() || !C || !C->GetMesh()) { return; }
			const auto Frame = [C](const FVector& V) { return C->GetActorForwardVector() * V.X + C->GetActorRightVector() * V.Y + FVector(0.f, 0.f, V.Z); };
			const FVector At = C->GetMesh()->GetSocketLocation(Close.Bone) + Frame(Close.Lift);
			const FVector Loc = C->GetMesh()->GetSocketLocation(Close.Bone) + Frame(Close.From);
			P->SetFreeView(Loc, (At - Loc).Rotation(), 0.f, Close.Focal);
		});
		Add(2.f, [this, Close] { HiShot(Close.Name); });
		if (Close.Bone == TEXT("head"))
		{
			// the face rig: both lids closed through its own curves (nothing blinks her by itself), then given back
			auto Face = [](const TCHAR* Value) { if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("visex.Body.MetaHuman.Face"))) { V->Set(Value, ECVF_SetByConsole); } };
			Add(0.6f, [Face] { Face(TEXT("CTRL_expressions_eyeBlinkL=1,CTRL_expressions_eyeBlinkR=1")); });
			Add(1.5f, [this] { HiShot(TEXT("07_portrait_blink")); });
			Add(0.6f, [Face] { Face(TEXT("off")); });
		}
	}
	Add(0.6f, [this] { if (AVisexPhotoMode* P = Photo()) { P->SetHudVisible(true); P->Close(); } });
	// Start: she walks off under the camera move (from behind), stands at its end, then is walked on
	// along her own heading and shot from the front three-quarter by a camera of the tour's
	Add(6.f, [D] { D->OnItemChosen(AVisexMenuDirector::ItemStart); });
	Add(4.f, [this, State] { State(TEXT("walk-off")); HiShot(TEXT("11_walk_behind")); });
	Add(10.f, [this, State] { State(TEXT("end of Start")); HiShot(TEXT("12_start_end")); });
	Add(1.f, [this] { if (const ACharacter* C = Her()) { WalkYaw = C->GetActorRotation().Yaw; WalkFor = 4.f; } });
	Add(2.f, [this]
	{
		ACharacter* C = Her();
		APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
		if (!C || !PC) { return; }
		if (!SideCam) { SideCam = GetWorld()->SpawnActor<ACameraActor>(); }
		const FVector At = C->GetActorLocation() + C->GetActorForwardVector() * 30.f + FVector(0.f, 0.f, -14.f);   // where she is a few frames on
		const FVector Loc = At + C->GetActorForwardVector().RotateAngleAxis(35.f, FVector::UpVector) * 480.f + FVector(0.f, 0.f, -20.f);
		SideCam->SetActorLocationAndRotation(Loc, (At - Loc).Rotation());
		SideCam->GetCameraComponent()->SetFieldOfView(40.f);
		SideCam->GetCameraComponent()->SetConstraintAspectRatio(false);
		PC->SetViewTarget(SideCam);
	});
	Add(0.15f, [this] { HiShot(TEXT("13_walk_front")); });
	Add(0.6f, [this] { if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0)) { PC->SetViewTarget(Her()); } WalkYaw = 1e9f; });
	Add(2.f, Quit);
}
