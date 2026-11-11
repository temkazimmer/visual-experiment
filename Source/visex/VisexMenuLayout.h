#pragma once

#include "CoreMinimal.h"
#include "VisexMenuLayout.generated.h"

/**
 * Where the start menu's block sits and how it is set: the panel's place and perspective
 * relative to the menu camera, its size, the spacing of its lines, and the logo. Edited
 * live with the menu layout editor (§ on the start menu, AVisexMenuLayoutEditor).
 *
 * Saved as one line in Game.ini, section [VisexMenuLayout]. Saved from the editor (PIE) it
 * goes to the project's Config/DefaultGame.ini, so it ships as the default; saved from a
 * packaged game it goes to that player's own Saved/Config.
 */
USTRUCT(BlueprintType)
struct FVisexMenuLayout
{
	GENERATED_BODY()

	/** In front of the menu camera (X, cm), left/right (Y), up/down (Z). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") FVector Offset = FVector(100.f, -34.f, 6.f);
	/** Perspective: turn toward her, tip back, tilt (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float Yaw = -55.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float Pitch = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float Roll = 4.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float Scale = 0.048f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") int32 LogoSize = 104;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") int32 LogoSpacing = 200;
	/** Space between the logo and the first item. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float LogoGap = 110.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") int32 ItemSize = 40;
	/** Space under each item. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float ItemGap = 18.f;

	/** The logo's text, and whether the build number follows it (VIS/EX-0291). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") FString Logo = TEXT("VIS/EX");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") bool bBuildNumber = true;

	/** The logo as shown. */
	FString LogoText() const;

	/** Read the saved layout over InOut (fields it lacks stay). False if none was saved. */
	static bool Load(FVisexMenuLayout& InOut);
	/** Save as the default (see above). Returns where it went. */
	static FString Save(const FVisexMenuLayout& Layout);
};
