#include "VisexBodyProfile.h"

#define LOCTEXT_NAMESPACE "VisexBody"

const TArray<FVisexBodySlider>& FVisexBodyProfile::Sliders()
{
	auto S = [](const TCHAR* Name, const FText& Label, float Min, float Max, float Default, bool bCm = false, bool bTone = false)
	{
		FVisexBodySlider Out;
		Out.bSkinTone = bTone;
		Out.Name = Name;
		Out.Label = Label;
		Out.Min = Min;
		Out.Max = Max;
		Out.Default = Default;
		Out.bCentimetres = bCm;
		return Out;
	};
	// defaults: the body Tema tuned on the Character page (2026-11-09): taller,
	// long legs, full bust and glutes, narrow hips, small hands and feet
	static TArray<FVisexBodySlider> Table = {
		S(TEXT("Skin"),      LOCTEXT("Skin", "Skin tone"),      0.f,   1.f,   0.25f, false, true),
		S(TEXT("Height"),    LOCTEXT("Height", "Height"),       145.f, 185.f, 166.f, true),
		S(TEXT("Head"),      LOCTEXT("Head", "Head"),           0.85f, 1.15f, 1.04f),
		S(TEXT("Neck"),      LOCTEXT("Neck", "Neck"),           0.80f, 1.30f, 0.90f),
		S(TEXT("Shoulders"), LOCTEXT("Shoulders", "Shoulders"), 0.80f, 1.20f, 0.88f),
		S(TEXT("Chest"),     LOCTEXT("Chest", "Ribcage"),       0.85f, 1.20f, 1.0175f),
		S(TEXT("Bust"),      LOCTEXT("Bust", "Bust"),           0.f,   1.f,   0.80f),
		S(TEXT("BustLift"),  LOCTEXT("BustLift", "Bust height"), -1.f,  1.f,   0.5f),
		S(TEXT("BustApart"), LOCTEXT("BustApart", "Bust spacing"), -1.f, 1.f,  0.8f),
		S(TEXT("Waist"),     LOCTEXT("Waist", "Waist"),         0.60f, 1.20f, 0.85f),
		S(TEXT("Abs"),       LOCTEXT("Abs", "Abs"),             0.f,   1.f,   0.5f),
		S(TEXT("Fat"),       LOCTEXT("Fat", "Body fat"),        0.f,   1.f,   0.38f),
		S(TEXT("Hips"),      LOCTEXT("Hips", "Hips"),           0.85f, 1.50f, 0.954f),
		S(TEXT("Glutes"),    LOCTEXT("Glutes", "Glutes"),       0.f,   1.f,   0.5f),
		S(TEXT("Legs"),      LOCTEXT("Legs", "Leg length"),     0.90f, 1.12f, 0.9836f),
		S(TEXT("Thighs"),    LOCTEXT("Thighs", "Thighs"),       0.80f, 1.40f, 1.208f),
		S(TEXT("ThighGap"),  LOCTEXT("ThighGap", "Thigh gap"),  0.f,   1.f,   0.f),
		S(TEXT("Calves"),    LOCTEXT("Calves", "Calves"),       0.80f, 1.25f, 1.16f),
		S(TEXT("ArmLength"), LOCTEXT("ArmLength", "Arm length"),0.92f, 1.08f, 0.976f),
		S(TEXT("Arms"),      LOCTEXT("Arms", "Arms"),           0.80f, 1.25f, 1.f),
		S(TEXT("Hands"),     LOCTEXT("Hands", "Hands"),         0.85f, 1.15f, 0.85f),
		S(TEXT("Feet"),      LOCTEXT("Feet", "Feet"),           0.85f, 1.15f, 0.955f),
	};
	static const bool bMarked = [] {
		for (FVisexBodySlider& Row : Table)
		{
			Row.bAbs = Row.Name == TEXT("Abs");
			Row.Group = Row.Name == TEXT("Head") ? 1 : 0;   // the FACE tab
		}
		return true; }();
	return Table;
}

const TArray<int32>& FVisexBodyProfile::SliderRows(int32 Group)
{
	static const TArray<int32> Rows[2] = {
		[] { TArray<int32> R; for (int32 i = 0; i < Sliders().Num(); ++i) { if (Sliders()[i].Group == 0) { R.Add(i); } } return R; }(),
		[] { TArray<int32> R; for (int32 i = 0; i < Sliders().Num(); ++i) { if (Sliders()[i].Group == 1) { R.Add(i); } } return R; }(),
	};
	return Rows[FMath::Clamp(Group, 0, 1)];
}

