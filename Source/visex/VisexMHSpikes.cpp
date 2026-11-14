#include "VisexMHSpikes.h"
#include "VisexMHBody.h"
#include "Components/LODSyncComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"
#include "Rendering/SkeletalMeshRenderData.h"

DEFINE_LOG_CATEGORY_STATIC(LogVisexSpikes, Log, All);

bool FVisexMHSpikes::Measure(USkeletalMeshComponent* Mesh, FVisexSpike& Out)
{
	Out = FVisexSpike();
	const USkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	FSkeletalMeshRenderData* RD = Mesh ? Mesh->GetSkeletalMeshRenderData() : nullptr;
	if (!Asset || !RD || RD->LODRenderData.Num() == 0)
	{
		return false;
	}
	const FSkeletalMeshLODRenderData& LOD = RD->LODRenderData[0];
	const FPositionVertexBuffer& PB = LOD.StaticVertexBuffers.PositionVertexBuffer;
	if (PB.GetNumVertices() == 0 || !PB.GetVertexData())
	{
		return false;
	}
	FCache* Cache = nullptr;
	for (FCache& C : Caches)
	{
		Cache = C.Mesh == Asset ? &C : Cache;
	}
	if (!Cache)
	{
		Cache = &Caches[Count - 1];
		for (int32 I = Count - 1; I >= 0; --I)
		{
			Cache = !Caches[I].Mesh.IsValid() ? &Caches[I] : Cache;
		}
		Cache->Mesh = Asset;
		Cache->Index.Reset();
		LOD.MultiSizeIndexContainer.GetIndexBuffer(Cache->Index);
		Cache->RefLength.SetNum(Cache->Index.Num());
		for (int32 I = 0; I + 2 < Cache->Index.Num(); I += 3)
		{
			for (int32 E = 0; E < 3; ++E)
			{
				Cache->RefLength[I + E] = FVector3f::Dist(PB.VertexPosition(Cache->Index[I + E]), PB.VertexPosition(Cache->Index[I + (E + 1) % 3]));
			}
		}
	}
	TArray<FMatrix44f> RefToLocal;
	Mesh->CacheRefToLocalMatrices(RefToLocal);
	TArray<FVector3f> P;
	USkinnedMeshComponent::ComputeSkinnedPositions(Mesh, P, RefToLocal, LOD, LOD.SkinWeightVertexBuffer);
	if (P.Num() != int32(PB.GetNumVertices()))
	{
		return false;
	}
	int32 GrowVertex = INDEX_NONE, CreaseVertex = INDEX_NONE;
	for (int32 I = 0; I + 2 < Cache->Index.Num(); I += 3)
	{
		for (int32 E = 0; E < 3; ++E)
		{
			// a collapsed reference edge has no length to compare with
			const float Ref = Cache->RefLength[I + E];
			if (Ref < 0.02f)
			{
				continue;
			}
			const uint32 A = Cache->Index[I + E], B = Cache->Index[I + (E + 1) % 3];
			const float Now = FVector3f::Dist(P[A], P[B]);
			const uint32 Third = Cache->Index[I + (E + 2) % 3];
			// of the edge's two ends, the one that left its place: the further from the third corner
			const int32 Far = FVector3f::Dist(P[A], P[Third]) > FVector3f::Dist(P[B], P[Third]) ? int32(A) : int32(B);
			const float Ratio = Now / Ref;
			if (Now - Ref > Out.Grow)
			{
				Out.Grow = Now - Ref;
				Out.GrowRef = Ref;
				GrowVertex = Far;
			}
			if (Ref >= MinRef)
			{
				Out.Over += Ratio > Limit ? 1 : 0;
				if (Ratio > Out.Ratio)
				{
					Out.Ratio = Ratio;
					Out.Ref = Ref;
					Out.Now = Now;
					Out.Vertex = Far;
				}
			}
			else if (Ref >= MinCrease)
			{
				Out.CreaseOver += Ratio > Limit ? 1 : 0;
				if (Ratio > Out.Crease)
				{
					Out.Crease = Ratio;
					Out.CreaseRef = Ref;
					Out.CreaseNow = Now;
					CreaseVertex = Far;
				}
			}
		}
	}
	// the bone a vertex hangs on most
	const FSkinWeightVertexBuffer& SW = LOD.SkinWeightVertexBuffer;
	const auto BoneOf = [&](int32 Vertex)
	{
		FName Name;
		for (const FSkelMeshRenderSection& Section : LOD.RenderSections)
		{
			if (Vertex < int32(Section.BaseVertexIndex) || Vertex >= int32(Section.BaseVertexIndex + Section.NumVertices))
			{
				continue;
			}
			int32 Best = 0, BestWeight = -1;
			for (uint32 K = 0; K < SW.GetMaxBoneInfluences(); ++K)
			{
				const int32 W = int32(SW.GetBoneWeight(Vertex, K));
				if (W > BestWeight)
				{
					BestWeight = W;
					Best = int32(SW.GetBoneIndex(Vertex, K));
				}
			}
			if (Section.BoneMap.IsValidIndex(Best))
			{
				Name = Asset->GetRefSkeleton().GetBoneName(Section.BoneMap[Best]);
			}
		}
		return Name;
	};
	Out.Bone = BoneOf(Out.Vertex);
	Out.GrowBone = BoneOf(GrowVertex);
	Out.CreaseBone = BoneOf(CreaseVertex);
	Out.bValid = true;
	return true;
}

