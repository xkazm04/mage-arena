#include "Hands/HandClipPlayer.h"

#include "HeadMountedDisplayTypes.h"

namespace
{
// The authored hold is bit-identical. 0.5 mm keeps the settling frame
// (0.27 cm short of the pose on ward-raise.normal) out of the plateau.
constexpr double PlateauPalmCentimetres = 0.05;
}

UHandClipPlayer::UHandClipPlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, FTickableGameObject(ETickableTickType::Never)
{
}

void UHandClipPlayer::Play(const FHandClip& InClip)
{
	Clip = InClip;
	bHasClip = true;
	TimeSeconds = 0.0;
	SampleTime = 0.0;
	Duration = Clip.GetDuration();
	Sequence = 0;
	bHasLeft = false;
	bHasRight = false;
	bHoldArmed = false;
	bHoldEmitting = false;
	bLowering = false;
	bHoldClip = false;
	HoldPoseTime = 0.0;
	bPlaying = Duration > 0.0;
	EmitSample(0.0, 0.0);
	RefreshTick();
}

void UHandClipPlayer::Stop()
{
	bPlaying = false;
	bHoldArmed = false;
	bHoldEmitting = false;
	bLowering = false;
	bHoldClip = false;
	RefreshTick();
}

void UHandClipPlayer::BeginHold(double InHoldPoseTime)
{
	if (!bHasClip)
	{
		return;
	}
	HoldPoseTime = FMath::Clamp(InHoldPoseTime, 0.0, Duration);
	bHoldClip = true;
	bLowering = false;
	bHoldArmed = true;
	bHoldEmitting = SampleTime + 1.0e-9 >= HoldPoseTime;
	if (bHoldEmitting)
	{
		bPlaying = true;
	}
	RefreshTick();
}

void UHandClipPlayer::EndHold()
{
	if (!bHasClip || !bHoldClip)
	{
		return;
	}
	if (bHoldEmitting)
	{
		SampleTime = HoldPoseTime;
	}
	bHoldArmed = false;
	bHoldEmitting = false;
	bLowering = true;
	bPlaying = true;
	RefreshTick();
}

bool UHandClipPlayer::CanResumeHold() const
{
	// !bPlaying used to count as resumable, so the next press continued the finished
	// clip and ignored a new Shift or Ctrl variant.
	return bHasClip && bHoldClip && bLowering && !bHoldEmitting;
}

void UHandClipPlayer::ResumeHold()
{
	if (!CanResumeHold())
	{
		return;
	}
	bLowering = false;
	bHoldArmed = true;
	bHoldEmitting = SampleTime + 1.0e-9 >= HoldPoseTime;
	bPlaying = true;
	RefreshTick();
}

void UHandClipPlayer::SetAimYaw(double YawRadians)
{
	AimYawRadians = YawRadians;
}

void UHandClipPlayer::SetTrackingDropped(bool bDropped)
{
	bTrackingDropped = bDropped;
	if (bDropped && GetWorld() != nullptr)
	{
		SetTickableTickType(ETickableTickType::Always);
	}
	else
	{
		RefreshTick();
	}
}

void UHandClipPlayer::Step(double DeltaSeconds)
{
	if (!(DeltaSeconds > 0.0))
	{
		return;
	}
	if (bTrackingDropped && !bPlaying)
	{
		TimeSeconds += DeltaSeconds;
		EmitUntracked(TimeSeconds);
		return;
	}
	if (!bHasClip || !bPlaying)
	{
		return;
	}

	if (bHoldEmitting)
	{
		TimeSeconds += DeltaSeconds;
		EmitSample(HoldPoseTime, TimeSeconds);
		return;
	}

	if (bLowering)
	{
		TimeSeconds += DeltaSeconds;
		SampleTime = FMath::Max(0.0, SampleTime - DeltaSeconds);
		EmitSample(SampleTime, TimeSeconds);
		if (SampleTime <= 0.0)
		{
			bPlaying = false;
			bLowering = false;
			bHoldClip = false;
			RefreshTick();
		}
		return;
	}

	const double NextSample = SampleTime + DeltaSeconds;
	if (bHoldArmed && NextSample >= HoldPoseTime)
	{
		SampleTime = HoldPoseTime;
		TimeSeconds += DeltaSeconds;
		bHoldEmitting = true;
		EmitSample(HoldPoseTime, TimeSeconds);
		return;
	}

	if (NextSample >= Duration)
	{
		SampleTime = Duration;
		TimeSeconds = Duration;
		EmitSample(Duration, Duration);
		bPlaying = false;
		RefreshTick();
		return;
	}

	SampleTime = NextSample;
	TimeSeconds += DeltaSeconds;
	EmitSample(SampleTime, TimeSeconds);
}

