#pragma once

#include "CoreMinimal.h"

class ACharacter;
class USkeletalMesh;
class USkeletalMeshComponent;

/** The most stretched triangle edges of a skinned mesh in its current pose. */
struct FVisexSpike
{
	// edges of real size (MinRef cm or more in the reference pose): posed length / reference length
	float Ratio = 0.f;
	float Ref = 0.f, Now = 0.f;   // that edge, cm in the mesh's own space
	int32 Vertex = INDEX_NONE;
	FName Bone;                   // the bone that vertex is weighted to most
	int32 Over = 0;               // such edges past Limit
	// the short edges packed into the creases (groin, under the seat, armpit), which unfold when
	// the joint opens: reported, looked at, not failed on the ratio alone
	float Crease = 0.f, CreaseRef = 0.f, CreaseNow = 0.f;
	FName CreaseBone;
	int32 CreaseOver = 0;
	// the edge that grew most in cm, any length
	float Grow = 0.f, GrowRef = 0.f;
	FName GrowBone;
	bool Fails() const;
	bool bValid = false;
};

/** Fabric against the skin under it, in the pose on screen (FVisexMHSpikes::MeasurePoke). */
struct FVisexPoke
{
	// fabric vertices lying under the skin's surface, and the deepest, cm: anywhere on the garment
	int32 Under = 0;
	float Depth = 0.f;
	FName Bone;
	// ... and within EdgeBand cm of a cut edge, where the skin is still drawn (further in, the
	// body's material clips it away, so fabric under the surface there shows nothing)
	int32 EdgeUnder = 0;
	float EdgeDepth = 0.f;
	FName EdgeBone;
	int32 Tracked = 0;            // fabric vertices compared
	bool Fails() const;
	bool bValid = false;
};

/**
 * The spike check for the MetaHuman body (visex.Body.MetaHuman 1): skins the body and the face on
 * the CPU at LOD 0 in the pose they are drawn in and compares every triangle edge with its length
 * in the reference pose. Skin that shoots off (a vertex bound to the wrong bone, a corrective joint
 * that flew away) shows as an edge many times its length and many centimetres longer; healthy
 * skin stays under 3x on edges of 1 cm or more, and only the short edges folded into the creases
 * (groin, under the seat, armpit) open further in a deep bend, by a centimetre or two.
 * Run every frame by the tour (-visexqa -visexspikes) with a line per pose, or once by the console
 * command visex.MH.Spikes. Holds the figure at LOD 0 while it measures.
 */
struct FVisexMHSpikes
{
	/** Fails: an edge of MinRef cm or more stretched past Limit times its length, or any edge grown
	 *  by more than MaxGrow cm. Edges from MinCrease to MinRef cm are the crease report. */
	static constexpr float Limit = 3.f, MinRef = 1.f, MinCrease = 0.25f, MaxGrow = 3.f;
	/** Poke-through: skin is drawn within EdgeBand cm of a fabric edge (the cover mask's margin and
	 *  its ramp); fabric more than MaxPoke cm under the skin there fails. */
	static constexpr float EdgeBand = 1.3f, MaxPoke = 0.1f;
	enum { Body, Face, Garment, Count };

	TFunction<void(const FString&)> Log;

	/** Measures this frame; nothing while the MetaHuman body is not shown. */
	void Tick(ACharacter* Her);
	/** Logs the finished phase's worst edges and starts the next. */
	void SetPhase(const FString& Name);

	/** One mesh, now. False when it has no CPU vertex data. */
	bool Measure(USkeletalMeshComponent* Mesh, FVisexSpike& Out);

	/** How far the garment's fabric sits under the body's skin, now. False without a garment or CPU data. */
	bool MeasurePoke(USkeletalMeshComponent* BodyMesh, USkeletalMeshComponent* GarmentMesh, FVisexPoke& Out);
	/** The worst over every phase so far (body, face, garment). */
	FVisexSpike Total[Count];
	FVisexPoke TotalPoke;

private:
	struct FCache
	{
		TWeakObjectPtr<const USkeletalMesh> Mesh;
		TArray<uint32> Index;
		TArray<float> RefLength;   // per index: the edge to the triangle's next corner
	};
	FCache Caches[Count];
	FVisexSpike Worst[Count];
	/** Per garment fabric vertex: the body vertex under it in the reference pose. */
	struct FPokeCache
	{
		TWeakObjectPtr<const USkeletalMesh> Body, Garment;
		TArray<int32> Vertex, Under;      // garment vertex, body vertex
		TArray<uint8> bEdge;              // within EdgeBand of a cut edge
		TArray<uint32> BodyIndex;
		float Sign = 1.f;                 // which way the body's stored triangle winding faces
	};
	FPokeCache PokeCache;
	FVisexPoke WorstPoke;
	FString Phase = TEXT("start");
	int32 Frames = 0;
};
