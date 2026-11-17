#pragma once

#include "CoreMinimal.h"
#include "VisexAnimInstance.h"

class ACharacter;
class ASceneCapture2D;
class UWorld;

/**
 * Her legs while she stands in the idle, measured frame by frame for the jitter tour
 * (-visexqa -visexjitter). The frames come from the anim proxy (FVisexFootDebug), so the foot,
 * ball, knee and pelvis tracks line up with what the pin and the plant pass did that frame.
 *
 * Jitter, as a number: how far each frame's point sits off the straight line between the frames
 * before and after it (cm, time-weighted, so an uneven frame time is not read as jitter). A smooth
 * move at 60 Hz leaves a few hundredths of a millimetre; a foot that pops shows as millimetres.
 * Per track: the RMS, the worst frame, and how often the motion reverses per second. Per leg: how
 * often the plant and pin states flip.
 */
struct FVisexJitterMeter
{
	TFunction<void(const FString&)> Log;

	/** One spot's numbers: the worst track's, and the state flips. */
	struct FResult
	{
		FString Spot, WorstTrack;
		float Seconds = 0.f, Fps = 0.f;
		float Rms = 0.f, Max = 0.f, Flips = 0.f;   // mm, mm, reversals per second: the worst track's
		float FootMax = 0.f, KneeMax = 0.f, PelvisMax = 0.f;   // mm
		int32 PlantFlips = 0, Shifts = 0, Frames = 0;
		float ExtensionMax = 0.f, TraceJump = 0.f;   // straightest leg; largest one-frame change of a ground height (cm)
		float Squash = 0.f;   // how much a thigh or calf changed length over the spot, percent (bones don't)
		// the MetaHuman body on screen (visex.Body.MetaHuman): its own bones, same measure; -1 when it is not shown
		float MHFootMax = -1.f, MHKneeMax = -1.f, MHPelvisMax = -1.f, MHSquash = 0.f;
	};

	void Begin(ACharacter* Her, const FString& Spot);
	void Tick();
	/** Closes the spot: logs its rows, writes Saved/QA/jitter_<spot>.csv. */
	void End();
	bool IsOpen() const { return bOpen; }
	const TArray<FResult>& Results() const { return Done; }

	/** A fixed 4K close-up of her legs, one frame per call, to Saved/QA/jitterfilm/<name>_#####.jpg. */
	void Film(UWorld* World, ACharacter* Her, const FString& Name);
	int32 FilmFrames = 0;

private:
	bool bOpen = false;
	FString Spot;
	uint64 LastSerial = 0;
	FVector Right = FVector::RightVector;
	TArray<FVisexFootDebug> Frames;
	/** The MetaHuman body's legs on the same frames: foot, ball, knee (calf), thigh per side, then pelvis. */
	struct FMHFrame { float Dt = 0.f; FVector P[9]; };
	TArray<FMHFrame> MHFrames;
	TWeakObjectPtr<ACharacter> Who;
	TArray<FResult> Done;
	TWeakObjectPtr<ASceneCapture2D> Capture;
};
