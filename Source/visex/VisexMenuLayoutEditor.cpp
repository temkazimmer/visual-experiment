#include "VisexMenuLayoutEditor.h"

#include "VisexMenuDirector.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	enum ERow : int32
	{
		RowSide, RowHeight, RowDistance, RowYaw, RowPitch, RowRoll, RowScale,
		RowLogoSize, RowLogoSpacing, RowLogoGap, RowItemSize, RowItemGap,
		RowLogo, RowBuild, RowSave, RowReset, RowCount
	};
	const TCHAR* RowNames[RowCount] = {
		TEXT("Left / right"), TEXT("Up / down"), TEXT("Distance"), TEXT("Turn"), TEXT("Tip back"), TEXT("Tilt"), TEXT("Size"),
		TEXT("Logo size"), TEXT("Logo letter spacing"), TEXT("Space under logo"), TEXT("Item size"), TEXT("Space between items"),
		TEXT("Logo"), TEXT("Build number"), TEXT("SAVE AS DEFAULT"), TEXT("RESET TO BUILT-IN"),
	};
}

AVisexMenuLayoutEditor::AVisexMenuLayoutEditor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AVisexMenuLayoutEditor::Toggle(AVisexMenuDirector* InDirector)
{
	Director = InDirector;
	if (bOpen) { Close(); } else { Open(); }
}

void AVisexMenuLayoutEditor::Open()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !Director.IsValid())
	{
		return;
	}
	Edit = Saved = Director->GetLayout();
	bOpen = true;
	bTyping = false;
	Status = TEXT("Changes show at once. SAVE AS DEFAULT keeps them.");
	// pushed after the menu's input, so it is on top: its keys don't reach the menu
	EnableInput(PC);
	BindKeys();
	ShowHud(true);
}

void AVisexMenuLayoutEditor::Close()
{
	bOpen = false;
	bTyping = false;
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		DisableInput(PC);
	}
	ShowHud(false);
}

void AVisexMenuLayoutEditor::EndPlay(const EEndPlayReason::Type Reason)
{
	ShowHud(false);
	Super::EndPlay(Reason);
}

void AVisexMenuLayoutEditor::BindKeys()
{
	if (bKeysBound || !InputComponent)
	{
		return;
	}
	for (const FKey& K : {EKeys::Up, EKeys::W, EKeys::Gamepad_DPad_Up}) { InputComponent->BindKey(K, IE_Pressed, this, &AVisexMenuLayoutEditor::Up); }
	for (const FKey& K : {EKeys::Down, EKeys::S, EKeys::Gamepad_DPad_Down}) { InputComponent->BindKey(K, IE_Pressed, this, &AVisexMenuLayoutEditor::Down); }
	for (const FKey& K : {EKeys::Left, EKeys::A, EKeys::Gamepad_DPad_Left})
	{
		InputComponent->BindKey(K, IE_Pressed, this, &AVisexMenuLayoutEditor::Left);
		InputComponent->BindKey(K, IE_Repeat, this, &AVisexMenuLayoutEditor::Left);
	}
	for (const FKey& K : {EKeys::Right, EKeys::D, EKeys::Gamepad_DPad_Right})
	{
		InputComponent->BindKey(K, IE_Pressed, this, &AVisexMenuLayoutEditor::Right);
		InputComponent->BindKey(K, IE_Repeat, this, &AVisexMenuLayoutEditor::Right);
	}
	for (const FKey& K : {EKeys::Enter, EKeys::Gamepad_FaceButton_Bottom}) { InputComponent->BindKey(K, IE_Pressed, this, &AVisexMenuLayoutEditor::Enter); }
	for (const FKey& K : {EKeys::Escape, EKeys::Section, EKeys::Gamepad_FaceButton_Right}) { InputComponent->BindKey(K, IE_Pressed, this, &AVisexMenuLayoutEditor::Escape); }
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &AVisexMenuLayoutEditor::ResetRow);
	// every other key: typed into the logo while typing, swallowed otherwise (the menu
	// underneath must not move while its layout is being edited)
	InputComponent->BindKey(EKeys::AnyKey, IE_Pressed, this, &AVisexMenuLayoutEditor::AnyKey);
	bKeysBound = true;
}

void AVisexMenuLayoutEditor::Move(int32 Delta)
{
	if (!bOpen || bTyping)
	{
		return;
	}
	Row = (Row + Delta + RowCount) % RowCount;
	RefreshHud();
}

void AVisexMenuLayoutEditor::Push()
{
	if (Director.IsValid())
	{
		Director->ApplyLayout(Edit);
	}
	RefreshHud();
}

