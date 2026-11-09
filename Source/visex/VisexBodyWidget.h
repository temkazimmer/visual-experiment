#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VisexBodyProfile.h"
#include "VisexBodyWidget.generated.h"

class UTextBlock;
class UHorizontalBox;
class UBorder;

/**
 * The body page: a title and one row per slider (label, thin track, value). Same type
 * and colours as the main menu panel. Builds its own tree; AVisexBodyEditor drives it.
 */
UCLASS(Blueprintable)
class VISEX_API UVisexBodyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UVisexBodyWidget(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Look") FText Title;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Look") FText Hint;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Look") int32 TitleSize = 84;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Look") int32 RowSize = 26;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Look") float TrackWidth = 300.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Look") FLinearColor TextColor = FLinearColor(1.0f, 0.92f, 0.82f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Look") FLinearColor AccentColor = FLinearColor(1.0f, 0.55f, 0.22f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Look") float IdleOpacity = 0.42f;

	/** Show these values and highlight one row. */
	void Show(const FVisexBodyProfile& Profile, int32 Selected);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void Build();
	UTextBlock* MakeText(const FText& Text, int32 Size, FName Typeface, int32 Spacing, const FLinearColor& Color);

	struct FRow
	{
		UHorizontalBox* Box = nullptr;
		UTextBlock* Marker = nullptr;
		UTextBlock* Label = nullptr;
		UBorder* Fill = nullptr;
		UBorder* Rest = nullptr;
		UTextBlock* Value = nullptr;
	};
	TArray<FRow> Rows;

	UPROPERTY(Transient) TArray<TObjectPtr<UWidget>> Keep;   // keep row widgets referenced
};
