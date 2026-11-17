#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Components/ActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "VisexMHBody.generated.h"

class ACharacter;
class USkeletalMeshComponent;

/**
 * The Body page's bone-scale sliders as the MetaHuman body takes them: each term is the slider's
 * value over its default, because the MetaHuman was shaped to her at the defaults (1 = as built).
 * Width and depth are separate terms on the torso, as in UVisexAnimInstance's Reshape.
 */
struct FVisexMHShape
{
	enum { Head, Neck, Shoulders, ChestW, ChestD, WaistW, WaistD, HipsW, HipsD, Legs, Thighs, Calves, ArmLength, Arms, Hands, Feet, Count };
	float V[Count];
	FVisexMHShape() { for (float& X : V) { X = 1.f; } }
	bool operator==(const FVisexMHShape& O) const { return FMemory::Memcmp(V, O.V, sizeof(V)) == 0; }
	bool operator!=(const FVisexMHShape& O) const { return !(*this == O); }
};

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

	/** The Body page's bone scales, set every frame by UVisexMHBodyComponent (game thread). */
	FVisexMHShape Shape;

	/** Corrective joints whose axes are turned a quarter against the scaled bone they hang on (the
	 *  pectoral and latissimus joints under spine_05): bone index and what their component-space
	 *  scale is multiplied by once the post-process AnimBP has run (UVisexMHBodyComponent::FixScales).
	 *  Filled by the proxy on the game thread. */
	TArray<TPair<int32, FVector>> ScaleFixes;

	/** The foot (ankle) and ball bones' height over the sole in the reference pose, cm. */
	float HeelOverSole = 0.f, BallOverSole = 0.f;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
};

/**
 * Face rig curves on the MetaHuman Face, for looking at expressions (visex.Body.MetaHuman.Face). No
 * graph: it gives out the reference pose with the named curves set, and the mesh's post-process AnimBP
 * (ABP_Face_PostProcess: the pose copied from the Body, then RigLogic) turns the curves into the face.
 * Only on the Face while the console variable is set; the Blueprint's own class comes back after.
 */
UCLASS(Transient, NotBlueprintable)
class VISEX_API UVisexMHFaceAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Curve name and value pairs, e.g. CTRL_expressions_eyeBlinkL 1. Game thread. */
	TArray<TPair<FName, float>> Curves;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
};

/**
 * Her card hair on the MetaHuman head. The style's own mesh and AnimBP, unchanged: the AnimBP copies
 * the pose from the attached parent (here the MetaHuman Body, so head, neck and spine are the
 * MetaHuman's, with its bone lengths) and Kawaii Physics swings the locks. The cards were grown on
 * the old skull, so each style carries a fit in the head bone's space (uniform scale and an offset:
 * Config/DefaultGame.ini [VisexMHHairFit], written by Tools/mh_hair_fit.py). Before its own pose each frame the
 * component turns that fit into its relative transform from the Body's final head bone (the attach
 * parent ticks first), so the fit turns with the head and nothing lags a frame.
 */
UCLASS(Transient, NotBlueprintable)
class VISEX_API UVisexMHHairComponent : public USkeletalMeshComponent
{
	GENERATED_BODY()

public:
	/** Hair-asset cm to MetaHuman head cm, and the offset in the head bone's axes (cm). */
	float FitScale = 1.f;
	FVector FitOffset = FVector::ZeroVector;

	/** Puts the fit on the component from the parent's head bone as it is now. */
	void Refit();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
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

	/** visex.Body.MetaHuman is on and the body could be shown: she is the MetaHuman. */
	static bool On();

	/** Can she wear this outfit (None = nothing) on the body she has? On today's body: anything.
	 *  On the MetaHuman: only an outfit with a cut on it (SKM_<Outfit>_MH), never nothing: bare, she
	 *  shows the baked briefs. visex.Body.MetaHuman.AnyOutfit 1 lifts that for checks. */
	static bool Wears(FName Outfit);

