#pragma once

#include "CoreMinimal.h"

class USkinnedAsset;
class UStaticMesh;
class UWorld;
struct FMinimalViewInfo;

/**
 * T46: the one visible-LOD0 triangle sum, used by both captures (UGreyboxCaptureDriver's budget.json and
 * UBudgetStressDriver's D-G4 run). LOD0 triangles of every visible static mesh component and instance, and of every
 * visible skinned mesh component (docs/research/DRAWCALL-BUDGET-2026-10-25.md: the creatures), whose bounds touch the view
 * frustum. No occlusion and no GPU culling: an estimate, not a counter (docs/research/BUDGET-DG4-2026-10.md section 4). It
 * has no independent desktop cross-check; the device number comes from V1.
 */
namespace Greybox
{
/** The in-frustum LOD0 sum, split so a row from before the skinned count (static only) stays comparable. */
struct FVisibleLod0
{
	int32 StaticTriangles = 0;
	/** USkinnedMeshComponent and its subclasses (USkeletalMeshComponent): the LOD0 render data, whatever LOD draws. */
	int32 SkeletalTriangles = 0;

	int32 Total() const
	{
		return StaticTriangles + SkeletalTriangles;
	}
};

/** LOD0 triangles of Mesh; 0 without render data. */
MAGEARENAVR_API int32 MeshLod0Triangles(const UStaticMesh* Mesh);
/** LOD0 triangles of a skinned asset's render data (every section); 0 without render data. */
MAGEARENAVR_API int32 SkinnedLod0Triangles(const USkinnedAsset* Asset);
MAGEARENAVR_API FVisibleLod0 CountVisibleLod0Split(UWorld* World, const FMinimalViewInfo& View);
/** CountVisibleLod0Split(...).Total(). */
MAGEARENAVR_API int32 CountVisibleLod0(UWorld* World, const FMinimalViewInfo& View);
/** The first player's camera view. False without a player controller and camera manager. */
MAGEARENAVR_API bool FirstPlayerView(UWorld* World, FMinimalViewInfo& OutView);
}
