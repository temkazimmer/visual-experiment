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
	/** The skin tone: shown by name, the track filled with the colour. */
	UPROPERTY(BlueprintReadOnly, Category = "Body") bool bSkinTone = false;
	/** The abs: shown by name (flat .. six-pack). */
	UPROPERTY(BlueprintReadOnly, Category = "Body") bool bAbs = false;
	/** Which Body page tab lists it: 0 body, 1 face. */
	UPROPERTY(BlueprintReadOnly, Category = "Body") int32 Group = 0;
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
	/** Optional Chaos cloth asset worn on top of Mesh (the skirt), empty for none. */
	UPROPERTY(BlueprintReadOnly, Category = "Body") FString Cloth;
	/** Shown in the wardrobe but not wearable yet (to be unlocked or bought later). */
	UPROPERTY(BlueprintReadOnly, Category = "Body") bool bLocked = false;
	/** How much it holds the bust still against the jiggle: 0 = bare, 1 = a sports bra. */
	UPROPERTY(BlueprintReadOnly, Category = "Body") float BustSupport = 0.f;
};

/** One hairstyle: card hair and its physics AnimBP (Docs/Research/HAIR.md). */
struct FVisexHairStyle
{
	FName Name;
	FText Label;
	const TCHAR* Mesh = nullptr;
	const TCHAR* Anim = nullptr;
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

	/** The outfit she wears (a name from Outfits(); None = nothing). She starts in the
	 *  bodystocking. Saves only store this when it differs from the default, so a save made
	 *  before she had a default outfit loads as the bodystocking too. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	FName Outfit = TEXT("Silversuit");

	/** Her hairstyle (a name from HairStyles()). Saves from before styles load as Long. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	FName Hair = TEXT("Long");

	float Get(FName Name) const;
	void Set(FName Name, float Value);

	/** The slider table, in menu order. */
	static VISEX_API const TArray<FVisexBodySlider>& Sliders();
	static VISEX_API const FVisexBodySlider* Find(FName Name);
	/** The slider indices one Body page tab lists, in menu order (Group: 0 body, 1 face). */
	static VISEX_API const TArray<int32>& SliderRows(int32 Group);
	/** The hairstyles on the Body page's HAIR tab, in menu order. */
	static VISEX_API const TArray<FVisexHairStyle>& HairStyles();
	static VISEX_API int32 HairIndex(FName Name);

	/** Skin tone 0..1 across the named stops: the skin's base colour (linear) and its name. */
	static VISEX_API FLinearColor SkinColor(float Tone);
	static VISEX_API FText SkinToneName(float Tone);

	/** Abs 0..1 across flat, toned, defined, six-pack: its name, and the weights of the two
	 *  morphs that make it (Tools/blender_torso.py). Fat lies on top and blurs them. */
	static VISEX_API FText AbsName(float Abs);
	static VISEX_API void AbsWeights(float Abs, float Fat, float& Lines, float& Pack);

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
