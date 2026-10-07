#include "Gestures/BlinkDetector.h"

#include "Hands/HandInputSubsystem.h"
#include "Hands/MageSettings.h"
#include "HeadMountedDisplayTypes.h"
#include "MageArenaVR.h"

void FBlinkDetector::Reset()
{
	ActivePad = 1;
	BlinkCount = 0;
	BoltCount = 0;
	LastBlink = FBlinkEvent();
	LastBolt = FBoltFlickEvent();
	Arm();
}

void FBlinkDetector::Arm()
{
	bHasPrev = false;
	bLatched = false;
	QuietFor = 0.0;
	LastTime = 0.0;
	PrevIndexTip = FVector::ZeroVector;
	HottestSpeed = 0.0;
	HottestOmega = 0.0;
	HottestYaw = 0.0;
	HottestTime = 0.0;
}

void FBlinkDetector::SetActivePad(int32 PadIndex)
{
	ActivePad = FMath::Clamp(PadIndex, 0, 2);
}

int32 FBlinkDetector::ResolvePad(EBlinkDirection Direction, int32 FromPad)
{
	const int32 Pad = FMath::Clamp(FromPad, 0, 2);
	switch (Direction)
	{
	case EBlinkDirection::Left:
		return FMath::Max(0, Pad - 1);
	case EBlinkDirection::Right:
		return FMath::Min(2, Pad + 1);
	default:
		return 1;
	}
}

void FBlinkDetector::Fire(double YawDegrees, double TimeSeconds)
{
	bLatched = true;
	if (FMath::Abs(YawDegrees) <= FBlinkThresholds::ForwardYawDeg)
	{
		++BoltCount;
		LastBolt.TimeSeconds = TimeSeconds;
		LastBolt.YawDegrees = YawDegrees;
		return;
	}

	EBlinkDirection Direction = EBlinkDirection::Back;
	if (YawDegrees < -FBlinkThresholds::ForwardYawDeg && YawDegrees > -FBlinkThresholds::SideYawDeg)
	{
		Direction = EBlinkDirection::Left;
	}
	else if (YawDegrees > FBlinkThresholds::ForwardYawDeg && YawDegrees < FBlinkThresholds::SideYawDeg)
	{
		Direction = EBlinkDirection::Right;
	}

	++BlinkCount;
	LastBlink.Direction = Direction;
	LastBlink.PadIndex = ResolvePad(Direction, ActivePad);
	LastBlink.TimeSeconds = TimeSeconds;
	LastBlink.YawDegrees = YawDegrees;
}

void FBlinkDetector::Flush()
{
	if (!bLatched && HottestSpeed >= FBlinkThresholds::MinTipSpeedMps && HottestOmega >= FBlinkThresholds::MinOmegaRadPerS)
	{
		Fire(HottestYaw, HottestTime);
	}
}

void FBlinkDetector::CancelForSystemGesture(EControllerHand Hand)
{
	if (Hand == FMageSettings::CastingHand())
	{
		Arm();
	}
}

