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
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"

UVisexMenuWidget::UVisexMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#ifndef VISEX_BUILD
#define VISEX_BUILD 0
#endif
	// the name carries the build: commits on the branch it was built from
	Title = FText::FromString(FString::Printf(TEXT("VIS/EX-%04d"), VISEX_BUILD));
	BuildLabel = FText::FromString(FString::Printf(TEXT("%04d"), VISEX_BUILD));
	Items = {
		FText::FromString(TEXT("Start")),
		FText::FromString(TEXT("Body")),
		FText::FromString(TEXT("Wardrobe")),
		FText::FromString(TEXT("Photo mode")),
		FText::FromString(TEXT("Settings")),
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

void UVisexMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!Root || Root->GetChildrenCount() == 0)
	{
		return;
	}
	// a render translation per row (not layout), so it follows font changes on selection
	const FGeometry& RootGeo = Root->GetCachedGeometry();
	auto CentreY = [&RootGeo](const UWidget* C)
	{
		const FGeometry& G = C->GetCachedGeometry();
		return RootGeo.AbsoluteToLocal(G.GetAbsolutePositionAtCoordinates(FVector2D(0.5f, 0.5f))).Y;
	};
	AlignY0 = CentreY(Root->GetChildAt(0));
	for (UWidget* C : Root->GetAllChildren())
	{
		const float Dx = AlignSlope * (CentreY(C) - AlignY0);
		if (!FMath::IsNearlyEqual(C->GetRenderTransform().Translation.X, Dx, 0.05f))
		{
			C->SetRenderTranslation(FVector2D(Dx, 0.f));
			bAlignChanged = true;
		}
	}
}

UTextBlock* UVisexMenuWidget::MakeText(const FText& Text, int32 Size, FName Typeface, int32 Spacing, const FLinearColor& Color)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Block->SetText(Text);
	FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(Typeface, Size);
	Font.LetterSpacing = Spacing;
	Block->SetFont(Font);
	Block->SetColorAndOpacity(FSlateColor(Color));
	// a soft dark shadow under every line, so the white reads on the bright haze (Tema)
	Block->SetShadowOffset(ShadowOffset);
	Block->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, ShadowOpacity));
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
	Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
	if (UOverlaySlot* TextSlot = Layers->AddChildToOverlay(Root))
	{
		TextSlot->SetHorizontalAlignment(HAlign_Left);
		TextSlot->SetVerticalAlignment(VAlign_Top);
		TextSlot->SetPadding(FMargin(ShadePadding));
	}
	BuildContent();
}

bool UVisexMenuWidget::HasValues() const
{
	return Values.ContainsByPredicate([](const FText& V) { return !V.IsEmpty(); });
}

FText UVisexMenuWidget::ValueText(int32 Index, bool bSelected) const
{
	if (!Values.IsValidIndex(Index) || Values[Index].IsEmpty())
	{
		return FText::GetEmpty();
	}
	// the selected row shows it can be stepped
	return bSelected ? FText::Format(FText::FromString(TEXT("\u2039  {0}  \u203A")), Values[Index]) : Values[Index];
}

