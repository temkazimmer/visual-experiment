#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VisexMenuWidget.generated.h"

class UTextBlock;

/**
 * The menu panel's content: logo, tagline, items. Builds its own widget tree, so it
 * needs no designer layout; a Blueprint child can still restyle it (all values below).
 * Selection is driven from outside (AVisexMenuDirector reads the pad/keys).
 */
UCLASS(Blueprintable)
class VISEX_API UVisexMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UVisexMenuWidget(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	FText Tagline;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	TArray<FText> Items;

	/** Per item, a value shown in a column right of it (Settings); empty = a plain item. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	TArray<FText> Values;

	/** Tabs over the items (Settings); empty = none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	TArray<FText> Tabs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	int32 ActiveTab = 0;

	/** Layout scale inside the panel's texture. The director draws the panel this many times
	 * larger and shrinks it back, so text stays sharp at 1440p and up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	float Supersample = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	int32 TitleSize = 104;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	int32 TitleLetterSpacing = 200;

	/** The game's logo, drawn in place of the title text on the main page (the one without
	 * tabs). Unset = the title as text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	TSoftObjectPtr<class UTexture2D> LogoTexture = TSoftObjectPtr<class UTexture2D>(FSoftObjectPath(TEXT("/Game/VisEx/Menu/T_Logo_UnopenedGate.T_Logo_UnopenedGate")));

	/** Logo height in layout px per point of TitleSize, so the layout editor's logo size scales it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	float LogoHeightPerSize = 3.f;

	/** Colour scale on the logo image. White = the same white as the text (the texture is white
	 * with its grunge in alpha, so the panel's 5x lighting can't burn detail out). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	FLinearColor LogoTint = FLinearColor::White;

	/** The build number, small under the logo; empty = none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	FText BuildLabel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	int32 BuildLabelSize = 14;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	float BuildLabelOpacity = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	int32 TaglineSize = 24;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	int32 ItemSize = 40;

	/** Space under the logo (before the items), and under each item (main page). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	float LogoGap = 110.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	float ItemGap = 18.f;

	/** Rebuild the panel's contents (after a look property changed). */
	void Refresh() { BuildContent(); }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	FLinearColor TextColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	FLinearColor AccentColor = FLinearColor::White;

	/** A dark feathered plate behind the text, inside the panel so it tilts with it; works with
	 * the menu camera's corner shade (AVisexMenuDirector::CornerShade). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	TSoftObjectPtr<class UMaterialInterface> ShadeMaterial = TSoftObjectPtr<class UMaterialInterface>(FSoftObjectPath(TEXT("/Game/VisEx/UI/M_MenuShade.M_MenuShade")));

	/** Room round the text (layout px) for the shade to fade out in; the director grows the
	 * panel by twice this, so the text stays where it was. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	float ShadePadding = 600.f;

	/** Where the value column starts, layout px from the left of the items. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	float ValueColumn = 400.f;

	/** Item size, selected scale and row gap on a page with values (more rows to fit). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	int32 ValueItemSize = 26;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	float ValueSelectedScale = 1.12f;

	/** How much larger the selected item is drawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	float SelectedScale = 1.4f;

	/** Colour scale of the items that aren't selected. The panel is lit well over white
	 * (AVisexMenuDirector::PanelBrightness), so this sits much lower than it looks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	float IdleBrightness = 0.12f;

	/** Opacity of the items that aren't selected: white at 60 % (Tema). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	float IdleOpacity = 0.6f;

	/** Drop shadow under the text, in layout units before Supersample. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	FVector2D ShadowOffset = FVector2D(3.f, 4.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	float ShadowOpacity = 0.9f;

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void SetSelected(int32 Index);

	/** Show another page in the same panel: title, tabs, items and their values. */
	void SetPage(const FText& InTitle, const TArray<FText>& InTabs, int32 InActiveTab, const TArray<FText>& InItems, const TArray<FText>& InValues, int32 Select);

	/** Update the value column only (no rebuild, so nothing jumps). */
	void SetValues(const TArray<FText>& InValues);

	UFUNCTION(BlueprintPure, Category = "Menu")
	int32 GetSelected() const { return Selected; }

	UFUNCTION(BlueprintPure, Category = "Menu")
	int32 NumItems() const { return Items.Num(); }

	UFUNCTION(BlueprintImplementableEvent, Category = "Menu")
	void OnSelectionChanged(int32 Index);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void Build();
	UTextBlock* MakeText(const FText& Text, int32 Size, FName Typeface, int32 Spacing, const FLinearColor& Color);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> Labels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> ValueLabels;

	UPROPERTY(Transient)
	TObjectPtr<class UVerticalBox> Root;

	void BuildContent();
	bool HasValues() const;
	FText ValueText(int32 Index, bool bSelected) const;

	int32 Selected = 0;
};
