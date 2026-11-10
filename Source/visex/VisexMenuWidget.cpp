#include "VisexMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Materials/MaterialInterface.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

UVisexMenuWidget::UVisexMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#ifndef VISEX_BUILD
#define VISEX_BUILD 0
#endif
	// the name carries the build: commits on the branch it was built from
	Title = FText::FromString(FString::Printf(TEXT("VIS/EX-%04d"), VISEX_BUILD));
	Items = {
		FText::FromString(TEXT("Start")),
		FText::FromString(TEXT("Body")),
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
	// a layout scale (not a render transform), so text is rasterised at the larger size
	UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("Supersample"));
	Scale->SetStretch(EStretch::UserSpecified);
	Scale->SetUserSpecifiedScale(FMath::Max(1.f, Supersample));
	WidgetTree->RootWidget = Scale;
	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Layers"));
	if (UScaleBoxSlot* Inner = Cast<UScaleBoxSlot>(Scale->AddChild(Layers)))
	{
		Inner->SetHorizontalAlignment(HAlign_Fill);
		Inner->SetVerticalAlignment(VAlign_Fill);
	}
	// the shade: a feathered dark pool under the text, filling the padded panel
	if (UMaterialInterface* ShadeMat = ShadeMaterial.LoadSynchronous())
	{
		UImage* Shade = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Shade"));
		Shade->SetBrushFromMaterial(ShadeMat);
		if (UOverlaySlot* ShadeSlot = Layers->AddChildToOverlay(Shade))
		{
			ShadeSlot->SetHorizontalAlignment(HAlign_Fill);
			ShadeSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}
	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
	if (UOverlaySlot* TextSlot = Layers->AddChildToOverlay(Root))
	{
		TextSlot->SetHorizontalAlignment(HAlign_Left);
		TextSlot->SetVerticalAlignment(VAlign_Top);
		TextSlot->SetPadding(FMargin(ShadePadding));
	}

	// logo, then (if set) a tagline in wide-spaced small caps under it
	Root->AddChildToVerticalBox(MakeText(Title, TitleSize, TEXT("Light"), TitleLetterSpacing, TextColor));
	if (!Tagline.IsEmpty())
	{
		UVerticalBoxSlot* TagSlot = Root->AddChildToVerticalBox(
			MakeText(Tagline, TaglineSize, TEXT("Regular"), 600, AccentColor));
		TagSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
	}

	USpacer* Gap = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	Gap->SetSize(FVector2D(1.f, 110.f));
	Root->AddChildToVerticalBox(Gap);

	Labels.Reset();
	Markers.Reset();
	for (const FText& Item : Items)
	{
		// no marker: the selected item is simply larger and full white (SetSelected)
		UTextBlock* Label = MakeText(Item, ItemSize, TEXT("Light"), 160, TextColor);
		UVerticalBoxSlot* RowSlot = Root->AddChildToVerticalBox(Label);
		RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));
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
		FSlateFontInfo Font = Labels[i]->GetFont();
		Font.Size = bOn ? FMath::RoundToInt(ItemSize * SelectedScale) : ItemSize;
		Font.TypefaceFontName = bOn ? FName(TEXT("Regular")) : FName(TEXT("Light"));
		Labels[i]->SetFont(Font);
	}
	if (Previous != Selected)
	{
		OnSelectionChanged(Selected);
	}
}
