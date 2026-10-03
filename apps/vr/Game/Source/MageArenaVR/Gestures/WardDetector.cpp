#include "Gestures/WardDetector.h"

#include "Hands/HandInputSubsystem.h"
#include "HeadMountedDisplayTypes.h"
#include "MageArenaVR.h"

namespace
{
FVector FlatFacing(const FVector& Facing)
{
	if (Facing.IsNearlyZero())
	{
		return FVector::ForwardVector;
	}
	return Facing.GetSafeNormal();
}
}

FVector FWardDetector::PalmNormal(const FHandFrame& Frame)
{
	const int32 Palm = static_cast<int32>(EHandKeypoint::Palm);
	const int32 Wrist = static_cast<int32>(EHandKeypoint::Wrist);
	const int32 IndexMeta = static_cast<int32>(EHandKeypoint::IndexMetacarpal);
	const int32 LittleMeta = static_cast<int32>(EHandKeypoint::LittleMetacarpal);
	if (!Frame.Joints.IsValidIndex(LittleMeta))
	{
		return FVector::ZeroVector;
	}
	const FVector Cross = FVector::CrossProduct(
		Frame.Joints[IndexMeta].Location - Frame.Joints[Wrist].Location,
		Frame.Joints[LittleMeta].Location - Frame.Joints[Wrist].Location);
	if (Cross.IsNearlyZero())
	{
		return Frame.Joints[Palm].Rotation.GetAxisX().GetSafeNormal();
	}
	return Cross.GetSafeNormal();
}

double FWardDetector::PalmHeightMetres(const FHandFrame& Frame)
{
	const int32 Palm = static_cast<int32>(EHandKeypoint::Palm);
	if (!Frame.Joints.IsValidIndex(Palm))
	{
		return 0.0;
	}
	return Frame.Joints[Palm].Location.Z / MageClipMetresToCentimetres;
}

double FWardDetector::AngleToFacingDeg(const FVector& Normal, const FVector& Facing)
{
	if (Normal.IsNearlyZero() || Facing.IsNearlyZero())
	{
		return 180.0;
	}
	const double Dot = FVector::DotProduct(Normal.GetSafeNormal(), Facing.GetSafeNormal());
	return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0, 1.0)));
}

bool FWardDetector::Init(FString& OutError)
{
	FAbsorbRules Loaded;
	if (!Loaded.LoadFromPinnedFile(OutError))
	{
		LoadedRules = FAbsorbRules();
		bReady = false;
		ResetStream();
		return false;
	}
	LoadedRules = MoveTemp(Loaded);
	bReady = true;
	ResetStream();
	return true;
}

void FWardDetector::ResetStream()
{
	History.Reset();
	State = FWardState();
	LastRaise = FWardEvent();
	LastTime = 0.0;
	LoweredTime = -1.0e9;
	RaiseCount = 0;
	LowerCount = 0;
	bRaised = false;
	bHasTime = false;
	bEverLowered = false;
	bSeenLowered = false;
	bHasLapBound = false;
	LapBoundTime = -1.0e9;
}

void FWardDetector::SetAimFacing(const FVector& Facing)
{
	AimFacing = FlatFacing(Facing);
}

void FWardDetector::SetThreatFacing(const FVector& Facing, bool bTargeted)
{
	bThreatTargeted = bTargeted && !Facing.IsNearlyZero();
	ThreatFacing = bThreatTargeted ? Facing.GetSafeNormal() : FVector::ForwardVector;
}

FVector FWardDetector::ReferenceFacing() const
{
	return bThreatTargeted ? ThreatFacing : AimFacing;
}

FVector FWardDetector::PalmFacing(const FVector& Normal) const
{
	FVector Facing(Normal.X, Normal.Y, 0.0);
	if (Facing.IsNearlyZero())
	{
		// A palm that is straight up has no yaw. Fall back to the aim, but an elevated
		// threat must not leak its vertical component into the ward arc.
		const FVector Reference = ReferenceFacing();
		Facing = FVector(Reference.X, Reference.Y, 0.0);
		if (Facing.IsNearlyZero())
		{
			Facing = FVector::ForwardVector;
		}
	}
	return Facing.GetSafeNormal();
}

