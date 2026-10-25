#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// T25: the closed fist held 0.40 s, and the missio choice after a rival falls. Literals and their sources:
// - apps/vr/Game/Clips/fist/: fist.{normal,slow,sloppy} and the fist-loose impostor (apps/vr/tools/clipgen/fist.mjs).
// - apps/vr/Game/Clips/stones/: the palm held over each stone; ward-raise and both-palms are open palms too.
// - teach.json offerHoldS 0.40 (the stone hold, the fist hold and the palm hold at missio).
// - The card: no choice in 5 s spares. Brennic is the semifinal rival (Tiro wave n 3), Lio the final's (n 4).
// - Flags: arena.tiro.1.w<n>.finish (true = finish) and arena.missio.total (rivals spared this day).

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
const double kMissioTick = 1.0 / 60.0;
const double kMissioTimeoutS = 5.0;

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

struct FMissioRig
{
	UGameInstance* Instance = nullptr;
	UHandInputSubsystem* Hands = nullptr;
	USigilRecognizerSubsystem* Sigils = nullptr;
	UWardDetectorSubsystem* Wards = nullptr;
	UBlinkDetectorSubsystem* Blinks = nullptr;
	UStaffDetectorSubsystem* Staff = nullptr;
	FString Dir;

	bool Open(FAutomationTestBase& Test)
	{
		Instance = NewObject<UGameInstance>(GetTransientPackage());
		Instance->AddToRoot();
		Hands = NewObject<UHandInputSubsystem>(Instance);
		Sigils = NewObject<USigilRecognizerSubsystem>(Instance);
		Sigils->LoadTemplatesFromDirectory(FPaths::Combine(FPaths::ProjectDir(), TEXT("Clips"), TEXT("templates")));
		Wards = NewObject<UWardDetectorSubsystem>(Instance);
		FString Error;
		if (!Wards->InitDetector(Error))
		{
			Test.AddError(Error);
			return false;
		}
		Blinks = NewObject<UBlinkDetectorSubsystem>(Instance);
		Staff = NewObject<UStaffDetectorSubsystem>(Instance);
		// Never Saved/MageArena: that is the desktop playthrough's save.
		Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArenaTests"), TEXT("missio"), FGuid::NewGuid().ToString());
		IFileManager::Get().MakeDirectory(*Dir, true);
		return true;
	}

	void Close(FArenaSession& Session)
	{
		Session.Unbind();
		IFileManager::Get().DeleteDirectory(*Dir, false, true);
		if (Instance)
		{
			Instance->RemoveFromRoot();
			Instance->MarkAsGarbage();
			Instance = nullptr;
		}
	}
};

double MissioRunUntil(FArenaSession& Session, TFunctionRef<bool()> Done, double LimitS)
{
	double Elapsed = 0.0;
	while (Elapsed < LimitS && !Done())
	{
		Session.Advance(kMissioTick, true);
		Elapsed += kMissioTick;
	}
	return Elapsed;
}

double MissioRunWhile(FArenaSession& Session, const TCHAR* Stage, double LimitS)
{
	return MissioRunUntil(Session, [&Session, Stage]()
	{ return Session.GetStage() != Stage; }, LimitS);
}

// Every opponent down, nothing of theirs in flight, until the bout leaves active (MageArena.Session.DayFlags does the same).
void MissioWinBout(FArenaSession& Session)
{
	for (int32 Step = 0; Step < 120 && Session.GetStage() == TEXT("active"); ++Step)
	{
		FArenaState& State = const_cast<FArenaState&>(Session.GetGames().State);
		for (FActor& Actor : State.Actors)
		{
			if (Actor.Id != Session.GetGames().PlayerId)
			{
				Actor.bDown = true;
			}
		}
		State.Projectiles.Reset();
		State.Telegraphs.Reset();
		Session.Advance(kMissioTick, true);
	}
}

