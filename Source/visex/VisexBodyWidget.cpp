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
	Hint = LOCTEXT("Hint", "L1 R1  tab      UP DOWN  choose      LEFT RIGHT  shape      RIGHT STICK  turn\nTRIANGLE  reset      CIRCLE  done, keeps her");
	WardrobeTitle = LOCTEXT("WardrobeTitle", "WARDROBE");
	WardrobeHint = LOCTEXT("WardrobeHint", "UP DOWN  try on      CROSS  wear      RIGHT STICK  turn\nCIRCLE  done, keeps her");
}

int32 UVisexBodyWidget::RowCount(int32 Page, int32 Tab)
{
	if (Page == 1)
	{
		return FVisexBodyProfile::Outfits().Num() + 1;
	}
	return Tab == 2 ? FVisexBodyProfile::HairStyles().Num() : FVisexBodyProfile::SliderRows(Tab).Num() + 1;
}

UTextBlock* UVisexBodyWidget::MakeNote(const FText& Text)
{
	UTextBlock* Note = MakeText(Text, RowSize - 6, TEXT("Light"), 120, TextColor);
	Note->SetRenderOpacity(IdleOpacity);
	Keep.Add(Note);
	return Note;
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
	LabelBox->SetWidthOverride(bTrack ? 250.f : 500.f);
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
	TitleText = MakeText(Title, TitleSize, TEXT("Light"), 260, TextColor);
	Root->AddChildToVerticalBox(TitleText);
	Keep.Add(TitleText);

	// Body page tabs, left to right (the wardrobe has none)
	TabBar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	const FText TabNames[BodyTabs] = {LOCTEXT("TabBody", "BODY"), LOCTEXT("TabFace", "FACE"), LOCTEXT("TabHair", "HAIR")};
	for (int32 t = 0; t < BodyTabs; ++t)
	{
		TabText[t] = MakeText(TabNames[t], TabSize, TEXT("Regular"), 320, TextColor);
		TabBar->AddChildToHorizontalBox(TabText[t])->SetPadding(FMargin(0.f, 0.f, 70.f, 0.f));
		Keep.Add(TabText[t]);
	}
	Root->AddChildToVerticalBox(TabBar)->SetPadding(FMargin(4.f, 18.f, 0.f, 26.f));
	Keep.Add(TabBar);
	// the wardrobe has no tabs: the same breathing room under its title
	TitleGap = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	TitleGap->SetSize(FVector2D(1.f, 40.f));
	Root->AddChildToVerticalBox(TitleGap);
	Keep.Add(TitleGap);

	const FText ApplyText = LOCTEXT("Apply", "APPLY");
	for (int32 l = 0; l < 4; ++l)
	{
		Lists[l] = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Keep.Add(Lists[l]);
	}
	auto Fill = [&](TArray<FRow>& Rows, UVerticalBox* List, float Gap, float ApplyGap, bool bApply)
	{
		if (bApply)
		{
			Rows.Add(MakeRow(ApplyText, false));
		}
		for (int32 i = 0; i < Rows.Num(); ++i)
		{
			const bool bLast = bApply && i == Rows.Num() - 1;
			List->AddChildToVerticalBox(Rows[i].Box)->SetPadding(FMargin(0.f, bLast ? ApplyGap : 0.f, 0.f, Gap));
		}
	};
	BodyRows.Reset(); FaceRows.Reset(); HairRows.Reset(); DressRows.Reset();
	for (const int32 i : FVisexBodyProfile::SliderRows(0)) { BodyRows.Add(MakeRow(FVisexBodyProfile::Sliders()[i].Label, true)); }
	for (const int32 i : FVisexBodyProfile::SliderRows(1)) { FaceRows.Add(MakeRow(FVisexBodyProfile::Sliders()[i].Label, true)); }
	for (const FText& Style : FVisexBodyProfile::HairStyles()) { HairRows.Add(MakeRow(Style, false)); }
	for (const FVisexOutfit& O : FVisexBodyProfile::Outfits()) { DressRows.Add(MakeRow(O.Label, false)); }
	Fill(BodyRows, Lists[0], 4.f, 14.f, true);
	Fill(FaceRows, Lists[1], 4.f, 14.f, true);
	Fill(HairRows, Lists[2], 14.f, 0.f, false);
	Fill(DressRows, Lists[3], 14.f, 22.f, true);
	// room to grow: the face and hair tabs say what comes next
	Lists[1]->AddChildToVerticalBox(MakeNote(LOCTEXT("FaceSoon", "More face shaping is on its way.")))->SetPadding(FMargin(40.f, 30.f, 0.f, 0.f));
	Lists[2]->AddChildToVerticalBox(MakeNote(LOCTEXT("HairSoon", "More hairstyles are on their way.")))->SetPadding(FMargin(40.f, 30.f, 0.f, 0.f));
	for (int32 l = 0; l < 4; ++l)
	{
		Root->AddChildToVerticalBox(Lists[l]);
	}

	// the "under construction" window, in place of the list while it is up
	Popup = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Keep.Add(Popup);
	UTextBlock* PopTitle = MakeText(LOCTEXT("SoonTitle", "UNDER CONSTRUCTION"), TabSize, TEXT("Regular"), 320, AccentColor);
	UTextBlock* PopText = MakeText(LOCTEXT("SoonText", "This costume isn't in the wardrobe yet.\nIt will unlock in a later version."), RowSize, TEXT("Light"), 120, TextColor);
	UTextBlock* PopBack = MakeText(LOCTEXT("SoonBack", "CROSS  or  CIRCLE   back"), 15, TEXT("Regular"), 300, AccentColor);
	Keep.Append({PopTitle, PopText, PopBack});
	Popup->AddChildToVerticalBox(PopTitle)->SetPadding(FMargin(0.f, 30.f, 0.f, 24.f));
	Popup->AddChildToVerticalBox(PopText)->SetPadding(FMargin(0.f, 0.f, 0.f, 30.f));
	Popup->AddChildToVerticalBox(PopBack);
	Popup->SetVisibility(ESlateVisibility::Collapsed);
	Root->AddChildToVerticalBox(Popup);

	USpacer* Gap = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	Gap->SetSize(FVector2D(1.f, 30.f));
	Root->AddChildToVerticalBox(Gap);
	HintText = MakeText(Hint, 15, TEXT("Regular"), 300, AccentColor);
	Root->AddChildToVerticalBox(HintText);
	Keep.Add(HintText);
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
	const bool bWardrobe = View.Page == 1;
	TitleText->SetText(bWardrobe ? WardrobeTitle : Title);
	HintText->SetText(bWardrobe ? WardrobeHint : Hint);
	TabBar->SetVisibility(bWardrobe ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	TitleGap->SetVisibility(bWardrobe ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	for (int32 t = 0; t < BodyTabs; ++t)
	{
		TabText[t]->SetRenderOpacity(View.Tab == t ? 1.f : IdleOpacity);
		TabText[t]->SetColorAndOpacity(FSlateColor(View.Tab == t ? AccentColor : TextColor));
	}
	const int32 Shown = bWardrobe ? 3 : FMath::Clamp(View.Tab, 0, 2);
	for (int32 l = 0; l < 4; ++l)
	{
		const bool bOn = l == Shown && !View.bUnderConstruction;
		Lists[l]->SetVisibility(bOn ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	Popup->SetVisibility(View.bUnderConstruction ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	auto Mark = [&](FRow& R, bool bOn)
	{
		R.Label->SetRenderOpacity(bOn ? 1.f : IdleOpacity);
		R.Value->SetRenderOpacity(bOn ? 1.f : IdleOpacity);
		R.Marker->SetVisibility(bOn ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	};
	const int32 Here = View.Selected;

	// sliders, both tabs
	const TArray<FVisexBodySlider>& Sliders = FVisexBodyProfile::Sliders();
	TArray<FRow>* SliderLists[2] = {&BodyRows, &FaceRows};
	for (int32 g = 0; g < 2; ++g)
	{
		const TArray<int32>& Rows = FVisexBodyProfile::SliderRows(g);
		for (int32 r = 0; r < Rows.Num(); ++r)
		{
			const FVisexBodySlider& S = Sliders[Rows[r]];
			const float V = View.Shown->Get(S.Name);
			const float T = FMath::Clamp((V - S.Min) / FMath::Max(S.Max - S.Min, KINDA_SMALL_NUMBER), 0.f, 1.f);
			FRow& R = (*SliderLists[g])[r];
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
			Mark(R, !bWardrobe && View.Tab == g && Here == r);
		}
	}

	// hairstyles: one for now, the one she has
	for (int32 i = 0; i < HairRows.Num(); ++i)
	{
		HairRows[i].Value->SetText(i == 0 ? LOCTEXT("Worn", "WORN") : FText::GetEmpty());
		Mark(HairRows[i], !bWardrobe && View.Tab == 2 && Here == i);
	}

	// costumes
	const TArray<FVisexOutfit>& Outfits = FVisexBodyProfile::Outfits();
	const int32 Worn = FVisexBodyProfile::OutfitIndex(View.Applied->Outfit);
	const int32 Trying = FVisexBodyProfile::OutfitIndex(View.Shown->Outfit);
	for (int32 i = 0; i < Outfits.Num(); ++i)
	{
		FRow& R = DressRows[i];
		R.Value->SetText(Outfits[i].bLocked ? LOCTEXT("Locked", "LOCKED")
			: i == Worn ? LOCTEXT("Worn", "WORN") : (i == Trying ? LOCTEXT("Trying", "TRYING ON") : FText::GetEmpty()));
		Mark(R, bWardrobe && Here == i);
		if (Outfits[i].bLocked && !(bWardrobe && Here == i))
		{
			R.Label->SetRenderOpacity(IdleOpacity * 0.8f);   // readable, a step back from the rest
			R.Value->SetRenderOpacity(IdleOpacity * 0.8f);
		}
	}

	// APPLY rows: lit when there is something to apply
	FRow* Apply[3] = {&BodyRows.Last(), &FaceRows.Last(), &DressRows.Last()};
	const bool bDirty[3] = {View.bBodyDirty, View.bBodyDirty, View.bDressDirty};
	const bool bHere[3] = {
		!bWardrobe && View.Tab == 0 && Here == BodyRows.Num() - 1,
		!bWardrobe && View.Tab == 1 && Here == FaceRows.Num() - 1,
		bWardrobe && Here == DressRows.Num() - 1};
	for (int32 t = 0; t < 3; ++t)
	{
		Apply[t]->Label->SetColorAndOpacity(FSlateColor(bDirty[t] ? AccentColor : TextColor));
		Apply[t]->Value->SetText(bDirty[t] ? FText::GetEmpty() : LOCTEXT("Applied", "applied"));
		Mark(*Apply[t], bHere[t]);
	}
}

#undef LOCTEXT_NAMESPACE
