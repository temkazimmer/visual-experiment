#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VisexBodyProfile.h"
#include "VisexBodyWidget.generated.h"

class UTextBlock;
class UHorizontalBox;
class UBorder;
class UVerticalBox;

/** What the Character page shows right now. */
struct FVisexCharacterView
{
	int32 Tab = 0;                      // 0 body, 1 dress
	int32 Selected = 0;                 // row in the tab; the last row is APPLY
	const FVisexBodyProfile* Shown = nullptr;     // what she wears now (previews included)
	const FVisexBodyProfile* Applied = nullptr;   // what was last applied
	bool bBodyDirty = false;
	bool bDressDirty = false;
};

/**
 * The Character page: a title, BODY and DRESS tabs (left to right), and the tab's rows.
 * BODY: one row per slider (label, thin track, value). DRESS: one row per outfit, the
 * applied one marked. Each tab ends in its own APPLY row. Same type and colours as the
 * main menu panel. Builds its own tree; AVisexBodyEditor drives it.
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Look") int32 TabSize = 34;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Look") int32 RowSize = 26;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Look") float TrackWidth = 300.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Look") FLinearColor TextColor = FLinearColor(1.0f, 0.92f, 0.82f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Look") FLinearColor AccentColor = FLinearColor(1.0f, 0.55f, 0.22f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Look") float IdleOpacity = 0.42f;

	void Show(const FVisexCharacterView& View);

	/** Rows in a tab, APPLY included. */
	static int32 RowCount(int32 Tab);

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
		UBorder* Fill = nullptr;     // body rows only
		UBorder* Rest = nullptr;
		UTextBlock* Value = nullptr;
	};
	FRow MakeRow(const FText& Label, bool bTrack);

	TArray<FRow> BodyRows;     // sliders, then APPLY
	TArray<FRow> DressRows;    // outfits, then APPLY
	UVerticalBox* BodyList = nullptr;
	UVerticalBox* DressList = nullptr;
	UTextBlock* TabText[2] = {nullptr, nullptr};

	UPROPERTY(Transient) TArray<TObjectPtr<UWidget>> Keep;   // keep row widgets referenced
};
