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
	if (!GConfig)
	{
		return false;
	}
	// a packaged game's own save first (see Save), then the default that shipped in DefaultGame.ini
	const bool bFound = (!WITH_EDITOR && GConfig->GetString(Section, Key, Text, GGameUserSettingsIni) && !Text.IsEmpty())
		|| (GConfig->GetString(Section, Key, Text, GGameIni) && !Text.IsEmpty());
	if (!bFound)
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
#if WITH_EDITOR
	// this session reads it back at once
	GConfig->SetString(Section, Key, *Text, GGameIni);
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
	// a packaged game: the player's Saved/Config/<Platform>/GameUserSettings.ini. Not Game.ini:
	// a saved Game.ini only keeps the sections its [SectionsToSave] lists (Base.ini sets
	// bCanSaveAllSections=false), so [VisexMenuLayout] was dropped on flush and the layout was
	// gone on relaunch. User files save every section, as the Settings page's [VisEx] does.
	GConfig->SetString(Section, Key, *Text, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	return SavedFile();
#endif
}

FString FVisexMenuLayout::SavedFile()
{
#if WITH_EDITOR
	return FPaths::ProjectConfigDir() / TEXT("DefaultGame.ini");
#else
	const FConfigBranch* Branch = GConfig ? GConfig->FindBranch(FName(*GGameUserSettingsIni), GGameUserSettingsIni) : nullptr;
	return Branch ? Branch->IniPath : FString();
#endif
}

bool FVisexMenuLayout::IsSavedOnDisk()
{
	FString Ini;
	return FFileHelper::LoadFileToString(Ini, *SavedFile())
		&& Ini.Contains(FString::Printf(TEXT("[%s]"), Section)) && Ini.Contains(FString::Printf(TEXT("%s="), Key));
}