bool FVisexSpike::Fails() const
{
	return Ratio > FVisexMHSpikes::Limit || Grow > FVisexMHSpikes::MaxGrow;
}

namespace
{
	/** Keeps the worse of each measure. */
	void Merge(FVisexSpike& Into, const FVisexSpike& S)
	{
		Into.Over = FMath::Max(Into.Over, S.Over);
		if (!Into.bValid || S.Ratio > Into.Ratio)
		{
			Into.Ratio = S.Ratio;
			Into.Ref = S.Ref;
			Into.Now = S.Now;
			Into.Vertex = S.Vertex;
			Into.Bone = S.Bone;
		}
		if (!Into.bValid || S.Grow > Into.Grow)
		{
			Into.Grow = S.Grow;
			Into.GrowRef = S.GrowRef;
			Into.GrowBone = S.GrowBone;
		}
		Into.CreaseOver = FMath::Max(Into.CreaseOver, S.CreaseOver);
		if (!Into.bValid || S.Crease > Into.Crease)
		{
			Into.Crease = S.Crease;
			Into.CreaseRef = S.CreaseRef;
			Into.CreaseNow = S.CreaseNow;
			Into.CreaseBone = S.CreaseBone;
		}
		Into.bValid = true;
	}
}

namespace
{
	void MergePoke(FVisexPoke& Into, const FVisexPoke& P)
	{
		Into.Under = FMath::Max(Into.Under, P.Under);
		Into.EdgeUnder = FMath::Max(Into.EdgeUnder, P.EdgeUnder);
		Into.Tracked = FMath::Max(Into.Tracked, P.Tracked);
		if (!Into.bValid || P.Depth > Into.Depth)
		{
			Into.Depth = P.Depth;
			Into.Bone = P.Bone;
		}
		if (!Into.bValid || P.EdgeDepth > Into.EdgeDepth)
		{
			Into.EdgeDepth = P.EdgeDepth;
			Into.EdgeBone = P.EdgeBone;
		}
		Into.bValid = true;
	}
}

bool FVisexPoke::Fails() const
{
	return EdgeDepth > FVisexMHSpikes::MaxPoke;
}

void FVisexMHSpikes::Tick(ACharacter* Her)
{
	USkeletalMeshComponent* Meshes[Count] = {UVisexMHBodyComponent::ShownBody(Her), UVisexMHBodyComponent::ShownFace(Her), UVisexMHBodyComponent::ShownGarment(Her)};
	if (!Meshes[0])
	{
		return;
	}
	// every joint is only evaluated at LOD 0 (the correctives stop at LOD 2)
	if (ULODSyncComponent* Sync = Meshes[0]->GetOwner()->FindComponentByClass<ULODSyncComponent>())
	{
		Sync->ForcedLOD = 0;
	}
	++Frames;
	for (int32 I = 0; I < Count; ++I)
	{
		FVisexSpike S;
		if (Meshes[I] && Meshes[I]->GetPredictedLODLevel() == 0 && Measure(Meshes[I], S))
		{
			Merge(Worst[I], S);
		}
	}
	FVisexPoke P;
	if (Meshes[Garment] && Meshes[Body]->GetPredictedLODLevel() == 0 && MeasurePoke(Meshes[Body], Meshes[Garment], P))
	{
		MergePoke(WorstPoke, P);
	}
}

