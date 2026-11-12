#include "VisexLaunchCover.h"
#include "VisexMenuDirector.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "ContentStreaming.h"
#include "PipelineStateCache.h"
#include "ShaderPipelineCache.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Styling/CoreStyle.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "FileMediaSource.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogVisexLaunch, Log, All);

/** A click or key while the film plays skips it, and goes no further (the menu never sees it). */
class FVisexFilmSkip : public IInputProcessor
{
public:
	explicit FVisexFilmSkip(TWeakObjectPtr<UVisexLaunchCover> InCover) : Cover(InCover) {}
	virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}
	virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Event) override
	{
		return !Event.IsRepeat() && Skip();
	}
	virtual bool HandleMouseButtonDownEvent(FSlateApplication&, const FPointerEvent&) override { return Skip(); }
	virtual const TCHAR* GetDebugName() const override { return TEXT("VisexFilmSkip"); }
private:
	bool Skip() const
	{
		UVisexLaunchCover* C = Cover.Get();
		return C && C->SkipFilm();
	}
	TWeakObjectPtr<UVisexLaunchCover> Cover;
};

void UVisexLaunchCover::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// launch only: in the editor PIE would sit under the film
	if (GIsEditor || IsRunningCommandlet())
	{
		bDone = true;
		return;
	}
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UVisexLaunchCover::OnMapLoaded);
}

void UVisexLaunchCover::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	FTSTicker::RemoveTicker(TickHandle);
	if (Player) { Player->Close(); }
	Super::Deinitialize();
}

void UVisexLaunchCover::OnMapLoaded(UWorld* World)
{
	UGameViewportClient* Viewport = GetGameInstance()->GetGameViewportClient();
	if (bShown || !World || !Viewport)
	{
		return;
	}
	bShown = true;
	// black, the film fitted over it (bars where the window isn't 16:10), the world live beneath
	const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / FilmPath);
	if (!FilmPath.IsEmpty() && FPaths::FileExists(File))
	{
		Player = NewObject<UMediaPlayer>(this);
		Player->PlayOnOpen = true;
		Player->SetLooping(false);
		Player->OnEndReached.AddDynamic(this, &UVisexLaunchCover::OnFilmEnd);
		Player->OnMediaOpenFailed.AddDynamic(this, &UVisexLaunchCover::OnFilmFailed);
		FilmTexture = NewObject<UMediaTexture>(this);
		FilmTexture->AutoClear = true;
		FilmTexture->ClearColor = FLinearColor::Black;
		FilmTexture->SetMediaPlayer(Player);
		FilmTexture->UpdateResource();
		FilmBrush.SetResourceObject(FilmTexture);
		FilmBrush.ImageSize = FilmSize;
		FilmBrush.DrawAs = ESlateBrushDrawType::Image;
		Source = NewObject<UFileMediaSource>(this);
		Source->SetFilePath(File);
		bFilm = Player->OpenSource(Source);
		// its sound is the music's opening, which plays live instead (ReleaseMusic)
		Player->SetNativeVolume(0.f);
	}
	FilmStart = FPlatformTime::Seconds();
	UE_LOG(LogVisexLaunch, Display, TEXT("Launch film: %s"), bFilm ? *File : TEXT("none, black"));
	Cover = SNew(SOverlay)
		.Visibility(EVisibility::HitTestInvisible)
		+ SOverlay::Slot()
		[
			SNew(SImage).Image(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
				.ColorAndOpacity_Lambda([this] { return FSlateColor(FLinearColor(0.f, 0.f, 0.f, Opacity)); })
		]
		+ SOverlay::Slot()
		[
			SNew(SScaleBox)
				.Stretch(EStretch::ScaleToFit)
				.Visibility_Lambda([this] { return bFilm && bFilmPlaying ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				[
					SNew(SImage).Image(&FilmBrush)
				]
		];
	Viewport->AddViewportWidgetContent(Cover.ToSharedRef(), 10000);
	if (bFilm && FSlateApplication::IsInitialized())
	{
		SkipInput = MakeShared<FVisexFilmSkip>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(SkipInput, 0);
	}
	HoldMusic(World);
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UVisexLaunchCover::Tick));
}

