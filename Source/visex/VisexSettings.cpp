#include "VisexSettings.h"

#include "GameFramework/GameUserSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Materials/MaterialInterface.h"
#include "EngineUtils.h"
#include "Containers/Ticker.h"

#define LOCTEXT_NAMESPACE "VisexSettings"

namespace
{
	const TCHAR* Section = TEXT("VisEx");
	const TCHAR* ChosenKey = TEXT("bQualityChosen");
	const int32 FrameRates[] = {30, 60, 120, 0};   // 0 = unlimited
	const int32 NumFrameRates = UE_ARRAY_COUNT(FrameRates);

	UGameUserSettings* Settings() { return UGameUserSettings::GetGameUserSettings(); }

	FText LevelName(int32 Level)
	{
		switch (Level)
		{
		case 0: return LOCTEXT("Low", "Low");
		case 1: return LOCTEXT("Medium", "Medium");
		case 2: return LOCTEXT("High", "High");
		case 3: return LOCTEXT("Epic", "Epic");
		case 4: return LOCTEXT("Ultra", "Ultra");
		default: return LOCTEXT("Custom", "Custom");
		}
	}

	using ERow = FVisexSettings::ERow;

	int32 GetLevel(const UGameUserSettings* S, ERow Row)
	{
		switch (Row)
		{
		case ERow::Preset: return S->GetOverallScalabilityLevel();   // -1 when the rows differ
		case ERow::Shadows: return S->GetShadowQuality();
		case ERow::Lighting: return S->GetGlobalIlluminationQuality();
		case ERow::Reflections: return S->GetReflectionQuality();
		case ERow::Textures: return S->GetTextureQuality();
		case ERow::Effects: return S->GetVisualEffectQuality();
		case ERow::Post: return S->GetPostProcessingQuality();
		case ERow::AntiAliasing: return S->GetAntiAliasingQuality();
		case ERow::ViewDistance: return S->GetViewDistanceQuality();
		case ERow::Foliage: return S->GetFoliageQuality();
		case ERow::Shading: return S->GetShadingQuality();
		default: return 0;
		}
	}

	void SetLevel(UGameUserSettings* S, ERow Row, int32 Level)
	{
		switch (Row)
		{
		case ERow::Preset: S->SetOverallScalabilityLevel(Level); break;
		case ERow::Shadows: S->SetShadowQuality(Level); break;
		case ERow::Lighting: S->SetGlobalIlluminationQuality(Level); break;
		case ERow::Reflections: S->SetReflectionQuality(Level); break;
		case ERow::Textures: S->SetTextureQuality(Level); break;
		case ERow::Effects: S->SetVisualEffectQuality(Level); break;
		case ERow::Post: S->SetPostProcessingQuality(Level); break;
		case ERow::AntiAliasing: S->SetAntiAliasingQuality(Level); break;
		case ERow::ViewDistance: S->SetViewDistanceQuality(Level); break;
		case ERow::Foliage: S->SetFoliageQuality(Level); break;
		case ERow::Shading: S->SetShadingQuality(Level); break;
		default: break;
		}
	}

	int32 FrameRateIndex(const UGameUserSettings* S)
	{
		const int32 Limit = FMath::RoundToInt(S->GetFrameRateLimit());
		for (int32 I = 0; I < NumFrameRates; ++I)
		{
			if (FrameRates[I] == Limit) { return I; }
		}
		return NumFrameRates - 1;
	}
}

TArray<FText> FVisexSettings::Labels()
{
	return {
		LOCTEXT("RowPreset", "Quality"),
		LOCTEXT("RowShadows", "Shadows"),
		LOCTEXT("RowLighting", "Lighting"),
		LOCTEXT("RowReflections", "Reflections"),
		LOCTEXT("RowTextures", "Textures"),
		LOCTEXT("RowEffects", "Effects"),
		LOCTEXT("RowPost", "Post-processing"),
		LOCTEXT("RowAA", "Anti-aliasing"),
		LOCTEXT("RowView", "View distance"),
		LOCTEXT("RowFoliage", "Foliage"),
		LOCTEXT("RowShading", "Shading"),
		LOCTEXT("RowScale", "Render scale"),
		LOCTEXT("RowVSync", "VSync"),
		LOCTEXT("RowFps", "Frame rate"),
		LOCTEXT("RowBack", "Back"),
	};
}

