#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "VisexBodyProfile.generated.h"

/** One body slider: what it's called, its range, and where it sits by default. */
USTRUCT(BlueprintType)
struct FVisexBodySlider
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Body") FName Name;
	UPROPERTY(BlueprintReadOnly, Category = "Body") FText Label;
	UPROPERTY(BlueprintReadOnly, Category = "Body") float Min = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Body") float Max = 1.f;
	UPROPERTY(BlueprintReadOnly, Category = "Body") float Default = 0.f;
	/** Shown as cm instead of a percentage. */
	UPROPERTY(BlueprintReadOnly, Category = "Body") bool bCentimetres = false;
};

/** One outfit she can wear: a skeletal mesh fitted to the skin (Tools/blender_outfits.py). */
USTRUCT(BlueprintType)
struct FVisexOutfit
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Body") FName Name;
	UPROPERTY(BlueprintReadOnly, Category = "Body") FText Label;
	/** Empty for "none". */
	UPROPERTY(BlueprintReadOnly, Category = "Body") FString Mesh;
};

/**
 * The character: every body slider value by name, and the outfit she wears. Bone sliders
 * are multipliers (1 = Quinn's proportions), Bust and Glutes are morph weights (0 = the base
 * body, 1 = the full sculpt), Height is in cm. Missing names fall back to the default.
 */
USTRUCT(BlueprintType)
struct FVisexBodyProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	TMap<FName, float> Values;

	/** The outfit she wears (a name from Outfits(); None = nothing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	FName Outfit = NAME_None;

	float Get(FName Name) const;
	void Set(FName Name, float Value);

	/** The slider table, in menu order. */
	static VISEX_API const TArray<FVisexBodySlider>& Sliders();
	static VISEX_API const FVisexBodySlider* Find(FName Name);

	/** The wardrobe, in menu order; the first entry is "none". */
	static VISEX_API const TArray<FVisexOutfit>& Outfits();
	static VISEX_API int32 OutfitIndex(FName Name);
};

UCLASS()
class VISEX_API UVisexBodySave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() FVisexBodyProfile Profile;
};
