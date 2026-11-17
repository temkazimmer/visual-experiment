#include "VisexJitterMeter.h"
#include "VisexMHBody.h"
#include "visex.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "TextureResource.h"

namespace
{
	constexpr float FlipStep = 0.003f;   // cm: a reversal counts only if both steps are larger (30 microns)

	/** One track's jitter: each frame's distance off the line between its neighbours. */
	struct FTrackStat { float Rms = 0.f, Max = 0.f, Flips = 0.f; };

	FTrackStat Measure(const TArray<FVisexFootDebug>& F, TFunctionRef<FVector(const FVisexFootDebug&)> At)
	{
		FTrackStat S;
		double Sum = 0.0, Time = 0.0;
		int32 N = 0, Flips = 0;
		for (int32 I = 1; I + 1 < F.Num(); ++I)
		{
			const float D0 = F[I].Dt, D1 = F[I + 1].Dt;
			if (D0 <= 1e-5f || D1 <= 1e-5f) { continue; }
			const FVector A = At(F[I - 1]), B = At(F[I]), C = At(F[I + 1]);
			const float Off = float((B - FMath::Lerp(A, C, D0 / (D0 + D1))).Size());
			Sum += double(Off) * Off;
			S.Max = FMath::Max(S.Max, Off);
			if (((B - A) | (C - B)) < 0.0 && (B - A).Size() > FlipStep && (C - B).Size() > FlipStep) { ++Flips; }
			Time += D0;
			++N;
		}
		S.Rms = N > 0 ? float(FMath::Sqrt(Sum / N)) : 0.f;
		S.Flips = Time > 0.1 ? float(Flips / Time) : 0.f;
		return S;
	}
}

void FVisexJitterMeter::Begin(ACharacter* Her, const FString& InSpot)
{
	if (bOpen) { End(); }
	Spot = InSpot;
	Frames.Reset();
	MHFrames.Reset();
	Who = Her;
	bOpen = Her && Her->GetMesh();
	if (!bOpen) { return; }
	Right = Her->GetActorRightVector();
	VisexFootDebugFor(Her->GetMesh());
	LastSerial = 0;
}

void FVisexJitterMeter::Tick()
{
	FVisexFootDebug D;
	if (!bOpen || !VisexFootDebugRead(D) || D.Serial == LastSerial) { return; }
	LastSerial = D.Serial;
	Frames.Add(D);
	// the body that is drawn: the follower's bones as the game thread has them, once per driver frame
	if (const USkeletalMeshComponent* MH = UVisexMHBodyComponent::ShownBody(Who.Get()))
	{
		static const FName Bones[9] = {TEXT("foot_l"), TEXT("foot_r"), TEXT("ball_l"), TEXT("ball_r"), TEXT("calf_l"), TEXT("calf_r"), TEXT("thigh_l"), TEXT("thigh_r"), TEXT("pelvis")};
		FMHFrame F;
		F.Dt = D.Dt;
		for (int32 B = 0; B < 9; ++B) { F.P[B] = MH->GetBoneLocation(Bones[B]); }
		MHFrames.Add(F);
	}
}

