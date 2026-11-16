#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VisexFootMeter.h"
#include "VisexMHSpikes.h"
#include "VisexQATour.generated.h"

/**
 * A scripted walk through every screen, for checking a packaged build without a pad:
 *   VIS-EX.app/Contents/MacOS/visex-Mac-Development -visexqa
 * Menu, start, walk, back to the menu, each outfit, photo mode through every motion and pose,
 * then quit. Each stop leaves a screenshot in Saved/QA/ and a "VisexQA:" log line saying
 * what was loaded, so a missing asset shows up as a name, not just a wrong picture.
 * AVisexMenuDirector spawns it when the command line has -visexqa.
 */
UCLASS(NotPlaceable)
class VISEX_API AVisexQATour : public AActor
{
	GENERATED_BODY()

public:
	AVisexQATour();
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY() TObjectPtr<class AVisexMenuDirector> Director;

private:
	struct FStep { float Wait; TFunction<void()> Run; };
	TArray<FStep> Steps;
	int32 Next = 0;
	float Clock = 0.f;
	bool bCoverLogged = false;
	float WalkFor = 0.f;
	int32 ShotCount = 0;
	int32 AfterPoses = INDEX_NONE;
	bool bBuilt = false;
	// frame times since the last Report: game-thread frame and GPU, ms
	mutable double FrameSum = 0.0, FrameMax = 0.0, GpuSum = 0.0, GameSum = 0.0, DrawSum = 0.0, RhiSum = 0.0;
	mutable int32 FrameN = 0;

	void Build();
	/** -visexfeet: only the menu's walks (arrival, Body, Wardrobe, photo), her planted steps
	 *  measured and her feet shot in close-up strips (VisexFootMeter.h). */
	void BuildFeet();
	/** -visexgaps [-visexcvar=name=value]: her soles' gap to the sand while she stands still, at
	 *  the menu lean and in the idle after Start, with one cvar set against the default. */
	void BuildGaps();
	/** -visexspikes [-visexcvar=visex.Body.MetaHuman=1]: every pose she has (arrival walk, menu
	 *  lean, Settings sit, each photo motion by name, Start walk, idle, height 145 and 185) with
	 *  the MetaHuman's skin checked for stretched edges each frame (VisexMHSpikes.h). */
	void BuildSpikes();
	/** -visexsliders [-visexcvar=visex.Body.MetaHuman=1] [-visexshots=<name>]: every bone-scale slider
	 *  at its minimum and maximum, height 145 and 185 and two combined profiles, each in the menu
	 *  lean, the photo LegLift and the photo Walk: spike and poke lines, the soles' gap, how far the
	 *  MetaHuman's hands sit from the driver's, the capsule, and a full-body frame of each. Nothing
	 *  is saved: the profile is set in memory and put back at the end. */
	void BuildSliders();
	/** One slider configuration of the sliders tour: defaults, then these values. */
	struct FBodySetup { FString Name; TArray<TPair<FName, float>> Values; };
	TArray<FBodySetup> Setups;
	void ApplySetup(const FBodySetup& Setup);
	/** The numbers of the setup on screen, then (with -visexshots) its portrait frames. */
	void SetupReport(const FString& Pose, const FBodySetup& Setup);
	/** -visexshots=<name>: a 1280x2160 capture of her whole figure from in front (grade on), or of
	 *  head and shoulders, to Saved/QA/<name>_<What>_{body,head}.png. */
	void PortraitShot(const FString& What, bool bHead, const FString& Frame = TEXT("body"));
	UPROPERTY(Transient) TObjectPtr<class USceneCaptureComponent2D> PortraitCapture;
	TMap<FString, TPair<FVector, FVector>> PortraitCameras;   // pose and framing -> from, target
	bool bSliders = false;
	/** -visexclip [-visexoutfit=None] -visexshots=<name>: she walks in, the photo Walk starts, and 150
	 *  frames of her torso (4K scene capture, grade on, a camera level with her from her right) go to
	 *  Saved/QA/<name>_clip_NNNN.png: the jiggle in the walk. Run at a fixed step (-benchmark -fps=30). */
	void BuildClip();
	void ClipFrame();
	UPROPERTY(Transient) TObjectPtr<class USceneCaptureComponent2D> ClipCapture;
	int32 ClipLeft = 0, ClipN = 0;
	FVector ClipOffset = FVector::ZeroVector;
	float ClipZ = 0.f;
	/** -visexbody=Name=value+Name=value: Body page values set in memory before any tour (not saved). */
	void ApplyBodyArg();
	/** -visexshots=<name>: a 4K frame of the game view to Saved/QA/<name>_<What>.png (no-op without it). */
	void HiShot(const FString& What);
	/** -visexclose=<What>:<bone>:<x,y,z>:<fov>[+...]: with that 4K frame, a 4K scene capture framed on a bone. */
	void CloseShots(const FString& Shots, const FString& What);
	UPROPERTY(Transient) TObjectPtr<class USceneCaptureComponent2D> CloseCapture;
	/** -visexcvar=name=value: sets it before the tour starts. */
	void ApplyCvarArg();
	bool bSpikes = false;
	FVisexMHSpikes Spikes;
	bool bFeet = false;
	bool bFilm = false;          // -visexfilm: every frame to Saved/QA/film
	int32 FeetFrames = 0;
	double FeetT0 = 0.0;
	float FeetClock = 0.f;
	int32 ArrivalShots = 0;
	FVisexFootMeter Feet;
	void Shot(const FString& Name);
	class ACharacter* Her() const;
	class AVisexPhotoMode* Photo() const;
	void Report(const FString& Where) const;
	/** Logs the window's shape and whether the view is letterboxed. */
	void ViewCheck(const FString& Where) const;
};