	/** What she wears on the body she has when the profile says Outfit: it, or the Silversuit. */
	static FName Worn(FName Outfit);

	/** Does the Hair tab do anything? Not while the MetaHuman wears its own groom. */
	static bool HairTab();

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
	/** Wears the driver's hairstyle on the MetaHuman head (UVisexMHHairComponent) and hides the
	 *  preset's hair groom; brows, lashes and fuzz stay. */
	void ApplyHair();
	void ClearHair();
	/** Drives face rig curves from visex.Body.MetaHuman.Face (a debug look at expressions). */
	void ApplyFace();
	/** Hands the Body page's bone-scale sliders (over their defaults) and the Feet value to the follower. */
	void ApplyShape();
	/** The Body page's shape sliders (Bust, Bust height, Bust spacing, Glutes, Abs, Body fat, Thigh gap)
	 *  and the driver's jiggle springs on the MetaHuman body's morph targets (Tools/mh_morphs.py): each
	 *  weight is the slider less its default, since the body was built at the defaults. */
	void ApplyMorphs();
	/** The flex layer (stage 8): muscle bellies tensed by joint angle, by the weight a leg or a propping
	 *  arm carries and by the walk's push-off, on the Flex* morph targets of Tools/mh_flex_build.py.
	 *  Read from the MetaHuman body's own last pose and the driver's foot plant state; it moves no bone
	 *  and touches no foot. visex.Body.MetaHuman.Flex 0 switches it off, FlexScale is its strength. */
	void ApplyFlex(float DeltaTime);
	enum { FlexCount = 21 };
	/** Each flex morph's weight as it is now (eased, so nothing pops). */
	float FlexNow[FlexCount] = {};
	float FlexLogClock = 0.f;
	/** The reference pose the angles are read against: pelvis, chest (spine_05) and head rotations in
	 *  component space, and each ankle's angle between shin and foot, degrees. */
	FQuat FlexRefQ[3] = {FQuat::Identity, FQuat::Identity, FQuat::Identity};
	float FlexRefAnkle[2] = {0.f, 0.f};
	TWeakObjectPtr<const class USkeletalMesh> FlexRefMesh;
	/** Skin tone from the Body page on the MetaHuman's skin: body, face (every LOD's material) and the
	 *  bare-skin copies worn under outfits, through the tone parameters of the copied skin materials. */
	void ApplySkin();
	/** After the Body's bones are final (post-process AnimBP included): puts the segment's scale on
	 *  the right axes of the joints in UVisexMHBodyAnimInstance::ScaleFixes. RigLogic writes those
	 *  joints' local scale itself, so this cannot be done in the pose. */
	void FixScales();
	TWeakObjectPtr<USkeletalMeshComponent> FixBound;
	TArray<FVector> FixWritten;

	/** Body and face skin materials the tone is set on (made over whatever the slots hold). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UMaterialInstanceDynamic>> ToneMaterials;
	/** Per body and face material slot: what it held when the tone was last put on it. */
	TArray<TWeakObjectPtr<class UMaterialInterface>> ToneSeen;
	float LastTone = -1.f;

	/** The console value the Face was last set up for, and the anim class it had before. */
	FString FaceCurves;
	UPROPERTY(Transient)
	TObjectPtr<UClass> FaceOwnClass;
	bool bFaceDriven = false;

	UPROPERTY(Transient)
	TObjectPtr<AActor> Figure;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> BodyMesh;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> FaceMesh;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> Garment;

	UPROPERTY(Transient)
	TObjectPtr<UVisexMHHairComponent> Hair;

	/** The preset's hair groom, hidden while her cards are on. */
	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> PresetHair;

	FName HairStyle = NAME_None;
	FString LastHairFit = TEXT("?");
	bool bHairApplied = false;
	/** visex.Body.MetaHuman.Hair at the last look (1 the Blueprint's groom, 2 none, 0 the cards); -1: not looked yet */
	int32 GroomMode = -1;

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
