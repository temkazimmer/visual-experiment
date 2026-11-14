#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Components/ActorComponent.h"
#include "VisexMHBody.generated.h"

class ACharacter;
class USkeletalMeshComponent;

/**
 * The MetaHuman body's main anim instance: no graph, it follows another mesh's final pose.
 * Each frame it takes the driver's component-space pose (today's Quinn-skeleton character, after
 * ABP_Unarmed, the C++ layers and the one ground-IK owner), copies the rotations of the bones
 * the two skeletons share by name, keeps the MetaHuman's own bone lengths, and pins each foot's
 * sole to the driver's sole with a two-bone leg solve. The mesh's post-process AnimBP
 * (ABP_Body_PostProcess: RigLogic twist and corrective joints) then runs on top of this pose,
 * which a Leader Pose follower would never get.
 */
UCLASS(Transient, NotBlueprintable)
class VISEX_API UVisexMHBodyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** The mesh to follow, and how many of our component-space cm one of its cm is. */
	void SetSource(USkeletalMeshComponent* InSource, float InSourceScale, float InFeetScale);

	UPROPERTY(Transient)
	TWeakObjectPtr<USkeletalMeshComponent> Source;

	float SourceScale = 1.f;
	float FeetScale = 1.f;

	/** The foot (ankle) and ball bones' height over the sole in the reference pose, cm. */
	float HeelOverSole = 0.f, BallOverSole = 0.f;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
};

/**
 * visex.Body.MetaHuman 1: she is drawn as her assembled MetaHuman (/Game/Character/MH, body, face
 * and grooms) instead of the android skin. Today's character stays the driver: its mesh keeps
 * running ABP_Unarmed and every C++ layer, only its skin, outfit and hair are hidden, and the
 * MetaHuman body follows its pose (UVisexMHBodyAnimInstance). At 0 nothing here exists.
 * Made at runtime by UVisexBodyComponent, never saved with a level or a Blueprint.
 */
UCLASS(ClassGroup = (Rendering))
class VISEX_API UVisexMHBodyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVisexMHBodyComponent();

	/** Adds the component to Owner once (game worlds only). */
	static void Ensure(AActor* Owner);

	/** The MetaHuman body mesh while it is the one on screen, else null. */
	static USkeletalMeshComponent* ShownBody(const ACharacter* Her);

	/** Its face mesh, likewise. */
	static USkeletalMeshComponent* ShownFace(const ACharacter* Her);

	/** The worn outfit's cut on the MetaHuman body while it is shown, else null. */
	static USkeletalMeshComponent* ShownGarment(const ACharacter* Her);

	/** The mesh whose feet are on screen: the MetaHuman body if shown, else her own mesh. */
	static USkeletalMeshComponent* FeetMesh(const ACharacter* Her);

	/** Heel (foot bone) and ball height over the sole for FeetMesh, world cm. False: use the ref pose. */
	static bool SoleHeights(const ACharacter* Her, float& OutHeel, float& OutBall);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void Show();
	/** Finds the figure's Body and Face, sets them up and starts the follow. */
	void Bind();
	void Hide();
	/** Hides (or gives back) the driver's skin, outfit, cloth and hair. */
	void HideDriver(bool bHide);
	void ApplyDebug();
	/** Wears the MetaHuman cut of the driver's outfit if there is one (/Game/Character/MH/Outfits,
	 *  Tools/mh_outfit.py): the garment follows the body's final pose, correctives included, and the
	 *  body's skin material is swapped for the one that clips the skin under opaque fabric. */
	void ApplyOutfit();
	void ClearOutfit();

	UPROPERTY(Transient)
	TObjectPtr<AActor> Figure;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> BodyMesh;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> FaceMesh;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> Garment;

	/** The body's own skin material while an outfit's masked one is on. */
	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInterface> BareSkin;

	/** The face mesh's own skin material (slot 0, LOD 0) while the one without the top's straps is on. */
	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInterface> FaceSkin;

	FName GarmentOutfit = NAME_None;
	bool bOutfitApplied = false;

	/** What HideDriver hid, to give back exactly that. */
	TArray<TWeakObjectPtr<USceneComponent>> Hidden;

	bool bShown = false;
	bool bFailed = false;
	int32 LastCorrectives = -1;
	int32 LastLOD = -2;
};
