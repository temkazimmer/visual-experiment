#include "VisexBodyProfile.h"

#define LOCTEXT_NAMESPACE "VisexBody"

const TArray<FVisexBodySlider>& FVisexBodyProfile::Sliders()
{
	auto S = [](const TCHAR* Name, const FText& Label, float Min, float Max, float Default, bool bCm = false)
	{
		FVisexBodySlider Out;
		Out.Name = Name;
		Out.Label = Label;
		Out.Min = Min;
		Out.Max = Max;
		Out.Default = Default;
		Out.bCentimetres = bCm;
		return Out;
	};
	// defaults: the starter build, an hourglass after Tema's reference (2026-11-09)
	static const TArray<FVisexBodySlider> Table = {
		S(TEXT("Height"),    LOCTEXT("Height", "Height"),       145.f, 185.f, 159.f, true),
		S(TEXT("Head"),      LOCTEXT("Head", "Head"),           0.85f, 1.15f, 1.f),
		S(TEXT("Neck"),      LOCTEXT("Neck", "Neck"),           0.80f, 1.30f, 1.f),
		S(TEXT("Shoulders"), LOCTEXT("Shoulders", "Shoulders"), 0.80f, 1.20f, 0.90f),
		S(TEXT("Chest"),     LOCTEXT("Chest", "Ribcage"),       0.85f, 1.20f, 0.92f),
		S(TEXT("Bust"),      LOCTEXT("Bust", "Bust"),           0.f,   1.f,   0.55f),
		S(TEXT("Waist"),     LOCTEXT("Waist", "Waist"),         0.60f, 1.20f, 0.72f),
		S(TEXT("Hips"),      LOCTEXT("Hips", "Hips"),           0.85f, 1.50f, 1.22f),
		S(TEXT("Glutes"),    LOCTEXT("Glutes", "Glutes"),       0.f,   1.f,   0.80f),
		S(TEXT("Legs"),      LOCTEXT("Legs", "Leg length"),     0.90f, 1.12f, 1.f),
		S(TEXT("Thighs"),    LOCTEXT("Thighs", "Thighs"),       0.80f, 1.40f, 1.12f),
		S(TEXT("Calves"),    LOCTEXT("Calves", "Calves"),       0.80f, 1.25f, 1.f),
		S(TEXT("ArmLength"), LOCTEXT("ArmLength", "Arm length"),0.92f, 1.08f, 1.f),
		S(TEXT("Arms"),      LOCTEXT("Arms", "Arms"),           0.80f, 1.25f, 1.f),
		S(TEXT("Hands"),     LOCTEXT("Hands", "Hands"),         0.85f, 1.15f, 1.f),
		S(TEXT("Feet"),      LOCTEXT("Feet", "Feet"),           0.85f, 1.15f, 1.f),
	};
	return Table;
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

#undef LOCTEXT_NAMESPACE