void AVisexMenuLayoutEditor::Step(float Dir)
{
	if (!bOpen || bTyping)
	{
		return;
	}
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	const bool bFine = PC && (PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift));
	const float K = Dir * (bFine ? 0.2f : 1.f);
	FVisexMenuLayout& L = Edit;
	switch (Row)
	{
	case RowSide:        L.Offset.Y += K * 1.f; break;
	case RowHeight:      L.Offset.Z += K * 1.f; break;
	case RowDistance:    L.Offset.X = FMath::Max(10.f, L.Offset.X + K * 2.f); break;
	case RowYaw:         L.Yaw += K * 1.f; break;
	case RowPitch:       L.Pitch = FMath::Clamp(L.Pitch + K * 1.f, -60.f, 60.f); break;
	case RowRoll:        L.Roll = FMath::Clamp(L.Roll + K * 0.5f, -45.f, 45.f); break;
	case RowScale:       L.Scale = FMath::Clamp(L.Scale * FMath::Pow(1.03f, K), 0.005f, 0.5f); break;
	case RowLogoSize:    L.LogoSize = FMath::Clamp(L.LogoSize + FMath::RoundToInt(K * 2.f), 20, 300); break;
	case RowLogoSpacing: L.LogoSpacing = FMath::Clamp(L.LogoSpacing + FMath::RoundToInt(K * 10.f), -100, 1000); break;
	case RowLogoGap:     L.LogoGap = FMath::Clamp(L.LogoGap + K * 5.f, 0.f, 600.f); break;
	case RowItemSize:    L.ItemSize = FMath::Clamp(L.ItemSize + FMath::RoundToInt(K * 1.f), 10, 120); break;
	case RowItemGap:     L.ItemGap = FMath::Clamp(L.ItemGap + K * 2.f, -20.f, 200.f); break;
	case RowBuild:       L.bBuildNumber = !L.bBuildNumber; break;
	default: return;
	}
	Push();
}

void AVisexMenuLayoutEditor::Enter()
{
	if (!bOpen)
	{
		return;
	}
	if (bTyping)
	{
		bTyping = false;
		Edit.Logo = Typed.TrimStartAndEnd().IsEmpty() ? Edit.Logo : Typed.TrimStartAndEnd();
		Status = TEXT("Logo set. SAVE AS DEFAULT keeps it.");
		Push();
		return;
	}
	switch (Row)
	{
	case RowLogo:
		bTyping = true;
		Typed = Edit.Logo;
		Status = TEXT("Type the logo. ENTER done, ESC cancel, BACKSPACE delete.");
		RefreshHud();
		break;
	case RowBuild:
		Step(1.f);
		break;
	case RowSave:
	{
		const FString Where = FVisexMenuLayout::Save(Edit);
		Saved = Edit;
		Status = FString::Printf(TEXT("Saved as default: %s"), *FPaths::GetCleanFilename(Where));
		RefreshHud();
		break;
	}
	case RowReset:
		if (Director.IsValid())
		{
			Edit = Director->GetBuiltInLayout();
			Status = TEXT("Back to the built-in layout. SAVE AS DEFAULT keeps it.");
			Push();
		}
		break;
	default:
		break;
	}
}

void AVisexMenuLayoutEditor::Escape()
{
	if (bTyping)
	{
		bTyping = false;
		Status = TEXT("Logo unchanged.");
		RefreshHud();
		return;
	}
	Close();
}

void AVisexMenuLayoutEditor::ResetRow()
{
	if (!bOpen || bTyping)
	{
		return;
	}
	FVisexMenuLayout& L = Edit;
	switch (Row)
	{
	case RowSide:        L.Offset.Y = Saved.Offset.Y; break;
	case RowHeight:      L.Offset.Z = Saved.Offset.Z; break;
	case RowDistance:    L.Offset.X = Saved.Offset.X; break;
	case RowYaw:         L.Yaw = Saved.Yaw; break;
	case RowPitch:       L.Pitch = Saved.Pitch; break;
	case RowRoll:        L.Roll = Saved.Roll; break;
	case RowScale:       L.Scale = Saved.Scale; break;
	case RowLogoSize:    L.LogoSize = Saved.LogoSize; break;
	case RowLogoSpacing: L.LogoSpacing = Saved.LogoSpacing; break;
	case RowLogoGap:     L.LogoGap = Saved.LogoGap; break;
	case RowItemSize:    L.ItemSize = Saved.ItemSize; break;
	case RowItemGap:     L.ItemGap = Saved.ItemGap; break;
	case RowLogo:        L.Logo = Saved.Logo; break;
	case RowBuild:       L.bBuildNumber = Saved.bBuildNumber; break;
	default: return;
	}
	Push();
}

