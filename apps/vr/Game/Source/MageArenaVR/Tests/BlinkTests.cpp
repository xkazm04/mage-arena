#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Gestures/BlinkDetector.h"
#include "Hands/ClipVariant.h"
#include "Hands/HandClip.h"
#include "Hands/HandClipPlayer.h"
#include "Hands/HandInputSubsystem.h"
#include "Hands/MageSettings.h"
#include "MageArenaVR.h"

namespace
{
const FHandClipTrack* CastingTrack(const FHandClip& Clip)
{
	for (const FHandClipTrack& Track : Clip.GetTracks())
	{
		if (Track.Hand == FMageSettings::CastingHand() && Track.Frames.Num() > 1)
		{
			return &Track;
		}
	}
	return nullptr;
}

// One stream of casting-hand frames with strictly increasing timestamps and no rewind. A device source looks like this;
// the clip player restarts at t = 0 for every action. Between two clips the hand eases from where the first ended to where
// the second starts, over a second, so the join is slow and cannot look like a flick.
bool FeedStream(FBlinkDetector& Detector, const TArray<const TCHAR*>& Actions, FString& OutError)
{
	double Time = 0.0;
	const FHandFrame* Previous = nullptr;
	TArray<FHandClip> Clips;
	Clips.Reserve(Actions.Num());
	for (const TCHAR* Action : Actions)
	{
		FHandClip& Clip = Clips.AddDefaulted_GetRef();
		if (!FHandClip::LoadFromFile(FHandClip::MakeFilePath(Action, EClipVariant::Normal), Clip, OutError))
		{
			return false;
		}
		const FHandClipTrack* Track = CastingTrack(Clip);
		if (!Track)
		{
			OutError = FString::Printf(TEXT("%s has no casting-hand track"), Action);
			return false;
		}
		if (Previous)
		{
			constexpr int32 BridgeSteps = 72;
			for (int32 Step = 1; Step <= BridgeSteps; ++Step)
			{
				const double Alpha = static_cast<double>(Step) / static_cast<double>(BridgeSteps);
				FHandFrame Frame = *Previous;
				Frame.TimeSeconds = Time + static_cast<double>(Step) / 72.0;
				for (int32 Joint = 0; Joint < Frame.Joints.Num(); ++Joint)
				{
					Frame.Joints[Joint].Location = FMath::Lerp(Previous->Joints[Joint].Location, Track->Frames[0].Joints[Joint].Location, Alpha);
				}
				Detector.Ingest(Frame);
			}
			Time += static_cast<double>(BridgeSteps) / 72.0 + 1.0 / 72.0;
		}
		for (const FHandFrame& Source : Track->Frames)
		{
			FHandFrame Frame = Source;
			Frame.TimeSeconds = Source.TimeSeconds + Time;
			Detector.Ingest(Frame);
		}
		Time += Track->Frames.Last().TimeSeconds;
		Previous = &Track->Frames.Last();
	}
	Detector.Flush();
	return true;
}

bool PlayClip(UHandInputSubsystem* Hands, const TCHAR* Action, EClipVariant Variant, FBlinkDetector& Detector, int32 Pad)
{
	Detector.Reset();
	Detector.SetActivePad(Pad);
	Hands->PlayQuickAction(Action, Variant);
	UHandClipPlayer* Player = Hands->GetClipPlayer();
	if (!Player || !Player->IsPlaying())
	{
		return false;
	}
	int32 Guard = 0;
	while (Player->IsPlaying() && Guard < 2000)
	{
		Hands->Step(1.0 / 72.0);
		++Guard;
	}
	Detector.Flush();
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaBlinkClips, "MageArena.Blink.Clips",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaBlinkClips::RunTest(const FString& Parameters)
{
	bool bPass = true;
	UGameInstance* Instance = NewObject<UGameInstance>(GetTransientPackage());
	Instance->AddToRoot();
	UHandInputSubsystem* Hands = NewObject<UHandInputSubsystem>(Instance);
	UBlinkDetectorSubsystem* Blinks = NewObject<UBlinkDetectorSubsystem>(Instance);
	Blinks->BindToHands(Hands);
	FBlinkDetector& Detector = Blinks->GetDetector();

	const EClipVariant Variants[] = {EClipVariant::Normal, EClipVariant::Slow, EClipVariant::Sloppy};
	const TCHAR* VariantNames[] = {TEXT("normal"), TEXT("slow"), TEXT("sloppy")};

	struct FExpect
	{
		const TCHAR* Action;
		int32 FromPad;
		int32 BlinkCount;
		int32 BoltCount;
		int32 Pad;
	};
	const FExpect Cases[] = {
		{TEXT("blink-left"), 1, 1, 0, 0},
		{TEXT("blink-right"), 1, 1, 0, 2},
		{TEXT("blink-back"), 1, 1, 0, 1},
		{TEXT("blink-left"), 0, 1, 0, 0},
		{TEXT("blink-right"), 0, 1, 0, 1},
		{TEXT("blink-back"), 0, 1, 0, 1},
		{TEXT("blink-left"), 2, 1, 0, 1},
		{TEXT("blink-right"), 2, 1, 0, 2},
		{TEXT("blink-back"), 2, 1, 0, 1},
		{TEXT("sigil-line1"), 1, 0, 0, 1},
		{TEXT("sigil-line2"), 1, 0, 0, 1},
		{TEXT("sigil-line3"), 1, 0, 0, 1},
		{TEXT("mudra"), 1, 0, 0, 1},
		{TEXT("ward-raise"), 1, 0, 0, 1},
		{TEXT("bolt"), 1, 0, 1, 1},
	};

	for (int32 VariantIndex = 0; VariantIndex < 3; ++VariantIndex)
	{
		for (const FExpect& Case : Cases)
		{
			const FString Label = FString::Printf(TEXT("%s.%s from %d"), Case.Action, VariantNames[VariantIndex], Case.FromPad);
			if (!PlayClip(Hands, Case.Action, Variants[VariantIndex], Detector, Case.FromPad))
			{
				AddError(Label + TEXT(" did not play"));
				bPass = false;
				continue;
			}
			if (Detector.GetBlinkCount() != Case.BlinkCount || Detector.GetBoltCount() != Case.BoltCount)
			{
				AddError(FString::Printf(TEXT("%s blinks=%d bolts=%d hottest=%.2f yaw=%.1f"),
					*Label, Detector.GetBlinkCount(), Detector.GetBoltCount(), Detector.GetHottestSpeed(),
					Detector.GetBlinkCount() > 0 ? Detector.GetLastBlink().YawDegrees : Detector.GetLastBolt().YawDegrees));
				bPass = false;
				continue;
			}
			if (Case.BlinkCount == 1)
			{
				bPass &= TestEqual(*Label, Detector.GetLastBlink().PadIndex, Case.Pad);
			}
		}
	}

	bPass &= TestEqual(TEXT("left from centre"), FBlinkDetector::ResolvePad(EBlinkDirection::Left, 1), 0);
	bPass &= TestEqual(TEXT("right from centre"), FBlinkDetector::ResolvePad(EBlinkDirection::Right, 1), 2);
	bPass &= TestEqual(TEXT("back from side"), FBlinkDetector::ResolvePad(EBlinkDirection::Back, 2), 1);

	Blinks->BindToHands(nullptr);
	Instance->RemoveFromRoot();
	Instance->MarkAsGarbage();
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaBlinkStream, "MageArena.Blink.Stream",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaBlinkStream::RunTest(const FString& Parameters)
{
	// The detector latches after a flick and must let go once the tip has been still, not only when the clock rewinds. A
	// stream with no rewinds, as a headset gives, has to detect every flick in it and nothing else.
	const FMageSettingsState SavedSettings = FMageSettings::Get();
	FMageSettings::Restore(FMageSettingsState());

	struct FStreamCase
	{
		const TCHAR* Label;
		TArray<const TCHAR*> Actions;
		int32 Blinks;
		int32 Bolts;
		EBlinkDirection LastDirection;
	};
	const FStreamCase Cases[] = {
		{TEXT("left then right"), {TEXT("blink-left"), TEXT("blink-right")}, 2, 0, EBlinkDirection::Right},
		{TEXT("bolt then left"), {TEXT("bolt"), TEXT("blink-left")}, 1, 1, EBlinkDirection::Left},
		{TEXT("left, a sigil, then right"), {TEXT("blink-left"), TEXT("sigil-line1"), TEXT("blink-right")}, 2, 0, EBlinkDirection::Right},
		{TEXT("left, left, back"), {TEXT("blink-left"), TEXT("blink-left"), TEXT("blink-back")}, 3, 0, EBlinkDirection::Back},
	};

	bool bPass = true;
	for (const FStreamCase& Case : Cases)
	{
		FBlinkDetector Detector;
		Detector.Reset();
		FString Error;
		if (!TestTrue(*FString::Printf(TEXT("%s: stream built (%s)"), Case.Label, *Error), FeedStream(Detector, Case.Actions, Error)))
		{
			bPass = false;
			continue;
		}
		bPass &= TestEqual(*FString::Printf(TEXT("%s: blinks"), Case.Label), Detector.GetBlinkCount(), Case.Blinks);
		bPass &= TestEqual(*FString::Printf(TEXT("%s: bolts"), Case.Label), Detector.GetBoltCount(), Case.Bolts);
		if (Case.Blinks > 0)
		{
			bPass &= TestTrue(*FString::Printf(TEXT("%s: the last blink goes the right way"), Case.Label), Detector.GetLastBlink().Direction == Case.LastDirection);
		}
	}
	FMageSettings::Restore(SavedSettings);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaBlinkTiming72, "MageArena.Blink.Timing72",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaBlinkTiming72::RunTest(const FString& Parameters)
{
	// The G1 fix of the fall and the re-arm (FBlinkThresholds::FallSpanS, RearmSpanMinS) must not move anything at 72 Hz.
	// Every blink and bolt clip, with 0.5 s of still tail so the latch can let go, through a bare detector. The fire frame,
	// the event frame, the direction and the count are pinned to what the detector gave before that fix; the re-arm may
	// come earlier, never later. Frames are 72 Hz indices from the clip start.
	const FMageSettingsState SavedSettings = FMageSettings::Get();
	FMageSettings::Restore(FMageSettingsState());

	struct FPinned
	{
		const TCHAR* Action;
		EClipVariant Variant;
		const TCHAR* VariantName;
		int32 Blinks;
		int32 Bolts;
		EBlinkDirection Direction;
		int32 FireFrame;
		int32 EventFrame;
		int32 RearmFrame;
	};
	const FPinned Cases[] = {
		{TEXT("blink-left"), EClipVariant::Normal, TEXT("normal"), 1, 0, EBlinkDirection::Left, 14, 11, 35},
		{TEXT("blink-left"), EClipVariant::Slow, TEXT("slow"), 1, 0, EBlinkDirection::Left, 22, 17, 33},
		{TEXT("blink-left"), EClipVariant::Sloppy, TEXT("sloppy"), 1, 0, EBlinkDirection::Left, 12, 10, 38},
		{TEXT("blink-right"), EClipVariant::Normal, TEXT("normal"), 1, 0, EBlinkDirection::Right, 14, 11, 35},
		{TEXT("blink-right"), EClipVariant::Slow, TEXT("slow"), 1, 0, EBlinkDirection::Right, 22, 17, 33},
		{TEXT("blink-right"), EClipVariant::Sloppy, TEXT("sloppy"), 1, 0, EBlinkDirection::Right, 12, 10, 36},
		{TEXT("blink-back"), EClipVariant::Normal, TEXT("normal"), 1, 0, EBlinkDirection::Back, 14, 11, 35},
		{TEXT("blink-back"), EClipVariant::Slow, TEXT("slow"), 1, 0, EBlinkDirection::Back, 22, 17, 33},
		{TEXT("blink-back"), EClipVariant::Sloppy, TEXT("sloppy"), 1, 0, EBlinkDirection::Back, 12, 10, 36},
		{TEXT("bolt"), EClipVariant::Normal, TEXT("normal"), 0, 1, EBlinkDirection::Left, 15, 12, 38},
		{TEXT("bolt"), EClipVariant::Slow, TEXT("slow"), 0, 1, EBlinkDirection::Left, 24, 19, 35},
		{TEXT("bolt"), EClipVariant::Sloppy, TEXT("sloppy"), 0, 1, EBlinkDirection::Left, 14, 11, 38},
	};
	const TCHAR* DirectionNames[] = {TEXT("left"), TEXT("right"), TEXT("back")};

	bool bPass = true;
	for (const FPinned& Case : Cases)
	{
		const FString Label = FString::Printf(TEXT("%s.%s"), Case.Action, Case.VariantName);
		FHandClip Clip;
		FString Error;
		if (!FHandClip::LoadFromFile(FHandClip::MakeFilePath(Case.Action, Case.Variant), Clip, Error))
		{
			AddError(FString::Printf(TEXT("%s: %s"), *Label, *Error));
			bPass = false;
			continue;
		}
		const FHandClipTrack* Track = CastingTrack(Clip);
		if (!Track)
		{
			AddError(Label + TEXT(" has no casting-hand track"));
			bPass = false;
			continue;
		}
		TArray<FHandFrame> Frames = Track->Frames;
		const FHandFrame Last = Frames.Last();
		for (int32 Index = 1; Index <= 36; ++Index)
		{
			FHandFrame Still = Last;
			Still.TimeSeconds = Last.TimeSeconds + Index / 72.0;
			Frames.Add(Still);
		}

		FBlinkDetector Detector;
		Detector.Reset();
		int32 FireFrame = -1;
		int32 RearmFrame = -1;
		for (const FHandFrame& Frame : Frames)
		{
			Detector.Ingest(Frame);
			const int32 FrameIndex = FMath::RoundToInt(Frame.TimeSeconds * 72.0);
			if (FireFrame < 0 && Detector.GetBlinkCount() + Detector.GetBoltCount() > 0)
			{
				FireFrame = FrameIndex;
			}
			else if (FireFrame >= 0 && RearmFrame < 0 && Detector.GetHottestSpeed() < FBlinkThresholds::MinTipSpeedMps)
			{
				RearmFrame = FrameIndex;
			}
		}
		Detector.Flush();
		const bool bBlink = Detector.GetBlinkCount() > 0;
		const double EventTime = bBlink ? Detector.GetLastBlink().TimeSeconds : Detector.GetLastBolt().TimeSeconds;
		const int32 EventFrame = FMath::RoundToInt(EventTime * 72.0);
		const FString Direction = bBlink ? DirectionNames[static_cast<int32>(Detector.GetLastBlink().Direction)] : TEXT("bolt");
		const FString Line = FString::Printf(TEXT("Blink72 %s blinks=%d bolts=%d direction=%s fireFrame=%d eventFrame=%d rearmFrame=%d"),
			*Label, Detector.GetBlinkCount(), Detector.GetBoltCount(), *Direction, FireFrame, EventFrame, RearmFrame);
		UE_LOG(LogMageArena, Log, TEXT("%s"), *Line);
		AddInfo(Line);

		bPass &= TestEqual(*FString::Printf(TEXT("%s blinks"), *Label), Detector.GetBlinkCount(), Case.Blinks);
		bPass &= TestEqual(*FString::Printf(TEXT("%s bolts"), *Label), Detector.GetBoltCount(), Case.Bolts);
		if (Case.Blinks > 0 && bBlink)
		{
			bPass &= TestTrue(*FString::Printf(TEXT("%s direction"), *Label), Detector.GetLastBlink().Direction == Case.Direction);
		}
		bPass &= TestEqual(*FString::Printf(TEXT("%s fire frame"), *Label), FireFrame, Case.FireFrame);
		bPass &= TestEqual(*FString::Printf(TEXT("%s event frame"), *Label), EventFrame, Case.EventFrame);
		if (RearmFrame < 0 || RearmFrame > Case.RearmFrame)
		{
			AddError(FString::Printf(TEXT("%s re-arms at frame %d, later than %d"), *Label, RearmFrame, Case.RearmFrame));
			bPass = false;
		}
	}
	FMageSettings::Restore(SavedSettings);
	return bPass;
}

#endif