double FWardDetector::FindOnset() const
{
	const int32 Last = History.Num() - 1;
	if (Last < 0)
	{
		return 0.0;
	}
	int32 Index = Last;
	const double ConfirmTime = History[Last].Time;
	// A re-raise stops at the lowering. The first raise has no lower: LoweredTime is still
	// far in the past, so a fidget before the hand was seen in the lap would be the onset.
	// Bound that walk at the first in-lap sample, and never earlier than the buffer.
	double Bound = History[0].Time;
	if (bEverLowered)
	{
		Bound = LoweredTime;
	}
	else if (bHasLapBound)
	{
		Bound = FMath::Max(LapBoundTime, History[0].Time);
	}
	// Bridge sub-threshold frames only while the palm is up. Stepping onto a lap frame
	// would keep walking through the stillness into an earlier twitch. Do not height-gate
	// the fast walk: the rise that this tail is measured on starts below LowerHeightM.
	while (Index > 0
		&& History[Index].SpeedMps < FWardThresholds::OnsetSpeedMps
		&& History[Index].HeightM >= FWardThresholds::LowerHeightM
		&& History[Index - 1].HeightM >= FWardThresholds::LowerHeightM
		&& (ConfirmTime - History[Index].Time) <= FWardThresholds::OnsetTailS
		&& History[Index - 1].Time >= Bound - 1.0e-9)
	{
		--Index;
	}
	while (Index > 0
		&& History[Index - 1].SpeedMps >= FWardThresholds::OnsetSpeedMps
		&& History[Index - 1].Time >= Bound - 1.0e-9)
	{
		--Index;
	}
	return History[Index].Time;
}

void FWardDetector::UpdateState(const FVector& Facing)
{
	State.bRaised = bRaised;
	State.bFresh = bRaised && LastRaise.bFresh;
	State.OnsetTime = LastRaise.OnsetTime;
	State.Facing = Facing;
}

void FWardDetector::NoteRewind()
{
	if (bRaised)
	{
		bRaised = false;
		++LowerCount;
		OnLowered.Broadcast();
	}
	History.Reset();
	State = FWardState();
	LastRaise = FWardEvent();
	bRaised = false;
	bHasTime = false;
	bEverLowered = false;
	bSeenLowered = false;
	bHasLapBound = false;
	LoweredTime = -1.0e9;
	LapBoundTime = -1.0e9;
}

