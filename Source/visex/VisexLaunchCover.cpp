#include "VisexLaunchCover.h"
#include "VisexMenuDirector.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "ContentStreaming.h"
#include "PipelineStateCache.h"
#include "ShaderPipelineCache.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Images/SImage.h"
#include "Styling/CoreStyle.h"
#include "MoviePlayer.h"
#include "Engine/GameEngine.h"
#include "Framework/Application/SlateApplication.h"
#include "RenderingThread.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogVisexLaunch, Log, All);

void UVisexLaunchCover::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// launch only: in the editor there's no startup film and PIE would sit under the cover
	if (GIsEditor || IsRunningCommandlet())
	{
		bDone = true;
		return;
	}
	LoadStill();
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UVisexLaunchCover::OnMapLoaded);
	if (IsMoviePlayerEnabled() && GetMoviePlayer()->IsMovieCurrentlyPlaying())
	{
		CoverFilmEnd();
	}
}

void UVisexLaunchCover::LoadStill()
{
	// a /Game texture, or a loose image under Content/ (Content/Movies is staged as files)
	if (CoverImage.StartsWith(TEXT("/")))
	{
		Still = Cast<UTexture2D>(FSoftObjectPath(CoverImage).TryLoad());
	}
	else if (!CoverImage.IsEmpty())
	{
		const FString File = FPaths::ProjectContentDir() / CoverImage;
		if (FPaths::FileExists(File))
		{
			Still = FImageUtils::ImportFileAsTexture2D(File);
		}
	}
	if (Still)
	{
		StillBrush.SetResourceObject(Still);
		StillBrush.ImageSize = FVector2D(Still->GetSizeX(), Still->GetSizeY());
		StillBrush.DrawAs = ESlateBrushDrawType::Image;
	}
	UE_LOG(LogVisexLaunch, Display, TEXT("Launch cover: %s"), Still
		? *FString::Printf(TEXT("still %s (%dx%d)"), *CoverImage, Still->GetSizeX(), Still->GetSizeY())
		: *FString::Printf(TEXT("no still at %s, flat colour"), *CoverImage));
}

TSharedRef<SWidget> UVisexLaunchCover::MakeCover(TAttribute<FSlateColor> Tint)
{
	if (!Still)
	{
		return SNew(SImage).Image(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"))).ColorAndOpacity(Tint);
	}
	// aspect-fill: the film's last frame, cropped to the window, never barred
	return SNew(SScaleBox)
		.Stretch(EStretch::ScaleToFill)
		[
			SNew(SImage).Image(&StillBrush).ColorAndOpacity(Tint)
		];
}

void UVisexLaunchCover::CoverFilmEnd()
{
	// The map can still be loading when the film ends, and the movie player then shows its
	// black border until it has. Lay the cover over that: an overlay that shows once the film's own
	// viewport has gone (the Apple streamer sends no clip-finished event). It runs on the
	// loading thread's Slate, so it only reads the widget tree.
	struct FFilmEnd { TWeakPtr<SWidget> Self; TWeakPtr<SWidget> Film; bool bSeen = false; };
	TSharedRef<FFilmEnd> State = MakeShared<FFilmEnd>();
	TSharedRef<SWidget> Haze = SNew(SBox)
		.Visibility_Lambda([State]
		{
			if (!State->Film.IsValid())
			{
				// movie border > overlay > [film box > film viewport, DPI scaler > holder > this]
				TSharedPtr<SWidget> W = State->Self.Pin();
				while (W.IsValid() && W->GetType() != FName(TEXT("SOverlay"))) { W = W->GetParentWidget(); }
				FChildren* Box = W.IsValid() && W->GetChildren()->Num() ? W->GetChildren()->GetChildAt(0)->GetChildren() : nullptr;
				if (Box && Box->Num()) { State->Film = Box->GetChildAt(0); }
			}
			const TSharedPtr<SWidget> Film = State->Film.Pin();
			const bool bPlaying = Film.IsValid() && Film->GetVisibility() == EVisibility::Visible;
			State->bSeen |= bPlaying;
			return State->bSeen && !bPlaying ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
		})
		[
			MakeCover(FSlateColor(Still ? FLinearColor::White : CoverColor))
		];
	State->Self = Haze;
	GetMoviePlayer()->SetSlateOverlayWidget(Haze);
	GetMoviePlayer()->OnMoviePlaybackFinished().AddUObject(this, &UVisexLaunchCover::OnFilmFinished);
}

void UVisexLaunchCover::OnFilmFinished()
{
	GetMoviePlayer()->OnMoviePlaybackFinished().RemoveAll(this);
	// The movie player leaves its black border in the window until the end of the first game
	// frame, which can take a second (first PSOs): hand the window to the game now and draw it
	// once, so what shows meanwhile is the cover, already in the viewport.
	UGameEngine* Game = Cast<UGameEngine>(GEngine);
	if (Game && FSlateApplication::IsInitialized())
	{
		Game->SwitchGameWindowToUseGameViewport();
		FSlateApplication::Get().Tick();
		FlushRenderingCommands();
	}
}

void UVisexLaunchCover::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	FTSTicker::RemoveTicker(TickHandle);
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
	// The map loads while the film still plays, and the world doesn't tick until it ends: the
	// cover goes in now, so the first game frame after the film's last is already the cover.
	// With the still, the fade is a cross-fade: the frozen last frame thins into the live shot.
	Cover = SNew(SBox)
		.Visibility(EVisibility::HitTestInvisible)
		[
			MakeCover(TAttribute<FSlateColor>::CreateLambda([this]
			{
				return FSlateColor((Still ? FLinearColor::White : CoverColor).CopyWithNewOpacity(Opacity));
			}))
		];
	Viewport->AddViewportWidgetContent(Cover.ToSharedRef(), 10000);
	HoldMusic(World, true);
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UVisexLaunchCover::Tick));
}