const TArray<FVisexHairStyle>& FVisexBodyProfile::HairStyles()
{
	// both share the card atlas and material; elf: swept behind the ears, gathered at the
	// back, a thin lock each side with its own physics (research e3555b4, Tema 2026-11-10)
	static const TArray<FVisexHairStyle> Styles = {
		{TEXT("Long"), LOCTEXT("HairLong", "Long, straight"),
			TEXT("/Game/Character/Hair/SKM_HairCards.SKM_HairCards"), TEXT("/Game/Character/Hair/ABP_HairCards.ABP_HairCards_C")},
		{TEXT("Elf"), LOCTEXT("HairElf", "Elven, swept back"),
			TEXT("/Game/Character/Hair/SKM_HairElf.SKM_HairElf"), TEXT("/Game/Character/Hair/ABP_HairElf.ABP_HairElf_C")},
	};
	return Styles;
}

int32 FVisexBodyProfile::HairIndex(FName Name)
{
	const int32 I = HairStyles().IndexOfByPredicate([Name](const FVisexHairStyle& S) { return S.Name == Name; });
	return I == INDEX_NONE ? 0 : I;
}

FText FVisexBodyProfile::AbsName(float Abs)
{
	static const FText Names[] = {LOCTEXT("AbsFlat", "flat"), LOCTEXT("AbsToned", "toned"),
		LOCTEXT("AbsDefined", "defined"), LOCTEXT("AbsSix", "six-pack")};
	return Names[FMath::Clamp(FMath::RoundToInt(FMath::Clamp(Abs, 0.f, 1.f) * 3.f), 0, 3)];
}

void FVisexBodyProfile::AbsWeights(float Abs, float Fat, float& Lines, float& Pack)
{
	// flat 0 · toned 1/3 · defined 2/3 · six-pack 1; fat over them takes up to 70% of the relief
	const float Show = 1.f - 0.7f * FMath::Clamp(Fat, 0.f, 1.f);
	Lines = FMath::Clamp(Abs / 0.45f, 0.f, 1.f) * Show;
	Pack = FMath::Clamp((Abs - 0.4f) / 0.6f, 0.f, 1.f) * Show;
}

const FVisexBodySlider* FVisexBodyProfile::Find(FName Name)
{
	return Sliders().FindByPredicate([Name](const FVisexBodySlider& S) { return S.Name == Name; });
}

float FVisexBodyProfile::Get(FName Name) const
{
	if (const float* V = Values.Find(Name))
	{
		return *V;
	}
	const FVisexBodySlider* S = Find(Name);
	return S ? S->Default : 1.f;
}

void FVisexBodyProfile::Set(FName Name, float Value)
{
	if (const FVisexBodySlider* S = Find(Name))
	{
		Value = FMath::Clamp(Value, S->Min, S->Max);
	}
	Values.Add(Name, Value);
}

namespace
{
	struct FTone { const TCHAR* Name; FColor SRGB; };
	// even steps from 0 to 1; real skin albedos, a touch below measured so the low sun
	// doesn't bleach the fair ones
	const FTone Tones[] = {
		{TEXT("East Asian"),    FColor(234, 200, 170)},
		{TEXT("European"),      FColor(232, 184, 158)},
		{TEXT("Mediterranean"), FColor(198, 146, 108)},
		{TEXT("South Asian"),   FColor(150, 100, 68)},
		{TEXT("African"),       FColor(88, 56, 40)},
	};
	constexpr int32 NumTones = UE_ARRAY_COUNT(Tones);
	constexpr float ToneAlbedo = 0.8f;
}

FLinearColor FVisexBodyProfile::SkinColor(float Tone)
{
	const float X = FMath::Clamp(Tone, 0.f, 1.f) * (NumTones - 1);
	const int32 I = FMath::Min(FMath::FloorToInt(X), NumTones - 2);
	const FLinearColor A = FLinearColor::FromSRGBColor(Tones[I].SRGB);
	const FLinearColor B = FLinearColor::FromSRGBColor(Tones[I + 1].SRGB);
	FLinearColor C = FMath::Lerp(A, B, X - I) * ToneAlbedo;
	C.A = 1.f;
	return C;
}

FText FVisexBodyProfile::SkinToneName(float Tone)
{
	const int32 I = FMath::Clamp(FMath::RoundToInt(FMath::Clamp(Tone, 0.f, 1.f) * (NumTones - 1)), 0, NumTones - 1);
	return FText::FromString(Tones[I].Name);
}