void FVisexJitterMeter::End()
{
	if (!bOpen) { return; }
	bOpen = false;
	VisexFootDebugFor(nullptr);
	FResult R;
	R.Spot = Spot;
	R.Frames = Frames.Num();
	if (Frames.Num() < 30) { Log(FString::Printf(TEXT("VisexJitter: %s: only %d frames, not measured"), *Spot, Frames.Num())); return; }
	for (const FVisexFootDebug& F : Frames) { R.Seconds += F.Dt; }
	R.Fps = R.Seconds > 0.f ? Frames.Num() / R.Seconds : 0.f;
	struct FTrack { const TCHAR* Name; TFunction<FVector(const FVisexFootDebug&)> At; };
	const FTrack Tracks[] = {
		{TEXT("foot L"), [](const FVisexFootDebug& F) { return F.Leg[0].Foot; }}, {TEXT("foot R"), [](const FVisexFootDebug& F) { return F.Leg[1].Foot; }},
		{TEXT("ball L"), [](const FVisexFootDebug& F) { return F.Leg[0].Ball; }}, {TEXT("ball R"), [](const FVisexFootDebug& F) { return F.Leg[1].Ball; }},
		{TEXT("knee L"), [](const FVisexFootDebug& F) { return F.Leg[0].Knee; }}, {TEXT("knee R"), [](const FVisexFootDebug& F) { return F.Leg[1].Knee; }},
		{TEXT("pelvis"), [](const FVisexFootDebug& F) { return F.Pelvis; }}};
	for (int32 T = 0; T < UE_ARRAY_COUNT(Tracks); ++T)
	{
		const FTrackStat S = Measure(Frames, Tracks[T].At);
		Log(FString::Printf(TEXT("VisexJitter: %s %s: off-line rms %.3f mm, worst %.2f mm, %.1f reversals/s"), *Spot, Tracks[T].Name, S.Rms * 10.f, S.Max * 10.f, S.Flips));
		if (S.Max > R.Max) { R.Max = S.Max; R.WorstTrack = Tracks[T].Name; }
		R.Rms = FMath::Max(R.Rms, S.Rms);
		R.Flips = FMath::Max(R.Flips, S.Flips);
		(T < 4 ? R.FootMax : T < 6 ? R.KneeMax : R.PelvisMax) = FMath::Max(T < 4 ? R.FootMax : T < 6 ? R.KneeMax : R.PelvisMax, S.Max);
	}
	R.Rms *= 10.f; R.Max *= 10.f; R.FootMax *= 10.f; R.KneeMax *= 10.f; R.PelvisMax *= 10.f;   // cm to mm
	// the states that should hold through a weight shift: each heel and ball planted (pin), the
	// plant pass on, ground under the foot; and the ground height the plant pass read
	for (int32 I = 1; I < Frames.Num(); ++I)
	{
		for (int32 L = 0; L < 2; ++L)
		{
			const FVisexFootDebug::FLeg& A = Frames[I - 1].Leg[L];
			const FVisexFootDebug::FLeg& B = Frames[I].Leg[L];
			R.PlantFlips += ((A.HeelW > 0.5f) != (B.HeelW > 0.5f)) + ((A.BallW > 0.5f) != (B.BallW > 0.5f)) + ((A.PlantW > 0.5f) != (B.PlantW > 0.5f)) + (A.bGround != B.bGround);
			R.ExtensionMax = FMath::Max(R.ExtensionMax, B.Extension);
			if (A.bGround && B.bGround) { R.TraceJump = FMath::Max(R.TraceJump, float(FMath::Abs(B.TraceAt.Z - A.TraceAt.Z))); }
		}
	}
	// a thigh or calf that changes length: the graph's Leg IK put the knee on the hip-ankle line
	for (int32 L = 0; L < 2; ++L)
	{
		float Lo[2] = {1e9f, 1e9f}, Hi[2] = {0.f, 0.f};
		for (const FVisexFootDebug& F : Frames)
		{
			const float Len[2] = {float(FVector::Dist(F.Leg[L].Hip, F.Leg[L].Knee)), float(FVector::Dist(F.Leg[L].Knee, F.Leg[L].Foot))};
			for (int32 B = 0; B < 2; ++B) { Lo[B] = FMath::Min(Lo[B], Len[B]); Hi[B] = FMath::Max(Hi[B], Len[B]); }
		}
		for (int32 B = 0; B < 2; ++B) { if (Hi[B] > 1.f) { R.Squash = FMath::Max(R.Squash, 100.f * (Hi[B] - Lo[B]) / Hi[B]); } }
	}
	// weight shifts: the pelvis crossing from over one foot to over the other (1 cm either side of
	// the middle of its range, on a half-second average)
	TArray<float> Side;
	float Lo = 1e9f, Hi = -1e9f;
	for (int32 I = 0; I < Frames.Num(); ++I)
	{
		double Sum = 0.0;
		int32 N = 0;
		for (int32 J = FMath::Max(0, I - 15); J <= FMath::Min(Frames.Num() - 1, I + 15); ++J) { Sum += (Frames[J].Pelvis - (Frames[J].Leg[0].Foot + Frames[J].Leg[1].Foot) * 0.5) | Right; ++N; }
		Side.Add(float(Sum / N));
		Lo = FMath::Min(Lo, Side.Last());
		Hi = FMath::Max(Hi, Side.Last());
	}
	const float Mid = (Lo + Hi) * 0.5f, Band = FMath::Min(1.f, (Hi - Lo) * 0.25f);
	int32 On = 0;
	for (float S : Side)
	{
		const int32 Now = S > Mid + Band ? 1 : S < Mid - Band ? -1 : On;
		if (Now != On && On != 0) { ++R.Shifts; }
		On = Now;
	}
	Log(FString::Printf(TEXT("VisexJitter: %s: %d frames in %.1f s (%.0f fps), %d weight shifts (pelvis %.1f cm side to side), %d plant/pin flips, straightest leg %.3f, ground height jump %.2f cm, thigh or calf length change %.1f%%"),
		*Spot, R.Frames, R.Seconds, R.Fps, R.Shifts, Hi - Lo, R.PlantFlips, R.ExtensionMax, R.TraceJump, R.Squash));
	if (MHFrames.Num() >= 30)
	{
		// the same off-line measure on the MetaHuman body's bones (cm here, mm in the result)
		auto Track = [this](int32 B)
		{
			float Max = 0.f;
			for (int32 I = 1; I + 1 < MHFrames.Num(); ++I)
			{
				const float D0 = MHFrames[I].Dt, D1 = MHFrames[I + 1].Dt;
				if (D0 <= 1e-5f || D1 <= 1e-5f) { continue; }
				Max = FMath::Max(Max, float((MHFrames[I].P[B] - FMath::Lerp(MHFrames[I - 1].P[B], MHFrames[I + 1].P[B], D0 / (D0 + D1))).Size()));
			}
			return Max * 10.f;
		};
		R.MHFootMax = FMath::Max(FMath::Max(Track(0), Track(1)), FMath::Max(Track(2), Track(3)));
		R.MHKneeMax = FMath::Max(Track(4), Track(5));
		R.MHPelvisMax = Track(8);
		for (int32 L = 0; L < 2; ++L)
		{
			float Short[2] = {1e9f, 1e9f}, Long[2] = {0.f, 0.f};
			for (const FMHFrame& F : MHFrames)
			{
				const float Len[2] = {float(FVector::Dist(F.P[6 + L], F.P[4 + L])), float(FVector::Dist(F.P[4 + L], F.P[L]))};
				for (int32 B = 0; B < 2; ++B) { Short[B] = FMath::Min(Short[B], Len[B]); Long[B] = FMath::Max(Long[B], Len[B]); }
			}
			for (int32 B = 0; B < 2; ++B) { if (Long[B] > 1.f) { R.MHSquash = FMath::Max(R.MHSquash, 100.f * (Long[B] - Short[B]) / Long[B]); } }
		}
		Log(FString::Printf(TEXT("VisexJitter: %s MetaHuman body (%d frames): worst off-line foot %.2f mm, knee %.2f mm, pelvis %.2f mm, thigh or calf length change %.1f%%"),
			*Spot, MHFrames.Num(), R.MHFootMax, R.MHKneeMax, R.MHPelvisMax, R.MHSquash));
	}
	Done.Add(R);
	// every frame, for the notes and for finding the cause off line
	auto V = [](const FVector& P) { return FString::Printf(TEXT("%.4f,%.4f,%.4f"), P.X, P.Y, P.Z); };
	TArray<FString> Csv;
	FString Head = TEXT("t,dt,pelvis_x,pelvis_y,pelvis_z,pelvisg_x,pelvisg_y,pelvisg_z,pelvis_fix,pelvis_drop,gate");
	for (const TCHAR* L : {TEXT("l"), TEXT("r")})
	{
		for (const TCHAR* P : {TEXT("hip"), TEXT("knee"), TEXT("foot"), TEXT("ball"), TEXT("hipg"), TEXT("footg"), TEXT("footlay"), TEXT("footpin"), TEXT("pinwant"), TEXT("plantwant"), TEXT("trace"), TEXT("normal")})
		{
			Head += FString::Printf(TEXT(",%s_%s_x,%s_%s_y,%s_%s_z"), P, L, P, L, P, L);
		}
		for (const TCHAR* P : {TEXT("k0graph"), TEXT("k1catwalk"), TEXT("k2alive"), TEXT("k3toes"), TEXT("k4reshape"), TEXT("k5contact"), TEXT("k6limits"), TEXT("k7head"), TEXT("k8pin"), TEXT("k9plant")})
		{
			Head += FString::Printf(TEXT(",%s_%s_x,%s_%s_y,%s_%s_z"), P, L, P, L, P, L);
		}
		for (const TCHAR* P : {TEXT("ground"), TEXT("soleunder"), TEXT("gap"), TEXT("plantw"), TEXT("plantask"), TEXT("plantfix"), TEXT("pinw"), TEXT("heelw"), TEXT("ballw"), TEXT("heelspeed"), TEXT("ballspeed"), TEXT("pinhold"), TEXT("pinheld"), TEXT("ext"), TEXT("lenoff")})
		{
			Head += FString::Printf(TEXT(",%s_%s"), P, L);
		}
	}
	Csv.Add(Head);
	double T = 0.0;
	for (const FVisexFootDebug& F : Frames)
	{
		T += F.Dt;
		FString Row = FString::Printf(TEXT("%.4f,%.5f,%s,%s,%.4f,%.4f,%.3f"), T, F.Dt, *V(F.Pelvis), *V(F.PelvisGraph), F.PelvisFix, F.PelvisDrop, F.PinGate);
		for (const FVisexFootDebug::FLeg& L : F.Leg)
		{
			for (const FVector* P : {&L.Hip, &L.Knee, &L.Foot, &L.Ball, &L.HipGraph, &L.FootGraph, &L.FootLayers, &L.FootPin, &L.PinWant, &L.PlantWant, &L.TraceAt, &L.Normal}) { Row += TEXT(",") + V(*P); }
			for (const FVector& K : L.KneeAt) { Row += TEXT(",") + V(K); }
			Row += FString::Printf(TEXT(",%d,%.4f,%.4f,%.3f,%.4f,%.4f,%.3f,%.3f,%.3f,%.2f,%.2f,%.4f,%.2f,%.5f,%.4f"), L.bGround ? 1 : 0, L.SoleUnder, L.Gap, L.PlantW, L.PlantAsk, L.PlantFix,
				L.PinW, L.HeelW, L.BallW, L.HeelSpeed, L.BallSpeed, L.PinHold, L.PinHeld, L.Extension, L.LengthOff);
		}
		Csv.Add(Row);
	}
	const FString File = FPaths::ProjectSavedDir() / TEXT("QA") / FString::Printf(TEXT("jitter_%s.csv"), *Spot.Replace(TEXT(" "), TEXT("_")));
	FFileHelper::SaveStringToFile(FString::Join(Csv, TEXT("\n")), *File);
	Log(FString::Printf(TEXT("VisexJitter: frames %s"), *File));
	Frames.Reset();
}

