#include "Hands/HandInputSubsystem.h"

#include "MageArenaVR.h"

void UHandInputSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	EnsurePlayers();
}

void UHandInputSubsystem::Deinitialize()
{
	if (WardPlayer)
	{
		WardPlayer->Stop();
	}
	if (ActionPlayer)
	{
		ActionPlayer->Stop();
	}
	WardPlayer = nullptr;
	ActionPlayer = nullptr;
	LastStarted = nullptr;
	Super::Deinitialize();
}

void UHandInputSubsystem::EnsurePlayers()
{
	if (!WardPlayer)
	{
		WardPlayer = NewObject<UHandClipPlayer>(this);
		WardPlayer->OnHandFrame().AddUObject(this, &UHandInputSubsystem::ForwardFrame);
	}
	if (!ActionPlayer)
	{
		ActionPlayer = NewObject<UHandClipPlayer>(this);
		ActionPlayer->OnHandFrame().AddUObject(this, &UHandInputSubsystem::ForwardFrame);
	}
}

void UHandInputSubsystem::ForwardFrame(const FHandFrame& Frame)
{
	FrameDelegate.Broadcast(Frame);
}

bool UHandInputSubsystem::ActionOwns(EControllerHand Hand) const
{
	return ActionPlayer && ActionPlayer->IsPlaying() && ActionPlayer->GetClip().GetHands().Contains(Hand);
}

void UHandInputSubsystem::PlayQuickAction(FName Action, EClipVariant Variant)
{
	EnsurePlayers();
	const FString Path = FHandClip::MakeFilePath(Action, Variant);
	FHandClip Loaded;
	FString Error;
	if (!FHandClip::LoadFromFile(Path, Loaded, Error))
	{
		UE_LOG(LogMageArena, Error, TEXT("PlayQuickAction failed for %s: %s"), *Action.ToString(), *Error);
		return;
	}
	const bool bUsesLeft = Loaded.GetHands().Contains(EControllerHand::Left);
	if (bUsesLeft)
	{
		bWardHeld = false;
		if (WardPlayer)
		{
			WardPlayer->Stop();
		}
	}
	ActionPlayer->Play(Loaded);
	LastStarted = ActionPlayer;
	UE_LOG(LogMageArena, Log, TEXT("Hand clip started: action=%s variant=%s source=%s frames=%d duration=%.3fs path=%s"),
		*Loaded.Action, *Loaded.Variant, *Loaded.Source, Loaded.GetSampleCount(), Loaded.GetDuration(), *Path);
}

void UHandInputSubsystem::SetWardHeld(bool bHeld, EClipVariant Variant)
{
	EnsurePlayers();
	if (bHeld)
	{
		if (ActionOwns(EControllerHand::Left))
		{
			ActionPlayer->Stop();
		}
		if (bWardHeld)
		{
			return;
		}
		if (WardPlayer->CanResumeHold())
		{
			bWardHeld = true;
			HeldVariant = Variant;
			WardPlayer->ResumeHold();
			LastStarted = WardPlayer;
			UE_LOG(LogMageArena, Log, TEXT("Ward hold resumed variant=%s"), FHandClip::VariantToString(Variant));
			return;
		}
		const FString Path = FHandClip::MakeFilePath(TEXT("ward-raise"), Variant);
		FHandClip Loaded;
		FString Error;
		if (!FHandClip::LoadFromFile(Path, Loaded, Error))
		{
			UE_LOG(LogMageArena, Error, TEXT("Ward hold failed: %s"), *Error);
			return;
		}
		const double Plateau = UHandClipPlayer::FindPlateauStart(Loaded, EControllerHand::Left);
		HeldVariant = Variant;
		bWardHeld = true;
		WardPlayer->Play(Loaded);
		WardPlayer->BeginHold(Plateau);
		LastStarted = WardPlayer;
		UE_LOG(LogMageArena, Log, TEXT("Ward hold started variant=%s plateau=%.3f path=%s"),
			FHandClip::VariantToString(Variant), Plateau, *Path);
		return;
	}

	if (!bWardHeld)
	{
		return;
	}
	bWardHeld = false;
	WardPlayer->EndHold();
	UE_LOG(LogMageArena, Log, TEXT("Ward hold released"));
}

void UHandInputSubsystem::SetAimYaw(double YawRadians)
{
	EnsurePlayers();
	WardPlayer->SetAimYaw(YawRadians);
}

void UHandInputSubsystem::Step(double DeltaSeconds)
{
	EnsurePlayers();
	WardPlayer->Step(DeltaSeconds);
	ActionPlayer->Step(DeltaSeconds);
}

bool UHandInputSubsystem::GetLatest(EControllerHand Hand, FHandFrame& Out) const
{
	if (Hand == EControllerHand::Left)
	{
		if (ActionOwns(Hand) && ActionPlayer->GetLatest(Hand, Out))
		{
			return true;
		}
		if (WardPlayer && (WardPlayer->IsPlaying() || !ActionPlayer || !ActionPlayer->GetClip().GetHands().Contains(Hand))
			&& WardPlayer->GetLatest(Hand, Out))
		{
			return true;
		}
		return ActionPlayer && ActionPlayer->GetLatest(Hand, Out);
	}
	if (ActionPlayer && ActionPlayer->GetClip().GetHands().Contains(Hand) && ActionPlayer->GetLatest(Hand, Out))
	{
		return true;
	}
	return WardPlayer && WardPlayer->GetLatest(Hand, Out);
}

uint64 UHandInputSubsystem::GetFrameSequence() const
{
	return LastStarted ? LastStarted->GetFrameSequence() : 0;
}

FOnHandSourceFrame& UHandInputSubsystem::OnHandFrame()
{
	EnsurePlayers();
	return FrameDelegate;
}

bool UHandInputSubsystem::IsActionPlaying() const
{
	return ActionPlayer && ActionPlayer->IsPlaying();
}

FString UHandInputSubsystem::GetActionName() const
{
	if (!ActionPlayer)
	{
		return FString();
	}
	return ActionPlayer->GetClip().Action;
}
