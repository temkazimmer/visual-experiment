#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VisexOutfitSubsystem.generated.h"

class ACharacter;
class USkeletalMesh;
class UVisexShellComponent;

/**
 * Dresses the player character. An outfit is one skeletal mesh on the character's
 * skeleton (built by Tools/blender_outfit.py); it is added at spawn as a second shell
 * that follows the body by Leader Pose, so animation, poses and the body sliders
 * (bone scales and morphs) drive the clothes too. Nothing is stored on the character
 * Blueprint: outfits can be swapped at runtime with SetOutfit.
 *
 * Console: visex.Outfit <asset path | none> picks what newly spawned characters wear.
 */
UCLASS()
class VISEX_API UVisexOutfitSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Put an outfit on a character (null takes it off). Returns the outfit component. */
	UFUNCTION(BlueprintCallable, Category = "Outfit")
	static UVisexShellComponent* SetOutfit(ACharacter* Character, USkeletalMesh* Outfit);

private:
	void OnActorSpawned(AActor* Actor);
	FDelegateHandle SpawnHandle;
};
