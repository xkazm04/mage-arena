#include "Hands/HandClipPlayer.h"

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
	Duration = Clip.GetDuration();
	bPlaying = Duration > 0.0;
	Sequence = 0;
	bHasLeft = false;
	bHasRight = false;
	Emit();
	if (bPlaying && GetWorld() != nullptr)
	{
		SetTickableTickType(ETickableTickType::Always);
	}
	else
	{
		SetTickableTickType(ETickableTickType::Never);
	}
}

void UHandClipPlayer::Stop()
{
	bPlaying = false;
	SetTickableTickType(ETickableTickType::Never);
}

void UHandClipPlayer::Step(double DeltaSeconds)
{
	if (!bHasClip)
	{
		return;
	}
	TimeSeconds = FMath::Clamp(TimeSeconds + DeltaSeconds, 0.0, Duration);
	Emit();
	if (TimeSeconds >= Duration)
	{
		bPlaying = false;
		SetTickableTickType(ETickableTickType::Never);
	}
}

void UHandClipPlayer::Emit()
{
	for (const EControllerHand Hand : Clip.GetHands())
	{
		FHandFrame Frame;
		if (!Clip.Sample(Hand, TimeSeconds, Frame))
		{
			continue;
		}
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
	return bPlaying && GetWorld() != nullptr;
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