// A saved day at Bout (0-based), through the offer, the ritual's both palms and the intro, into the live bout.
bool MissioReachBout(FAutomationTestBase& Test, FMissioRig& Rig, FArenaSession& Session, int32 Bout)
{
	FFileHelper::SaveStringToFile(FString::Printf(TEXT("bout=%d\n"), Bout), *FPaths::Combine(Rig.Dir, TEXT("bout.txt")));
	Session.Bind(Rig.Hands, Rig.Sigils, Rig.Wards, Rig.Blinks, Rig.Staff);
	Session.SetSaveDirectory(Rig.Dir);
	Session.SetScripted(false);
	bool bPass = Test.TestTrue(TEXT("day begins"), Session.BeginArc(1));
	Rig.Wards->OnWardRaised.Broadcast(0.0, FVector::ForwardVector);
	Session.Advance(kMissioTick, true);
	MissioRunUntil(Session, [&Session]()
	{ return Session.GetPhaseClock() + 1.0e-9 >= Session.GetDayTuning().RitualLineS; }, 5.0);
	Rig.Hands->PlayQuickAction(TEXT("both-palms"), EClipVariant::Normal);
	MissioRunWhile(Session, TEXT("ritual"), 5.0);
	MissioRunWhile(Session, TEXT("intro"), 15.0);
	bPass &= Test.TestEqual(TEXT("the bout is live"), Session.GetStage(), FString(TEXT("active")));
	bPass &= Test.TestEqual(TEXT("the saved bout"), Session.GetGames().Wave, Bout);
	return bPass;
}

bool HasFinish(const FArenaSession& Session, int32 WaveN, bool& bOut)
{
	return Session.GetFlags().GetBool(FArenaFlags::TiroKey(WaveN, TEXT("finish")), bOut);
}

double MissioTotal(const FArenaSession& Session)
{
	double Total = -1.0;
	Session.GetFlags().GetNumber(TEXT("arena.missio.total"), Total);
	return Total;
}