TArray<FText> FVisexSettings::Values()
{
	TArray<FText> Out;
	const UGameUserSettings* S = Settings();
	for (int32 Row = 0; Row < NumRows(); ++Row)
	{
		const ERow R = ERow(Row);
		if (!S || R == ERow::Back) { Out.Add(FText::GetEmpty()); continue; }
		if (R <= ERow::Shading) { Out.Add(LevelName(GetLevel(S, R))); continue; }
		switch (R)
		{
		case ERow::RenderScale:
			Out.Add(FText::Format(LOCTEXT("Percent", "{0} %"), FText::AsNumber(FMath::RoundToInt(S->GetResolutionScaleNormalized() * 100.f))));
			break;
		case ERow::VSync:
			Out.Add(S->IsVSyncEnabled() ? LOCTEXT("On", "On") : LOCTEXT("Off", "Off"));
			break;
		case ERow::FrameRate:
		{
			const int32 Fps = FrameRates[FrameRateIndex(S)];
			Out.Add(Fps > 0 ? FText::Format(LOCTEXT("Fps", "{0} fps"), FText::AsNumber(Fps)) : LOCTEXT("Unlimited", "Unlimited"));
			break;
		}
		default:
			Out.Add(FText::GetEmpty());
		}
	}
	return Out;
}

void FVisexSettings::Adjust(int32 Row, int32 Dir)
{
	UGameUserSettings* S = Settings();
	const ERow R = ERow(Row);
	if (!S || Dir == 0 || R == ERow::Back)
	{
		return;
	}
	if (R <= ERow::Shading)
	{
		int32 Level = GetLevel(S, R);
		// Custom (mixed rows) steps to the nearest preset in that direction
		Level = Level < 0 ? (Dir > 0 ? 4 : 0) : FMath::Clamp(Level + Dir, 0, 4);
		SetLevel(S, R, Level);
		ApplyBetweenFrames();
		return;
	}
	switch (R)
	{
	case ERow::RenderScale:
	{
		// 50..100 % in tens; the scale is what the GPU draws before upscaling to the window
		const float Now = FMath::RoundToFloat(S->GetResolutionScaleNormalized() * 10.f) / 10.f;
		S->SetResolutionScaleNormalized(FMath::Clamp(Now + 0.1f * Dir, 0.5f, 1.f));
		ApplyBetweenFrames(true);
		break;
	}
	case ERow::VSync:
		S->SetVSyncEnabled(!S->IsVSyncEnabled());
		ApplyBetweenFrames();
		break;
	case ERow::FrameRate:
		S->SetFrameRateLimit(float(FrameRates[(FrameRateIndex(S) + Dir + NumFrameRates) % NumFrameRates]));
		ApplyBetweenFrames();
		break;
	default:
		break;
	}
}

void FVisexSettings::ApplyBetweenFrames(bool bResolution)
{
	// the core ticker runs in the engine loop before the world ticks: one shot, next frame
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([bResolution](float)
	{
		if (UGameUserSettings* S = Settings())
		{
			S->ApplyNonResolutionSettings();
			if (bResolution) { S->ApplyResolutionSettings(false); }
		}
		return false;
	}));
}

void FVisexSettings::Save()
{
	if (UGameUserSettings* S = Settings())
	{
		GConfig->SetBool(Section, ChosenKey, true, GGameUserSettingsIni);
		S->SaveSettings();
	}
}

void FVisexSettings::InitialApply()
{
	UGameUserSettings* S = Settings();
	if (!S)
	{
		return;
	}
	bool bChosen = false;
	GConfig->GetBool(Section, ChosenKey, bChosen, GGameUserSettingsIni);
	if (GIsEditor)
	{
		// Play in the editor always shows the look at full quality
		S->SetOverallScalabilityLevel(4);
		S->SetResolutionScaleNormalized(1.f);
	}
	else if (!bChosen)
	{
		// a first launch starts at Epic: Ultra (Cinematic) costs about three times its GPU time
		// here (QA tour, 1080p); the player can raise it on the Settings page
		S->SetOverallScalabilityLevel(3);
		S->SetResolutionScaleNormalized(1.f);
	}
	S->ApplyNonResolutionSettings();
}

const FName FVisexSettings::StyleTag(TEXT("VisexStyle"));

namespace
{
	using EVisual = FVisexSettings::EVisual;

	struct FVisualRow { const TCHAR* Key; int32 Default; int32 Count; };
	// fog: Off, Light, Normal, Dense; style: Cinematic, Natural, Mono; the rest Off/On
	const FVisualRow VisualRows[] = {
		{TEXT("Fog"), 2, 4},
		{TEXT("Style"), 0, 3},
		{TEXT("Anime80s"), 0, 2},
		{TEXT("FilmGrain"), 1, 2},
		{TEXT("MotionBlur"), 1, 2},
	};
	const float FogScale[] = {0.f, 0.45f, 1.f, 1.9f};
	const TCHAR* AnimeMaterial = TEXT("/Game/VisEx/UI/M_Anime80s.M_Anime80s");

	int32 GetVisual(EVisual Row)
	{
		const FVisualRow& R = VisualRows[int32(Row)];
		int32 V = R.Default;
		GConfig->GetInt(Section, R.Key, V, GGameUserSettingsIni);
		return FMath::Clamp(V, 0, R.Count - 1);
	}

	FText OnOff(int32 V) { return V ? LOCTEXT("VOn", "On") : LOCTEXT("VOff", "Off"); }