void FWardDetector::Ingest(const FHandFrame& Frame)
{
	if (!bReady || Frame.Hand != EControllerHand::Left)
	{
		return;
	}
	const int32 Palm = static_cast<int32>(EHandKeypoint::Palm);
	if (!Frame.Joints.IsValidIndex(Palm))
	{
		return;
	}
	if (bHasTime && Frame.TimeSeconds < LastTime - 1.0e-4)
	{
		NoteRewind();
	}
	else if (bHasTime && Frame.TimeSeconds <= LastTime)
	{
		return;
	}

	const FVector Normal = PalmNormal(Frame);
	const double HeightM = PalmHeightMetres(Frame);
	const double AngleDeg = AngleToFacingDeg(Normal, ReferenceFacing());
	const FVector PalmCm = Frame.Joints[Palm].Location;

	double Speed = 0.0;
	if (History.Num() > 0)
	{
		const double Dt = Frame.TimeSeconds - History.Last().Time;
		if (Dt >= 1.0e-6)
		{
			const double DistM = FVector::Distance(PalmCm, History.Last().PalmCm) / MageClipMetresToCentimetres;
			Speed = DistM / Dt;
		}
	}

	FSample Sample;
	Sample.Time = Frame.TimeSeconds;
	Sample.PalmCm = PalmCm;
	Sample.SpeedMps = Speed;
	Sample.HeightM = HeightM;
	History.Add(Sample);
	if (!bHasLapBound && HeightM < FWardThresholds::LowerHeightM)
	{
		LapBoundTime = Frame.TimeSeconds;
		bHasLapBound = true;
	}

	const bool bWasRaised = bRaised;
	const bool bNowRaised = bWasRaised
		? (AngleDeg <= FWardThresholds::LowerAngleDeg && HeightM >= FWardThresholds::LowerHeightM)
		: (AngleDeg <= FWardThresholds::RaiseAngleDeg && HeightM >= FWardThresholds::RaiseHeightM);
	const FVector Facing = PalmFacing(Normal);

	if (!bWasRaised && bNowRaised)
	{
		const double Onset = FindOnset();
		// A hand that is already raised when tracking starts has not been seen lowered, so it is not fresh.
		// A re-raise is fresh only after the pinned release gap. The first raise from an observed lap is fresh.
		const bool bGap = !bEverLowered || (Onset - LoweredTime) >= (LoadedRules.MinReleaseS - 1.0e-9);
		const bool bFresh = bSeenLowered && bGap;
		bRaised = true;
		LastRaise.OnsetTime = Onset;
		LastRaise.Facing = Facing;
		LastRaise.bFresh = bFresh;
		++RaiseCount;
		UpdateState(Facing);
		UE_LOG(LogMageArena, Log, TEXT("Ward raised onset=%.4f facing=(%.3f,%.3f,%.3f) fresh=%d"),
			Onset, Facing.X, Facing.Y, Facing.Z, bFresh ? 1 : 0);
		OnRaised.Broadcast(Onset, Facing);
	}
	else if (bWasRaised && !bNowRaised)
	{
		bRaised = false;
		bEverLowered = true;
		bSeenLowered = true;
		LoweredTime = Frame.TimeSeconds;
		++LowerCount;
		UpdateState(Facing);
		UE_LOG(LogMageArena, Log, TEXT("Ward lowered at %.4f"), Frame.TimeSeconds);
		OnLowered.Broadcast();
	}
	else if (bNowRaised)
	{
		bRaised = true;
		UpdateState(Facing);
	}
	else
	{
		bSeenLowered = true;
	}

	const double Cutoff = Frame.TimeSeconds - FWardThresholds::HistorySeconds;
	while (History.Num() > 2 && History[0].Time < Cutoff)
	{
		History.RemoveAt(0);
	}
	LastTime = Frame.TimeSeconds;
	bHasTime = true;
}

void UWardDetectorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency(UHandInputSubsystem::StaticClass());
	FString Error;
	if (!InitDetector(Error))
	{
		UE_LOG(LogMageArena, Error, TEXT("%s"), *Error);
	}
	if (UGameInstance* Instance = GetGameInstance())
	{
		BindToHands(Instance->GetSubsystem<UHandInputSubsystem>());
	}
}

void UWardDetectorSubsystem::Deinitialize()
{
	BindToHands(nullptr);
	Detector.OnRaised.Clear();
	Detector.OnLowered.Clear();
	bForwarding = false;
	Super::Deinitialize();
}

bool UWardDetectorSubsystem::InitDetector(FString& OutError)
{
	if (!bForwarding)
	{
		RaisedHandle = Detector.OnRaised.AddLambda([this](double OnsetTime, FVector Facing)
		{
			OnWardRaised.Broadcast(OnsetTime, Facing);
		});
		LoweredHandle = Detector.OnLowered.AddLambda([this]()
		{
			OnWardLowered.Broadcast();
		});
		bForwarding = true;
	}
	return Detector.Init(OutError);
}

void UWardDetectorSubsystem::BindToHands(UHandInputSubsystem* Hands)
{
	if (BoundHands.IsValid())
	{
		BoundHands->OnHandFrame().Remove(HandHandle);
	}
	HandHandle.Reset();
	BoundHands = Hands;
	Detector.ResetStream();
	if (Hands)
	{
		HandHandle = Hands->OnHandFrame().AddUObject(this, &UWardDetectorSubsystem::HandleHandFrame);
	}
}

void UWardDetectorSubsystem::SetAimFacing(const FVector& Facing)
{
	Detector.SetAimFacing(Facing);
}

void UWardDetectorSubsystem::SetThreatFacing(const FVector& Facing, bool bTargeted)
{
	Detector.SetThreatFacing(Facing, bTargeted);
}

void UWardDetectorSubsystem::HandleHandFrame(const FHandFrame& Frame)
{
	Detector.Ingest(Frame);
}
