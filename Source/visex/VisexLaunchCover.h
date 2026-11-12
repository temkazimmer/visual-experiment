#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "Styling/SlateBrush.h"

class SWidget;
#include "VisexLaunchCover.generated.h"

/**
 * The launch: the intro film plays in-game, over the live scene, as soon as the world is up,
 * while the world warms up behind it (textures streamed, PSOs compiled, the menu column fitted,
 * a run of calm frames, her arrival held off frame). The film ends on black; the live scene
 * fades up from black at once, and her arrival starts with the fade. Should the scene not be
 * settled yet, black holds for the rest (logged). The music runs from its top with the film.
 * Click or any key skips the film. Once per launch: never on returning to the menu, never in
 * the editor. Tunables in DefaultGame.ini under [/Script/visex.VisexLaunchCover].
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
	/** The film or the black is still up, before the fade: her arrival waits for it. */
	bool IsHolding() const { return !bDone && FadeStart < 0.0; }
	/** Seconds black held after the film before its fade began, and what let it go. */
	float GetHeldSeconds() const { return HeldSeconds; }
	const FString& GetReleaseReason() const { return ReleaseReason; }

	/** The film, a file relative to Content/ (Content/Movies is staged as loose files). Empty: no film. */
	UPROPERTY(config) FString FilmPath = TEXT("Movies/VisexIntro.mp4");
	/** Its frame size: fitted to the window, black bars where the aspect differs. */
	UPROPERTY(config) FVector2D FilmSize = FVector2D(3840.f, 2400.f);
	/** Gives up on a film that hasn't opened by then (s), or runs this far past its length. */
	UPROPERTY(config) float FilmTimeout = 5.f;
	/** At most this long black after the film, waiting on the scene (s). */
	UPROPERTY(config) float MaxHold = 8.f;
	/** The fade up from black into the live scene (s). */
	UPROPERTY(config) float FadeTime = 1.5f;
	/** Calm frames (all else settled, no hitch) before the fade: Lumen and shadows converge. */
	UPROPERTY(config) int32 SettleFrames = 30;
	/** A frame this slow, or 1.6x the recent average if more, breaks the calm run. */
	UPROPERTY(config) float HitchSeconds = 0.1f;
	/** After the menu column is first fitted: the VSM cache stays off its first 0.5 s (e550725). */
	UPROPERTY(config) float AfterColumn = 0.7f;
	/** Music under this content path waits for the film and starts from its top with it. */
	UPROPERTY(config) FString MusicPath = TEXT("/Game/VisEx/Audio/");
	/** After the fade, the menu panel fades in over this long (unless her arrival fades it in). */
	UPROPERTY(config) float PanelFadeTime = 0.8f;

	/** Ends the film now (click or key); the fade follows as at its end. False: no film up. */
	bool SkipFilm();

private:
	void OnMapLoaded(UWorld* World);
	UFUNCTION() void OnFilmEnd();
	UFUNCTION() void OnFilmFailed(FString Url);
	bool Tick(float DeltaTime);
	FString Pending(UWorld* World);
	void HoldMusic(UWorld* World);
	void ReleaseMusic();
	void RemoveCover();

	UPROPERTY() TObjectPtr<class UMediaPlayer> Player;
	UPROPERTY() TObjectPtr<class UFileMediaSource> Source;
	UPROPERTY() TObjectPtr<class UMediaTexture> FilmTexture;
	FSlateBrush FilmBrush;
	TSharedPtr<SWidget> Cover;
	TSharedPtr<class IInputProcessor> SkipInput;
	TArray<TWeakObjectPtr<class UAudioComponent>> Music;
	TWeakObjectPtr<class AVisexMenuDirector> Director;
	FTSTicker::FDelegateHandle TickHandle;
	bool bShown = false;
	bool bDone = false;
	bool bFilm = false;          // the film is up (opening or playing)
	bool bFilmPlaying = false;   // its first frame has come
	double FilmStart = 0.0;      // when it was opened, then when it started playing
	double FilmEnd = -1.0;       // when it ended (or was skipped, or failed)
	double ColumnTime = -1.0;
	double FadeStart = -1.0;
	int32 CalmFrames = 0;
	float AvgDelta = 0.f;
	float Opacity = 1.f;
	float HeldSeconds = 0.f;
	FString LastWait;
	FString ReleaseReason;
};
