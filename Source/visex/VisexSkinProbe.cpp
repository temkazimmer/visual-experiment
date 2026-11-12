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

bool FVisexSkinProbe::Build(const USkeletalMeshComponent* Component, bool bEveryVertex)
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

	// limb radii: the furthest skin from the bone's axis, sampled along it
	constexpr int32 RadiusBins = 6;
	Radii.Reset();
	Frames.Reset();
	RefRots.Reset();
	constexpr int32 RadiusSectors = 8;
	struct FLimb { int32 Bone; FVector A, Dir, U, V; float Len; TArray<float> R; };
	TArray<FLimb> Limbs;
	for (const TCHAR* Name : {TEXT("thigh_l"), TEXT("thigh_r"), TEXT("calf_l"), TEXT("calf_r"), TEXT("upperarm_l"), TEXT("upperarm_r"), TEXT("lowerarm_l"), TEXT("lowerarm_r"), TEXT("head")})
	{
		const int32 B = RefSkel.FindBoneIndex(Name);
		if (B == INDEX_NONE) { continue; }
		int32 Child = INDEX_NONE;
		for (int32 b = 0; b < NumBones && Child == INDEX_NONE; ++b)
		{
			if (RefSkel.GetParentIndex(b) == B && !RefSkel.GetBoneName(b).ToString().Contains(TEXT("twist"))) { Child = b; }
		}
		FLimb L; L.Bone = B; L.A = RefCS[B].GetLocation();
		const FVector End = Child != INDEX_NONE && FCString::Strcmp(Name, TEXT("head")) != 0 ? RefCS[Child].GetLocation() : L.A + FVector(0, 0, 14.0);
		L.Dir = (End - L.A).GetSafeNormal(); L.Len = FMath::Max(float((End - L.A).Size()), 1.f);
		// sectors are counted in the bone's own frame, so they mean the same at runtime
		L.U = (RefCS[B].GetRotation().GetAxisY() - L.Dir * (RefCS[B].GetRotation().GetAxisY() | L.Dir)).GetSafeNormal();
		L.V = FVector::CrossProduct(L.Dir, L.U);
		L.R.Init(0.f, RadiusBins * RadiusSectors);
		Limbs.Add(L);
	}
	const auto Measure = [&](const FVert& V, int32 B)
	{
		for (FLimb& L : Limbs)
		{
			if (!Under(B, L.Bone)) { continue; }
			const FVector P(V.Ref);
			const float Along = float((P - L.A) | L.Dir);
			if (Along < 0.f || Along > L.Len) { continue; }
			const FVector Out = P - L.A - L.Dir * Along;
			const float R = float(Out.Size());
			const int32 Sector = (int32(FMath::Floor((FMath::Atan2(float(Out | L.V), float(Out | L.U)) + PI) / (2.f * PI) * RadiusSectors)) % RadiusSectors + RadiusSectors) % RadiusSectors;
			float& Bin = L.R[FMath::Clamp(int32(Along / L.Len * RadiusBins), 0, RadiusBins - 1) * RadiusSectors + Sector];
			Bin = FMath::Max(Bin, R);
		}
	};

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
		Measure(V, B);
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

	for (FLimb& L : Limbs)
	{
		// an empty sector (no skin there in the reference mesh) borrows its neighbours
		for (int32 b = 0; b < RadiusBins; ++b)
		{
			for (int32 k = 0; k < RadiusSectors; ++k)
			{
				float& R = L.R[b * RadiusSectors + k];
				if (R <= 0.f) { R = FMath::Max(L.R[b * RadiusSectors + (k + 1) % RadiusSectors], L.R[b * RadiusSectors + (k + RadiusSectors - 1) % RadiusSectors]); }
			}
		}
		Radii.Add(RefSkel.GetBoneName(L.Bone), L.R);
		Frames.Add(RefSkel.GetBoneName(L.Bone), FMatrix(L.Dir, L.U, L.V, FVector::ZeroVector)); // rows: along, U, V in the ref component frame
		RefRots.Add(RefSkel.GetBoneName(L.Bone), RefCS[L.Bone].GetRotation());
	}

	// an even spread of each region, capped
	for (int32 r = 0; r < Num; ++r)
	{
		const int32 Step = FMath::Max(1, FMath::DivideAndRoundUp(All[r].Num(), bEveryVertex ? FMath::Max(All[r].Num(), 1) : MaxPerRegion[r]));
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

void FVisexSkinProbe::Skinned(const USkeletalMeshComponent* Component, ERegion Region, TArray<FVector>& Out, TArray<FName>& OutBone) const
{
	Out.Reset();
	OutBone.Reset();
	if (!IsBuiltFor(Component))
	{
		return;
	}
	// a follower (her shell, her outfit) draws with its leader's bones
	const USkinnedMeshComponent* Leader = Component->LeaderPoseComponent.Get();
	const USkinnedMeshComponent* Pose = Leader ? Leader : Component;
	const TArray<FTransform>& CS = Pose->GetComponentSpaceTransforms();
	const TArray<int32>& Map = Component->GetLeaderBoneMap();
	const USkeletalMesh* Skel = Mesh.Get();
	const TArray<FMatrix44f>& RefInv = Skel->GetRefBasesInvMatrix();
	const FTransform ToWorld = Pose->GetComponentTransform();
	for (const FVert& V : Regions[Region])
	{
		FVector3f P = FVector3f::ZeroVector;
		int32 Heaviest = 0;
		bool bOk = true;
		for (int32 i = 0; i < V.Num; ++i)
		{
			const int32 L = Leader ? (Map.IsValidIndex(V.Bone[i]) ? Map[V.Bone[i]] : INDEX_NONE) : V.Bone[i];
			if (!CS.IsValidIndex(L) || !RefInv.IsValidIndex(V.Bone[i]))
			{
				bOk = false;
				break;
			}
			P += (RefInv[V.Bone[i]] * FMatrix44f(CS[L].ToMatrixWithScale())).TransformPosition(V.Ref) * V.Weight[i];
			Heaviest = V.Weight[i] > V.Weight[Heaviest] ? i : Heaviest;
		}
		if (bOk)
		{
			Out.Add(ToWorld.TransformPosition(FVector(P)));
			OutBone.Add(Skel->GetRefSkeleton().GetBoneName(V.Bone[Heaviest]));
		}
	}
}

float FVisexSkinProbe::Radius(const USkeletalMeshComponent* Component, FName Bone, float Along01, const FVector& OutWorld) const
{
	const TArray<float>* R = Radii.Find(Bone);
	const FMatrix* F = Frames.Find(Bone);
	const int32 BoneIndex = Component && Component->GetSkeletalMeshAsset() ? Component->GetSkeletalMeshAsset()->GetRefSkeleton().FindBoneIndex(Bone) : INDEX_NONE;
	if (!R || !F || R->Num() == 0 || BoneIndex == INDEX_NONE)
	{
		return 0.f;
	}
	constexpr int32 Sectors = 8;
	const int32 Bins = R->Num() / Sectors;
	// the outward direction in the bone's current frame, matched to the sectors of its reference frame
	const FQuat BoneNow = Component->GetBoneQuaternion(Bone, EBoneSpaces::ComponentSpace);
	const FQuat* BoneRef = RefRots.Find(Bone);
	if (!BoneRef) { return 0.f; }
	const FVector OutLocal = BoneNow.UnrotateVector(Component->GetComponentTransform().InverseTransformVectorNoScale(OutWorld));
	const FVector OutRef = BoneRef->RotateVector(OutLocal);
	const float A = FMath::Atan2(float(OutRef | F->GetScaledAxis(EAxis::Z)), float(OutRef | F->GetScaledAxis(EAxis::Y)));
	const int32 Sector = (int32(FMath::Floor((A + PI) / (2.f * PI) * Sectors)) % Sectors + Sectors) % Sectors;
	const float X = FMath::Clamp(Along01, 0.f, 1.f) * (Bins - 1);
	const int32 I = FMath::Clamp(int32(X), 0, Bins - 2);
	return FMath::Lerp((*R)[I * Sectors + Sector], (*R)[(I + 1) * Sectors + Sector], X - I);
}
