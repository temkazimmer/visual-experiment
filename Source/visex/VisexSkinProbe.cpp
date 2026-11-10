#include "VisexSkinProbe.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#if WITH_EDITOR
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#endif

namespace
{
	constexpr int32 MaxPerRegion[FVisexSkinProbe::Num] = {500, 500, 700, 2500};
}

bool FVisexSkinProbe::IsBuiltFor(const USkeletalMeshComponent* Component) const
{
	return bBuilt && Component && Mesh.Get() == Component->GetSkeletalMeshAsset();
}

bool FVisexSkinProbe::Build(const USkeletalMeshComponent* Component)
{
	bBuilt = false;
	for (TArray<FVert>& R : Regions)
	{
		R.Reset();
	}
	const USkeletalMesh* Skel = Component ? Component->GetSkeletalMeshAsset() : nullptr;
	if (!Skel)
	{
		return false;
	}
	Mesh = Skel;
	const FReferenceSkeleton& RefSkel = Skel->GetRefSkeleton();
	const int32 NumBones = RefSkel.GetNum();

	// the reference pose in component space, to place vertices relative to the hips
	TArray<FTransform> RefCS;
	RefCS.SetNum(NumBones);
	for (int32 b = 0; b < NumBones; ++b)
	{
		const int32 P = RefSkel.GetParentIndex(b);
		RefCS[b] = P == INDEX_NONE ? RefSkel.GetRefBonePose()[b] : RefSkel.GetRefBonePose()[b] * RefCS[P];
	}
	const auto Under = [&](int32 Bone, int32 Ancestor)
	{
		for (int32 b = Bone; b != INDEX_NONE; b = RefSkel.GetParentIndex(b))
		{
			if (b == Ancestor)
			{
				return true;
			}
		}
		return false;
	};
	const int32 BoneHandL = RefSkel.FindBoneIndex(TEXT("hand_l")), BoneHandR = RefSkel.FindBoneIndex(TEXT("hand_r"));
	const int32 Pelvis = RefSkel.FindBoneIndex(TEXT("pelvis")), CalfL = RefSkel.FindBoneIndex(TEXT("calf_l"));
	if (BoneHandL == INDEX_NONE || BoneHandR == INDEX_NONE || Pelvis == INDEX_NONE || CalfL == INDEX_NONE)
	{
		return false;
	}
	const FVector PelvisAt = RefCS[Pelvis].GetLocation(), KneeAt = RefCS[CalfL].GetLocation();
	const auto Named = [&](int32 b, const TCHAR* Prefix) { return RefSkel.GetBoneName(b).ToString().StartsWith(Prefix); };

	TArray<FVert> All[Num];
	const auto Take = [&](const FVert& V)
	{
		int32 Dom = 0;
		for (int32 i = 1; i < V.Num; ++i)
		{
			if (V.Weight[i] > V.Weight[Dom]) { Dom = i; }
		}
		const int32 B = V.Bone[Dom];
		const FVector P(V.Ref);
		// the palm: the hand bone and the metacarpals, not the fingers (they curl over edges)
		const bool bPalm = Named(B, TEXT("hand_")) || RefSkel.GetBoneName(B).ToString().Contains(TEXT("metacarpal"));
		if (Under(B, BoneHandL)) { if (bPalm) { All[FVisexSkinProbe::HandL].Add(V); } return; }
		if (Under(B, BoneHandR)) { if (bPalm) { All[FVisexSkinProbe::HandR].Add(V); } return; }
		const bool bHip = B == Pelvis || Named(B, TEXT("thigh_"));
		if (bHip && P.Y < PelvisAt.Y && P.Z < PelvisAt.Z + 8.0 && P.Z > KneeAt.Z)
		{
			All[FVisexSkinProbe::Seat].Add(V); // buttocks and the backs of the thighs
		}
		All[FVisexSkinProbe::Body].Add(V); // everything but the hands: chest, arms and head lie down too
	};

	bool bRead = false;
#if WITH_EDITOR
	if (const FSkeletalMeshModel* Model = const_cast<USkeletalMesh*>(Skel)->GetImportedModel())
	{
		if (Model->LODModels.Num() > 0)
		{
			for (const FSkelMeshSection& Section : Model->LODModels[0].Sections)
			{
				for (const FSoftSkinVertex& S : Section.SoftVertices)
				{
					FVert V;
					V.Ref = S.Position;
					float Sum = 0.f;
					for (int32 i = 0; i < MAX_TOTAL_INFLUENCES && V.Num < 4; ++i)
					{
						if (S.InfluenceWeights[i] == 0) { continue; }
						V.Bone[V.Num] = Section.BoneMap[S.InfluenceBones[i]];
						V.Weight[V.Num] = S.InfluenceWeights[i];
						Sum += V.Weight[V.Num++];
					}
					if (V.Num == 0) { continue; }
					for (int32 i = 0; i < V.Num; ++i) { V.Weight[i] /= Sum; }
					Take(V);
				}
			}
			bRead = true;
		}
	}
#endif
	if (!bRead)
	{
		// cooked builds: the render data, if its CPU copy was kept
		const FSkeletalMeshRenderData* RD = const_cast<USkeletalMesh*>(Skel)->GetResourceForRendering();
		if (!RD || RD->LODRenderData.Num() == 0)
		{
			return false;
		}
		const FSkeletalMeshLODRenderData& LOD = RD->LODRenderData[0];
		const FSkinWeightVertexBuffer* Weights = LOD.GetSkinWeightVertexBuffer();
		const FPositionVertexBuffer& Pos = LOD.StaticVertexBuffers.PositionVertexBuffer;
		if (!Weights || !Pos.GetVertexData() || Weights->GetNumVertices() == 0)
		{
			return false;
		}
		for (uint32 v = 0; v < Pos.GetNumVertices(); ++v)
		{
			int32 SectionIndex = 0, Local = 0;
			LOD.GetSectionFromVertexIndex(v, SectionIndex, Local);
			const FSkelMeshRenderSection& Section = LOD.RenderSections[SectionIndex];
			FVert V;
			V.Ref = Pos.VertexPosition(v);
			float Sum = 0.f;
			for (uint32 i = 0; i < Weights->GetMaxBoneInfluences() && V.Num < 4; ++i)
			{
				const float W = Weights->GetBoneWeight(v, i);
				if (W <= 0.f) { continue; }
				V.Bone[V.Num] = Section.BoneMap[Weights->GetBoneIndex(v, i)];
				V.Weight[V.Num] = W;
				Sum += V.Weight[V.Num++];
			}
			if (V.Num == 0) { continue; }
			for (int32 i = 0; i < V.Num; ++i) { V.Weight[i] /= Sum; }
			Take(V);
		}
	}

	// an even spread of each region, capped
	for (int32 r = 0; r < Num; ++r)
	{
		const int32 Step = FMath::Max(1, FMath::DivideAndRoundUp(All[r].Num(), MaxPerRegion[r]));
		for (int32 i = 0; i < All[r].Num(); i += Step)
		{
			Regions[r].Add(All[r][i]);
		}
	}
	bBuilt = Regions[FVisexSkinProbe::HandL].Num() > 0 && Regions[FVisexSkinProbe::HandR].Num() > 0 && Regions[FVisexSkinProbe::Body].Num() > 0;
	return bBuilt;
}

