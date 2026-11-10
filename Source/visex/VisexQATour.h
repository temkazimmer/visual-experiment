#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VisexQATour.generated.h"

/**
 * A scripted walk through every screen, for checking a packaged build without a pad:
 *   VIS-EX.app/Contents/MacOS/visex-Mac-Development -visexqa
 * Menu, start, walk, back to the menu, each outfit, photo mode through every motion and pose,
 * then quit. Each stop leaves a screenshot in Saved/QA/ and a "VisexQA:" log line saying
 * what was loaded, so a missing asset shows up as a name, not just a wrong picture.
 * AVisexMenuDirector spawns it when the command line has -visexqa.
 */
UCLASS(NotPlaceable)
class VISEX_API AVisexQATour : public AActor
{
	GENERATED_BODY()

public:
	AVisexQATour();
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY() TObjectPtr<class AVisexMenuDirector> Director;

private:
	struct FStep { float Wait; TFunction<void()> Run; };
	TArray<FStep> Steps;
	int32 Next = 0;
	float Clock = 0.f;
	float WalkFor = 0.f;
	int32 ShotCount = 0;
	bool bBuilt = false;

	void Build();
	void Shot(const FString& Name);
	class ACharacter* Her() const;
	class AVisexPhotoMode* Photo() const;
	void Report(const FString& Where) const;
};
