#include "VisexBodyWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "VisexBody"

UVisexBodyWidget::UVisexBodyWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Title = LOCTEXT("Title", "CHARACTER");
	Hint = LOCTEXT("Hint", "L1 R1  tab      UP DOWN  choose      LEFT RIGHT  shape      RIGHT STICK  turn\nCROSS  apply      TRIANGLE  reset      R3  face me      CIRCLE  back");
}

int32 UVisexBodyWidget::RowCount(int32 Tab)
{
	return (Tab == 0 ? FVisexBodyProfile::Sliders().Num() : FVisexBodyProfile::Outfits().Num()) + 1;
}

TSharedRef<SWidget> UVisexBodyWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Build();
	}
	return Super::RebuildWidget();
}

UTextBlock* UVisexBodyWidget::MakeText(const FText& Text, int32 Size, FName Typeface, int32 Spacing, const FLinearColor& Color)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Block->SetText(Text);
	FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(Typeface, Size);
	Font.LetterSpacing = Spacing;
	Block->SetFont(Font);
	Block->SetColorAndOpacity(FSlateColor(Color));
	return Block;
}

UVisexBodyWidget::FRow UVisexBodyWidget::MakeRow(const FText& Label, bool bTrack)
{
	FRow Row;
	Row.Box = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Row.Marker = MakeText(FText::FromString(TEXT("—")), RowSize, TEXT("Light"), 0, AccentColor);
	Row.Label = MakeText(Label, RowSize, TEXT("Light"), 140, TextColor);
	Row.Value = MakeText(FText::GetEmpty(), RowSize - 4, TEXT("Light"), 80, TextColor);

	UHorizontalBoxSlot* M = Row.Box->AddChildToHorizontalBox(Row.Marker);
	M->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
	M->SetVerticalAlignment(VAlign_Center);

	USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	LabelBox->SetWidthOverride(bTrack ? 250.f : 420.f);
	LabelBox->AddChild(Row.Label);
	Row.Box->AddChildToHorizontalBox(LabelBox)->SetVerticalAlignment(VAlign_Center);
	Keep.Append({Row.Box, Row.Marker, Row.Label, Row.Value, LabelBox});

	if (bTrack)
	{
		// the track: a filled part and the rest, 3 px tall
		USizeBox* TrackBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		TrackBox->SetWidthOverride(TrackWidth);
		TrackBox->SetHeightOverride(3.f);
		UHorizontalBox* Track = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Row.Fill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Row.Rest = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Row.Fill->SetBrushColor(AccentColor);
		Row.Rest->SetBrushColor(TextColor * FLinearColor(1.f, 1.f, 1.f, 0.25f));
		Track->AddChildToHorizontalBox(Row.Fill)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Track->AddChildToHorizontalBox(Row.Rest)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TrackBox->AddChild(Track);
		UHorizontalBoxSlot* T = Row.Box->AddChildToHorizontalBox(TrackBox);
		T->SetVerticalAlignment(VAlign_Center);
		T->SetPadding(FMargin(0.f, 0.f, 24.f, 0.f));
		Keep.Append({Row.Fill, Row.Rest, TrackBox, Track});
	}
	Row.Box->AddChildToHorizontalBox(Row.Value)->SetVerticalAlignment(VAlign_Center);
	return Row;
}

void UVisexBodyWidget::Build()
{
	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;
	Root->AddChildToVerticalBox(MakeText(Title, TitleSize, TEXT("Light"), 260, TextColor));

	// tabs, left to right
	UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	const FText TabNames[2] = {LOCTEXT("TabBody", "BODY"), LOCTEXT("TabDress", "DRESS")};
	for (int32 t = 0; t < 2; ++t)
	{
		TabText[t] = MakeText(TabNames[t], TabSize, TEXT("Regular"), 320, TextColor);
		Tabs->AddChildToHorizontalBox(TabText[t])->SetPadding(FMargin(0.f, 0.f, 70.f, 0.f));
		Keep.Add(TabText[t]);
	}
	Root->AddChildToVerticalBox(Tabs)->SetPadding(FMargin(4.f, 18.f, 0.f, 26.f));
	Keep.Add(Tabs);

	const FText ApplyText = LOCTEXT("Apply", "APPLY");
	BodyRows.Reset();
	DressRows.Reset();
	BodyList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	DressList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Keep.Append({BodyList, DressList});
	for (const FVisexBodySlider& S : FVisexBodyProfile::Sliders())
	{
		BodyRows.Add(MakeRow(S.Label, true));
	}
	for (const FVisexOutfit& O : FVisexBodyProfile::Outfits())
	{
		DressRows.Add(MakeRow(O.Label, false));
	}
	BodyRows.Add(MakeRow(ApplyText, false));
	DressRows.Add(MakeRow(ApplyText, false));
	for (int32 i = 0; i < BodyRows.Num(); ++i)
	{
		const bool bLast = i == BodyRows.Num() - 1;
		BodyList->AddChildToVerticalBox(BodyRows[i].Box)->SetPadding(FMargin(0.f, bLast ? 18.f : 0.f, 0.f, 8.f));
	}
	for (int32 i = 0; i < DressRows.Num(); ++i)
	{
		const bool bLast = i == DressRows.Num() - 1;
		DressList->AddChildToVerticalBox(DressRows[i].Box)->SetPadding(FMargin(0.f, bLast ? 22.f : 0.f, 0.f, 14.f));
	}
	Root->AddChildToVerticalBox(BodyList);
	Root->AddChildToVerticalBox(DressList);

	USpacer* Gap = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	Gap->SetSize(FVector2D(1.f, 30.f));
	Root->AddChildToVerticalBox(Gap);
	Root->AddChildToVerticalBox(MakeText(Hint, 15, TEXT("Regular"), 300, AccentColor));
}

