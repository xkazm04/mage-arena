#include "Gestures/StaffDetector.h"

#include "Hands/HandInputSubsystem.h"
#include "HeadMountedDisplayTypes.h"
#include "MageArenaVR.h"

namespace
{
constexpr double GripPinch = 0.70;
constexpr double CloseYM = 0.12;
constexpr double TravelM = 0.18;
constexpr double LaneSkewS = 0.05;
constexpr double WindowS = 1.5;
}

void FStaffDetector::Reset()
{
	Left = FHandSample();
	Right = FHandSample();
	ResetWindow();
	PlantCount = 0;
	LiftCount = 0;
}

void FStaffDetector::ResetWindow()
{
	Window.Reset();
	bLatched = false;
}

void FStaffDetector::CancelForSystemGesture(EControllerHand Hand)
{
	FHandSample& Slot = Hand == EControllerHand::Left ? Left : Right;
	Slot.bSeen = false;
	ResetWindow();
}

void FStaffDetector::Ingest(const FHandFrame& Frame)
{
	const int32 Palm = static_cast<int32>(EHandKeypoint::Palm);
	if (!Frame.Joints.IsValidIndex(Palm))
	{
		return;
	}
	FHandSample& Slot = Frame.Hand == EControllerHand::Left ? Left : Right;
	if (Slot.bSeen && Frame.TimeSeconds + 1.0e-4 < Slot.Time)
	{
		ResetWindow();
	}
	Slot.Time = Frame.TimeSeconds;
	Slot.PalmZM = Frame.Joints[Palm].Location.Z / MageClipMetresToCentimetres;
	Slot.PalmYM = Frame.Joints[Palm].Location.Y / MageClipMetresToCentimetres;
	Slot.Pinch = Frame.Pinch;
	Slot.bSeen = true;

	if (!Left.bSeen || !Right.bSeen || FMath::Abs(Left.Time - Right.Time) > LaneSkewS)
	{
		return;
	}
	const bool bGrip = Left.Pinch >= GripPinch && Right.Pinch >= GripPinch && FMath::Abs(Left.PalmYM - Right.PalmYM) < CloseYM;
	if (!bGrip)
	{
		ResetWindow();
		return;
	}
	const double Now = 0.5 * (Left.Time + Right.Time);
	FWindowSample Sample;
	Sample.Time = Now;
	Sample.PalmZM = 0.5 * (Left.PalmZM + Right.PalmZM);
	Window.Add(Sample);
	while (Window.Num() > 2 && Window[0].Time < Now - WindowS)
	{
		Window.RemoveAt(0);
	}
	if (bLatched || Window.Num() < 2)
	{
		return;
	}
	const double Delta = Window.Last().PalmZM - Window[0].PalmZM;
	if (Delta <= -TravelM)
	{
		bLatched = true;
		++PlantCount;
		UE_LOG(LogMageArena, Log, TEXT("Staff planted dz=%.3f"), Delta);
		OnPlant.Broadcast();
	}
	else if (Delta >= TravelM)
	{
		bLatched = true;
		++LiftCount;
		UE_LOG(LogMageArena, Log, TEXT("Staff lifted dz=%.3f"), Delta);
		OnLift.Broadcast();
	}
}

void UStaffDetectorSubsystem::EnsureForwarding()
{
	if (bForwarding)
	{
		return;
	}
	PlantHandle = Detector.OnPlant.AddLambda([this]()
	{
		OnPlant.Broadcast();
	});
	LiftHandle = Detector.OnLift.AddLambda([this]()
	{
		OnLift.Broadcast();
	});
	bForwarding = true;
}

void UStaffDetectorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency(UHandInputSubsystem::StaticClass());
	if (UGameInstance* Instance = GetGameInstance())
	{
		BindToHands(Instance->GetSubsystem<UHandInputSubsystem>());
	}
}

void UStaffDetectorSubsystem::Deinitialize()
{
	BindToHands(nullptr);
	Detector.OnPlant.Clear();
	Detector.OnLift.Clear();
	bForwarding = false;
	Super::Deinitialize();
}

void UStaffDetectorSubsystem::BindToHands(UHandInputSubsystem* Hands)
{
	if (BoundHands.IsValid())
	{
		BoundHands->OnHandFrame().Remove(HandHandle);
	}
	HandHandle.Reset();
	BoundHands = Hands;
	Detector.Reset();
	Gate.Reset();
	if (Hands)
	{
		EnsureForwarding();
		HandHandle = Hands->OnHandFrame().AddUObject(this, &UStaffDetectorSubsystem::HandleHandFrame);
	}
}

void UStaffDetectorSubsystem::HandleHandFrame(const FHandFrame& Frame)
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
}