void FBlinkDetector::Ingest(const FHandFrame& Frame)
{
	if (Frame.Hand != FMageSettings::CastingHand())
	{
		return;
	}
	const int32 WristIndex = static_cast<int32>(EHandKeypoint::Wrist);
	const int32 IndexTip = static_cast<int32>(EHandKeypoint::IndexTip);
	const int32 MiddleTip = static_cast<int32>(EHandKeypoint::MiddleTip);
	if (!Frame.Joints.IsValidIndex(MiddleTip))
	{
		return;
	}

	// A new clip rewinds the stamp. Finish the previous flick, then arm.
	if (bHasPrev && Frame.TimeSeconds + 1.0e-4 < LastTime)
	{
		Flush();
		Arm();
	}

	const FVector Index = Frame.Joints[IndexTip].Location;
	const FVector Wrist = Frame.Joints[WristIndex].Location;
	const FVector Middle = Frame.Joints[MiddleTip].Location;
	if (!bHasPrev)
	{
		bHasPrev = true;
		LastTime = Frame.TimeSeconds;
		PrevIndexTip = Index;
		return;
	}

	const double Dt = Frame.TimeSeconds - LastTime;
	LastTime = Frame.TimeSeconds;
	const FVector Previous = PrevIndexTip;
	PrevIndexTip = Index;
	if (Dt < 1.0e-5)
	{
		return;
	}
	// A hitch is not a flick. Drop the sample so the next pair has a real dt.
	if (Dt > 0.2)
	{
		return;
	}

	const FVector Delta = Index - Previous;
	const double Horizontal = FVector2D(Delta.X, Delta.Y).Size() / Dt / 100.0;
	const FVector Relative = Index - Wrist;
	const FVector Velocity = Delta / Dt;
	const double RelativeSquared = FMath::Max(Relative.SizeSquared(), 1.0);
	const double Omega = FVector::CrossProduct(Relative, Velocity).Size() / RelativeSquared;
	const double IndexReach = Relative.Size();
	const double MiddleReach = (Middle - Wrist).Size();
	const bool bPointing = MiddleReach > 1.0 && IndexReach / MiddleReach >= FBlinkThresholds::PointRatio;
	const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));

	// Let go of a fired flick once the tip has been still. Without this only a clock rewind (a new clip) re-armed the
	// detector, so a stream with no rewinds saw its first flick and no other.
	if (bLatched)
	{
		QuietFor = Horizontal < FBlinkThresholds::RearmSpeedMps ? QuietFor + Dt : 0.0;
		if (QuietFor >= FBlinkThresholds::RearmQuietS)
		{
			bLatched = false;
			QuietFor = 0.0;
			HottestSpeed = 0.0;
			HottestOmega = 0.0;
			HottestYaw = 0.0;
			HottestTime = 0.0;
		}
	}

	if (bPointing && Omega >= FBlinkThresholds::MinOmegaRadPerS && Horizontal > HottestSpeed)
	{
		HottestSpeed = Horizontal;
		HottestOmega = Omega;
		HottestYaw = Yaw;
		HottestTime = Frame.TimeSeconds;
	}
	if (!bLatched && HottestSpeed >= FBlinkThresholds::MinTipSpeedMps
		&& Horizontal < HottestSpeed * FBlinkThresholds::FallFraction)
	{
		Fire(HottestYaw, HottestTime);
	}
}

void UBlinkDetectorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency(UHandInputSubsystem::StaticClass());
	if (UGameInstance* Instance = GetGameInstance())
	{
		BindToHands(Instance->GetSubsystem<UHandInputSubsystem>());
	}
}

void UBlinkDetectorSubsystem::Deinitialize()
{
	BindToHands(nullptr);
	Super::Deinitialize();
}

void UBlinkDetectorSubsystem::BindToHands(UHandInputSubsystem* Hands)
{
	if (BoundHands.IsValid())
	{
		BoundHands->OnHandFrame().Remove(HandHandle);
	}
	HandHandle.Reset();
	BoundHands = Hands;
	Detector.Reset();
	Gate.Reset();
	SeenBlinks = 0;
	SeenBolts = 0;
	if (Hands)
	{
		HandHandle = Hands->OnHandFrame().AddUObject(this, &UBlinkDetectorSubsystem::HandleHandFrame);
	}
}

void UBlinkDetectorSubsystem::SetActivePad(int32 PadIndex)
{
	Detector.SetActivePad(PadIndex);
}

void UBlinkDetectorSubsystem::ResetDetector()
{
	Detector.Reset();
	SeenBlinks = 0;
	SeenBolts = 0;
}

void UBlinkDetectorSubsystem::Publish()
{
	if (Detector.GetBlinkCount() != SeenBlinks)
	{
		SeenBlinks = Detector.GetBlinkCount();
		OnBlink.Broadcast(Detector.GetLastBlink());
	}
	if (Detector.GetBoltCount() != SeenBolts)
	{
		SeenBolts = Detector.GetBoltCount();
		OnBolt.Broadcast(Detector.GetLastBolt());
	}
}

void UBlinkDetectorSubsystem::FlushPending()
{
	Detector.Flush();
	Publish();
}

void UBlinkDetectorSubsystem::HandleHandFrame(const FHandFrame& Frame)
{
	bool bBegan = false;
	if (Gate.Filter(Frame, bBegan))
	{
		if (bBegan)
		{
			Detector.CancelForSystemGesture(Frame.Hand);
		}
		return;
	}
	Detector.Ingest(Frame);
	Publish();
}
