#include "VisexClothLibrary.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(VisexClothLibrary)

FString UVisexClothLibrary::FitCapsules(UPhysicsAsset* PhysicsAsset, USkeletalMesh* Mesh, const TArray<FName>& Bones,
	const TArray<FName>& Ends, float Percentile, float RadiusScale)
{
	if (!PhysicsAsset || !Mesh || Bones.Num() != Ends.Num())
	{
		return TEXT("bad arguments");
	}
	const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
	TArray<FTransform> Component;   // reference pose, component space
	Component.SetNum(Ref.GetNum());
	for (int32 I = 0; I < Ref.GetNum(); ++I)
	{
		const int32 Parent = Ref.GetParentIndex(I);
		Component[I] = Parent == INDEX_NONE ? Ref.GetRefBonePose()[I] : Ref.GetRefBonePose()[I] * Component[Parent];
	}
	// distances of each bone's mostly-owned vertices to its segment
	TArray<TArray<float>> Dist;
	Dist.SetNum(Bones.Num());
	TArray<int32> BoneIdx, EndIdx;
	for (int32 B = 0; B < Bones.Num(); ++B)
	{
		BoneIdx.Add(Ref.FindBoneIndex(Bones[B]));
		EndIdx.Add(Ref.FindBoneIndex(Ends[B]));
		if (BoneIdx.Last() == INDEX_NONE || EndIdx.Last() == INDEX_NONE)
		{
			return FString::Printf(TEXT("missing bone %s or %s"), *Bones[B].ToString(), *Ends[B].ToString());
		}
	}
	// the render data's CPU copy (the skin keeps one, see SKM_AndroidSkin's bAllowCPUAccess)
	const FSkeletalMeshRenderData* Render = Mesh->GetResourceForRendering();
	if (!Render || Render->LODRenderData.IsEmpty())
	{
		return TEXT("no render data");
	}
	const FSkeletalMeshLODRenderData& LOD = Render->LODRenderData[0];
	const FPositionVertexBuffer& Positions = LOD.StaticVertexBuffers.PositionVertexBuffer;
	const FSkinWeightVertexBuffer& Weights = LOD.SkinWeightVertexBuffer;
	if (Positions.GetNumVertices() == 0 || Weights.GetNumVertices() == 0)
	{
		return FString::Printf(TEXT("no CPU vertex data (%d positions, %d weights)"), Positions.GetNumVertices(), Weights.GetNumVertices());
	}
	const int32 Influences = Weights.GetMaxBoneInfluences();
	const uint16 Half = Weights.Use16BitBoneWeight() ? 32768 : 128;
	for (const FSkelMeshRenderSection& Section : LOD.RenderSections)
	{
		for (uint32 V = Section.BaseVertexIndex; V < Section.BaseVertexIndex + Section.NumVertices; ++V)
		{
			int32 Best = 0;
			for (int32 K = 1; K < Influences; ++K)
			{
				if (Weights.GetBoneWeight(V, K) > Weights.GetBoneWeight(V, Best)) { Best = K; }
			}
			if (Weights.GetBoneWeight(V, Best) < Half)
			{
				continue;
			}
			const uint32 Local = Weights.GetBoneIndex(V, Best);
			const int32 Bone = Section.BoneMap.IsValidIndex(Local) ? Section.BoneMap[Local] : INDEX_NONE;
			for (int32 B = 0; B < Bones.Num(); ++B)
			{
				// the bone itself or a child of it short of the end bone (twist bones carry most of a limb)
				bool bOwned = false;
				for (int32 Up = Bone; Up != INDEX_NONE && Up != EndIdx[B]; Up = Ref.GetParentIndex(Up))
				{
					if (Up == BoneIdx[B]) { bOwned = true; break; }
				}
				if (bOwned)
				{
					const FVector A = Component[BoneIdx[B]].GetLocation(), E = Component[EndIdx[B]].GetLocation();
					Dist[B].Add(FMath::Sqrt(FMath::PointDistToSegmentSquared(FVector(Positions.VertexPosition(V)), A, E)));
				}
			}
		}
	}
	PhysicsAsset->Modify();
	PhysicsAsset->SkeletalBodySetups.Empty();
	PhysicsAsset->ConstraintSetup.Empty();
	FString Out;
	for (int32 B = 0; B < Bones.Num(); ++B)
	{
		if (Dist[B].Num() < 10)
		{
			Out += FString::Printf(TEXT("%s: only %d vertices, skipped\n"), *Bones[B].ToString(), Dist[B].Num());
			continue;
		}
		Dist[B].Sort();
		const float Radius = Dist[B][FMath::Clamp(FMath::FloorToInt(Percentile * Dist[B].Num()), 0, Dist[B].Num() - 1)] * RadiusScale;
		const FTransform& BoneTM = Component[BoneIdx[B]];
		const FVector A = BoneTM.GetLocation(), E = Component[EndIdx[B]].GetLocation();
		FKSphylElem Capsule;
		Capsule.Radius = Radius;
		Capsule.Length = FVector::Dist(A, E);
		// in bone space: centred on the segment, Z along it
		Capsule.Center = BoneTM.InverseTransformPosition((A + E) * 0.5);
		const FVector Axis = Capsule.Length > KINDA_SMALL_NUMBER ? BoneTM.InverseTransformVectorNoScale((E - A).GetSafeNormal()) : FVector::UpVector;
		Capsule.Rotation = FRotationMatrix::MakeFromZ(Axis).Rotator();
		USkeletalBodySetup* Body = NewObject<USkeletalBodySetup>(PhysicsAsset, NAME_None, RF_Transactional);
		Body->BoneName = Bones[B];
		Body->AggGeom.SphylElems.Add(Capsule);
		PhysicsAsset->SkeletalBodySetups.Add(Body);
		Out += FString::Printf(TEXT("%s->%s: radius %.1f length %.1f (%d verts)\n"), *Bones[B].ToString(), *Ends[B].ToString(), Radius, Capsule.Length, Dist[B].Num());
	}
	PhysicsAsset->UpdateBodySetupIndexMap();
	PhysicsAsset->UpdateBoundsBodiesArray();
	PhysicsAsset->SetPreviewMesh(Mesh);
	PhysicsAsset->MarkPackageDirty();
	return Out;
}
