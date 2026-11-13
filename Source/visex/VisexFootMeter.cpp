#include "VisexFootMeter.h"
#include "visex.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "TextureResource.h"

namespace
{
	constexpr float StepGap = 4.f;    // a foot whose lower sole point is this close (cm) is on the ground
	constexpr float PointSlack = 1.5f; // a sole point within this of the other one carries weight too
	constexpr int32 FrameW = 1024, FrameH = 576;   // a strip is 2x2 frames: 2048x1152

	float RefHeight(const USkeletalMeshComponent* Mesh, FName Bone)
	{
		const USkeletalMesh* Asset = Mesh ? Cast<USkeletalMesh>(Mesh->GetSkinnedAsset()) : nullptr;
		if (!Asset)
		{
			return 0.f;
		}
		const FReferenceSkeleton& Ref = Asset->GetRefSkeleton();
		FTransform T = FTransform::Identity;
		for (int32 I = Ref.FindBoneIndex(Bone); I != INDEX_NONE; I = Ref.GetParentIndex(I))
		{
			T = T * Ref.GetRefBonePose()[I];
		}
		return float(T.GetLocation().Z) * float(Mesh->GetComponentScale().Z);
	}
}

float FVisexFootMeter::GapAt(ACharacter* Her, const FVector& P, float Ref, FString* Surface) const
{
	FCollisionQueryParams Q(TEXT("VisexFootMeter"), true, Her);
	FHitResult Hit;
	const FVector From = P + FVector(0.f, 0.f, 30.f), To = P - FVector(0.f, 0.f, 80.f);
	bool bHit = Her->GetWorld()->LineTraceSingleByChannel(Hit, From, To, VisexFootTraceChannel, Q);
	if (!bHit)
	{
		bHit = Her->GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Q);
	}
	if (Surface)
	{
		*Surface = bHit && Hit.GetActor() ? Hit.GetActor()->GetActorNameOrLabel() : TEXT("none");
	}
	return bHit ? float(P.Z - Ref - Hit.ImpactPoint.Z) : 99.f;
}

void FVisexFootMeter::Tick(ACharacter* Her, float Dt)
{
	USkeletalMeshComponent* Mesh = Her ? Her->GetMesh() : nullptr;
	if (!Mesh || Dt <= 0.f)
	{
		return;
	}
	Clock += Dt;
	if (RefHeel < 0.f)
	{
		RefHeel = RefHeight(Mesh, TEXT("foot_l"));
		RefBall = RefHeight(Mesh, TEXT("ball_l"));
		Log(FString::Printf(TEXT("VisexFeet: sole reference: heel (foot bone) %.2f cm, ball %.2f cm over the sole"), RefHeel, RefBall));
	}
	static const FName Heels[2] = {TEXT("foot_l"), TEXT("foot_r")}, Balls[2] = {TEXT("ball_l"), TEXT("ball_r")};
	for (int32 S = 0; S < 2; ++S)
	{
		FFoot& F = Feet[S];
		const FVector H = Mesh->GetSocketLocation(Heels[S]), B = Mesh->GetSocketLocation(Balls[S]);
		FString Surface;
		const float GH = GapAt(Her, H, RefHeel, &Surface), GB = GapAt(Her, B, RefBall, nullptr);
		const float G = FMath::Min(GH, GB);
		if (G > StepGap)
		{
			if (F.bStep) { EndStep(S); }
			continue;
		}
		if (!F.bStep)
		{
			F = FFoot();
			F.bStep = true;
			F.GapMin = F.GapMax = G;
			F.Surface = Surface;
		}
		F.Time += Dt;
		F.GapSum += G;
		F.GapMin = FMath::Min(F.GapMin, G);
		F.GapMax = FMath::Max(F.GapMax, G);
		++F.N;
		auto Track = [](FPoint& P, const FVector& At, bool bDown)
		{
			if (bDown && !P.bDown) { P.At = At; }
			if (bDown) { P.Drift = FMath::Max(P.Drift, float(FVector::Dist2D(At, P.At))); }
			P.bDown = bDown;
		};
		Track(F.Heel, H, GH < StepGap && GH <= GB + PointSlack);
		Track(F.Ball, B, GB < StepGap && GB <= GH + PointSlack);
	}
}

