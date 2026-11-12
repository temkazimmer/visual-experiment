#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "VisexLaunchCover.generated.h"

/**
 * The launch hand-off from the intro film to the menu. The startup movie (DefaultGame.ini,
 * MoviePlayerSettings) ends on a fade to the haze colour; the first map comes up under a
 * full-screen cover in that same colour, which holds until the scene has settled (textures
 * streamed, PSOs compiled, the menu column fitted, a run of calm frames), then fades into the
 * menu while the menu music fades in. Once per launch: never on returning to the menu, never
 * in the editor. Tunables in DefaultGame.ini under [/Script/visex.VisexLaunchCover].
 */
UCLASS(config = Game)
class VISEX_API UVisexLaunchCover : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** True once the cover has fully faded (or never showed). The QA tour waits on it. */
	bool IsDone() const { return bDone; }
	/** Seconds the cover held at full before its fade began, and what let it go. */
	float GetHeldSeconds() const { return HeldSeconds; }
	const FString& GetReleaseReason() const { return ReleaseReason; }

	/** Linear; on screen it must equal the intro film's last frame. */
	UPROPERTY(config) FLinearColor CoverColor = FLinearColor(0.92f, 0.62f, 0.36f, 1.f);
	UPROPERTY(config) float MinHold = 0.5f;
	UPROPERTY(config) float MaxHold = 8.f;
	UPROPERTY(config) float FadeTime = 1.5f;
	/** Calm frames (all else settled, no hitch) before the fade: Lumen and shadows converge. */
	UPROPERTY(config) int32 SettleFrames = 30;
	/** Frames slower than this break the calm run. */
	UPROPERTY(config) float HitchSeconds = 0.1f;
	/** After the menu column is first fitted: the VSM cache stays off its first 0.5 s (e550725). */
	UPROPERTY(config) float AfterColumn = 0.7f;
	/** Music under this content path is held during the cover and faded in with the menu. */
	UPROPERTY(config) FString MusicPath = TEXT("/Game/VisEx/Audio/");

private:
	void OnMapLoaded(UWorld* World);
	bool Tick(float DeltaTime);
	/** Empty when settled, else what it still waits on. */
	FString Pending(UWorld* World);
	void HoldMusic(UWorld* World, bool bHold);

	TSharedPtr<class SWidget> Cover;
	FTSTicker::FDelegateHandle TickHandle;
	TArray<TWeakObjectPtr<class UAudioComponent>> Music;
	bool bShown = false;
	bool bDone = false;
	double StartTime = 0.0;
	double ColumnTime = -1.0;
	double FadeStart = -1.0;
	int32 CalmFrames = 0;
	float Opacity = 1.f;
	float HeldSeconds = 0.f;
	FString ReleaseReason;
	/** The last thing it waited on: the condition whose clearing let the cover go. */
	FString LastWait;
};
