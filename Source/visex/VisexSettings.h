#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * The Settings page's rows and what they do, kept apart from the menu director that shows them.
 * Every change applies at once (the scene behind the panel is the preview); Save() writes
 * GameUserSettings.ini so the next launch starts where the player left it.
 *
 * Levels are Unreal's scalability levels: 0 Low, 1 Medium, 2 High, 3 Epic, 4 Ultra (Cinematic).
 * Until the player saves a choice, the game runs at Ultra (see InitialApply).
 */
struct VISEX_API FVisexSettings
{
	enum class ERow : uint8
	{
		Preset, Shadows, Lighting, Reflections, Textures, Effects, Post, AntiAliasing,
		ViewDistance, Foliage, Shading, RenderScale, VSync, FrameRate, Back
	};

	/** Row labels, in page order. */
	static TArray<FText> Labels();
	/** Each row's value as shown ("Ultra", "85 %", "On"); empty for Back. */
	static TArray<FText> Values();
	static int32 NumRows() { return int32(ERow::Back) + 1; }
	static bool IsBack(int32 Row) { return Row == int32(ERow::Back); }

	/** Step a row's value left (-1) or right (+1) and apply it. */
	static void Adjust(int32 Row, int32 Dir);
	static void Save();

	// ---- Visuals tab: the look, not the cost ----
	enum class EVisual : uint8 { Fog, Style, Anime, Grain, MotionBlur, Back };
	static TArray<FText> VisualLabels();
	static TArray<FText> VisualValues();
	static int32 NumVisualRows() { return int32(EVisual::Back) + 1; }
	static bool IsVisualBack(int32 Row) { return Row == int32(EVisual::Back); }
	static void AdjustVisual(UWorld* World, int32 Row, int32 Dir);
	/** Put the saved look on the scene: fog density, grade, the 80s anime filter, grain, blur. */
	static void ApplyVisuals(UWorld* World);
	/** The style post-process volume's tag; the menu director's studio look skips it. */
	static const FName StyleTag;

	/** On start: the saved settings if the player has chosen some, else Ultra. In the editor
	 * (Play) always Ultra, so playtests show the look at full quality. */
	static void InitialApply();
};
