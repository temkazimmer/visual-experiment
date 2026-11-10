#include "VisexAnimGraphTools.h"

#include "Animation/AnimBlueprint.h"
#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_Root.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, visexTools);

namespace
{
UEdGraph* FindAnimGraph(UAnimBlueprint* BP)
{
	if (!BP) return nullptr;
	for (UEdGraph* G : BP->FunctionGraphs)
	{
		if (G && G->GetFName() == UEdGraphSchema_K2::GN_AnimGraph) return G;
	}
	return nullptr;
}

bool IsPosePin(const UEdGraphPin* Pin, EEdGraphPinDirection Dir)
{
	if (!Pin || Pin->Direction != Dir || Pin->bHidden) return false;
	const UScriptStruct* S = Cast<UScriptStruct>(Pin->PinType.PinSubCategoryObject.Get());
	return S && S->GetName().Contains(TEXT("PoseLink"));
}
}

void UVisexAnimGraphTools::ClearAnimGraph(UAnimBlueprint* AnimBlueprint)
{
	UEdGraph* G = FindAnimGraph(AnimBlueprint);
	if (!G) return;
	TArray<UEdGraphNode*> Kill;
	for (UEdGraphNode* N : G->Nodes)
	{
		if (N && !N->IsA<UAnimGraphNode_Root>()) Kill.Add(N);
	}
	for (UEdGraphNode* N : Kill)
	{
		N->BreakAllNodeLinks();
		G->RemoveNode(N);
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
}

UEdGraphNode* UVisexAnimGraphTools::GetOutputNode(UAnimBlueprint* AnimBlueprint)
{
	UEdGraph* G = FindAnimGraph(AnimBlueprint);
	if (!G) return nullptr;
	for (UEdGraphNode* N : G->Nodes)
	{
		if (N && N->IsA<UAnimGraphNode_Root>()) return N;
	}
	return nullptr;
}

UEdGraphNode* UVisexAnimGraphTools::AddNode(UAnimBlueprint* AnimBlueprint, UClass* NodeClass, int32 X, int32 Y)
{
	UEdGraph* G = FindAnimGraph(AnimBlueprint);
	if (!G || !NodeClass || !NodeClass->IsChildOf(UAnimGraphNode_Base::StaticClass())) return nullptr;
	G->Modify();
	UEdGraphNode* N = NewObject<UEdGraphNode>(G, NodeClass, NAME_None, RF_Transactional);
	N->CreateNewGuid();
	N->NodePosX = X;
	N->NodePosY = Y;
	G->AddNode(N, /*bFromUI*/ false, /*bSelectNewNode*/ false);
	N->PostPlacedNewNode();
	N->AllocateDefaultPins();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
	return N;
}

bool UVisexAnimGraphTools::LinkPose(UEdGraphNode* From, UEdGraphNode* To)
{
	if (!From || !To) return false;
	UEdGraphPin* Out = nullptr;
	UEdGraphPin* In = nullptr;
	for (UEdGraphPin* P : From->Pins) { if (IsPosePin(P, EGPD_Output)) { Out = P; break; } }
	for (UEdGraphPin* P : To->Pins) { if (IsPosePin(P, EGPD_Input)) { In = P; break; } }
	if (!Out || !In) return false;
	const bool bOk = From->GetGraph()->GetSchema()->TryCreateConnection(Out, In);
	if (UBlueprint* BP = FBlueprintEditorUtils::FindBlueprintForNode(From))
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	}
	return bOk;
}

namespace
{
FProperty* Resolve(UObject* Object, const FString& Path, void*& OutContainer)
{
	TArray<FString> Parts;
	Path.ParseIntoArray(Parts, TEXT("."));
	const UStruct* Owner = Object->GetClass();
	void* Container = Object;
	FProperty* Prop = nullptr;
	for (int32 i = 0; i < Parts.Num(); ++i)
	{
		Prop = FindFProperty<FProperty>(Owner, *Parts[i]);
		if (!Prop) return nullptr;
		if (i < Parts.Num() - 1)
		{
			FStructProperty* SP = CastField<FStructProperty>(Prop);
			if (!SP) return nullptr;
			Container = SP->ContainerPtrToValuePtr<void>(Container);
			Owner = SP->Struct;
		}
	}
	OutContainer = Container;
	return Prop;
}
}

bool UVisexAnimGraphTools::SetObjectValue(UObject* Object, const FString& Path, const FString& Value)
{
	if (!Object) return false;
	void* Container = nullptr;
	FProperty* Prop = Resolve(Object, Path, Container);
	if (!Prop) return false;
	Object->Modify();
	if (FBoolProperty* BP = CastField<FBoolProperty>(Prop))   // bitfield bools
	{
		BP->SetPropertyValue_InContainer(Container, Value.ToBool());
	}
	else if (!Prop->ImportText_InContainer(*Value, Container, Object, PPF_None))
	{
		return false;
	}
	FPropertyChangedEvent Ev(Prop);
	Object->PostEditChangeProperty(Ev);
	return true;
}

FString UVisexAnimGraphTools::GetObjectValue(UObject* Object, const FString& Path)
{
	if (!Object) return FString();
	void* Container = nullptr;
	FProperty* Prop = Resolve(Object, Path, Container);
	if (!Prop) return FString();
	FString Out;
	Prop->ExportText_InContainer(0, Out, Container, Container, Object, PPF_None);
	return Out;
}

bool UVisexAnimGraphTools::SetNodeValue(UEdGraphNode* Node, const FString& Path, const FString& Value)
{
	if (!SetObjectValue(Node, Path, Value)) return false;
	if (UBlueprint* BP = FBlueprintEditorUtils::FindBlueprintForNode(Node))
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	}
	return true;
}
