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

/**
 * The still, drawn byte for byte. Slate would decode an sRGB texture with the sRGB curve and
 * re-encode with gamma 2.2 (Mac), lifting the darks; the film and the game viewport both go
 * to the screen ungamma'd, so the still does too: sampled raw (texture not sRGB), NoGamma.
 * Framed as the movie player frames the film (aspect-fit, centred, black bars) at Fill 0, as the
 * live camera frames the shot at Fill 1 (its horizontal FOV is kept: scaled to the window's width).
 */
class SVisexStill : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SVisexStill) {}
		SLATE_ARGUMENT(const FSlateBrush*, Brush)
		SLATE_ATTRIBUTE(FSlateColor, Tint)
		SLATE_ATTRIBUTE(float, Fill)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Brush = InArgs._Brush;
		Tint = InArgs._Tint;
		Fill = InArgs._Fill;
	}
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Out,
		int32 Layer, const FWidgetStyle& Style, bool) const override
	{
		const FLinearColor C = Style.GetColorAndOpacityTint() * Tint.Get().GetSpecifiedColor();
		const FVector2D Size = Geometry.GetLocalSize();
		const FVector2D Image = Brush->ImageSize;
		if (Image.X <= 0.0 || Image.Y <= 0.0 || Size.X <= 0.0 || Size.Y <= 0.0)
		{
			return Layer;
		}
		const double Fit = FMath::Min(Size.X / Image.X, Size.Y / Image.Y);
		const double Live = Size.X / Image.X;
		const FVector2D Drawn = Image * FMath::Lerp(Fit, Live, (double)FMath::Clamp(Fill.Get(), 0.f, 1.f));
		// the movie player's bars (its border is BlackBrush), then the frame centred over them
		FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(), FCoreStyle::Get().GetBrush(TEXT("BlackBrush")),
			ESlateDrawEffect::NoGamma, FLinearColor::Black.CopyWithNewOpacity(C.A));
		FSlateDrawElement::MakeBox(Out, Layer + 1, Geometry.ToPaintGeometry(Drawn, FSlateLayoutTransform((Size - Drawn) * 0.5)),
			Brush, ESlateDrawEffect::NoGamma, C);
		return Layer + 1;
	}
	virtual FVector2D ComputeDesiredSize(float) const override { return Brush ? Brush->ImageSize : FVector2D::ZeroVector; }

private:
	const FSlateBrush* Brush = nullptr;
	TAttribute<FSlateColor> Tint;
	TAttribute<float> Fill;
};

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
		// raw bytes, see SVisexStill
		Still->SRGB = false;
		Still->UpdateResource();
		StillBrush.SetResourceObject(Still);
		StillBrush.ImageSize = FVector2D(Still->GetSizeX(), Still->GetSizeY());
		StillBrush.DrawAs = ESlateBrushDrawType::Image;
	}
	UE_LOG(LogVisexLaunch, Display, TEXT("Launch cover: %s"), Still
		? *FString::Printf(TEXT("still %s (%dx%d)"), *CoverImage, Still->GetSizeX(), Still->GetSizeY())
		: *FString::Printf(TEXT("no still at %s, flat colour"), *CoverImage));
}

TSharedRef<SWidget> UVisexLaunchCover::MakeCover(TAttribute<FSlateColor> Tint, TAttribute<float> InFill)
{
	if (!Still)
	{
		return SNew(SImage).Image(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"))).ColorAndOpacity(Tint);
	}
	// the film's last frame where the film left it, eased onto the live framing by Fill
	return SNew(SVisexStill).Brush(&StillBrush).Tint(Tint).Fill(InFill);
}

void UVisexLaunchCover::CoverFilmEnd()
{
	// The map can still be loading when the film ends, and the movie player then shows its
	// black border until it has. Lay the cover over that: an overlay that shows once the film's own
	// viewport has gone (the Apple streamer sends no clip-finished event). It runs on the
	// loading thread's Slate, so it only reads the widget tree.
	TSharedRef<FFilmEnd> State = MakeShared<FFilmEnd>();
	FilmEnd = State;
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
			if (State->bSeen && !bPlaying && State->EndTime == 0.0) { State->EndTime = FPlatformTime::Seconds(); }
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
	// Black (no still): the live scene fades up from it. With a still: a cross-fade.
	Cover = SNew(SBox)
		.Visibility(EVisibility::HitTestInvisible)
		[
			MakeCover(TAttribute<FSlateColor>::CreateLambda([this]
			{
				return FSlateColor((Still ? FLinearColor::White : CoverColor).CopyWithNewOpacity(Opacity));
			}), TAttribute<float>::CreateLambda([this] { return Fill; }))
		];
	Viewport->AddViewportWidgetContent(Cover.ToSharedRef(), 10000);
	HoldMusic(World);
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UVisexLaunchCover::Tick));
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

void UVisexLaunchCover::ReleaseMusic(float StartAt, float Fade)
{
	for (const TWeakObjectPtr<UAudioComponent>& M : Music)
	{
		if (!M.IsValid()) { continue; }
		if (Fade > 0.f) { M->FadeIn(Fade, 1.f, StartAt); }
		else { M->Play(StartAt); }
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
		if (LaunchMusicOffset > 0.f)
		{
			// the film carries the soundtrack's opening: pick it up where the film left it, at full
			// volume, no fade
			const double Ended = FilmEnd.IsValid() && FilmEnd->EndTime > 0.0 ? FilmEnd->EndTime.load() : Now;
			UE_LOG(LogVisexLaunch, Display, TEXT("Launch music from %.2f s, %.2f s after the film's end"), LaunchMusicOffset, float(Now - Ended));
			ReleaseMusic(LaunchMusicOffset, 0.f);
		}
		return true;
	}
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	// the camera's drift waits at its start under the black, and runs from the fade on; the
	// panel stays out until the fade is over (unless her arrival fades it in itself)
	if (Director.IsValid() && FadeStart < 0.0) { Director->HoldDrift(); }
	if (Director.IsValid() && (FadeStart < 0.0 || Now - FadeStart < FadeTime) && !Director->FadesOwnPanel())
	{
		Director->SetPanelOpacity(0.f);
	}
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
			ReleaseMusic(0.f, FadeTime);
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
	// off the film's framing onto the live one within the fade's first part, while the still is
	// still mostly there (a no-op at the film's own 16:10)
	Fill = FMath::SmoothStep(0.f, 1.f, FMath::Clamp(float(Now - FadeStart) / FMath::Max(FadeTime * FillShare, 0.01f), 0.f, 1.f));
	if (Now - FadeStart < FadeTime)
	{
		return true;
	}
	if (Cover.IsValid())
	{
		if (UGameViewportClient* Viewport = GetGameInstance()->GetGameViewportClient())
		{
			Viewport->RemoveViewportWidgetContent(Cover.ToSharedRef());
		}
		Cover.Reset();
	}
	// then the menu panel, which the film doesn't have, fades in over the live shot
	const float PanelAlpha = FMath::Clamp(float(Now - FadeStart - FadeTime) / FMath::Max(PanelFadeTime, 0.01f), 0.f, 1.f);
	// (unless her arrival walk fades it in itself)
	if (Director.IsValid() && !Director->FadesOwnPanel()) { Director->SetPanelOpacity(FMath::InterpEaseInOut(0.f, 1.f, PanelAlpha, 2.f)); }
	if (PanelAlpha < 1.f)
	{
		return true;
	}
	bDone = true;
	return false;
}
