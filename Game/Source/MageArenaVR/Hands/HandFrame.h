#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

/** Clip schema v1 stores one pose per EHandKeypoint, in enum order. */
inline constexpr int32 MageHandJointCount = 26;

/** Clip positions are metres. Unreal world positions are centimetres. */
inline constexpr double MageClipMetresToCentimetres = 100.0;

/** One joint in Unreal space: location in centimetres, rotation in seated-origin space. */
struct FHandJointPose
{
	FVector Location = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity;
};

/**
 * One hand at one sample. Joints[i] is EHandKeypoint value i
 * (Palm, Wrist, ThumbMetacarpal ... LittleTip).
 */
struct FHandFrame
{
	double TimeSeconds = 0.0;
	EControllerHand Hand = EControllerHand::Left;
	float Confidence = 0.0f;
	float Pinch = 0.0f;
	TArray<FHandJointPose> Joints;
};
