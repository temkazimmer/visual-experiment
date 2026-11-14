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
	FCache* Cache = Caches[0].Mesh == Asset ? &Caches[0] : Caches[1].Mesh == Asset ? &Caches[1] : nullptr;
	if (!Cache)
	{
		Cache = !Caches[0].Mesh.IsValid() ? &Caches[0] : &Caches[1];
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

void FVisexMHSpikes::Tick(ACharacter* Her)
{
	USkeletalMeshComponent* Meshes[2] = {UVisexMHBodyComponent::ShownBody(Her), UVisexMHBodyComponent::ShownFace(Her)};
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
	for (int32 I = 0; I < 2; ++I)
	{
		FVisexSpike S;
		if (Meshes[I] && Meshes[I]->GetPredictedLODLevel() == 0 && Measure(Meshes[I], S))
		{
			Merge(Worst[I], S);
		}
	}
}

void FVisexMHSpikes::SetPhase(const FString& Name)
{
	static const TCHAR* Names[2] = {TEXT("body"), TEXT("face")};
	FString Line = FString::Printf(TEXT("VisexSpikes: %s (%d frames):"), *Phase, Frames);
	for (int32 I = 0; I < 2; ++I)
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
	if (Frames > 0 && Log)
	{
		Log(Line);
	}
	Worst[0] = Worst[1] = FVisexSpike();
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