void FVisexMHSpikes::SetPhase(const FString& Name)
{
	static const TCHAR* Names[Count] = {TEXT("body"), TEXT("face"), TEXT("garment")};
	FString Line = FString::Printf(TEXT("VisexSpikes: %s (%d frames):"), *Phase, Frames);
	for (int32 I = 0; I < Count; ++I)
	{
		const FVisexSpike& S = Worst[I];
		if (!S.bValid)
		{
			Line += FString::Printf(TEXT(" %s not measured;"), Names[I]);
			continue;
		}
		Line += FString::Printf(TEXT(" %s %s: stretch %.2fx (%.2f to %.2f cm on %s), %d over %.0fx; creases %.2fx (%.2f to %.2f cm on %s), %d over; grew %.2f cm at most (a %.2f cm edge on %s);"),
			Names[I], S.Fails() ? TEXT("FAIL") : TEXT("pass"), S.Ratio, S.Ref, S.Now, *S.Bone.ToString(), S.Over, Limit,
			S.Crease, S.CreaseRef, S.CreaseNow, *S.CreaseBone.ToString(), S.CreaseOver, S.Grow, S.GrowRef, *S.GrowBone.ToString());
		Merge(Total[I], S);
	}
	if (WorstPoke.bValid)
	{
		const FVisexPoke& K = WorstPoke;
		Line += FString::Printf(TEXT(" poke %s: where skin is drawn (%.1f cm from a fabric edge) %d fabric vertices under it, deepest %.2f cm on %s; under clipped skin %d, deepest %.2f cm on %s (%d compared);"),
			K.Fails() ? TEXT("FAIL") : TEXT("pass"), EdgeBand, K.EdgeUnder, K.EdgeDepth, *K.EdgeBone.ToString(), K.Under, K.Depth, *K.Bone.ToString(), K.Tracked);
		MergePoke(TotalPoke, K);
	}
	if (Frames > 0 && Log)
	{
		Log(Line);
	}
	for (FVisexSpike& W : Worst)
	{
		W = FVisexSpike();
	}
	WorstPoke = FVisexPoke();
	Frames = 0;
	Phase = Name;
}

// visex.MH.Spikes: the check, once, on whoever wears the MetaHuman body in this world
static FAutoConsoleCommandWithWorld CmdVisexMHSpikes(
	TEXT("visex.MH.Spikes"),
	TEXT("Logs the most stretched skin edge of the MetaHuman body and face in their current pose (LOD 0)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		for (TActorIterator<ACharacter> It(World); It; ++It)
		{
			if (!UVisexMHBodyComponent::ShownBody(*It))
			{
				continue;
			}
			FVisexMHSpikes Check;
			Check.Log = [](const FString& Line) { UE_LOG(LogVisexSpikes, Display, TEXT("%s"), *Line); };
			Check.Tick(*It);
			Check.SetPhase(TEXT("done"));
		}
	}));

// visex.MH.Sections: what each LOD of the MetaHuman's meshes draws with which material
static FAutoConsoleCommandWithWorld CmdVisexMHSections(
	TEXT("visex.MH.Sections"),
	TEXT("Logs the render sections of the shown MetaHuman's face and body per LOD: material, triangles, where they are."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		for (TActorIterator<ACharacter> It(World); It; ++It)
		{
			for (USkeletalMeshComponent* Mesh : {UVisexMHBodyComponent::ShownFace(*It), UVisexMHBodyComponent::ShownBody(*It)})
			{
				const USkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
				const FSkeletalMeshRenderData* RD = Asset ? Asset->GetResourceForRendering() : nullptr;
				for (int32 L = 0; RD && L < RD->LODRenderData.Num(); ++L)
				{
					const FSkeletalMeshLODRenderData& LOD = RD->LODRenderData[L];
					const FSkeletalMeshLODInfo* Info = Asset->GetLODInfo(L);
					const FPositionVertexBuffer& PB = LOD.StaticVertexBuffers.PositionVertexBuffer;
					FString Line = FString::Printf(TEXT("VisexMH: %s LOD %d:"), *Asset->GetName(), L);
					for (int32 S = 0; S < LOD.RenderSections.Num(); ++S)
					{
						const FSkelMeshRenderSection& Sec = LOD.RenderSections[S];
						const int32 Slot = Info && Info->LODMaterialMap.IsValidIndex(S) && Info->LODMaterialMap[S] != INDEX_NONE ? Info->LODMaterialMap[S] : int32(Sec.MaterialIndex);
						FVector3f Mid = FVector3f::ZeroVector;
						if (PB.GetVertexData() && Sec.NumVertices > 0)
						{
							for (uint32 V = 0; V < Sec.NumVertices; ++V) { Mid += PB.VertexPosition(Sec.BaseVertexIndex + V); }
							Mid /= float(Sec.NumVertices);
						}
						Line += FString::Printf(TEXT(" [%d own %d draws %d %s, %d tris at %.1f %.1f %.1f, %d bones]"), S, int32(Sec.MaterialIndex), Slot,
							Asset->GetMaterials().IsValidIndex(Slot) ? *Asset->GetMaterials()[Slot].MaterialSlotName.ToString() : TEXT("?"), int32(Sec.NumTriangles), Mid.X, Mid.Y, Mid.Z, Sec.BoneMap.Num());
					}
					UE_LOG(LogVisexSpikes, Display, TEXT("%s"), *Line);
				}
			}
		}
	}));