void UHandClipPlayer::EmitSample(double SampleAt, double StampTime)
{
	for (const EControllerHand Hand : Clip.GetHands())
	{
		FHandFrame Frame;
		if (!Clip.Sample(Hand, SampleAt, Frame))
		{
			continue;
		}
		Frame.TimeSeconds = StampTime;
		if (bTrackingDropped)
		{
			Frame.Confidence = 0.0f;
		}
		ApplyAimYaw(Frame);
		if (Hand == EControllerHand::Left)
		{
			LatestLeft = Frame;
			bHasLeft = true;
		}
		else if (Hand == EControllerHand::Right)
		{
			LatestRight = Frame;
			bHasRight = true;
		}
		else
		{
			continue;
		}
		++Sequence;
		FrameDelegate.Broadcast(Frame);
	}
}

void UHandClipPlayer::EmitUntracked(double StampTime)
{
	auto Emit = [this, StampTime](EControllerHand Hand)
	{
		FHandFrame Frame;
		Frame.Hand = Hand;
		Frame.TimeSeconds = StampTime;
		Frame.Confidence = 0.0f;
		if (Hand == EControllerHand::Left)
		{
			LatestLeft = Frame;
			bHasLeft = true;
		}
		else if (Hand == EControllerHand::Right)
		{
			LatestRight = Frame;
			bHasRight = true;
		}
		else
		{
			return;
		}
		++Sequence;
		FrameDelegate.Broadcast(Frame);
	};
	Emit(EControllerHand::Left);
	Emit(EControllerHand::Right);
}

void UHandClipPlayer::ApplyAimYaw(FHandFrame& Frame) const
{
	if (FMath::Abs(AimYawRadians) <= 1.0e-8)
	{
		return;
	}
	const FQuat Yaw(FVector::UpVector, static_cast<float>(AimYawRadians));
	for (FHandJointPose& Joint : Frame.Joints)
	{
		Joint.Location = Yaw.RotateVector(Joint.Location);
		Joint.Rotation = Yaw * Joint.Rotation;
	}
}

void UHandClipPlayer::RefreshTick()
{
	if ((bPlaying || bTrackingDropped) && GetWorld() != nullptr)
	{
		SetTickableTickType(ETickableTickType::Always);
	}
	else
	{
		SetTickableTickType(ETickableTickType::Never);
	}
}

double UHandClipPlayer::FindPlateauStart(const FHandClip& InClip, EControllerHand Hand)
{
	const FHandClipTrack* Track = nullptr;
	for (const FHandClipTrack& Candidate : InClip.GetTracks())
	{
		if (Candidate.Hand == Hand)
		{
			Track = &Candidate;
			break;
		}
	}
	if (Track == nullptr || Track->Frames.Num() == 0)
	{
		return InClip.GetDuration();
	}
	const int32 Palm = static_cast<int32>(EHandKeypoint::Palm);
	const TArray<FHandFrame>& Frames = Track->Frames;
	const FHandFrame& Last = Frames.Last();
	if (!Last.Joints.IsValidIndex(Palm))
	{
		return Last.TimeSeconds;
	}
	const FVector FinalPalm = Last.Joints[Palm].Location;
	double Start = Last.TimeSeconds;
	for (int32 Index = Frames.Num() - 1; Index >= 0; --Index)
	{
		const FHandFrame& Frame = Frames[Index];
		if (!Frame.Joints.IsValidIndex(Palm))
		{
			break;
		}
		if (FVector::Distance(Frame.Joints[Palm].Location, FinalPalm) > PlateauPalmCentimetres)
		{
			break;
		}
		Start = Frame.TimeSeconds;
	}
	return Start;
}

bool UHandClipPlayer::GetLatest(EControllerHand Hand, FHandFrame& Out) const
{
	if (Hand == EControllerHand::Left && bHasLeft)
	{
		Out = LatestLeft;
		return true;
	}
	if (Hand == EControllerHand::Right && bHasRight)
	{
		Out = LatestRight;
		return true;
	}
	return false;
}

void UHandClipPlayer::Tick(float DeltaTime)
{
	if (IsTickable())
	{
		Step(static_cast<double>(DeltaTime));
	}
}

TStatId UHandClipPlayer::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UHandClipPlayer, STATGROUP_Tickables);
}

bool UHandClipPlayer::IsTickable() const
{
	return GetWorld() != nullptr && (bPlaying || bTrackingDropped);
}

UWorld* UHandClipPlayer::GetTickableGameObjectWorld() const
{
	return GetWorld();
}

void UHandClipPlayer::BeginDestroy()
{
	SetTickableTickType(ETickableTickType::Never);
	Super::BeginDestroy();
}