void FVisexFootMeter::EndStep(int32 Side)
{
	FFoot& F = Feet[Side];
	F.bStep = false;
	if (F.Time < 0.15f || F.N == 0)
	{
		return;   // a brush of the ground in the swing, not a step
	}
	const float Drift = FMath::Max(F.Heel.Drift, F.Ball.Drift), Gap = F.GapSum / F.N;
	PhaseDrift.Add(Drift);
	PhaseGap.Add(Gap);
	Log(FString::Printf(TEXT("VisexFeet: %s %s step at %.2f s, %.2f s down: drift %.2f cm (heel %.2f, ball %.2f), sole gap %.2f cm (%.2f..%.2f) on %s"),
		*Phase, Side ? TEXT("R") : TEXT("L"), Clock - F.Time, F.Time, Drift, F.Heel.Drift, F.Ball.Drift, Gap, F.GapMin, F.GapMax, *F.Surface));
}

void FVisexFootMeter::SetPhase(const FString& Name)
{
	for (int32 S = 0; S < 2; ++S)
	{
		if (Feet[S].bStep) { EndStep(S); }
		Feet[S] = FFoot();
	}
	if (PhaseDrift.Num() > 0)
	{
		TArray<float> D = PhaseDrift, G = PhaseGap;
		D.Sort();
		G.Sort();
		float GapAbsMax = 0.f;
		for (float X : G) { GapAbsMax = FMath::Max(GapAbsMax, FMath::Abs(X)); }
		Log(FString::Printf(TEXT("VisexFeet: SUMMARY %s: %d steps, drift median %.2f max %.2f cm, sole gap median %.2f, worst %.2f cm"),
			*Phase, D.Num(), D[D.Num() / 2], D.Last(), G[G.Num() / 2], GapAbsMax));
	}
	else
	{
		Log(FString::Printf(TEXT("VisexFeet: SUMMARY %s: no planted steps"), *Phase));
	}
	PhaseDrift.Reset();
	PhaseGap.Reset();
	Phase = Name;
}

void FVisexFootMeter::Shoot(UWorld* World, ACharacter* Her, const FString& Name, int32 Index)
{
	USkeletalMeshComponent* Mesh = Her ? Her->GetMesh() : nullptr;
	if (!World || !Mesh)
	{
		return;
	}
	if (!Capture.IsValid())
	{
		Capture = World->SpawnActor<ASceneCapture2D>();
		USceneCaptureComponent2D* C = Capture->GetCaptureComponent2D();
		UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(Capture.Get());
		RT->RenderTargetFormat = RTF_RGBA8_SRGB;
		RT->InitAutoFormat(FrameW, FrameH);
		RT->UpdateResourceImmediate(true);
		C->TextureTarget = RT;
		C->CaptureSource = SCS_FinalColorLDR;
		C->bCaptureEveryFrame = false;
		C->bCaptureOnMovement = false;
		C->FOVAngle = 24.f;
	}
	// her trailing foot falls outside her mesh bounds and gets culled (AGENT_PLAYBOOK)
	TArray<USkeletalMeshComponent*> Parts;
	Her->GetComponents(Parts);
	for (USkeletalMeshComponent* P : Parts) { P->SetBoundsScale(3.f); }
	// from her side, low, at the sand: the sole line and the sand in one frame
	const FVector Mid = (Mesh->GetSocketLocation(TEXT("ball_l")) + Mesh->GetSocketLocation(TEXT("ball_r"))) * 0.5f;
	const FVector Side = Her->GetActorRightVector();
	const FVector Eye = Mid + Side * 190.f + FVector(0.f, 0.f, 6.f);
	USceneCaptureComponent2D* C = Capture->GetCaptureComponent2D();
	Capture->SetActorLocationAndRotation(Eye, (Mid + FVector(0.f, 0.f, 4.f) - Eye).Rotation());
	C->CaptureScene();
	TArray<FColor> Pixels;
	C->TextureTarget->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
	if (Pixels.Num() != FrameW * FrameH)
	{
		return;
	}
	if (Index == 0 || Strip.Num() != FrameW * FrameH * 4)
	{
		Strip.Init(FColor::Black, FrameW * FrameH * 4);
	}
	const int32 OX = (Index % 2) * FrameW, OY = (Index / 2) * FrameH;
	for (int32 Y = 0; Y < FrameH; ++Y)
	{
		FMemory::Memcpy(&Strip[(OY + Y) * FrameW * 2 + OX], &Pixels[Y * FrameW], FrameW * sizeof(FColor));
	}
	if (Index == 3)
	{
		for (FColor& P : Strip) { P.A = 255; }
		const FString File = FPaths::ProjectSavedDir() / TEXT("QA") / FString::Printf(TEXT("fp_%s.jpg"), *Name);
		FImageUtils::SaveImageByExtension(*File, FImageView(Strip.GetData(), FrameW * 2, FrameH * 2), 90);
		Log(FString::Printf(TEXT("VisexFeet: strip %s"), *File));
	}
}
