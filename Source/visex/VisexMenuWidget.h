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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	int32 TitleSize = 150;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	int32 TitleLetterSpacing = 420;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	int32 TaglineSize = 24;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	int32 ItemSize = 40;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	FLinearColor TextColor = FLinearColor(1.0f, 0.92f, 0.82f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	FLinearColor AccentColor = FLinearColor(1.0f, 0.55f, 0.22f, 1.0f);

	/** Opacity of the items that aren't selected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Look")
	float IdleOpacity = 0.42f;

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
