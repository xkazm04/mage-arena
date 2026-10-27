#pragma once

#include "CoreMinimal.h"
#include "Hands/HandFrame.h"
#include "HeadMountedDisplayTypes.h"

class UMaterialInterface;
class UObject;
class UStaticMesh;

/**
 * The unit meshes the hand presentation draws, each one instanced layer. Both are built in code (BuildPartMesh), no asset:
 * a finger segment (round, tapering toward the tip) per phalanx, and a palm block (a rounded slab) per metacarpal,
 * carpal and the forearm, which overlap into one palm.
 */
enum class EHandPart : uint8
{
	Finger,
	Palm,
	Count
};

/** One drawn part of one hand, in the hand presentation's space (clip centimetres on the seated origin). */
struct FHandPartPose
{
	EHandPart Part = EHandPart::Finger;
	FVector CentreCm = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity;
	/** Full size along the unit mesh's X, Y and Z. Z runs along the bone. */
	FVector SizeCm = FVector::OneVector;
	/** EHandKeypoint the part runs from and to. Both INDEX_NONE for a part no single bone owns. */
	int32 From = INDEX_NONE;
	int32 To = INDEX_NONE;
};

namespace MageHandMesh
{
/** The parts that draw Frame. Empty when the frame has fewer than MageHandJointCount joints. */
void PoseParts(const FHandFrame& Frame, TArray<FHandPartPose>& Out);

/** The unit mesh for Part (bounds -0.5..0.5 cm on each axis), made under Outer. */
UStaticMesh* BuildPartMesh(EHandPart Part, UObject* Outer, UMaterialInterface* Material);
}