void UVisexMenuWidget::BuildContent()
{
	if (!Root)
	{
		return;
	}
	Root->ClearChildren();

	// logo, then (if set) a tagline in wide-spaced small caps under it
	// the main page shows the logo image (the build small under it); other pages their title
	UTexture2D* Logo = Tabs.Num() == 0 ? LogoTexture.LoadSynchronous() : nullptr;
	if (Logo)
	{
		const float H = TitleSize * LogoHeightPerSize;
		const float W = H * Logo->GetSizeX() / FMath::Max(1, Logo->GetSizeY());
		UImage* LogoImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		LogoImage->SetBrushFromTexture(Logo);
		FSlateBrush LogoBrush = LogoImage->GetBrush();
		LogoBrush.ImageSize = FVector2D(W, H);
		LogoImage->SetBrush(LogoBrush);
		LogoImage->SetColorAndOpacity(LogoTint);
		if (UVerticalBoxSlot* LogoSlot = Root->AddChildToVerticalBox(LogoImage))
		{
			LogoSlot->SetHorizontalAlignment(HAlign_Left);
			// the letters start LogoInset inside the image: pull it left so they sit on the items' edge
			LogoSlot->SetPadding(FMargin(-W * LogoInset, 0.f, 0.f, 0.f));
		}
		if (!BuildLabel.IsEmpty())
		{
			UTextBlock* Build = MakeText(BuildLabel, BuildLabelSize, TEXT("Regular"), 400, TextColor);
			Build->SetColorAndOpacity(FSlateColor((TextColor * 0.3f).CopyWithNewOpacity(BuildLabelOpacity)));
			Root->AddChildToVerticalBox(Build)->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}
	}
	else
	{
		Root->AddChildToVerticalBox(MakeText(Title, TitleSize, TEXT("Light"), TitleLetterSpacing, TextColor));
	}
	if (!Tagline.IsEmpty())
	{
		UVerticalBoxSlot* TagSlot = Root->AddChildToVerticalBox(
			MakeText(Tagline, TaglineSize, TEXT("Regular"), 600, AccentColor));
		TagSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
	}

	const bool bValues = HasValues();
	USpacer* Gap = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	Gap->SetSize(FVector2D(1.f, Tabs.Num() > 0 ? 50.f : LogoGap));
	Root->AddChildToVerticalBox(Gap);

	if (Tabs.Num() > 0)
	{
		// tabs in a row, the open one full white; L1/R1 (Q/E) switch
		UHorizontalBox* TabRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		for (int32 T = 0; T < Tabs.Num(); ++T)
		{
			UTextBlock* Tab = MakeText(Tabs[T], ValueItemSize, T == ActiveTab ? TEXT("Regular") : TEXT("Light"), 400, TextColor);
			if (T != ActiveTab) { Tab->SetColorAndOpacity(FSlateColor(TextColor.CopyWithNewOpacity(IdleOpacity))); }
			if (UHorizontalBoxSlot* TabSlot = TabRow->AddChildToHorizontalBox(Tab))
			{
				TabSlot->SetPadding(FMargin(0.f, 0.f, 70.f, 0.f));
			}
		}
		UVerticalBoxSlot* TabsSlot = Root->AddChildToVerticalBox(TabRow);
		TabsSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 46.f));
	}

	Labels.Reset();
	ValueLabels.Reset();
	const int32 Size = bValues ? ValueItemSize : ItemSize;
	for (int32 i = 0; i < Items.Num(); ++i)
	{
		// no marker: the selected item is simply larger and full white (SetSelected)
		UTextBlock* Label = MakeText(Items[i], Size, TEXT("Light"), 160, TextColor);
		Labels.Add(Label);
		UTextBlock* Value = MakeText(ValueText(i, false), Size, TEXT("Light"), 160, TextColor);
		ValueLabels.Add(Value);
		UWidget* Row = Label;
		if (bValues)
		{
			USizeBox* Column = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			Column->SetWidthOverride(ValueColumn);
			Column->AddChild(Label);
			UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			if (UHorizontalBoxSlot* L = Line->AddChildToHorizontalBox(Column)) { L->SetVerticalAlignment(VAlign_Center); }
			if (UHorizontalBoxSlot* V = Line->AddChildToHorizontalBox(Value)) { V->SetVerticalAlignment(VAlign_Center); }
			Row = Line;
		}
		UVerticalBoxSlot* RowSlot = Root->AddChildToVerticalBox(Row);
		RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, bValues ? 10.f : ItemGap));
	}
	SetSelected(Selected);
}

void UVisexMenuWidget::SetPage(const FText& InTitle, const TArray<FText>& InTabs, int32 InActiveTab, const TArray<FText>& InItems, const TArray<FText>& InValues, int32 Select)
{
	Title = InTitle;
	Tabs = InTabs;
	ActiveTab = InActiveTab;
	Items = InItems;
	Values = InValues;
	Selected = Select;
	BuildContent();
}

void UVisexMenuWidget::SetValues(const TArray<FText>& InValues)
{
	Values = InValues;
	for (int32 i = 0; i < ValueLabels.Num(); ++i)
	{
		ValueLabels[i]->SetText(ValueText(i, i == Selected));
	}
}

void UVisexMenuWidget::SetSelected(int32 Index)
{
	if (Items.Num() == 0)
	{
		return;
	}
	const int32 Previous = Selected;
	Selected = (Index % Items.Num() + Items.Num()) % Items.Num();
	const bool bValues = HasValues();
	const int32 Size = bValues ? ValueItemSize : ItemSize;
	const float Scale = bValues ? ValueSelectedScale : SelectedScale;
	for (int32 i = 0; i < Labels.Num(); ++i)
	{
		const bool bOn = i == Selected;
		for (UTextBlock* Block : {Labels[i].Get(), ValueLabels.IsValidIndex(i) ? ValueLabels[i].Get() : nullptr})
		{
			if (!Block) { continue; }
			// dimmed by colour, not transparency: solid text is what the panel material masks out
			// of screen filters (stencil, opacity over 0.9), and over the shade it reads the same
			Block->SetColorAndOpacity(FSlateColor(bOn ? TextColor : TextColor.CopyWithNewOpacity(IdleOpacity)));
			FSlateFontInfo Font = Block->GetFont();
			Font.Size = bOn ? FMath::RoundToInt(Size * Scale) : Size;
			Font.TypefaceFontName = bOn ? FName(TEXT("Regular")) : FName(TEXT("Light"));
			Block->SetFont(Font);
		}
		if (ValueLabels.IsValidIndex(i)) { ValueLabels[i]->SetText(ValueText(i, bOn)); }
	}
	if (Previous != Selected)
	{
		OnSelectionChanged(Selected);
	}
}
