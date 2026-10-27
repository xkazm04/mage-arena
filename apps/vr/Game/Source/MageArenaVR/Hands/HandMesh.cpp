#include "Hands/HandMesh.h"

#include "Greybox/GreyboxUtil.h"

namespace
{
// arena-layout.json hands.jointRadiusM and boneRadiusM.
constexpr double JointCm = 0.012 * 2.0 * 100.0;
constexpr double BoneCm = 0.0045 * 2.0 * 100.0;

const int32 BoneLinks[][2] = {
    {static_cast<int32>(EHandKeypoint::Wrist), static_cast<int32>(EHandKeypoint::Palm)},
    {static_cast<int32>(EHandKeypoint::Wrist), static_cast<int32>(EHandKeypoint::ThumbMetacarpal)},
    {static_cast<int32>(EHandKeypoint::ThumbMetacarpal), static_cast<int32>(EHandKeypoint::ThumbProximal)},
    {static_cast<int32>(EHandKeypoint::ThumbProximal), static_cast<int32>(EHandKeypoint::ThumbDistal)},
    {static_cast<int32>(EHandKeypoint::ThumbDistal), static_cast<int32>(EHandKeypoint::ThumbTip)},
    {static_cast<int32>(EHandKeypoint::Wrist), static_cast<int32>(EHandKeypoint::IndexMetacarpal)},
    {static_cast<int32>(EHandKeypoint::IndexMetacarpal), static_cast<int32>(EHandKeypoint::IndexProximal)},
    {static_cast<int32>(EHandKeypoint::IndexProximal), static_cast<int32>(EHandKeypoint::IndexIntermediate)},
    {static_cast<int32>(EHandKeypoint::IndexIntermediate), static_cast<int32>(EHandKeypoint::IndexDistal)},
    {static_cast<int32>(EHandKeypoint::IndexDistal), static_cast<int32>(EHandKeypoint::IndexTip)},
    {static_cast<int32>(EHandKeypoint::Wrist), static_cast<int32>(EHandKeypoint::MiddleMetacarpal)},
    {static_cast<int32>(EHandKeypoint::MiddleMetacarpal), static_cast<int32>(EHandKeypoint::MiddleProximal)},
    {static_cast<int32>(EHandKeypoint::MiddleProximal), static_cast<int32>(EHandKeypoint::MiddleIntermediate)},
    {static_cast<int32>(EHandKeypoint::MiddleIntermediate), static_cast<int32>(EHandKeypoint::MiddleDistal)},
    {static_cast<int32>(EHandKeypoint::MiddleDistal), static_cast<int32>(EHandKeypoint::MiddleTip)},
    {static_cast<int32>(EHandKeypoint::Wrist), static_cast<int32>(EHandKeypoint::RingMetacarpal)},
    {static_cast<int32>(EHandKeypoint::RingMetacarpal), static_cast<int32>(EHandKeypoint::RingProximal)},
    {static_cast<int32>(EHandKeypoint::RingProximal), static_cast<int32>(EHandKeypoint::RingIntermediate)},
    {static_cast<int32>(EHandKeypoint::RingIntermediate), static_cast<int32>(EHandKeypoint::RingDistal)},
    {static_cast<int32>(EHandKeypoint::RingDistal), static_cast<int32>(EHandKeypoint::RingTip)},
    {static_cast<int32>(EHandKeypoint::Wrist), static_cast<int32>(EHandKeypoint::LittleMetacarpal)},
    {static_cast<int32>(EHandKeypoint::LittleMetacarpal), static_cast<int32>(EHandKeypoint::LittleProximal)},
    {static_cast<int32>(EHandKeypoint::LittleProximal), static_cast<int32>(EHandKeypoint::LittleIntermediate)},
    {static_cast<int32>(EHandKeypoint::LittleIntermediate), static_cast<int32>(EHandKeypoint::LittleDistal)},
    {static_cast<int32>(EHandKeypoint::LittleDistal), static_cast<int32>(EHandKeypoint::LittleTip)},
};
}

void MageHandMesh::PoseParts(const FHandFrame& Frame, TArray<FHandPartPose>& Out)
{
	Out.Reset();
	if (Frame.Joints.Num() < MageHandJointCount)
	{
		return;
	}
	for (int32 Joint = 0; Joint < MageHandJointCount; ++Joint)
	{
		FHandPartPose& Pose = Out.AddDefaulted_GetRef();
		Pose.Part = EHandPart::Joint;
		Pose.CentreCm = Frame.Joints[Joint].Location;
		Pose.SizeCm = FVector(JointCm);
		Pose.From = Joint;
		Pose.To = Joint;
	}
	for (int32 LinkIndex = 0; LinkIndex < UE_ARRAY_COUNT(BoneLinks); ++LinkIndex)
	{
		const FVector A = Frame.Joints[BoneLinks[LinkIndex][0]].Location;
		const FVector B = Frame.Joints[BoneLinks[LinkIndex][1]].Location;
		const double Length = FVector::Distance(A, B);
		if (Length < 0.4)
		{
			continue;
		}
		FHandPartPose& Pose = Out.AddDefaulted_GetRef();
		Pose.Part = EHandPart::Bone;
		Pose.CentreCm = (A + B) * 0.5;
		Pose.Rotation = FQuat::FindBetweenNormals(FVector::UpVector, (B - A).GetSafeNormal());
		Pose.SizeCm = FVector(BoneCm, BoneCm, Length);
		Pose.From = BoneLinks[LinkIndex][0];
		Pose.To = BoneLinks[LinkIndex][1];
	}
}

UStaticMesh* MageHandMesh::BuildPartMesh(EHandPart Part, UObject* Outer, UMaterialInterface* Material)
{
	switch (Part)
	{
	case EHandPart::Joint:
		return Greybox::LoadShape(TEXT("Sphere"));
	case EHandPart::Bone:
		return Greybox::LoadShape(TEXT("Cylinder"));
	default:
		return nullptr;
	}
}
