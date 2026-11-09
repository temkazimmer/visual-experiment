#include "VisexMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

UVisexMenuWidget::UVisexMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Title = FText::FromString(TEXT("VIS-EX"));
	Tagline = FText::FromString(TEXT("A  VISUAL  EXPERIMENT"));
	Items = {
		FText::FromString(TEXT("Start")),
		FText::FromString(TEXT("Photo mode")),
		FText::FromString(TEXT("Quit")),
	};
}

TSharedRef<SWidget> UVisexMenuWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Build();
	}
	return Super::RebuildWidget();
}

UTextBlock* UVisexMenuWidget::MakeText(const FText& Text, int32 Size, FName Typeface, int32 Spacing, const FLinearColor& Color)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Block->SetText(Text);
	FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(Typeface, Size);
	Font.LetterSpacing = Spacing;
	Block->SetFont(Font);
	Block->SetColorAndOpacity(FSlateColor(Color));
	return Block;
}

void UVisexMenuWidget::Build()
{
	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// logo, then the tagline in wide-spaced small caps under it
	Root->AddChildToVerticalBox(MakeText(Title, TitleSize, TEXT("Light"), TitleLetterSpacing, TextColor));
	UVerticalBoxSlot* TagSlot = Root->AddChildToVerticalBox(
		MakeText(Tagline, TaglineSize, TEXT("Regular"), 600, AccentColor));
	TagSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));

	USpacer* Gap = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	Gap->SetSize(FVector2D(1.f, 110.f));
	Root->AddChildToVerticalBox(Gap);

	Labels.Reset();
	Markers.Reset();
	for (const FText& Item : Items)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock* Marker = MakeText(FText::FromString(TEXT("—")), ItemSize, TEXT("Light"), 0, AccentColor);
		UTextBlock* Label = MakeText(Item, ItemSize, TEXT("Light"), 160, TextColor);
		UHorizontalBoxSlot* MarkerSlot = Row->AddChildToHorizontalBox(Marker);
		MarkerSlot->SetPadding(FMargin(8.f, 0.f, 22.f, 0.f));
		Row->AddChildToHorizontalBox(Label);
		UVerticalBoxSlot* RowSlot = Root->AddChildToVerticalBox(Row);
		RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));
		Markers.Add(Marker);
		Labels.Add(Label);
	}
	SetSelected(Selected);
}

void UVisexMenuWidget::SetSelected(int32 Index)
{
	if (Items.Num() == 0)
	{
		return;
	}
	const int32 Previous = Selected;
	Selected = (Index % Items.Num() + Items.Num()) % Items.Num();
	for (int32 i = 0; i < Labels.Num(); ++i)
	{
		const bool bOn = i == Selected;
		Labels[i]->SetRenderOpacity(bOn ? 1.f : IdleOpacity);
		Markers[i]->SetVisibility(bOn ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (Previous != Selected)
	{
		OnSelectionChanged(Selected);
	}
}
