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
	// defaults: the starter build, an hourglass after Tema's reference (2026-11-09)
	static TArray<FVisexBodySlider> Table = {
		S(TEXT("Skin"),      LOCTEXT("Skin", "Skin tone"),      0.f,   1.f,   0.25f, false, true),
		S(TEXT("Height"),    LOCTEXT("Height", "Height"),       145.f, 185.f, 159.f, true),
		S(TEXT("Head"),      LOCTEXT("Head", "Head"),           0.85f, 1.15f, 1.f),
		S(TEXT("Neck"),      LOCTEXT("Neck", "Neck"),           0.80f, 1.30f, 1.f),
		S(TEXT("Shoulders"), LOCTEXT("Shoulders", "Shoulders"), 0.80f, 1.20f, 0.90f),
		S(TEXT("Chest"),     LOCTEXT("Chest", "Ribcage"),       0.85f, 1.20f, 0.92f),
		S(TEXT("Bust"),      LOCTEXT("Bust", "Bust"),           0.f,   1.f,   0.55f),
		S(TEXT("Waist"),     LOCTEXT("Waist", "Waist"),         0.60f, 1.20f, 0.72f),
		S(TEXT("Abs"),       LOCTEXT("Abs", "Abs"),             0.f,   1.f,   0.33f),
		S(TEXT("Fat"),       LOCTEXT("Fat", "Body fat"),        0.f,   1.f,   0.20f),
		S(TEXT("Hips"),      LOCTEXT("Hips", "Hips"),           0.85f, 1.50f, 1.22f),
		S(TEXT("Glutes"),    LOCTEXT("Glutes", "Glutes"),       0.f,   1.f,   0.55f),
		S(TEXT("Legs"),      LOCTEXT("Legs", "Leg length"),     0.90f, 1.12f, 1.f),
		S(TEXT("Thighs"),    LOCTEXT("Thighs", "Thighs"),       0.80f, 1.40f, 1.12f),
		S(TEXT("ThighGap"),  LOCTEXT("ThighGap", "Thigh gap"),  0.f,   1.f,   0.35f),
		S(TEXT("Calves"),    LOCTEXT("Calves", "Calves"),       0.80f, 1.25f, 1.f),
		S(TEXT("ArmLength"), LOCTEXT("ArmLength", "Arm length"),0.92f, 1.08f, 1.f),
		S(TEXT("Arms"),      LOCTEXT("Arms", "Arms"),           0.80f, 1.25f, 1.f),
		S(TEXT("Hands"),     LOCTEXT("Hands", "Hands"),         0.85f, 1.15f, 1.f),
		S(TEXT("Feet"),      LOCTEXT("Feet", "Feet"),           0.85f, 1.15f, 1.f),
	};
	static const bool bMarked = [] { for (FVisexBodySlider& Row : Table) { Row.bAbs = Row.Name == TEXT("Abs"); } return true; }();
	return Table;
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
	auto O = [](const TCHAR* Name, const FText& Label, const TCHAR* Asset)
	{
		FVisexOutfit Out;
		Out.Name = Name;
		Out.Label = Label;
		Out.Mesh = Asset;
		return Out;
	};
	// meshes: Tools/blender_outfits.py, imported by Tools/char_outfits.py
	static const TArray<FVisexOutfit> Table = {
		O(TEXT("None"),    LOCTEXT("OutfitNone", "Nothing"),          TEXT("")),
		O(TEXT("Noir"),    LOCTEXT("OutfitNoir", "Noir bodysuit"),    TEXT("/Game/Character/Outfits/SKM_Outfit_Noir.SKM_Outfit_Noir")),
		O(TEXT("Garter"),  LOCTEXT("OutfitGarter", "Lace garter set"), TEXT("/Game/Character/Outfits/SKM_Outfit_Garter.SKM_Outfit_Garter")),
		O(TEXT("Cocoa"),   LOCTEXT("OutfitCocoa", "Cocoa satin"),     TEXT("/Game/Character/Outfits/SKM_Outfit_Cocoa.SKM_Outfit_Cocoa")),
		O(TEXT("Fishnet"), LOCTEXT("OutfitFishnet", "Fishnet"),       TEXT("/Game/Character/Outfits/SKM_Outfit_Fishnet.SKM_Outfit_Fishnet")),
		O(TEXT("Catsuit"), LOCTEXT("OutfitCatsuit", "Latex catsuit"), TEXT("/Game/Character/Outfits/SKM_Outfit_Catsuit.SKM_Outfit_Catsuit")),
		// layered build: Tools/blender_outfit.py, imported by Tools/char_outfit.py
		O(TEXT("Sheer"),   LOCTEXT("OutfitSheer", "Sheer and latex"), TEXT("/Game/Character/Clothes/SKM_Outfit_Sheer.SKM_Outfit_Sheer")),
	};
	return Table;
}

int32 FVisexBodyProfile::OutfitIndex(FName Name)
{
	const int32 I = Outfits().IndexOfByPredicate([Name](const FVisexOutfit& O) { return O.Name == Name; });
	return I == INDEX_NONE ? 0 : I;
}

#undef LOCTEXT_NAMESPACE
