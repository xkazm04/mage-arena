#pragma once

#include "CoreMinimal.h"

class AActor;
class FJsonObject;
class UStaticMeshComponent;

/** T46: the pass a part draws in. Per-pass splits are diagnostics; the D-G4 row stays one draw-call total (DECISIONS 2026-10-16). */
enum class ECensusPass : uint8
{
	Opaque,
	Translucent,
};

/** One kind of drawn part: mesh, colour, base material and pass. Colours compare at 1/1000 per channel, alpha included. */
struct MAGEARENAVR_API FCensusKey
{
	FString Mesh;
	FLinearColor Colour = FLinearColor::Transparent;
	/** False for a part whose material has no vector parameter (a textured pass-through plane): its colour is not compared. */
	bool bTinted = true;
	FString Material;
	ECensusPass Pass = ECensusPass::Opaque;

	/** Same mesh, material and pass. */
	bool SameKind(const FCensusKey& Other) const;
	/** Same kind and the same colour. */
	bool Matches(const FCensusKey& Other) const;
	/** "Cube (0.280, 0.070, 0.020, 1.000) LevelColorationUnlitMaterial opaque". */
	FString ToString() const;
	FString ColourText() const;
};

struct MAGEARENAVR_API FCensusRow
{
	FCensusKey Key;
	int32 Count = 0;
	/** What the parts are, for a reader ("cinder_hound body"). Planned rows carry one; drawn rows do not. */
	FString Role;
};

/** T46: the distinct (mesh, colour, material, pass) tuples of a frame's parts and the number of parts on each. */
struct MAGEARENAVR_API FSceneCensus
{
	TArray<FCensusRow> Rows;
	/** Passes the frame must draw, beyond the passes of the rows. */
	TArray<ECensusPass> RequiredPasses;

	/** Adds Count parts of Key, to the row it matches or a new one. */
	void Add(const FCensusKey& Key, int32 Count = 1, const FString& Role = FString());
	int32 CountOf(const FCensusKey& Key) const;
	int32 Parts() const;
	bool HasPass(ECensusPass Pass) const;
	/** MD5 over the sorted rows (colours at 3 decimals): two equal censuses give one hash. */
	FString Hash() const;
	TSharedRef<FJsonObject> ToJson() const;
};

struct MAGEARENAVR_API FSceneParity
{
	bool bOk = true;
	/** One line per divergence: the planned rows drawn short, then the missing passes. */
	TArray<FString> Divergences;
	/** Drawn rows that no planned row matches: the session's own parts. Listed, never failed. */
	TArray<FCensusRow> Extras;
};

/**
 * The scene-parity rule (runs/T46/parity-census.md section 4). Every planned row must be drawn at least its planned count:
 * the stress population is a deterministic function of the plan, so the tolerance on it is zero parts. Every pass the plan
 * holds or requires must be drawn. Drawn parts beyond the plan are the session's own frame and go to Extras.
 * A short row names the drawn colours of the same kind that no planned row holds, so a recoloured part reads as both
 * colours.
 */
MAGEARENAVR_API FSceneParity CompareSceneCensus(const FSceneCensus& Planned, const FSceneCensus& Drawn);

/** The planned key of an opaque greybox part: the shared unlit tint (Greybox::SharedTint) of Colour on an engine shape. */
MAGEARENAVR_API FCensusKey OpaquePartKey(const TCHAR* Mesh, const FLinearColor& Colour);

/** The key of a part as it draws: its mesh, its material's first vector parameter, its base material and the material's pass. */
MAGEARENAVR_API bool CensusKeyOfPart(const UStaticMeshComponent* Part, FCensusKey& OutKey);

/** Adds every registered, visible mesh component of Actor that has a mesh and a material. */
MAGEARENAVR_API void AddDrawnParts(FSceneCensus& Census, const AActor& Actor);
