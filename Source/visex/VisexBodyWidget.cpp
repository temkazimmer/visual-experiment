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

UVisexBodyWidget::UVisexBodyWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Title = FText::FromString(TEXT("BODY"));
	Hint = FText::FromString(TEXT("UP DOWN  choose      LEFT RIGHT  shape      TRIANGLE  reset      CIRCLE  done"));
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

void UVisexBodyWidget::Build()
{
	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;
	Root->AddChildToVerticalBox(MakeText(Title, TitleSize, TEXT("Light"), 420, TextColor));
	USpacer* Gap = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	Gap->SetSize(FVector2D(1.f, 40.f));
	Root->AddChildToVerticalBox(Gap);

	Rows.Reset();
	Keep.Reset();
	for (const FVisexBodySlider& S : FVisexBodyProfile::Sliders())
	{
		FRow Row;
		Row.Box = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Row.Marker = MakeText(FText::FromString(TEXT("—")), RowSize, TEXT("Light"), 0, AccentColor);
		Row.Label = MakeText(S.Label, RowSize, TEXT("Light"), 140, TextColor);
		Row.Value = MakeText(FText::GetEmpty(), RowSize - 4, TEXT("Light"), 80, TextColor);

		UHorizontalBoxSlot* M = Row.Box->AddChildToHorizontalBox(Row.Marker);
		M->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
		M->SetVerticalAlignment(VAlign_Center);

		USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		LabelBox->SetWidthOverride(250.f);
		LabelBox->AddChild(Row.Label);
		Row.Box->AddChildToHorizontalBox(LabelBox)->SetVerticalAlignment(VAlign_Center);

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

		Row.Box->AddChildToHorizontalBox(Row.Value)->SetVerticalAlignment(VAlign_Center);
		UVerticalBoxSlot* RowSlot = Root->AddChildToVerticalBox(Row.Box);
		RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
		Keep.Append({Row.Box, Row.Marker, Row.Label, Row.Fill, Row.Rest, Row.Value, LabelBox, TrackBox, Track});
		Rows.Add(Row);
	}

	USpacer* Gap2 = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	Gap2->SetSize(FVector2D(1.f, 30.f));
	Root->AddChildToVerticalBox(Gap2);
	UTextBlock* HintText = MakeText(Hint, 15, TEXT("Regular"), 300, AccentColor);
	Root->AddChildToVerticalBox(HintText);
}

void UVisexBodyWidget::Show(const FVisexBodyProfile& Profile, int32 Selected)
{
	if (Rows.Num() == 0 && WidgetTree && !WidgetTree->RootWidget)
	{
		Build();
	}
	const TArray<FVisexBodySlider>& Sliders = FVisexBodyProfile::Sliders();
	for (int32 i = 0; i < Rows.Num() && i < Sliders.Num(); ++i)
	{
		const FVisexBodySlider& S = Sliders[i];
		const float V = Profile.Get(S.Name);
		const float T = FMath::Clamp((V - S.Min) / FMath::Max(S.Max - S.Min, KINDA_SMALL_NUMBER), 0.f, 1.f);
		FRow& R = Rows[i];
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
		const FString Text = S.bCentimetres
			? FString::Printf(TEXT("%d cm"), FMath::RoundToInt(V))
			: FString::Printf(TEXT("%d"), FMath::RoundToInt(T * 100.f));
		R.Value->SetText(FText::FromString(Text));
		const bool bOn = i == Selected;
		R.Label->SetRenderOpacity(bOn ? 1.f : IdleOpacity);
		R.Value->SetRenderOpacity(bOn ? 1.f : IdleOpacity);
		R.Marker->SetVisibility(bOn ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}
