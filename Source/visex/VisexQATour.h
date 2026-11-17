#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VisexFootMeter.h"
#include "VisexMHSpikes.h"
#include "VisexJitterMeter.h"
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
	float WalkYaw = 1e9f;   // below 1e8: WalkFor drives her along this yaw, not the camera's (a tour camera in front of her)
	// -visexpin's third-person drive: camera-relative stick (x forward, y right) and a turn on the spot (deg/s)
	FVector2D Drive = FVector2D::ZeroVector;
	float Spin = 0.f;
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
	void BuildLayers();   // -visexlayers: the menu walks once per layer switched off, then pinned
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
	/** -visexclipcam=<angle>,<distance>,<height>,<fov>[,<frames>]: the clip's camera, as degrees round her
	 *  from her front (to her right), cm from her, cm over her feet and its field of view; then how many
	 *  frames. -visexcliplight=<angle>,<distance>,<height>,<candela>: a spot that travels with her, for a
	 *  raking light (the flex clip: legs close, the light from behind the camera's side). */
	UPROPERTY(Transient) TObjectPtr<class USpotLightComponent> ClipLight;
	FVector ClipLightOffset = FVector::ZeroVector;
	int32 ClipFrames = 150;
	/** -visexflex -visexshots=<name> -visexclose=<pose>:...: the flex layer off and on in the same pose
	 *  from the same camera: the menu lean, the photo Idle, then every pose of -visexflexposes
	 *  ("name=<TestPose>|name=<TestPose>", see visex.Body.MetaHuman.TestPose). Close shots go to
	 *  Saved/QA/<name>_flex0_<pose>_close*.png and ..._flex1_... */
	void BuildFlex();
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
	/** -visexship: the walk-through for the body that ships. The lean and its three idle beats (do
	 *  the toes move on the body on screen), the Body page with sliders dragged step by step through
	 *  the editor's own Adjust and one change applied (saved), the FACE tab and what follows it, every
	 *  wardrobe row, the Settings sit and each Visuals row, photo mode with each Visuals row, Start
	 *  and a walk, then Quit from the menu's own item. The spike and poke check runs all through.
	 *  -visexship=2: the relaunch: what the save and the settings came back as, then Quit. */
	void BuildShip();
	/** -visexside -visexshots=<name>: the old | new sheet's frames, 4K of the game view, the same steps and
	 *  cameras whatever body is on (run once with visex.Body.MetaHuman 1, once with 0, at a fixed step):
	 *  the menu lean, the Character page's body and face views, the wardrobe, the Settings sit, photo
	 *  poses, a portrait, hands and feet, the end of Start and a walk from behind and from the front.
	 *  -visexside=film: Start only, every second game frame to Saved/QA/<name>_film/ (walk-off and idle). */
	void BuildSide();
	UPROPERTY(Transient) TObjectPtr<class ACameraActor> SideCam;
	int32 SideFilmLeft = 0, SideFilmN = 0;
	void SpikeTotals();
	/** How far each big toe turned against its foot since the phase began, on the driver and on the
	 *  MetaHuman body (degrees; the same turn reads the same on both whatever their bone axes). */
	void ToePhase(const FString& Name);
	void ToeTick();
	bool bShip = false, bToeRef = false;
	bool ToeHave[2][2] = {{false, false}, {false, false}};
	FString ToeName;
	FQuat ToeRef[2][2];
	float ToeMax[2][2] = {{0.f, 0.f}, {0.f, 0.f}};
	void BuildPin();   // -visexpin: the pin under the sit, third-person play, photo poses and the Body turn, pin 2 / gated / 0
	void PinPhase(const FString& Name);
	/** -visexjitter: she stands in the idle on uneven spots of the level (and a flat one) through
	 *  its weight shifts; her legs' jitter per spot as a table, exit code 3 above the limit.
	 *  Options: -visexjitterspots=name:x:y:z:yaw;...  -visexjittersecs=40  -visexjitterhitch (an
	 *  uneven frame time)  -visexjitterfilm=<spot> -visexjitterfilmfrom=<s> -visexjitterfilmsecs=<s>
	 *  (4K frames of her legs)  -visexjitterscan (lists the ground around her instead)
	 *  -visexcvars=name=value,name=value */
	void BuildJitter();
	void JitterScan();
	bool JitterPlace(const FVector& At, float Yaw);
	FVisexJitterMeter Jitter;
	bool bJitter = false, bHitch = false, bFilming = false;
	FString FilmSpot, FilmName;
	float FilmFrom = 0.f, FilmSecs = 0.f, JitterSpotClock = 0.f;
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