namespace
{
	/** Area-weighted vertex normals from a triangle list (winding as stored; the caller fixes the sign). */
	void VertexNormals(const TArray<uint32>& Index, const TFunctionRef<FVector3f(uint32)>& At, int32 Num, TArray<FVector3f>& N)
	{
		N.Init(FVector3f::ZeroVector, Num);
		for (int32 I = 0; I + 2 < Index.Num(); I += 3)
		{
			const FVector3f A = At(Index[I]), B = At(Index[I + 1]), C = At(Index[I + 2]);
			const FVector3f Face = FVector3f::CrossProduct(B - A, C - A);
			N[Index[I]] += Face;
			N[Index[I + 1]] += Face;
			N[Index[I + 2]] += Face;
		}
		for (FVector3f& V : N)
		{
			V = V.GetSafeNormal();
		}
	}

	FName DominantBone(const USkeletalMesh* Asset, const FSkeletalMeshLODRenderData& LOD, int32 Vertex)
	{
		const FSkinWeightVertexBuffer& SW = LOD.SkinWeightVertexBuffer;
		for (const FSkelMeshRenderSection& Section : LOD.RenderSections)
		{
			if (Vertex < int32(Section.BaseVertexIndex) || Vertex >= int32(Section.BaseVertexIndex + Section.NumVertices))
			{
				continue;
			}
			int32 Best = 0, BestWeight = -1;
			for (uint32 K = 0; K < SW.GetMaxBoneInfluences(); ++K)
			{
				const int32 W = int32(SW.GetBoneWeight(Vertex, K));
				if (W > BestWeight)
				{
					BestWeight = W;
					Best = int32(SW.GetBoneIndex(Vertex, K));
				}
			}
			return Section.BoneMap.IsValidIndex(Best) ? Asset->GetRefSkeleton().GetBoneName(Section.BoneMap[Best]) : FName();
		}
		return FName();
	}
}

