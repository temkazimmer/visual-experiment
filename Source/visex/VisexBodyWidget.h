#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VisexBodyProfile.h"
#include "VisexBodyWidget.generated.h"

class UTextBlock;
class UHorizontalBox;
class UBorder;
class UVerticalBox;

/** What the Body or Wardrobe page shows right now. */
struct FVisexCharacterView
{
	int32 Page = 0;                     // 0 the Body page, 1 the Wardrobe
	int32 Tab = 0;                      // Body page: 0 body, 1 face, 2 hair
	int32 Selected = 0;                 // row in the tab; the last row is APPLY (not on HAIR)
	const FVisexBodyProfile* Shown = nullptr;     // what she wears now (previews included)
	const FVisexBodyProfile* Applied = nullptr;   // what was last applied
	bool bBodyDirty = false;
	bool bDressDirty = false;
	bool bHairDirty = false;
	bool bUnderConstruction = false;    // the "under construction" window over the list
};

/**
 * Two pages in one panel. BODY: a title, BODY / FACE / HAIR tabs (left to right) and the
 * tab's rows: one per slider (label, thin track, value), or per hairstyle. WARDROBE: one row
 * per costume, the worn one marked, the locked ones marked LOCKED; choosing a locked one
 * opens a small "under construction" window. Slider and costume lists end in an APPLY row.
 * Same type and colours as the main menu panel. Builds its own tree; AVisexBodyEditor
 * drives it.
 */
UCLASS(Blueprintable)
class VISEX_API UVisexBodyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UVisexBodyWidget(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Look") FText Title;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Look") FText Hint;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Look") FText WardrobeTitle;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Look") FText WardrobeHint;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Look") int32 TitleSize = 84;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Look") int32 TabSize = 34;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Look") int32 RowSize = 26;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Look") float TrackWidth = 300.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Look") FLinearColor TextColor = FLinearColor(1.0f, 0.92f, 0.82f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Look") FLinearColor AccentColor = FLinearColor(1.0f, 0.55f, 0.22f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body|Look") float IdleOpacity = 0.42f;

	void Show(const FVisexCharacterView& View);

	/** Rows in a page's tab, APPLY included. */
	static int32 RowCount(int32 Page, int32 Tab);
	static constexpr int32 BodyTabs = 3;

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

	UTextBlock* MakeNote(const FText& Text);

	TArray<FRow> BodyRows;     // body sliders, then APPLY
	TArray<FRow> FaceRows;     // face sliders, then APPLY
	TArray<FRow> HairRows;     // hairstyles, then APPLY
	TArray<FRow> DressRows;    // costumes, then APPLY
	UVerticalBox* Lists[4] = {nullptr, nullptr, nullptr, nullptr};   // body, face, hair, wardrobe
	UVerticalBox* Popup = nullptr;
	UTextBlock* TitleText = nullptr;
	UTextBlock* HintText = nullptr;
	UHorizontalBox* TabBar = nullptr;
	class USpacer* TitleGap = nullptr;
	UTextBlock* TabText[BodyTabs] = {nullptr, nullptr, nullptr};

	UPROPERTY(Transient) TArray<TObjectPtr<UWidget>> Keep;   // keep row widgets referenced
};