bool FVisexSkinProbe::Lowest(const USkeletalMeshComponent* Component, ERegion Region, TFunctionRef<double(const FVector&)> Score, FVector& Out) const
{
	if (!IsBuiltFor(Component) || Regions[Region].Num() == 0)
	{
		return false;
	}
	const USkeletalMesh* Skel = Mesh.Get();
	const TArray<FTransform>& CS = Component->GetComponentSpaceTransforms();
	const TArray<FMatrix44f>& RefInv = Skel->GetRefBasesInvMatrix();
	if (CS.Num() == 0 || CS.Num() != RefInv.Num())
	{
		return false;
	}
	const FTransform ToWorld = Component->GetComponentTransform();
	double Best = TNumericLimits<double>::Max();
	for (const FVert& V : Regions[Region])
	{
		FVector3f P = FVector3f::ZeroVector;
		for (int32 i = 0; i < V.Num; ++i)
		{
			const FMatrix44f M = RefInv[V.Bone[i]] * FMatrix44f(CS[V.Bone[i]].ToMatrixWithScale());
			P += M.TransformPosition(V.Ref) * V.Weight[i];
		}
		const FVector W = ToWorld.TransformPosition(FVector(P));
		const double S = Score(W);
		if (S < Best)
		{
			Best = S;
			Out = W;
		}
	}
	return true;
}
