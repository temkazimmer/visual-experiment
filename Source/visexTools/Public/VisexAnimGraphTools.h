#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "VisexAnimGraphTools.generated.h"

class UAnimBlueprint;
class UEdGraphNode;

/**
 * Build an Anim Blueprint's AnimGraph from Python (unreal.VisexAnimGraphTools).
 * Python in 5.7 can create an Anim Blueprint but not add or wire its graph nodes.
 */
UCLASS()
class UVisexAnimGraphTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Remove every AnimGraph node except the output. */
	UFUNCTION(BlueprintCallable, Category = "Visex|AnimGraph")
	static void ClearAnimGraph(UAnimBlueprint* AnimBlueprint);

	/** The AnimGraph's Output Pose node. */
	UFUNCTION(BlueprintCallable, Category = "Visex|AnimGraph")
	static UEdGraphNode* GetOutputNode(UAnimBlueprint* AnimBlueprint);

	/** Add a node of NodeClass (an AnimGraphNode_* class) to the AnimGraph. */
	UFUNCTION(BlueprintCallable, Category = "Visex|AnimGraph")
	static UEdGraphNode* AddNode(UAnimBlueprint* AnimBlueprint, UClass* NodeClass, int32 X, int32 Y);

	/** Wire From's pose output into To's first pose input (adds space conversion if needed). */
	UFUNCTION(BlueprintCallable, Category = "Visex|AnimGraph")
	static bool LinkPose(UEdGraphNode* From, UEdGraphNode* To);

	/**
	 * Set a property on a node from text, e.g. ("Node.bUseAttachedParent", "True") or
	 * ("Node.PhysicsSettings", "(Damping=0.8,Stiffness=0.05)"). Returns false if not found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Visex|AnimGraph")
	static bool SetNodeValue(UEdGraphNode* Node, const FString& Path, const FString& Value);

	/** Same for any object, e.g. an editor settings default object Python can't write. */
	UFUNCTION(BlueprintCallable, Category = "Visex|AnimGraph")
	static bool SetObjectValue(UObject* Object, const FString& Path, const FString& Value);

	/** Read a property as text (same path rules). Empty if not found. */
	UFUNCTION(BlueprintCallable, Category = "Visex|AnimGraph")
	static FString GetObjectValue(UObject* Object, const FString& Path);
};