void FVisexJitterMeter::Film(UWorld* World, ACharacter* Her, const FString& Name)
{
	USkeletalMeshComponent* Mesh = Her ? Her->GetMesh() : nullptr;
	if (!World || !Mesh) { return; }
	if (!Capture.IsValid())
	{
		Capture = World->SpawnActor<ASceneCapture2D>();
		USceneCaptureComponent2D* C = Capture->GetCaptureComponent2D();
		UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(Capture.Get());
		RT->RenderTargetFormat = RTF_RGBA8_SRGB;
		RT->InitAutoFormat(3840, 2160);
		RT->UpdateResourceImmediate(true);
		C->TextureTarget = RT;
		C->CaptureSource = SCS_FinalColorLDR;
		C->bCaptureEveryFrame = false;
		C->bCaptureOnMovement = false;
		C->bAlwaysPersistRenderingState = true;   // keeps the history: temporal AA and exposure settle
		C->FOVAngle = 34.f;
		// her trailing foot falls outside her mesh bounds and gets culled (AGENT_PLAYBOOK)
		TArray<USkeletalMeshComponent*> Parts;
		Her->GetComponents(Parts);
		for (USkeletalMeshComponent* P : Parts) { P->SetBoundsScale(3.f); }
		// fixed in the world for the whole clip, so a moving camera hides nothing, and placed from her
		// capsule alone, so two runs on one spot get the same camera whatever her legs do: front
		// three-quarter, low, her feet to mid thigh
		const FVector Look = Her->GetActorLocation() - FVector(0.f, 0.f, Her->GetSimpleCollisionHalfHeight() - 30.f);
		const FVector Eye = Look + Her->GetActorForwardVector() * 215.f + Her->GetActorRightVector() * 100.f + FVector(0.f, 0.f, 10.f);
		Capture->SetActorLocationAndRotation(Eye, (Look - Eye).Rotation());
		FilmFrames = 0;
	}
	USceneCaptureComponent2D* C = Capture->GetCaptureComponent2D();
	C->CaptureScene();
	TArray<FColor> Pixels;
	C->TextureTarget->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
	if (Pixels.Num() != 3840 * 2160) { return; }
	for (FColor& P : Pixels) { P.A = 255; }
	const FString File = FPaths::ProjectSavedDir() / TEXT("QA/jitterfilm") / FString::Printf(TEXT("%s_%05d.jpg"), *Name, FilmFrames++);
	FImageUtils::SaveImageByExtension(*File, FImageView(Pixels.GetData(), 3840, 2160), 92);
}
