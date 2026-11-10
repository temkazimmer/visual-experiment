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
	// distances of each bone's mostly-owned vertices to its segment, kept per end: a quarter of the
	// segment at each end sets that end's radius, so a thigh tapers to the knee (one radius would
	// be the top of the thigh all the way down, and pushed a tight skirt's hem out)
	TArray<TArray<float>> DistA, DistE;
	DistA.SetNum(Bones.Num());
	DistE.SetNum(Bones.Num());
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
					const FVector P(Positions.VertexPosition(V));
					const FVector Seg = E - A;
					const double T = Seg.SizeSquared() > KINDA_SMALL_NUMBER ? FVector::DotProduct(P - A, Seg) / Seg.SizeSquared() : 0.5;
					const float D = FMath::Sqrt(FMath::PointDistToSegmentSquared(P, A, E));
					if (T <= 0.25) { DistA[B].Add(D); }
					if (T >= 0.75) { DistE[B].Add(D); }
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
		if (DistA[B].Num() < 10 || DistE[B].Num() < 10)
		{
			Out += FString::Printf(TEXT("%s: only %d/%d vertices, skipped\n"), *Bones[B].ToString(), DistA[B].Num(), DistE[B].Num());
			continue;
		}
		auto Pick = [Percentile, RadiusScale](TArray<float>& Ds)
		{
			Ds.Sort();
			return Ds[FMath::Clamp(FMath::FloorToInt(Percentile * Ds.Num()), 0, Ds.Num() - 1)] * RadiusScale;
		};
		const float RadiusA = Pick(DistA[B]), RadiusE = Pick(DistE[B]);
		const FTransform& BoneTM = Component[BoneIdx[B]];
		const FVector A = BoneTM.GetLocation(), E = Component[EndIdx[B]].GetLocation();
		FKTaperedCapsuleElem Capsule;
		Capsule.Radius0 = RadiusE;      // Radius0 sits at the +Z end, which points at the end bone
		Capsule.Radius1 = RadiusA;
		Capsule.Length = FVector::Dist(A, E);
		// in bone space: centred on the segment, Z along it
		Capsule.Center = BoneTM.InverseTransformPosition((A + E) * 0.5);
		const FVector Axis = Capsule.Length > KINDA_SMALL_NUMBER ? BoneTM.InverseTransformVectorNoScale((E - A).GetSafeNormal()) : FVector::UpVector;
		Capsule.Rotation = FRotationMatrix::MakeFromZ(Axis).Rotator();
		USkeletalBodySetup* Body = NewObject<USkeletalBodySetup>(PhysicsAsset, NAME_None, RF_Transactional);
		Body->BoneName = Bones[B];
		Body->AggGeom.TaperedCapsuleElems.Add(Capsule);
		PhysicsAsset->SkeletalBodySetups.Add(Body);
		Out += FString::Printf(TEXT("%s->%s: radius %.1f to %.1f length %.1f (%d/%d verts)\n"), *Bones[B].ToString(), *Ends[B].ToString(), RadiusA, RadiusE, Capsule.Length, DistA[B].Num(), DistE[B].Num());
	}
	PhysicsAsset->UpdateBodySetupIndexMap();
	PhysicsAsset->UpdateBoundsBodiesArray();
	PhysicsAsset->SetPreviewMesh(Mesh);
	PhysicsAsset->MarkPackageDirty();
	return Out;
}