bool FVisexMHSpikes::MeasurePoke(USkeletalMeshComponent* BodyMesh, USkeletalMeshComponent* GarmentMesh, FVisexPoke& Out)
{
	Out = FVisexPoke();
	const USkeletalMesh* BA = BodyMesh ? BodyMesh->GetSkeletalMeshAsset() : nullptr;
	const USkeletalMesh* GA = GarmentMesh ? GarmentMesh->GetSkeletalMeshAsset() : nullptr;
	FSkeletalMeshRenderData* BR = BodyMesh ? BodyMesh->GetSkeletalMeshRenderData() : nullptr;
	FSkeletalMeshRenderData* GR = GarmentMesh ? GarmentMesh->GetSkeletalMeshRenderData() : nullptr;
	if (!BA || !GA || !BR || !GR || BR->LODRenderData.Num() == 0 || GR->LODRenderData.Num() == 0)
	{
		return false;
	}
	const FSkeletalMeshLODRenderData& BL = BR->LODRenderData[0];
	const FSkeletalMeshLODRenderData& GL = GR->LODRenderData[0];
	const FPositionVertexBuffer& BP = BL.StaticVertexBuffers.PositionVertexBuffer;
	const FPositionVertexBuffer& GP = GL.StaticVertexBuffers.PositionVertexBuffer;
	if (BP.GetNumVertices() == 0 || GP.GetNumVertices() == 0 || !BP.GetVertexData() || !GP.GetVertexData())
	{
		return false;
	}
	FPokeCache& C = PokeCache;
	if (C.Body != BA || C.Garment != GA)
	{
		// once, in the reference pose: each fabric vertex that lies on the skin (0.05-0.8 cm above
		// it, the body vertex under it within 0.7 cm) and whether it is near a cut edge (UV2.x, cm)
		C = FPokeCache();
		C.Body = BA;
		C.Garment = GA;
		BL.MultiSizeIndexContainer.GetIndexBuffer(C.BodyIndex);
		TArray<FVector3f> N;
		VertexNormals(C.BodyIndex, [&BP](uint32 V) { return BP.VertexPosition(V); }, BP.GetNumVertices(), N);
		TMap<FIntVector, TArray<int32>> Grid;
		const auto Cell = [](const FVector3f& P) { return FIntVector(FMath::FloorToInt(P.X), FMath::FloorToInt(P.Y), FMath::FloorToInt(P.Z)); };
		for (uint32 V = 0; V < BP.GetNumVertices(); ++V)
		{
			Grid.FindOrAdd(Cell(BP.VertexPosition(V))).Add(int32(V));
		}
		const FStaticMeshVertexBuffer& UV = GL.StaticVertexBuffers.StaticMeshVertexBuffer;
		const bool bSeamUV = UV.GetNumTexCoords() >= 3;
		TArray<float> Height;
		int32 Plus = 0, Minus = 0;
		for (uint32 V = 0; V < GP.GetNumVertices(); ++V)
		{
			const float Edge = bSeamUV ? UV.GetVertexUV(V, 2).X : 0.f;
			if (Edge > 8.9f)
			{
				continue;   // cords, tapes and hardware: not fabric cut from the skin
			}
			const FVector3f P = GP.VertexPosition(V);
			const FIntVector At = Cell(P);
			int32 Best = INDEX_NONE;
			float BestD = FMath::Square(1.2f);
			for (int32 X = -1; X <= 1; ++X) for (int32 Y = -1; Y <= 1; ++Y) for (int32 Z = -1; Z <= 1; ++Z)
			{
				if (const TArray<int32>* Bin = Grid.Find(At + FIntVector(X, Y, Z)))
				{
					for (int32 B : *Bin)
					{
						const float D = FVector3f::DistSquared(P, BP.VertexPosition(B));
						if (D < BestD) { BestD = D; Best = B; }
					}
				}
			}
			if (Best == INDEX_NONE)
			{
				continue;
			}
			const FVector3f Off = P - BP.VertexPosition(Best);
			const float H = FVector3f::DotProduct(Off, N[Best]);
			if (FMath::Abs(H) < 0.05f || FMath::Abs(H) > 0.8f || (Off - N[Best] * H).Size() > 0.7f)
			{
				continue;
			}
			(H > 0.f ? Plus : Minus)++;
			C.Vertex.Add(int32(V));
			C.Under.Add(Best);
			C.bEdge.Add(Edge < EdgeBand ? 1 : 0);
			Height.Add(H);
		}
		// fabric is outside the skin in the reference pose: that tells which way the stored winding faces
		C.Sign = Plus >= Minus ? 1.f : -1.f;
		for (int32 I = C.Vertex.Num() - 1; I >= 0; --I)
		{
			if (Height[I] * C.Sign < 0.f)
			{
				C.Vertex.RemoveAtSwap(I); C.Under.RemoveAtSwap(I); C.bEdge.RemoveAtSwap(I); Height.RemoveAtSwap(I);
			}
		}
	}
	TArray<FMatrix44f> RefToLocal;
	TArray<FVector3f> PB, PG, N;
	BodyMesh->CacheRefToLocalMatrices(RefToLocal);
	USkinnedMeshComponent::ComputeSkinnedPositions(BodyMesh, PB, RefToLocal, BL, BL.SkinWeightVertexBuffer);
	GarmentMesh->CacheRefToLocalMatrices(RefToLocal);
	USkinnedMeshComponent::ComputeSkinnedPositions(GarmentMesh, PG, RefToLocal, GL, GL.SkinWeightVertexBuffer);
	if (PB.Num() != int32(BP.GetNumVertices()) || PG.Num() != int32(GP.GetNumVertices()) || C.Vertex.Num() == 0)
	{
		return false;
	}
	VertexNormals(C.BodyIndex, [&PB](uint32 V) { return PB[V]; }, PB.Num(), N);
	int32 Deep = INDEX_NONE, EdgeDeep = INDEX_NONE;
	for (int32 I = 0; I < C.Vertex.Num(); ++I)
	{
		const int32 B = C.Under[I];
		const float Depth = -C.Sign * FVector3f::DotProduct(PG[C.Vertex[I]] - PB[B], N[B]);
		if (Depth <= 0.02f)
		{
			continue;
		}
		++Out.Under;
		if (Depth > Out.Depth) { Out.Depth = Depth; Deep = C.Vertex[I]; }
		if (C.bEdge[I])
		{
			++Out.EdgeUnder;
			if (Depth > Out.EdgeDepth) { Out.EdgeDepth = Depth; EdgeDeep = C.Vertex[I]; }
		}
	}
	Out.Bone = Deep != INDEX_NONE ? DominantBone(GA, GL, Deep) : FName();
	Out.EdgeBone = EdgeDeep != INDEX_NONE ? DominantBone(GA, GL, EdgeDeep) : FName();
	Out.Tracked = C.Vertex.Num();
	Out.bValid = true;
	return true;
}