const TArray<FVisexOutfit>& FVisexBodyProfile::Outfits()
{
	auto O = [](const TCHAR* Name, const FText& Label, const TCHAR* Asset, const TCHAR* Cloth = TEXT(""), float Support = 0.f)
	{
		FVisexOutfit Out;
		Out.BustSupport = Support;
		Out.Name = Name;
		Out.Label = Label;
		Out.Mesh = Asset;
		Out.Cloth = Cloth;
		return Out;
	};
	auto L = [](const TCHAR* Name, const FText& Label)
	{
		FVisexOutfit Out;
		Out.Name = Name;
		Out.Label = Label;
		Out.bLocked = true;
		return Out;
	};
	// The bodystocking, alone or with a satin skirt over it (Tema, 2026-11-10). Meshes: Tools/blender_outfit.py, imported by
	// Tools/char_outfit.py. The others (Noir, Garter, Cocoa, Fishnet, Sheer) are
	// still on disk under /Game/Character/Outfits and /Game/Character/Clothes; add a row to
	// list one again. A saved profile naming an unlisted outfit falls back to "None".
	static const TArray<FVisexOutfit> Table = {
		O(TEXT("None"),    LOCTEXT("OutfitNone", "Nothing"),          TEXT("")),
		O(TEXT("Bodystocking"), LOCTEXT("OutfitBodystocking", "Sheer bodystocking"), TEXT("/Game/Character/Clothes/SKM_Outfit_Bodystocking.SKM_Outfit_Bodystocking"), TEXT(""), 0.15f),
		O(TEXT("BodystockingSkirt"), LOCTEXT("OutfitBodystockingSkirt", "Bodystocking and skirt"), TEXT("/Game/Character/Clothes/SKM_Outfit_Bodystocking.SKM_Outfit_Bodystocking"),
			TEXT("/Game/Character/Clothes/CA_Skirt.CA_Skirt"), 0.15f),   // the skirt is Chaos cloth (Docs/Research/STUDY_SKIRT_SIM.md)
		// refreshed and unlocked 2026-11-11 (Tema): smooth navy latex, keyhole, bound edges, belt and buckle
		O(TEXT("Catsuit"), LOCTEXT("OutfitCatsuit", "Latex catsuit"), TEXT("/Game/Character/Outfits/SKM_Outfit_Catsuit.SKM_Outfit_Catsuit"), TEXT(""), 0.7f),
		// refreshed and unlocked 2026-11-11 (Tema): nylon bodysuit, lace cups as the neckline, satin cincher, seamed thigh-highs
		O(TEXT("Noir"),    LOCTEXT("OutfitNoir", "Noir lace"),        TEXT("/Game/Character/Outfits/SKM_Outfit_Noir.SKM_Outfit_Noir"), TEXT(""), 0.55f),
		// premium bundle 01 (Tema's references, 2026-11-11): sheer mesh bodysuit, bandeau, curved boning, opera gloves
		O(TEXT("Mirror"), LOCTEXT("OutfitMirror", "Mirror"), TEXT("/Game/Character/Clothes/SKM_Outfit_Mirror.SKM_Outfit_Mirror"), TEXT(""), 0.4f),
		// bundle 02 (Tema's references, 2026-11-11): black strap harness, ring-joined, opaque chap stockings
		O(TEXT("Harness"), LOCTEXT("OutfitHarness", "Harness"), TEXT("/Game/Character/Clothes/SKM_Outfit_Harness.SKM_Outfit_Harness"), TEXT(""), 0.45f),
		// bundle 03 (Tema's references, 2026-11-11): red tartan slingshot, collar and tie, white thigh-highs
		O(TEXT("Tartan"), LOCTEXT("OutfitTartan", "Tartan"), TEXT("/Game/Character/Clothes/SKM_Outfit_Tartan.SKM_Outfit_Tartan"), TEXT(""), 0.25f),   // a triangle top: little support
		// wardrobe order (Tema, 2026-11-11): the base set, then Noir, then the premium bundles
		// the dressing room's locked costumes: listed, not wearable yet (unlocked in play or
		// bought, later; Tema, 2026-11-11). Choosing one shows "under construction".
		L(TEXT("Garter"),  LOCTEXT("OutfitGarter", "Garter set")),
		L(TEXT("Cocoa"),   LOCTEXT("OutfitCocoa", "Cocoa satin")),
		L(TEXT("Fishnet"), LOCTEXT("OutfitFishnet", "Fishnet")),
	};
	return Table;
}

int32 FVisexBodyProfile::OutfitIndex(FName Name)
{
	const int32 I = Outfits().IndexOfByPredicate([Name](const FVisexOutfit& O) { return O.Name == Name; });
	return I == INDEX_NONE || Outfits()[I].bLocked ? 0 : I;
}

#undef LOCTEXT_NAMESPACE
