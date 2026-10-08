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
	TipRate.Reset();
	HottestSpeed = 0.0;
	HottestOmega = 0.0;
	HottestYaw = 0.0;
	HottestTime = 0.0;
	HottestDirection = FVector2D::ZeroVector;
	HottestSpanSpeed = 0.0;
	HistoryCount = 0;
	HistoryHead = 0;
}

void FBlinkDetector::PushHistory(const FVector2D& Point, double TimeSeconds)
{
	History[HistoryHead] = {Point, TimeSeconds};
	HistoryHead = (HistoryHead + 1) % HistoryCapacity;
	HistoryCount = FMath::Min(HistoryCount + 1, HistoryCapacity);
}

bool FBlinkDetector::SpanSpeed(const FVector2D& Point, double TimeSeconds, double SpanS, double& OutSpeed) const
{
	for (int32 Back = 1; Back <= HistoryCount; ++Back)
	{
		const FTipSample& Sample = History[(HistoryHead - Back + HistoryCapacity) % HistoryCapacity];
		const double Age = TimeSeconds - Sample.TimeSeconds;
		if (Age >= SpanS - 1.0e-6)
		{
			OutSpeed = FVector2D::Distance(Point, Sample.Point) / Age / 100.0;
			return true;
		}
	}
	return false;
}

bool FBlinkDetector::BackWithinRearmSpan(const FVector2D& Point, double TimeSeconds) const
{
	for (int32 Back = 1; Back <= HistoryCount; ++Back)
	{
		const FTipSample& Sample = History[(HistoryHead - Back + HistoryCapacity) % HistoryCapacity];
		const double Age = TimeSeconds - Sample.TimeSeconds;
		if (Age > FBlinkThresholds::RearmSpanMaxS + 1.0e-6)
		{
			break;
		}
		if (Age >= FBlinkThresholds::RearmSpanMinS - 1.0e-6 && FVector2D::Distance(Point, Sample.Point) / Age / 100.0 < FBlinkThresholds::RearmSpeedMps)
		{
			return true;
		}
	}
	return false;
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
	FVector Delta;
	double Dt = 0.0;
	if (!bHasPrev)
	{
		bHasPrev = true;
		LastTime = Frame.TimeSeconds;
		TipRate.Step(Index, Frame.TimeSeconds, Delta, Dt);
		return;
	}

	LastTime = Frame.TimeSeconds;
	// A held tip is no new sample (F7(b)). Read as speed 0 it would release the flick after its first stepped burst.
	if (TipRate.Step(Index, Frame.TimeSeconds, Delta, Dt) != FHeldPointRate::EStep::Measured)
	{
		return;
	}
	if (Dt < 1.0e-5)
	{
		return;
	}
	// A hitch is not a flick. Drop the sample so the next pair has a real dt.
	if (Dt > 0.2)
	{
		return;
	}

	const double Horizontal = FVector2D(Delta.X, Delta.Y).Size() / Dt / 100.0;
	const FVector Relative = Index - Wrist;
	const FVector Velocity = Delta / Dt;
	const double RelativeSquared = FMath::Max(Relative.SizeSquared(), 1.0);
	const double Omega = FVector::CrossProduct(Relative, Velocity).Size() / RelativeSquared;
	const double IndexReach = Relative.Size();
	const double MiddleReach = (Middle - Wrist).Size();
	const bool bPointing = MiddleReach > 1.0 && IndexReach / MiddleReach >= FBlinkThresholds::PointRatio;
	const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
	const FVector2D Point(Index.X, Index.Y);
	PushHistory(Point, Frame.TimeSeconds);

	// Let go of a fired flick once the tip has been still. Without this only a clock rewind (a new clip) re-armed the
	// detector, so a stream with no rewinds saw its first flick and no other. A tip back where it was a camera period or
	// more ago is still too: that is an overshoot snapping back (G1), not motion.
	if (bLatched)
	{
		const bool bQuiet = Horizontal < FBlinkThresholds::RearmSpeedMps || BackWithinRearmSpan(Point, Frame.TimeSeconds);
		QuietFor = bQuiet ? QuietFor + Dt : 0.0;
		if (QuietFor >= FBlinkThresholds::RearmQuietS)
		{
			bLatched = false;
			QuietFor = 0.0;
			HottestSpeed = 0.0;
			HottestOmega = 0.0;
			HottestYaw = 0.0;
			HottestTime = 0.0;
			HottestDirection = FVector2D::ZeroVector;
			HottestSpanSpeed = 0.0;
		}
	}

	double SpanNow = 0.0;
	const bool bHasSpan = SpanSpeed(Point, Frame.TimeSeconds, FBlinkThresholds::FallSpanS, SpanNow);
	if (!bLatched && HottestSpeed > 0.0 && bHasSpan)
	{
		HottestSpanSpeed = FMath::Max(HottestSpanSpeed, SpanNow);
	}
	// The fall is judged before this sample may become the hottest: an extrapolated stop snaps back against the flick, and
	// that snap can be faster than the flick. Measured along the flick it is a fall, not a new peak the other way.
	if (!bLatched && HottestSpeed >= FBlinkThresholds::MinTipSpeedMps)
	{
		const double Along = FVector2D::DotProduct(FVector2D(Delta.X, Delta.Y), HottestDirection) / Dt / 100.0;
		const bool bSpanFell = !bHasSpan || HottestSpanSpeed <= 0.0 || SpanNow < HottestSpanSpeed * FBlinkThresholds::FallSpanPeakFraction;
		if (Along < HottestSpeed * FBlinkThresholds::FallFraction && bSpanFell)
		{
			Fire(HottestYaw, HottestTime);
		}
	}
	if (bPointing && Omega >= FBlinkThresholds::MinOmegaRadPerS && Horizontal > HottestSpeed)
	{
		HottestSpeed = Horizontal;
		HottestOmega = Omega;
		HottestYaw = Yaw;
		HottestTime = Frame.TimeSeconds;
		HottestDirection = FVector2D(Delta.X, Delta.Y).GetSafeNormal();
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
