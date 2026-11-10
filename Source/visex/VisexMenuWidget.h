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

	/** Layout scale inside the panel's texture. The director draws the panel this many times
	 * larger and shrinks it back, so text stays sharp at 1440p and up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	float Supersample = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	int32 TitleSize = 104;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	int32 TitleLetterSpacing = 200;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	int32 TaglineSize = 24;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	int32 ItemSize = 40;

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
	float ShadePadding = 260.f;

	/** How much larger the selected item is drawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	float SelectedScale = 1.4f;

	/** Opacity of the items that aren't selected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	float IdleOpacity = 0.5f;

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void SetSelected(int32 Index);

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
	TArray<TObjectPtr<UTextBlock>> Markers;

	int32 Selected = 0;
};
