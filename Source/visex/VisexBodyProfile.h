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

/**
 * The character's body: every slider value by name. Bone sliders are multipliers
 * (1 = Quinn's proportions), Bust and Glutes drive morph targets (0..1, 0.5 = base),
 * Height is in cm. Missing names fall back to the slider default.
 */
USTRUCT(BlueprintType)
struct FVisexBodyProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	TMap<FName, float> Values;

	float Get(FName Name) const;
	void Set(FName Name, float Value);

	/** The slider table, in menu order. */
	static VISEX_API const TArray<FVisexBodySlider>& Sliders();
	static VISEX_API const FVisexBodySlider* Find(FName Name);
};

UCLASS()
class VISEX_API UVisexBodySave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() FVisexBodyProfile Profile;
};
