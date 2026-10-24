#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// T25: the closed fist held 0.40 s. Literals and their sources:
// - apps/vr/Game/Clips/fist/: fist.{normal,slow,sloppy} and the fist-loose impostor (apps/vr/tools/clipgen/fist.mjs).
// - apps/vr/Game/Clips/stones/: the palm held over each stone; ward-raise and both-palms are open palms too.
// - teach.json offerHoldS 0.40 (the stone hold, the fist hold and the palm hold at missio).

#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Gestures/BlinkDetector.h"
#include "Gestures/FistDetector.h"
#include "Gestures/SigilRecognizerSubsystem.h"
#include "Gestures/StaffDetector.h"
#include "Gestures/WardDetector.h"
#include "HAL/FileManager.h"
#include "Hands/ClipVariant.h"
#include "Hands/HandClip.h"
#include "Hands/HandInputSubsystem.h"
#include "Kernel/ArenaKernel.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Session/ArenaSession.h"

namespace
{
bool LoadFistClip(FAutomationTestBase& Test, const TCHAR* Action, EClipVariant Variant, FHandClip& Out)
{
	FString Error;
	const FString Path = FHandClip::MakeFilePath(Action, Variant);
	if (!FHandClip::LoadFromFile(Path, Out, Error))
	{
		Test.AddError(FString::Printf(TEXT("%s: %s"), *Path, *Error));
		return false;
	}
	return true;
}

// Every frame of every track, in time order per hand, into the detector. Returns the fists held.
int32 FeedFist(FFistDetector& Detector, const FHandClip& Clip, int32* OutClosedFrames = nullptr)
{
	int32 Closed = 0;
	for (const FHandClipTrack& Track : Clip.GetTracks())
	{
		for (const FHandFrame& Frame : Track.Frames)
		{
			Detector.Ingest(Frame);
			Closed += Detector.IsClosed(Frame.Hand) ? 1 : 0;
		}
	}
	if (OutClosedFrames)
	{
		*OutClosedFrames = Closed;
	}
	return Detector.GetHeldCount();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaFistTakes, "MageArena.Fist.Takes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaFistTakes::RunTest(const FString& Parameters)
{
	(void)Parameters;
	bool bPass = true;
	for (const EClipVariant Variant : {EClipVariant::Normal, EClipVariant::Slow, EClipVariant::Sloppy})
	{
		const FString Label = FString::Printf(TEXT("fist.%s"), FHandClip::VariantToString(Variant));
		FHandClip Clip;
		if (!LoadFistClip(*this, TEXT("fist"), Variant, Clip))
		{
			bPass = false;
			continue;
		}
		FFistDetector Detector;
		int32 Closed = 0;
		const int32 Held = FeedFist(Detector, Clip, &Closed);
		bPass &= TestEqual(*(Label + TEXT(" is one fist held 0.40 s")), Held, 1);
		// 72 Hz: the hold needs about 29 closed frames; every take holds well past that.
		bPass &= TestTrue(*FString::Printf(TEXT("%s closed for %d frames (>= 29)"), *Label, Closed), Closed >= 29);
		bPass &= TestFalse(*(Label + TEXT(" opens again at the end")), Detector.IsClosed(EControllerHand::Left));
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaFistImpostor, "MageArena.Fist.ImpostorRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaFistImpostor::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FHandClip Clip;
	if (!LoadFistClip(*this, TEXT("fist-loose"), EClipVariant::Normal, Clip))
	{
		return false;
	}
	FFistDetector Detector;
	int32 Closed = 0;
	bool bPass = TestEqual(TEXT("the loose half-curl is never a fist held"), FeedFist(Detector, Clip, &Closed), 0);
	bPass &= TestEqual(TEXT("not one frame of the half-curl is closed"), Closed, 0);
	// The same hand with the thumb taken off the fingers: four curled fingers alone are not the fist.
	FHandClip Fist;
	if (!LoadFistClip(*this, TEXT("fist"), EClipVariant::Normal, Fist))
	{
		return false;
	}
	const FHandClipTrack& Track = Fist.GetTracks()[0];
	FHandFrame Held = Track.Frames[Track.Frames.Num() / 2];
	bPass &= TestTrue(TEXT("mid-take frame is a fist"), FFistDetector::IsFistPose(Held, false));
	const int32 Thumb = static_cast<int32>(EHandKeypoint::ThumbTip);
	const int32 Index = static_cast<int32>(EHandKeypoint::IndexIntermediate);
	const FVector Away = (Held.Joints[Thumb].Location - Held.Joints[Index].Location).GetSafeNormal();
	Held.Joints[Thumb].Location += Away * 4.0;
	bPass &= TestFalse(*FString::Printf(TEXT("thumb out %.3f m is not a fist"), FFistDetector::ThumbOverMetres(Held)),
		FFistDetector::IsFistPose(Held, false));
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaFistNotPalm, "MageArena.Fist.OpenPalmNeverFist",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaFistNotPalm::RunTest(const FString& Parameters)
{
	(void)Parameters;
	bool bPass = true;
	// The palm over each stone (held 0.40 s in a pick), the ward palm and both palms: open hands, never a fist.
	const TCHAR* const Actions[] = {TEXT("stone-left"), TEXT("stone-centre"), TEXT("stone-right"), TEXT("ward-raise"), TEXT("both-palms")};
	for (const TCHAR* Action : Actions)
	{
		FHandClip Clip;
		if (!LoadFistClip(*this, Action, EClipVariant::Normal, Clip))
		{
			bPass = false;
			continue;
		}
		FFistDetector Detector;
		int32 Closed = 0;
		bPass &= TestEqual(*FString::Printf(TEXT("%s holds no fist"), Action), FeedFist(Detector, Clip, &Closed), 0);
		bPass &= TestEqual(*FString::Printf(TEXT("%s has no closed frame"), Action), Closed, 0);
		int32 Open = 0;
		for (const FHandClipTrack& Track : Clip.GetTracks())
		{
			Open += FFistDetector::IsOpenHand(Track.Frames.Last()) ? 1 : 0;
		}
		bPass &= TestTrue(*FString::Printf(TEXT("%s ends on an open hand"), Action), Open > 0);
	}
	// And the other way: no frame of a fist take is an open hand.
	FHandClip Fist;
	if (LoadFistClip(*this, TEXT("fist"), EClipVariant::Sloppy, Fist))
	{
		int32 Both = 0;
		for (const FHandFrame& Frame : Fist.GetTracks()[0].Frames)
		{
			Both += FFistDetector::IsFistPose(Frame, true) && FFistDetector::IsOpenHand(Frame) ? 1 : 0;
		}
		bPass &= TestEqual(TEXT("no frame is both a fist and an open hand"), Both, 0);
	}
	else
	{
		bPass = false;
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaFistNotWard, "MageArena.Fist.NeverWard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaFistNotWard::RunTest(const FString& Parameters)
{
	(void)Parameters;
	bool bPass = true;
	// The fist takes are on the ward hand (left, the default), so the ward detector reads every frame of them.
	for (const EClipVariant Variant : {EClipVariant::Normal, EClipVariant::Slow, EClipVariant::Sloppy})
	{
		FHandClip Clip;
		if (!LoadFistClip(*this, TEXT("fist"), Variant, Clip))
		{
			bPass = false;
			continue;
		}
		bPass &= TestTrue(TEXT("the fist is on the ward hand"), Clip.GetHands().Contains(EControllerHand::Left));
		FWardDetector Ward;
		FString Error;
		if (!TestTrue(TEXT("ward rules load"), Ward.Init(Error)))
		{
			AddError(Error);
			return false;
		}
		for (const FHandClipTrack& Track : Clip.GetTracks())
		{
			for (const FHandFrame& Frame : Track.Frames)
			{
				Ward.Ingest(Frame);
			}
		}
		bPass &= TestEqual(*FString::Printf(TEXT("fist.%s raises no ward"), FHandClip::VariantToString(Variant)), Ward.GetRaiseCount(), 0);
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaFistHold, "MageArena.Fist.HoldMatchesStones",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaFistHold::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FString Text;
	TSharedPtr<FJsonObject> Teach;
	double OfferHoldS = -1.0;
	bool bPass = TestTrue(TEXT("teach.json reads"), FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectDir(), TEXT("../data/vr/teach.json")))
		&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Teach) && Teach.IsValid() && Teach->TryGetNumberField(TEXT("offerHoldS"), OfferHoldS));
	bPass &= TestEqual(TEXT("fist hold is teach.json offerHoldS"), FFistThresholds::HoldS, OfferHoldS);
	FHandClip Clip;
	if (!LoadFistClip(*this, TEXT("fist"), EClipVariant::Normal, Clip))
	{
		return false;
	}
	// Closed at T0: no fist on any frame before T0 + 0.40 s, one from the first frame at or after it.
	FFistDetector Detector;
	double ClosedAt = -1.0;
	bool bEarly = false;
	bool bOnTime = false;
	for (const FHandFrame& Frame : Clip.GetTracks()[0].Frames)
	{
		Detector.Ingest(Frame);
		if (ClosedAt < 0.0 && Detector.IsClosed(Frame.Hand))
		{
			ClosedAt = Frame.TimeSeconds;
		}
		if (ClosedAt >= 0.0 && Frame.TimeSeconds - ClosedAt < FFistThresholds::HoldS - 1.0e-6)
		{
			bEarly |= Detector.GetHeldCount() > 0;
		}
		else if (ClosedAt >= 0.0 && !bOnTime)
		{
			bOnTime = Detector.GetHeldCount() == 1;
			bPass &= TestTrue(*FString::Printf(TEXT("held on the first frame at %.4f s after closing at %.4f s"), Frame.TimeSeconds, ClosedAt), bOnTime);
		}
	}
	bPass &= TestTrue(TEXT("the take closes"), ClosedAt >= 0.0);
	bPass &= TestFalse(TEXT("no fist before 0.40 s"), bEarly);
	// A rewind (a new clip) starts the hold over: the same take again is a second fist, not a continuation.
	FeedFist(Detector, Clip);
	bPass &= TestEqual(TEXT("the take played twice is two fists"), Detector.GetHeldCount(), 2);
	// The system gesture takes the hand: nothing held through it.
	FFistDetector Gated;
	int32 Seen = 0;
	for (FHandFrame Frame : Clip.GetTracks()[0].Frames)
	{
		Frame.bSystemGesture = Seen++ % 20 == 19;
		Gated.Ingest(Frame);
	}
	bPass &= TestEqual(TEXT("a system gesture every 20 frames cuts every hold short"), Gated.GetHeldCount(), 0);
	return bPass;
}

#endif