void UVisexLaunchCover::OnFilmEnd()
{
	if (bFilm)
	{
		bFilm = false;
		FilmEnd = FPlatformTime::Seconds();
		UE_LOG(LogVisexLaunch, Display, TEXT("Launch film ended after %.2f s"), float(FilmEnd - FilmStart));
	}
}

void UVisexLaunchCover::OnFilmFailed(FString Url)
{
	UE_LOG(LogVisexLaunch, Warning, TEXT("Launch film failed to open: %s"), *Url);
	OnFilmEnd();
}

bool UVisexLaunchCover::SkipFilm()
{
	if (!bFilm)
	{
		return false;
	}
	UE_LOG(LogVisexLaunch, Display, TEXT("Launch film skipped"));
	if (Player) { Player->Pause(); }
	OnFilmEnd();
	return true;
}

void UVisexLaunchCover::HoldMusic(UWorld* World)
{
	for (TObjectIterator<UAudioComponent> It; It; ++It)
	{
		if (It->GetWorld() == World && It->Sound && It->Sound->GetPathName().StartsWith(MusicPath))
		{
			It->Stop();
			Music.Add(*It);
		}
	}
}

void UVisexLaunchCover::ReleaseMusic()
{
	// from its top, full volume: the film carries the same opening, so it runs on unbroken
	for (const TWeakObjectPtr<UAudioComponent>& M : Music)
	{
		if (M.IsValid()) { M->Play(0.f); }
	}
	Music.Reset();
}

FString UVisexLaunchCover::Pending(UWorld* World)
{
	if (!Director.IsValid())
	{
		for (TActorIterator<AVisexMenuDirector> It(World); It; ++It) { Director = *It; break; }
	}
	if (Director.IsValid())
	{
		if (!Director->HasFittedColumn())
		{
			return TEXT("menu column");
		}
		const double Now = FPlatformTime::Seconds();
		if (ColumnTime < 0.0) { ColumnTime = Now; }
		if (Now - ColumnTime < AfterColumn)
		{
			return TEXT("shadow cache");
		}
	}
	if (const int32 N = IStreamingManager::Get().GetNumWantingResources())
	{
		return FString::Printf(TEXT("texture streaming (%d)"), N);
	}
	if (const uint32 N = PipelineStateCache::NumActivePrecacheRequests())
	{
		return FString::Printf(TEXT("PSO precache (%u)"), N);
	}
	if (const uint32 N = FShaderPipelineCache::NumPrecompilesRemaining())
	{
		return FString::Printf(TEXT("PSO cache (%u)"), N);
	}
	return FString();
}

void UVisexLaunchCover::RemoveCover()
{
	if (SkipInput.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(SkipInput);
	}
	SkipInput.Reset();
	if (Cover.IsValid())
	{
		if (UGameViewportClient* Viewport = GetGameInstance()->GetGameViewportClient())
		{
			Viewport->RemoveViewportWidgetContent(Cover.ToSharedRef());
		}
		Cover.Reset();
	}
	if (Player) { Player->Close(); }
	Player = nullptr;
	FilmTexture = nullptr;
	Source = nullptr;
	FilmBrush.SetResourceObject(nullptr);
}

