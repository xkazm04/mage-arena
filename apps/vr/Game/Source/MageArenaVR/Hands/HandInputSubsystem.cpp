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
	bWardHeld = false;
	ClipPlayer->SetAimYaw(0.0);
	ClipPlayer->Play(Loaded);
	UE_LOG(LogMageArena, Log, TEXT("Hand clip started: action=%s variant=%s source=%s frames=%d duration=%.3fs path=%s"),
		*Loaded.Action, *Loaded.Variant, *Loaded.Source, Loaded.GetSampleCount(), Loaded.GetDuration(), *Path);
}

void UHandInputSubsystem::SetWardHeld(bool bHeld, EClipVariant Variant)
{
	EnsurePlayer();
	if (bHeld)
	{
		if (bWardHeld)
		{
			return;
		}
		if (ClipPlayer->CanResumeHold())
		{
			bWardHeld = true;
			HeldVariant = Variant;
			ClipPlayer->ResumeHold();
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
		ClipPlayer->Play(Loaded);
		ClipPlayer->BeginHold(Plateau);
		UE_LOG(LogMageArena, Log, TEXT("Ward hold started variant=%s plateau=%.3f path=%s"),
			FHandClip::VariantToString(Variant), Plateau, *Path);
		return;
	}

	if (!bWardHeld)
	{
		return;
	}
	bWardHeld = false;
	ClipPlayer->EndHold();
	UE_LOG(LogMageArena, Log, TEXT("Ward hold released"));
}

void UHandInputSubsystem::SetAimYaw(double YawRadians)
{
	EnsurePlayer();
	ClipPlayer->SetAimYaw(YawRadians);
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
