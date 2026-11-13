#pragma once

#include "CoreMinimal.h"

class ACharacter;
class ASceneCapture2D;
class UWorld;

/**
 * Her planted steps, measured at real speed for the feet tour (-visexqa -visexfeet). Per step:
 * how far the grounded sole points (heel, ball) slid while down, and how far the soles sit off
 * the surface under them (FootTrace, else Visibility). Also shoots 2x2 close-up strips of her
 * feet through a SceneCapture2D (framing, not the look), saved as Saved/QA/fp_<name>.jpg.
 */
struct FVisexFootMeter
{
	TFunction<void(const FString&)> Log;

	void Tick(ACharacter* Her, float Dt);
	/** Closes open steps, logs the phase's summary, starts the next. */
	void SetPhase(const FString& Name);
	/** Frame Index (0..3) of the strip Name; the strip is written once frame 3 is in. */
	void Shoot(UWorld* World, ACharacter* Her, const FString& Name, int32 Index);

private:
	struct FPoint { bool bDown = false; FVector At = FVector::ZeroVector; float Drift = 0.f; };
	struct FFoot
	{
		bool bStep = false;
		float Time = 0.f, GapSum = 0.f, GapMin = 0.f, GapMax = 0.f;
		int32 N = 0;
		float VGap = 99.f;   // the lowest skinned sole vertex's gap over the step
		FPoint Heel, Ball;
		TArray<FVector4f> H, B;   // per frame: the point (xyz) and its gap (w)
		FString Surface;
	};
	FFoot Feet[2];
	float RefHeel = -1.f, RefBall = -1.f;   // the heel (foot) and ball bones' height over the sole, ref pose
	float Clock = 0.f;
	FString Phase = TEXT("held");
	TArray<float> PhaseDrift, PhaseGap;
	double PhaseGapSum = 0.0;   // every frame's lower sole gap, both feet, whether stepping or not
	int32 PhaseGapN = 0;
	TArray<FString> Csv;   // every frame, both feet: written to Saved/QA/feet.csv on the phase "done"
	TArray<FColor> Strip;
	TWeakObjectPtr<ASceneCapture2D> Capture;
	TWeakObjectPtr<ACharacter> LastHer;
	/** Where her mesh origin and capsule sit against the surfaces under her (the base float). */
	void LogGround(ACharacter* Her) const;
	/** Her soles' real gap: the lowest skinned vertex of each foot over the surface under it (cm). */
	void SoleGaps(ACharacter* Her, float Out[2]);
	float SoleOverOrigin[2] = {0.f, 0.f};

	void EndStep(int32 Side);
	float GapAt(ACharacter* Her, const FVector& P, float Ref, FString* Surface) const;
};
