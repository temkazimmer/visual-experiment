#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VisexIntroRig.generated.h"

class ACharacter;

/**
 * Puts her in the intro film (LS_Intro, rendered with Movie Render Queue): a spawnable in
 * the sequence that, once the render's game world starts, spawns the player character at
 * its own transform, possesses her with player 0 and walks her along its forward vector
 * at the keyed Walk input. She moves through the real character movement, so the walk,
 * catwalk, toe and foot-IK layers and the sand footprints all run as in the game.
 *
 * It parks the menu director first (the render's game mode starts with no pawn, and the
 * director would grab her for the menu as soon as she is possessed).
 */
UCLASS()
class VISEX_API AVisexIntroRig : public AActor
{
	GENERATED_BODY()

public:
	AVisexIntroRig();
	virtual void Tick(float DeltaSeconds) override;

	/** Analog walk input, 0..1 (keyed in the sequence). */
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category = "Intro")
	float Walk = 0.f;

	/** 1 shows her, 0 hides her (keyed in the sequence). Applied every tick, which also undoes
	 *  the sequence player's hide-the-player for the possessed one. Hidden, she still moves. */
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category = "Intro")
	float Show = 1.f;

	/** Possess her with player 0 (the sand footprints follow the player only). Off: she gets
	 *  her own AI controller, so two rigs can walk at once around a cut. */
	UPROPERTY(EditAnywhere, Category = "Intro")
	bool bPlayer = false;

	/** Who to spawn. */
	UPROPERTY(EditAnywhere, Category = "Intro")
	TSoftClassPtr<ACharacter> CharacterClass;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Intro")
	TObjectPtr<ACharacter> Her;

protected:
	float LogClock = 0.f;
	bool bSpawned = false;

	void SpawnHer();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
};
