#include "Gestures/SigilRecognizerSubsystem.h"

#include "HAL/FileManager.h"
#include "Hands/HandClip.h"
#include "Hands/HandInputSubsystem.h"
#include "HeadMountedDisplayTypes.h"
#include "MageArenaVR.h"
#include "Misc/Paths.h"

void USigilRecognizerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency(UHandInputSubsystem::StaticClass());
	LoadTemplatesFromDirectory(FPaths::Combine(FPaths::ProjectDir(), TEXT("Clips"), TEXT("templates")));
	if (UGameInstance* Instance = GetGameInstance())
	{
		BindToHands(Instance->GetSubsystem<UHandInputSubsystem>());
	}
}

void USigilRecognizerSubsystem::Deinitialize()
{
	BindToHands(nullptr);
	Super::Deinitialize();
}

void USigilRecognizerSubsystem::SetDrawStyle(ESigilDrawStyle Style)
{
	DrawStyle = Style;
	HandBuilder.SetStyle(Style);
	bHasHandTime = false;
}

void USigilRecognizerSubsystem::LoadTemplatesFromDirectory(const FString& Directory)
{
	Recognizer.Reset();
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *Directory, TEXT("*.jsonl"), true, false);
	Files.Sort();

	int32 Line1 = 0;
	int32 Line2 = 0;
	int32 Line3 = 0;
	int32 Skipped = 0;
	for (const FString& Path : Files)
	{
		FHandClip Clip;
		FString Error;
		if (!FHandClip::LoadFromFile(Path, Clip, Error))
		{
			UE_LOG(LogMageArena, Warning, TEXT("Sigil template skipped %s: %s"), *Path, *Error);
			++Skipped;
			continue;
		}
		FName Label;
		if (Clip.Action.EndsWith(TEXT("line1")))
		{
			Label = TEXT("line1");
		}
		else if (Clip.Action.EndsWith(TEXT("line2")))
		{
			Label = TEXT("line2");
		}
		else if (Clip.Action.EndsWith(TEXT("line3")))
		{
			Label = TEXT("line3");
		}
		else
		{
			UE_LOG(LogMageArena, Warning, TEXT("Sigil template skipped %s: action %s is not a line"), *Path, *Clip.Action);
			++Skipped;
			continue;
		}

		TArray<FQPoint> Points;
		// Templates are authored with pinch as the pen, including the one-stroke continuous set.
		if (!ExtractFirstGesture(Clip, ESigilDrawStyle::StyleA, Points) || !Recognizer.AddTemplate(Label, Points))
		{
			UE_LOG(LogMageArena, Warning, TEXT("Sigil template skipped %s: no loop"), *Path);
			++Skipped;
			continue;
		}
		if (Label == TEXT("line1"))
		{
			++Line1;
		}
		else if (Label == TEXT("line2"))
		{
			++Line2;
		}
		else
		{
			++Line3;
		}
	}
	UE_LOG(LogMageArena, Log, TEXT("Sigil templates loaded line1=%d line2=%d line3=%d skipped=%d from %s"),
		Line1, Line2, Line3, Skipped, *Directory);
}

void USigilRecognizerSubsystem::BindToHands(UHandInputSubsystem* Hands)
{
	if (BoundHands.IsValid())
	{
		BoundHands->OnHandFrame().Remove(HandHandle);
	}
	HandHandle.Reset();
	BoundHands = Hands;
	HandBuilder.Reset();
	HandBuilder.SetStyle(DrawStyle);
	bHasHandTime = false;
	if (Hands)
	{
		HandHandle = Hands->OnHandFrame().AddUObject(this, &USigilRecognizerSubsystem::HandleHandFrame);
	}
}

void USigilRecognizerSubsystem::HandleHandFrame(const FHandFrame& Frame)
{
	if (Frame.Hand != EControllerHand::Right)
	{
		return;
	}
	const int32 TipIndex = static_cast<int32>(EHandKeypoint::IndexTip);
	if (!Frame.Joints.IsValidIndex(TipIndex))
	{
		return;
	}
	// A new clip restarts at t=0. Do not let that sample continue the previous stroke.
	// UHandClipPlayer::Step also keeps emitting after the clip ends; callers stop at Duration.
	if (bHasHandTime && Frame.TimeSeconds + 1.0e-4 < LastHandTime)
	{
		HandBuilder.Reset();
		HandBuilder.SetStyle(DrawStyle);
	}
	bHasHandTime = true;
	LastHandTime = Frame.TimeSeconds;

	TArray<FQPoint> Points;
	const ESigilStrokeEvent Event = HandBuilder.AddTip(Frame.Joints[TipIndex].Location, Frame.Pinch, Frame.TimeSeconds, Points);
	Dispatch(Event, Points);
}

void USigilRecognizerSubsystem::IngestMouse(const FVector& TipCm, float Pinch, double TimeSeconds)
{
	TArray<FQPoint> Points;
	const ESigilStrokeEvent Event = MouseBuilder.AddTip(TipCm, Pinch, TimeSeconds, Points);
	Dispatch(Event, Points);
}

void USigilRecognizerSubsystem::Dispatch(ESigilStrokeEvent Event, const TArray<FQPoint>& Points)
{
	if (Event == ESigilStrokeEvent::Rejected)
	{
		UE_LOG(LogMageArena, Log, TEXT("Sigil rejected: no circle"));
		OnSigilRejected.Broadcast(-1.0);
		return;
	}
	if (Event != ESigilStrokeEvent::Gesture)
	{
		return;
	}
	const FQClassifyResult Result = Recognizer.Classify(Points);
	if (Result.bReject)
	{
		UE_LOG(LogMageArena, Log, TEXT("Sigil rejected: line=%s score=%.3f distance=%.1f latency=%.3fms"),
			*Result.Label.ToString(), Result.Score, Result.Distance, Result.Milliseconds);
		OnSigilRejected.Broadcast(Result.Distance);
		return;
	}
	UE_LOG(LogMageArena, Log, TEXT("Sigil cast: line=%s score=%.3f distance=%.1f latency=%.3fms"),
		*Result.Label.ToString(), Result.Score, Result.Distance, Result.Milliseconds);
	OnSigilCast.Broadcast(Result.Label, Result.Score, Result.Milliseconds);
}
