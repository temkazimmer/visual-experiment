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

DEFINE_LOG_CATEGORY_STATIC(LogVisexLaunch, Log, All);

void UVisexLaunchCover::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// launch only: in the editor there's no startup film and PIE would sit under haze
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
	// cover goes in now, so the first game frame after the film's last is already haze.
	Cover = SNew(SImage)
		.Image(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
		.Visibility(EVisibility::HitTestInvisible)
		.ColorAndOpacity_Lambda([this] { return FSlateColor(CoverColor.CopyWithNewOpacity(Opacity)); });
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
	AVisexMenuDirector* Director = nullptr;
	for (TActorIterator<AVisexMenuDirector> It(World); It; ++It) { Director = *It; break; }
	if (Director)
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
	if (StartTime == 0.0) { StartTime = Now; }
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (FadeStart < 0.0)
	{
		FString Wait = World ? Pending(World) : FString(TEXT("world"));
		CalmFrames = (Wait.IsEmpty() && DeltaTime < HitchSeconds) ? CalmFrames + 1 : 0;
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
			UE_LOG(LogVisexLaunch, Display, TEXT("Launch cover held %.2f s, released: %s"), HeldSeconds, *ReleaseReason);
			HoldMusic(World, false);
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