void UVisexLaunchCover::HoldMusic(UWorld* World, bool bHold)
{
	if (bHold)
	{
		for (TObjectIterator<UAudioComponent> It; It; ++It)
		{
			if (It->GetWorld() == World && It->Sound && It->Sound->GetPathName().StartsWith(MusicPath))
			{
				It->Stop();
				Music.Add(*It);
			}
		}
		return;
	}
	for (const TWeakObjectPtr<UAudioComponent>& M : Music)
	{
		if (M.IsValid()) { M->FadeIn(FadeTime); }
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

bool UVisexLaunchCover::Tick(float DeltaTime)
{
	const double Now = FPlatformTime::Seconds();
	// the first tick is the first frame after the film (nothing ticks while it plays)
	if (StartTime == 0.0)
	{
		// its delta spans the whole load: nothing to judge by yet
		StartTime = Now;
		return true;
	}
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	// the film ends on the menu shot at the drift's start: hold it there until the cover is gone
	if (Director.IsValid()) { Director->HoldDrift(); }
	if (FadeStart < 0.0)
	{
		FString Wait = World ? Pending(World) : FString(TEXT("world"));
		// calm: no spike against the recent frame rate (a slow machine is calm at its own pace)
		const bool bSpike = AvgDelta > 0.f && DeltaTime > FMath::Max(HitchSeconds, AvgDelta * 1.6f);
		AvgDelta = AvgDelta > 0.f ? FMath::Lerp(AvgDelta, DeltaTime, 0.2f) : DeltaTime;
		CalmFrames = (Wait.IsEmpty() && !bSpike) ? CalmFrames + 1 : 0;
		if (Wait.IsEmpty() && CalmFrames < SettleFrames)
		{
			Wait = TEXT("calm frames");
		}
		if (!Wait.IsEmpty()) { LastWait = Wait; }
		const float Held = float(Now - StartTime);
		const bool bSettled = Wait.IsEmpty();
		if ((bSettled && Held >= MinHold) || Held >= MaxHold)
		{
			FadeStart = Now;
			HeldSeconds = Held;
			ReleaseReason = bSettled
				? FString::Printf(TEXT("settled, last wait %s"), LastWait.IsEmpty() ? TEXT("none (min hold)") : *LastWait)
				: FString::Printf(TEXT("max hold, still waiting on %s"), *Wait);
			ReleaseReason += FString::Printf(TEXT(" (%.0f ms frames)"), AvgDelta * 1000.f);
			UE_LOG(LogVisexLaunch, Display, TEXT("Launch cover held %.2f s, released: %s"), HeldSeconds, *ReleaseReason);
			HoldMusic(World, false);
			if (FParse::Param(FCommandLine::Get(), TEXT("visexqa")))
			{
				// the live frame the still fades into, without the cover: diff it against the still
				FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("QA/00_live_at_release.png"), false, false);
			}
		}
		return true;
	}
	Opacity = 1.f - FMath::Clamp(float(Now - FadeStart) / FMath::Max(FadeTime, 0.01f), 0.f, 1.f);
	// eased: the haze thins slowly at first, like the fog lifting
	Opacity = FMath::InterpEaseInOut(0.f, 1.f, Opacity, 2.f);
	if (Now - FadeStart < FadeTime)
	{
		return true;
	}
	if (UGameViewportClient* Viewport = GetGameInstance()->GetGameViewportClient())
	{
		Viewport->RemoveViewportWidgetContent(Cover.ToSharedRef());
	}
	Cover.Reset();
	bDone = true;
	return false;
}