int32 CountChain(const FArenaSession& Session, const TCHAR* Prefix)
{
	int32 Count = 0;
	for (const FString& Line : Session.GetChain())
	{
		Count += Line.StartsWith(Prefix) ? 1 : 0;
	}
	return Count;
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
	const FString TeachPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("../data/vr/teach.json"));
	const bool bRead = FFileHelper::LoadFileToString(Text, *TeachPath);
	const bool bJson = bRead && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Teach) && Teach.IsValid();
	bool bPass = TestTrue(TEXT("teach.json reads"), bJson && Teach->TryGetNumberField(TEXT("offerHoldS"), OfferHoldS));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaFistMissioSpare, "MageArena.Fist.MissioSpare",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaFistMissioSpare::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMissioRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	FArenaSession Session;
	bool bPass = MissioReachBout(*this, Rig, Session, 2);
	MissioWinBout(Session);
	bPass &= TestEqual(TEXT("Brennic falls: missio"), Session.GetStage(), FString(TEXT("missio")));
	bool bWon = false;
	bPass &= TestTrue(TEXT("the bout is recorded won first"), Session.GetFlags().GetBool(TEXT("arena.tiro.1.w3.won"), bWon) && bWon);
	bool bFinish = true;
	bPass &= TestFalse(TEXT("no choice yet"), HasFinish(Session, 3, bFinish));
	// The open palm toward him, held: the ward hold rises from the lap and stays up.
	Rig.Hands->SetWardHeld(true, EClipVariant::Normal);
	const double Took = MissioRunWhile(Session, TEXT("missio"), kMissioTimeoutS + 1.0);
	bPass &= TestEqual(TEXT("the spare leads to the intermission"), Session.GetStage(), FString(TEXT("intermission")));
	bPass &= TestTrue(*FString::Printf(TEXT("the palm chose in %.2f s, before the timeout"), Took), Took < 2.0);
	bPass &= TestTrue(TEXT("w3.finish is false"), HasFinish(Session, 3, bFinish) && !bFinish);
	bPass &= TestEqual(TEXT("arena.missio.total counts the spare"), MissioTotal(Session), 1.0);
	bPass &= TestEqual(TEXT("chain: spared by the palm"), CountChain(Session, TEXT("day missio bout=3 spare palm")), 1);
	FArenaFlags Saved;
	bPass &= TestTrue(TEXT("flags file loads"), Saved.Load(Session.FlagsFilePath()));
	bPass &= TestTrue(TEXT("the file holds w3.finish"), Saved.GetBool(TEXT("arena.tiro.1.w3.finish"), bFinish) && !bFinish);
	Rig.Hands->SetWardHeld(false, EClipVariant::Normal);
	Rig.Close(Session);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaFistMissioFinish, "MageArena.Fist.MissioFinish",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaFistMissioFinish::RunTest(const FString& Parameters)
{
	(void)Parameters;
	bool bPass = true;
	// The semifinal (Brennic, then the intermission) and the final (Lio, then the aftermath), each with the fist.
	struct FCase
	{
		int32 Bout;
		const TCHAR* Next;
	};
	for (const FCase& Case : {FCase{2, TEXT("intermission")}, FCase{3, TEXT("aftermath")}})
	{
		const int32 WaveN = Case.Bout + 1;
		const FString Label = FString::Printf(TEXT("bout %d"), WaveN);
		FMissioRig Rig;
		if (!Rig.Open(*this))
		{
			return false;
		}
		FArenaSession Session;
		bPass &= MissioReachBout(*this, Rig, Session, Case.Bout);
		MissioWinBout(Session);
		bPass &= TestEqual(*(Label + TEXT(" missio")), Session.GetStage(), FString(TEXT("missio")));
		Rig.Hands->PlayQuickAction(TEXT("fist"), EClipVariant::Normal);
		const double Took = MissioRunWhile(Session, TEXT("missio"), kMissioTimeoutS + 1.0);
		bPass &= TestEqual(*(Label + TEXT(" the finish leads on")), Session.GetStage(), FString(Case.Next));
		bPass &= TestTrue(*FString::Printf(TEXT("%s the fist chose in %.2f s, before the timeout"), *Label, Took), Took < 2.0);
		bool bFinish = false;
		bPass &= TestTrue(*(Label + TEXT(" finish is true")), HasFinish(Session, WaveN, bFinish) && bFinish);
		bPass &= TestEqual(*(Label + TEXT(" a finish spares no one")), MissioTotal(Session), 0.0);
		bPass &= TestEqual(*(Label + TEXT(" chain: finished by the fist")),
		    CountChain(Session, *FString::Printf(TEXT("day missio bout=%d finish fist"), WaveN)), 1);
		if (Case.Bout == 3)
		{
			bPass &= TestTrue(TEXT("the final is still won"), Session.WasFinalWon());
			FString Save;
			FFileHelper::LoadFileToString(Save, *FPaths::Combine(Rig.Dir, TEXT("bout.txt")));
			bPass &= TestEqual(TEXT("the day over saves a new day"), Save, FString(TEXT("bout=0\n")));
		}
		Rig.Close(Session);
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaFistMissioTimeout, "MageArena.Fist.MissioTimeout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaFistMissioTimeout::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMissioRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	FArenaSession Session;
	bool bPass = MissioReachBout(*this, Rig, Session, 2);
	MissioWinBout(Session);
	bPass &= TestEqual(TEXT("missio"), Session.GetStage(), FString(TEXT("missio")));
	// The impostor, then nothing: the half-curl does not finish, and the default spares at 5 s.
	Rig.Hands->PlayQuickAction(TEXT("fist-loose"), EClipVariant::Normal);
	const double Took = MissioRunWhile(Session, TEXT("missio"), kMissioTimeoutS + 1.0);
	bPass &= TestEqual(TEXT("the timeout leads to the intermission"), Session.GetStage(), FString(TEXT("intermission")));
	bPass &= TestTrue(*FString::Printf(TEXT("chosen at %.3f s, the 5 s timeout"), Took), FMath::Abs(Took - kMissioTimeoutS) <= 2.0 * kMissioTick);
	bool bFinish = true;
	bPass &= TestTrue(TEXT("w3.finish is false"), HasFinish(Session, 3, bFinish) && !bFinish);
	bPass &= TestEqual(TEXT("arena.missio.total counts the spare"), MissioTotal(Session), 1.0);
	bPass &= TestEqual(TEXT("chain: spared by the timeout"), CountChain(Session, TEXT("day missio bout=3 spare timeout")), 1);
	Rig.Close(Session);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaFistNoFlagMidBout, "MageArena.Fist.NoChoiceMidBout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaFistNoFlagMidBout::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMissioRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	FArenaSession Session;
	bool bPass = MissioReachBout(*this, Rig, Session, 2);
	// Mid-bout against Brennic: a fist held, then an open palm held, change nothing but the fight.
	bool bSawMissio = false;
	Rig.Hands->PlayQuickAction(TEXT("fist"), EClipVariant::Slow);
	MissioRunUntil(Session, [&Session, &bSawMissio]()
	{
		bSawMissio |= Session.GetStage() == TEXT("missio");
		return Session.GetStage() != TEXT("active");
	}, 2.0);
	Rig.Hands->SetWardHeld(true, EClipVariant::Normal);
	MissioRunUntil(Session, [&Session, &bSawMissio]()
	{
		bSawMissio |= Session.GetStage() == TEXT("missio");
		return Session.GetStage() != TEXT("active");
	}, 1.0);
	Rig.Hands->SetWardHeld(false, EClipVariant::Normal);
	bPass &= TestEqual(TEXT("still fighting"), Session.GetStage(), FString(TEXT("active")));
	bPass &= TestFalse(TEXT("no missio during the bout"), bSawMissio);
	bPass &= TestFalse(TEXT("no w3.finish mid-bout"), Session.GetFlags().Has(TEXT("arena.tiro.1.w3.finish")));
	bPass &= TestFalse(TEXT("no arena.missio.total mid-bout"), Session.GetFlags().Has(TEXT("arena.missio.total")));
	bPass &= TestEqual(TEXT("no missio line in the chain"), CountChain(Session, TEXT("day missio")), 0);
	// The fist held before the rival fell does not carry into missio: it needs a fresh 0.40 s there.
	Rig.Hands->PlayQuickAction(TEXT("fist"), EClipVariant::Slow);
	MissioRunUntil(Session, [&Session]()
	{ return Session.GetStage() != TEXT("active"); }, 0.8);
	MissioWinBout(Session);
	bPass &= TestEqual(TEXT("missio after the fall"), Session.GetStage(), FString(TEXT("missio")));
	MissioRunUntil(Session, [&Session]()
	{ return Session.GetStage() != TEXT("missio"); }, 1.0);
	bPass &= TestEqual(TEXT("the bout's fist chose nothing in missio's first second"), Session.GetStage(), FString(TEXT("missio")));
	bPass &= TestFalse(TEXT("no finish from the bout's fist"), Session.GetFlags().Has(TEXT("arena.tiro.1.w3.finish")));
	// Bouts without a rival (Bout 1, the soldiers) go straight to the intermission, with no missio and no flag.
	FMissioRig Plain;
	if (!Plain.Open(*this))
	{
		Rig.Close(Session);
		return false;
	}
	FArenaSession First;
	bPass &= MissioReachBout(*this, Plain, First, 0);
	MissioWinBout(First);
	bPass &= TestEqual(TEXT("no rival, no missio"), First.GetStage(), FString(TEXT("intermission")));
	bPass &= TestFalse(TEXT("no w1.finish"), First.GetFlags().Has(TEXT("arena.tiro.1.w1.finish")));
	Plain.Close(First);
	Rig.Close(Session);
	return bPass;
}

#endif
