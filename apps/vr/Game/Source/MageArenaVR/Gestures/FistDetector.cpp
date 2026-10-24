#include "Gestures/FistDetector.h"

namespace
{
const EHandKeypoint GFingers[] = {
    EHandKeypoint::IndexMetacarpal,
    EHandKeypoint::MiddleMetacarpal,
    EHandKeypoint::RingMetacarpal,
    EHandKeypoint::LittleMetacarpal,
};

double AngleDeg(const FVector& A, const FVector& B)
{
	if (A.IsNearlyZero() || B.IsNearlyZero())
	{
		return 0.0;
	}
	const double Dot = FVector::DotProduct(A.GetSafeNormal(), B.GetSafeNormal());
	return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0, 1.0)));
}
}

double FFistDetector::FingerCurlDeg(const FHandFrame& Frame, EHandKeypoint Metacarpal)
{
	// Metacarpal, Proximal, Intermediate, Distal, Tip are consecutive in EHandKeypoint.
	const int32 First = static_cast<int32>(Metacarpal);
	if (!Frame.Joints.IsValidIndex(First + 4))
	{
		return 0.0;
	}
	double Sum = 0.0;
	for (int32 Bone = 0; Bone < 3; ++Bone)
	{
		const FVector A = Frame.Joints[First + Bone + 1].Location - Frame.Joints[First + Bone].Location;
		const FVector B = Frame.Joints[First + Bone + 2].Location - Frame.Joints[First + Bone + 1].Location;
		Sum += AngleDeg(A, B);
	}
	return Sum;
}

double FFistDetector::ThumbOverMetres(const FHandFrame& Frame)
{
	const int32 Thumb = static_cast<int32>(EHandKeypoint::ThumbTip);
	const int32 Index = static_cast<int32>(EHandKeypoint::IndexIntermediate);
	const int32 Middle = static_cast<int32>(EHandKeypoint::MiddleIntermediate);
	if (!Frame.Joints.IsValidIndex(Thumb) || !Frame.Joints.IsValidIndex(Index) || !Frame.Joints.IsValidIndex(Middle))
	{
		return TNumericLimits<double>::Max();
	}
	const FVector Knuckles = (Frame.Joints[Index].Location + Frame.Joints[Middle].Location) * 0.5;
	return FVector::Dist(Frame.Joints[Thumb].Location, Knuckles) / MageClipMetresToCentimetres;
}

bool FFistDetector::IsFistPose(const FHandFrame& Frame, bool bWasClosed)
{
	if (Frame.Joints.Num() < MageHandJointCount || Frame.Confidence < FFistThresholds::MinConfidence)
	{
		return false;
	}
	const double MinCurl = bWasClosed ? FFistThresholds::OpenCurlDeg : FFistThresholds::CloseCurlDeg;
	for (const EHandKeypoint Finger : GFingers)
	{
		if (FingerCurlDeg(Frame, Finger) < MinCurl)
		{
			return false;
		}
	}
	return ThumbOverMetres(Frame) <= (bWasClosed ? FFistThresholds::ThumbReleaseM : FFistThresholds::ThumbOverM);
}

bool FFistDetector::IsOpenHand(const FHandFrame& Frame)
{
	if (Frame.Joints.Num() < MageHandJointCount || Frame.Confidence < FFistThresholds::MinConfidence)
	{
		return false;
	}
	for (const EHandKeypoint Finger : GFingers)
	{
		if (FingerCurlDeg(Frame, Finger) >= FFistThresholds::OpenPalmCurlDeg)
		{
			return false;
		}
	}
	return true;
}

void FFistDetector::Reset()
{
	Left = FHandState();
	Right = FHandState();
	Gate.Reset();
	HeldCount = 0;
}

FFistDetector::FHandState* FFistDetector::StateFor(EControllerHand Hand)
{
	return Hand == EControllerHand::Left ? &Left : (Hand == EControllerHand::Right ? &Right : nullptr);
}

const FFistDetector::FHandState* FFistDetector::StateFor(EControllerHand Hand) const
{
	return Hand == EControllerHand::Left ? &Left : (Hand == EControllerHand::Right ? &Right : nullptr);
}

bool FFistDetector::IsClosed(EControllerHand Hand) const
{
	const FHandState* State = StateFor(Hand);
	return State && State->bClosed;
}

void FFistDetector::Ingest(const FHandFrame& Frame)
{
	FHandState* State = StateFor(Frame.Hand);
	if (!State)
	{
		return;
	}
	// VRC.Quest.Input.8: a hand in the system gesture holds nothing, and the hold it had does not resume after it.
	bool bBegan = false;
	if (Gate.Filter(Frame, bBegan))
	{
		*State = FHandState();
		return;
	}
	// A new clip restarts its clock: the hold starts over, as a ward raise does on a rewind.
	if (State->bHasTime && Frame.TimeSeconds < State->LastTime - 1.0e-4)
	{
		*State = FHandState();
	}
	else if (State->bHasTime && Frame.TimeSeconds <= State->LastTime)
	{
		return;
	}
	State->LastTime = Frame.TimeSeconds;
	State->bHasTime = true;
	const bool bClosed = IsFistPose(Frame, State->bClosed);
	if (bClosed && !State->bClosed)
	{
		State->Since = Frame.TimeSeconds;
		State->bCounted = false;
	}
	State->bClosed = bClosed;
	if (bClosed && !State->bCounted && Frame.TimeSeconds - State->Since + 1.0e-9 >= FFistThresholds::HoldS)
	{
		State->bCounted = true;
		++HeldCount;
	}
}