bool UVisexLaunchCover::Tick(float DeltaTime)
{
	const double Now = FPlatformTime::Seconds();
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	// the world warms up behind the film: the scene keeps settling, the camera's drift waits at
	// its start and runs from the fade on, the panel stays out (unless her arrival fades it in)
	FString Wait = World ? Pending(World) : FString(TEXT("world"));
	if (Director.IsValid() && FadeStart < 0.0) { Director->HoldDrift(); }
	if (Director.IsValid() && (FadeStart < 0.0 || Now - FadeStart < FadeTime) && !Director->FadesOwnPanel())
	{
		Director->SetPanelOpacity(0.f);
	}
	if (FadeStart < 0.0)
	{
		// calm: no spike against the recent frame rate (a slow machine is calm at its own pace)
		const bool bSpike = AvgDelta > 0.f && DeltaTime > FMath::Max(HitchSeconds, AvgDelta * 1.6f);
		AvgDelta = AvgDelta > 0.f ? FMath::Lerp(AvgDelta, DeltaTime, 0.2f) : FMath::Min(DeltaTime, 0.1f);
		CalmFrames = (Wait.IsEmpty() && !bSpike) ? CalmFrames + 1 : 0;
		if (Wait.IsEmpty() && CalmFrames < SettleFrames) { Wait = TEXT("calm frames"); }
		if (!Wait.IsEmpty()) { LastWait = Wait; }
	}
	if (bFilm)
	{
		if (!bFilmPlaying && Player && Player->IsPlaying() && Player->GetTime() > FTimespan::Zero())
		{
			// its first frame: the music starts from its top with it
			bFilmPlaying = true;
			UE_LOG(LogVisexLaunch, Display, TEXT("Launch film playing %.2f s after the world came up (%s)"),
				float(Now - FilmStart), *Player->GetPlayerName().ToString());
			FilmStart = Now;
			ReleaseMusic();
		}
		const double Length = Player ? Player->GetDuration().GetTotalSeconds() : 0.0;
		if (Now - FilmStart > (bFilmPlaying ? Length : 0.0) + FilmTimeout)
		{
			UE_LOG(LogVisexLaunch, Warning, TEXT("Launch film timed out (%s)"), bFilmPlaying ? TEXT("playing") : TEXT("never opened"));
			OnFilmEnd();
		}
		if (bFilm) { return true; }
	}
	if (FadeStart < 0.0)
	{
		// the film is over (it ends on black): up from black now, or once the scene has settled
		if (FilmEnd < 0.0) { FilmEnd = Now; }
		ReleaseMusic();
		const float Held = float(Now - FilmEnd);
		if (!Wait.IsEmpty() && Held < MaxHold)
		{
			return true;
		}
		FadeStart = Now;
		HeldSeconds = Held;
		ReleaseReason = Wait.IsEmpty()
			? FString::Printf(TEXT("settled, last wait %s"), LastWait.IsEmpty() ? TEXT("none") : *LastWait)
			: FString::Printf(TEXT("max hold, still waiting on %s"), *Wait);
		ReleaseReason += FString::Printf(TEXT(" (%.0f ms frames)"), AvgDelta * 1000.f);
		UE_LOG(LogVisexLaunch, Display, TEXT("Launch fade from black %.3f s after the film's end%s: %s"),
			HeldSeconds, HeldSeconds > 0.1f ? TEXT(" (held for the scene)") : TEXT(""), *ReleaseReason);
		if (FParse::Param(FCommandLine::Get(), TEXT("visexqa")))
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("QA/00_live_at_release.png"), false, false);
		}
		return true;
	}
	// eased: the black thins slowly at first, like the fog lifting
	const float T = FMath::Clamp(float(Now - FadeStart) / FMath::Max(FadeTime, 0.01f), 0.f, 1.f);
	Opacity = FMath::InterpEaseInOut(0.f, 1.f, 1.f - T, 2.f);
	if (T < 1.f)
	{
		return true;
	}
	RemoveCover();
	// then the menu panel fades in over the live shot (unless her arrival fades it in itself)
	const float PanelAlpha = FMath::Clamp(float(Now - FadeStart - FadeTime) / FMath::Max(PanelFadeTime, 0.01f), 0.f, 1.f);
	if (Director.IsValid() && !Director->FadesOwnPanel()) { Director->SetPanelOpacity(FMath::InterpEaseInOut(0.f, 1.f, PanelAlpha, 2.f)); }
	if (PanelAlpha < 1.f)
	{
		return true;
	}
	bDone = true;
	return false;
}