void AVisexMenuLayoutEditor::AnyKey(FKey Key)
{
	if (!bOpen || !bTyping)
	{
		return;   // swallowed: the menu underneath stays put
	}
	if (Key == EKeys::BackSpace)
	{
		Typed.LeftChopInline(1);
		RefreshHud();
		return;
	}
	if (Key == EKeys::Enter || Key == EKeys::Escape || Typed.Len() >= 24)
	{
		return;
	}
	// physical keys, so it types the same on any keyboard layout
	const FString Name = Key.GetFName().ToString();
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	const bool bShift = PC && (PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift));
	static const TMap<FString, TCHAR> Named = {
		{TEXT("Zero"), '0'}, {TEXT("One"), '1'}, {TEXT("Two"), '2'}, {TEXT("Three"), '3'}, {TEXT("Four"), '4'},
		{TEXT("Five"), '5'}, {TEXT("Six"), '6'}, {TEXT("Seven"), '7'}, {TEXT("Eight"), '8'}, {TEXT("Nine"), '9'},
		{TEXT("SpaceBar"), ' '}, {TEXT("Hyphen"), '-'}, {TEXT("Slash"), '/'}, {TEXT("Period"), '.'},
		{TEXT("Backslash"), '\\'}, {TEXT("Semicolon"), ':'}, {TEXT("Apostrophe"), '\''},
	};
	TCHAR C = 0;
	if (Name.Len() == 1 && FChar::IsAlpha(Name[0]))
	{
		C = bShift ? FChar::ToUpper(Name[0]) : FChar::ToLower(Name[0]);
	}
	else if (const TCHAR* Found = Named.Find(Name))
	{
		C = (*Found == '-' && bShift) ? '_' : *Found;
	}
	if (C)
	{
		Typed.AppendChar(C);
		RefreshHud();
	}
}

FString AVisexMenuLayoutEditor::RowValue(int32 R) const
{
	const FVisexMenuLayout& L = Edit;
	switch (R)
	{
	case RowSide:        return FString::Printf(TEXT("%.1f cm"), L.Offset.Y);
	case RowHeight:      return FString::Printf(TEXT("%.1f cm"), L.Offset.Z);
	case RowDistance:    return FString::Printf(TEXT("%.0f cm"), L.Offset.X);
	case RowYaw:         return FString::Printf(TEXT("%.0f°"), L.Yaw);
	case RowPitch:       return FString::Printf(TEXT("%.0f°"), L.Pitch);
	case RowRoll:        return FString::Printf(TEXT("%.1f°"), L.Roll);
	case RowScale:       return FString::Printf(TEXT("%.0f%%"), 100.f * L.Scale / FMath::Max(Director.IsValid() ? Director->GetBuiltInLayout().Scale : 0.048f, 0.0001f));
	case RowLogoSize:    return FString::Printf(TEXT("%d"), L.LogoSize);
	case RowLogoSpacing: return FString::Printf(TEXT("%d"), L.LogoSpacing);
	case RowLogoGap:     return FString::Printf(TEXT("%.0f"), L.LogoGap);
	case RowItemSize:    return FString::Printf(TEXT("%d"), L.ItemSize);
	case RowItemGap:     return FString::Printf(TEXT("%.0f"), L.ItemGap);
	case RowLogo:        return bTyping ? Typed + TEXT("_") : L.Logo;
	case RowBuild:       return L.bBuildNumber ? TEXT("on") : TEXT("off");
	default:             return FString();
	}
}

void AVisexMenuLayoutEditor::ShowHud(bool bShow)
{
	UGameViewportClient* Viewport = GEngine ? GEngine->GameViewport : nullptr;
	if (!Viewport)
	{
		return;
	}
	if (Hud.IsValid())
	{
		Viewport->RemoveViewportWidgetContent(Hud.ToSharedRef());
		Hud.Reset();
		Body.Reset();
	}
	if (!bShow)
	{
		return;
	}
	FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 13);
	Font.LetterSpacing = 60;
	Hud = SNew(SBox)
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.f, 0.f, 60.f, 60.f))
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.72f))
			.Padding(FMargin(26.f, 20.f))
			[
				SAssignNew(Body, STextBlock).Font(Font).LineHeightPercentage(1.35f)
				.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.92f))
			]
		];
	Viewport->AddViewportWidgetContent(Hud.ToSharedRef(), 30);
	RefreshHud();
}

void AVisexMenuLayoutEditor::RefreshHud()
{
	if (!Body.IsValid())
	{
		return;
	}
	FString Text = TEXT("MENU LAYOUT\n\n");
	for (int32 R = 0; R < RowCount; ++R)
	{
		if (R == RowLogoSize || R == RowLogo || R == RowSave)
		{
			Text += TEXT("\n");   // groups: block, spacing, logo, actions
		}
		const FString Value = RowValue(R);
		Text += FString::Printf(TEXT("%s %s%s%s\n"), R == Row ? TEXT("›") : TEXT(" "), RowNames[R],
			Value.IsEmpty() ? TEXT("") : TEXT("   "), *Value);
	}
	Text += TEXT("\n↑ ↓ choose    ← → change (SHIFT fine)    R undo row\nENTER type logo / save / reset    § or ESC close\n\n");
	Text += Status;
	Body->SetText(FText::FromString(Text));
}