void UVisexBodyWidget::Show(const FVisexCharacterView& View)
{
	if (BodyRows.Num() == 0 && WidgetTree && !WidgetTree->RootWidget)
	{
		Build();
	}
	if (!View.Shown || !View.Applied || BodyRows.Num() == 0)
	{
		return;
	}
	for (int32 t = 0; t < 2; ++t)
	{
		TabText[t]->SetRenderOpacity(View.Tab == t ? 1.f : IdleOpacity);
		TabText[t]->SetColorAndOpacity(FSlateColor(View.Tab == t ? AccentColor : TextColor));
	}
	BodyList->SetVisibility(View.Tab == 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	DressList->SetVisibility(View.Tab == 1 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	auto Mark = [&](FRow& R, bool bOn)
	{
		R.Label->SetRenderOpacity(bOn ? 1.f : IdleOpacity);
		R.Value->SetRenderOpacity(bOn ? 1.f : IdleOpacity);
		R.Marker->SetVisibility(bOn ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	};

	const TArray<FVisexBodySlider>& Sliders = FVisexBodyProfile::Sliders();
	for (int32 i = 0; i < Sliders.Num(); ++i)
	{
		const FVisexBodySlider& S = Sliders[i];
		const float V = View.Shown->Get(S.Name);
		const float T = FMath::Clamp((V - S.Min) / FMath::Max(S.Max - S.Min, KINDA_SMALL_NUMBER), 0.f, 1.f);
		FRow& R = BodyRows[i];
		if (UHorizontalBoxSlot* F = Cast<UHorizontalBoxSlot>(R.Fill->Slot))
		{
			FSlateChildSize Size(ESlateSizeRule::Fill);
			Size.Value = FMath::Max(T, 0.001f);
			F->SetSize(Size);
		}
		if (UHorizontalBoxSlot* Rr = Cast<UHorizontalBoxSlot>(R.Rest->Slot))
		{
			FSlateChildSize Size(ESlateSizeRule::Fill);
			Size.Value = FMath::Max(1.f - T, 0.001f);
			Rr->SetSize(Size);
		}
		if (S.bSkinTone)
		{
			R.Value->SetText(FVisexBodyProfile::SkinToneName(V));
			R.Fill->SetBrushColor(FVisexBodyProfile::SkinColor(V) * 1.6f);
		}
		else if (S.bAbs)
		{
			R.Value->SetText(FVisexBodyProfile::AbsName(V));
		}
		else
		{
			R.Value->SetText(FText::FromString(S.bCentimetres
				? FString::Printf(TEXT("%d cm"), FMath::RoundToInt(V))
				: FString::Printf(TEXT("%d"), FMath::RoundToInt(T * 100.f))));
		}
		Mark(R, View.Tab == 0 && View.Selected == i);
	}

	const TArray<FVisexOutfit>& Outfits = FVisexBodyProfile::Outfits();
	const int32 Worn = FVisexBodyProfile::OutfitIndex(View.Applied->Outfit);
	const int32 Trying = FVisexBodyProfile::OutfitIndex(View.Shown->Outfit);
	for (int32 i = 0; i < Outfits.Num(); ++i)
	{
		FRow& R = DressRows[i];
		R.Value->SetText(i == Worn ? LOCTEXT("Worn", "WORN") : (i == Trying ? LOCTEXT("Trying", "TRYING ON") : FText::GetEmpty()));
		Mark(R, View.Tab == 1 && View.Selected == i);
	}

	// APPLY rows: lit when there is something to apply
	FRow* Apply[2] = {&BodyRows.Last(), &DressRows.Last()};
	const bool bDirty[2] = {View.bBodyDirty, View.bDressDirty};
	for (int32 t = 0; t < 2; ++t)
	{
		Apply[t]->Label->SetColorAndOpacity(FSlateColor(bDirty[t] ? AccentColor : TextColor));
		Apply[t]->Value->SetText(bDirty[t] ? FText::GetEmpty() : LOCTEXT("Applied", "applied"));
		Mark(*Apply[t], View.Tab == t && View.Selected == RowCount(t) - 1);
	}
}

#undef LOCTEXT_NAMESPACE
