#include "VisexMenuLayout.h"

#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

namespace
{
	const TCHAR* Section = TEXT("VisexMenuLayout");
	const TCHAR* Key = TEXT("Layout");
}

FString FVisexMenuLayout::LogoText() const
{
#ifndef VISEX_BUILD
#define VISEX_BUILD 0
#endif
	return bBuildNumber ? FString::Printf(TEXT("%s-%04d"), *Logo, VISEX_BUILD) : Logo;
}

bool FVisexMenuLayout::Load(FVisexMenuLayout& InOut)
{
	FString Text;
	if (!GConfig || !GConfig->GetString(Section, Key, Text, GGameIni) || Text.IsEmpty())
	{
		return false;
	}
	// fields missing from an older line keep what InOut had
	return StaticStruct()->ImportText(*Text, &InOut, nullptr, PPF_None, GLog, TEXT("VisexMenuLayout")) != nullptr;
}

FString FVisexMenuLayout::Save(const FVisexMenuLayout& Layout)
{
	FString Text;
	StaticStruct()->ExportText(Text, &Layout, nullptr, nullptr, PPF_None, nullptr);
	if (!GConfig)
	{
		return FString();
	}
	// this session reads it back at once
	GConfig->SetString(Section, Key, *Text, GGameIni);
	GConfig->Flush(false, GGameIni);
#if WITH_EDITOR
	// from the editor: into the project's defaults, so the next build ships with it. Edited as
	// text, so the rest of DefaultGame.ini stays exactly as it is
	const FString Default = FPaths::ProjectConfigDir() / TEXT("DefaultGame.ini");
	FString Ini;
	FFileHelper::LoadFileToString(Ini, *Default);
	const FString Line = FString::Printf(TEXT("%s=%s"), Key, *Text);
	const FString Header = FString::Printf(TEXT("[%s]"), Section);
	const int32 At = Ini.Find(Header);
	if (At == INDEX_NONE)
	{
		Ini += FString::Printf(TEXT("%s\n%s\n%s\n"), Ini.EndsWith(TEXT("\n")) || Ini.IsEmpty() ? TEXT("") : TEXT("\n"), *Header, *Line);
	}
	else
	{
		const int32 LineStart = Ini.Find(FString(Key) + TEXT("="), ESearchCase::CaseSensitive, ESearchDir::FromStart, At);
		const int32 NextSection = Ini.Find(TEXT("\n["), ESearchCase::CaseSensitive, ESearchDir::FromStart, At + Header.Len());
		if (LineStart != INDEX_NONE && (NextSection == INDEX_NONE || LineStart < NextSection))
		{
			int32 LineEnd = Ini.Find(TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, LineStart);
			LineEnd = LineEnd == INDEX_NONE ? Ini.Len() : LineEnd;
			Ini = Ini.Left(LineStart) + Line + Ini.Mid(LineEnd);
		}
		else
		{
			const int32 After = At + Header.Len();
			Ini = Ini.Left(After) + TEXT("\n") + Line + Ini.Mid(After);
		}
	}
	FFileHelper::SaveStringToFile(Ini, *Default);
	return Default;
#else
	return GGameIni;
#endif
}
