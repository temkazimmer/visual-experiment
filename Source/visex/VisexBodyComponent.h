#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VisexBodyProfile.h"
#include "VisexBodyComponent.generated.h"

/**
 * The character's body shape. Lives on the player character; the body page edits it.
 * Height scales the whole figure (mesh and capsule); leg length lifts the mesh so her
 * feet stay on the ground; bone sliders go to UVisexAnimInstance; Bust and Glutes drive
 * the BustUp/BustDown and GlutesUp/GlutesDown morph targets. Saved to a slot.
 */
UCLASS(ClassGroup = (Visex), meta = (BlueprintSpawnableComponent))
class VISEX_API UVisexBodyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVisexBodyComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	FVisexBodyProfile Profile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	FString SaveSlot = TEXT("VisexBody");

	/** Load the saved body when play starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	bool bLoadOnBeginPlay = true;

	/** Quinn at mesh scale 1: height (top of head), capsule half-height and radius, mesh Z. */
	UPROPERTY(EditAnywhere, Category = "Body|Reference")
	float ReferenceHeight = 177.5f;
	UPROPERTY(EditAnywhere, Category = "Body|Reference")
	FVector2D ReferenceCapsule = FVector2D(90.f, 35.f);
	UPROPERTY(EditAnywhere, Category = "Body|Reference")
	float ReferenceMeshZ = -89.f;

	UFUNCTION(BlueprintCallable, Category = "Body")
	void Apply();

	UFUNCTION(BlueprintCallable, Category = "Body")
	void SetValue(FName Name, float Value);

	UFUNCTION(BlueprintPure, Category = "Body")
	float GetValue(FName Name) const { return Profile.Get(Name); }

	UFUNCTION(BlueprintCallable, Category = "Body")
	void ResetValue(FName Name);

	UFUNCTION(BlueprintCallable, Category = "Body")
	bool Save() const;

	UFUNCTION(BlueprintCallable, Category = "Body")
	bool Load();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;

private:
	class USkeletalMeshComponent* Body() const;
	void PushToAnim() const;
};
