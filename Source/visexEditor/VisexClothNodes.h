#pragma once

#include "Dataflow/DataflowNode.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "VisexClothNodes.generated.h"

/**
 * A sim weight map from height: 0 at and above TopZ, rising to 1 at BottomZ (cm, in the mesh's
 * reference pose), shaped by Exponent. Feeds the skirt's max-distance map, so the waistband is
 * pinned and the hem is free, without hand painting.
 */
USTRUCT(Meta = (DataflowCloth))
struct FVisexHeightWeightMapNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FVisexHeightWeightMapNode, "VisexHeightWeightMap", "Cloth", "Visex Height Weight Map")
	DATAFLOW_NODE_RENDER_TYPE("SurfaceRender", FName("FClothCollection"), "Collection")

public:
	UPROPERTY(Meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "Collection"))
	FManagedArrayCollection Collection;

	UPROPERTY(EditAnywhere, Category = "Height Weight Map", Meta = (DataflowOutput))
	FString WeightMapName = TEXT("MaxDistance");

	/** At and above this height the weight is 0. */
	UPROPERTY(EditAnywhere, Category = "Height Weight Map")
	float TopZ = 100.f;

	/** At and below this height the weight is 1. */
	UPROPERTY(EditAnywhere, Category = "Height Weight Map")
	float BottomZ = 80.f;

	/** Shapes the ramp: above 1 keeps the upper part tighter. */
	UPROPERTY(EditAnywhere, Category = "Height Weight Map", Meta = (ClampMin = "0.1"))
	float Exponent = 1.f;

	FVisexHeightWeightMapNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};
