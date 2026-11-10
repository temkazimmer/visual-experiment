#include "VisexClothNodes.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "Dataflow/DataflowInputOutput.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(VisexClothNodes)

FVisexHeightWeightMapNode::FVisexHeightWeightMapNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&Collection);
	RegisterOutputConnection(&Collection, &Collection);
	RegisterOutputConnection(&WeightMapName);
}

void FVisexHeightWeightMapNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	using namespace UE::Chaos::ClothAsset;

	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>(MoveTemp(InCollection));
		FCollectionClothFacade ClothFacade(ClothCollection);
		const FName MapName(*WeightMapName);
		if (ClothFacade.IsValid() && MapName != NAME_None)
		{
			ClothFacade.AddWeightMap(MapName);
			TArrayView<float> Weights = ClothFacade.GetWeightMap(MapName);
			const TConstArrayView<FVector3f> Positions = ClothFacade.GetSimPosition3D();
			const float Span = FMath::Max(TopZ - BottomZ, KINDA_SMALL_NUMBER);
			for (int32 Index = 0; Index < Weights.Num() && Index < Positions.Num(); ++Index)
			{
				const float T = FMath::Clamp((TopZ - Positions[Index].Z) / Span, 0.f, 1.f);
				Weights[Index] = FMath::Pow(T, Exponent);
			}
		}
		SetValue(Context, MoveTemp(*ClothCollection), &Collection);
	}
	else if (Out->IsA<FString>(&WeightMapName))
	{
		SetValue(Context, WeightMapName, &WeightMapName);
	}
}