	// what the level started with, read once per world, so a setting scales from it and can go back
	TMap<TWeakObjectPtr<UExponentialHeightFogComponent>, float> BaseFog;
	TMap<TWeakObjectPtr<APostProcessVolume>, TArray<float>> BaseBlend;
}

TArray<FText> FVisexSettings::VisualLabels()
{
	return {
		LOCTEXT("VFog", "Fog"),
		LOCTEXT("VStyle", "Style"),
		LOCTEXT("VAnime", "80s anime"),
		LOCTEXT("VGrain", "Film grain"),
		LOCTEXT("VBlur", "Motion blur"),
		LOCTEXT("VBack", "Back"),
	};
}

TArray<FText> FVisexSettings::VisualValues()
{
	static const FText Fog[] = {LOCTEXT("FogOff", "Off"), LOCTEXT("FogLight", "Light"), LOCTEXT("FogNormal", "Normal"), LOCTEXT("FogDense", "Dense")};
	static const FText Style[] = {LOCTEXT("StyleCinematic", "Cinematic"), LOCTEXT("StyleNatural", "Natural"), LOCTEXT("StyleMono", "Mono")};
	return {
		Fog[GetVisual(EVisual::Fog)],
		Style[GetVisual(EVisual::Style)],
		OnOff(GetVisual(EVisual::Anime)),
		OnOff(GetVisual(EVisual::Grain)),
		OnOff(GetVisual(EVisual::MotionBlur)),
		FText::GetEmpty(),
	};
}

void FVisexSettings::AdjustVisual(UWorld* World, int32 Row, int32 Dir)
{
	if (Row < 0 || Row >= int32(EVisual::Back) || Dir == 0)
	{
		return;
	}
	const FVisualRow& R = VisualRows[Row];
	// on/off rows flip either way; the others step and stop at the ends
	const int32 Now = GetVisual(EVisual(Row));
	const int32 Next = R.Count == 2 ? 1 - Now : FMath::Clamp(Now + Dir, 0, R.Count - 1);
	GConfig->SetInt(Section, R.Key, Next, GGameUserSettingsIni);
	ApplyVisuals(World);
}

void FVisexSettings::ApplyVisuals(UWorld* World)
{
	if (!World)
	{
		return;
	}
	// fog: every height fog in the level, scaled from its own density
	const int32 Fog = GetVisual(EVisual::Fog);
	for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
	{
		UExponentialHeightFogComponent* C = It->GetComponent();
		if (!C) { continue; }
		const float* Base = BaseFog.Find(C);
		const float Density = Base ? *Base : BaseFog.Add(C, C->FogDensity);
		C->SetFogDensity(Density * FogScale[Fog]);
		C->SetVisibility(Fog > 0);
	}

	// the level's own grade (its unbound volume's blendables) is the Cinematic style
	const int32 Style = GetVisual(EVisual::Style);
	APostProcessVolume* Mine = nullptr;
	for (TActorIterator<APostProcessVolume> It(World); It; ++It)
	{
		if (It->ActorHasTag(StyleTag)) { Mine = *It; continue; }
		if (!It->bUnbound) { continue; }
		TArray<FWeightedBlendable>& Blends = It->Settings.WeightedBlendables.Array;
		TArray<float>* Base = BaseBlend.Find(*It);
		if (!Base)
		{
			Base = &BaseBlend.Add(*It);
			for (const FWeightedBlendable& B : Blends) { Base->Add(B.Weight); }
		}
		for (int32 I = 0; I < Blends.Num() && I < Base->Num(); ++I)
		{
			Blends[I].Weight = Style == 1 ? 0.f : (*Base)[I];
		}
	}
	if (!Mine)
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Mine = World->SpawnActor<APostProcessVolume>(Params);
		if (!Mine) { return; }
		Mine->Tags.Add(StyleTag);
		Mine->bUnbound = true;
		Mine->Priority = 100.f;
		Mine->BlendWeight = 1.f;
	}
	FPostProcessSettings& P = Mine->Settings;
	P.bOverride_ColorSaturation = Style == 2;
	P.ColorSaturation = FVector4(0.f, 0.f, 0.f, 1.f);
	P.bOverride_ColorContrast = Style == 2;
	P.ColorContrast = FVector4(1.15f, 1.15f, 1.15f, 1.f);
	// grain under the anime look dithers the band edges into specks
	P.bOverride_FilmGrainIntensity = GetVisual(EVisual::Grain) == 0 || GetVisual(EVisual::Anime) == 1;
	P.FilmGrainIntensity = 0.f;
	P.bOverride_MotionBlurAmount = GetVisual(EVisual::MotionBlur) == 0;
	P.MotionBlurAmount = 0.f;
	P.WeightedBlendables.Array.Reset();
	if (GetVisual(EVisual::Anime))
	{
		if (UMaterialInterface* Anime = LoadObject<UMaterialInterface>(nullptr, AnimeMaterial, nullptr, LOAD_NoWarn))
		{
			P.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, Anime));
		}
	}
}

#undef LOCTEXT_NAMESPACE
