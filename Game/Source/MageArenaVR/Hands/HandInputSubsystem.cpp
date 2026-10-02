#include "Hands/HandInputSubsystem.h"

#include "MageArenaVR.h"

void UHandInputSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	EnsurePlayer();
}

void UHandInputSubsystem::Deinitialize()
{
	if (ClipPlayer)
	{
		ClipPlayer->Stop();
	}
	ClipPlayer = nullptr;
	Super::Deinitialize();
}

void UHandInputSubsystem::EnsurePlayer()
{
	if (!ClipPlayer)
	{
		ClipPlayer = NewObject<UHandClipPlayer>(this);
	}
}

void UHandInputSubsystem::PlayQuickAction(FName Action, EClipVariant Variant)
{
	EnsurePlayer();
	const FString Path = FHandClip::MakeFilePath(Action, Variant);
	FHandClip Loaded;
	FString Error;
	if (!FHandClip::LoadFromFile(Path, Loaded, Error))
	{
		UE_LOG(LogMageArena, Error, TEXT("PlayQuickAction failed for %s: %s"), *Action.ToString(), *Error);
		return;
	}
	ClipPlayer->Play(Loaded);
	UE_LOG(LogMageArena, Log, TEXT("Hand clip started: action=%s variant=%s source=%s frames=%d duration=%.3fs path=%s"),
		*Loaded.Action, *Loaded.Variant, *Loaded.Source, Loaded.GetSampleCount(), Loaded.GetDuration(), *Path);
}

void UHandInputSubsystem::Step(double DeltaSeconds)
{
	if (ClipPlayer)
	{
		ClipPlayer->Step(DeltaSeconds);
	}
}

bool UHandInputSubsystem::GetLatest(EControllerHand Hand, FHandFrame& Out) const
{
	return ClipPlayer && ClipPlayer->GetLatest(Hand, Out);
}

uint64 UHandInputSubsystem::GetFrameSequence() const
{
	return ClipPlayer ? ClipPlayer->GetFrameSequence() : 0;
}

FOnHandSourceFrame& UHandInputSubsystem::OnHandFrame()
{
	EnsurePlayer();
	return ClipPlayer->OnHandFrame();
}
