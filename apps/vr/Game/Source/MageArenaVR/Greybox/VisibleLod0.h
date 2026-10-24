#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
class UWorld;
struct FMinimalViewInfo;

/**
 * T46: the one visible-LOD0 triangle sum, used by both captures (UGreyboxCaptureDriver's budget.json and
 * UBudgetStressDriver's D-G4 run). LOD0 triangles of every visible static mesh component and instance whose bounds touch
 * the view frustum. No occlusion and no GPU culling: an estimate, not a counter (docs/research/BUDGET-DG4-2026-10.md
 * section 4). It has no independent desktop cross-check; the device number comes from V1.
 */
namespace Greybox
{
/** LOD0 triangles of Mesh; 0 without render data. */
MAGEARENAVR_API int32 MeshLod0Triangles(const UStaticMesh* Mesh);
MAGEARENAVR_API int32 CountVisibleLod0(UWorld* World, const FMinimalViewInfo& View);
/** The first player's camera view. False without a player controller and camera manager. */
MAGEARENAVR_API bool FirstPlayerView(UWorld* World, FMinimalViewInfo& OutView);
}
