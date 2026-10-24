#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Gestures/BlinkDetector.h"
#include "Gestures/SigilRecognizerSubsystem.h"
#include "Gestures/StaffDetector.h"
#include "Gestures/WardDetector.h"
#include "HAL/FileManager.h"
#include "Hands/HandInputSubsystem.h"
#include "Kernel/ArenaKernel.h"
#include "Kernel/SimTypes.h"
#include "Kernel/VrRules.h"
#include "MageArenaVR.h"
#include "Misc/Paths.h"
#include "Session/ArenaSession.h"

// T47: the outcome-independent invariants of the five MageArenaDesign scenarios. Each test opens the same session as
// its design twin in SessionTests.cpp (same bout, SetScripted(true), same overlay) and plays the reference script for a
// fixed window of active simulated time. Every clip goes through FArenaSession::PlayAction ->
// UHandInputSubsystem::PlayQuickAction, the one gesture pipeline; nothing here broadcasts a detector delegate. The
// assertions are what must hold whoever wins: a played sigil reaches a kernel cast, a blink a kernel roll, a ward an
// absorb, the seat stays on a pad, the rival is bound, and the run ends. Winning, bands and win rates stay in the twins.
namespace
{
// The active-time windows the gesture invariants are judged over, chosen from the recorded chains (T47 report): the
// first ward, cast and blink land inside 3 s in Wave 1 (sigil + kernel cast tier=1, roll at 2.35 s), the day's bout 1
// (the same opening) and both duels (roll at 0.60 s, bolts from the first second); the shortest bout ends at 23.12 s
// (Lio). 10 s holds them with margin and stays inside every bout, won or lost.
constexpr double GestureWindowS = 10.0;
// Bout 2 (creatures) plays its first blink later: kernel roll at 18.08 s, the bout ends at 31.02 s. 25 s holds it.
constexpr double CreaturesWindowS = 25.0;
constexpr double InvariantDt = 1.0 / 72.0;

struct FInvariantRig
{
	UGameInstance* Instance = nullptr;
	UHandInputSubsystem* Hands = nullptr;
	USigilRecognizerSubsystem* Sigils = nullptr;
	UWardDetectorSubsystem* Wards = nullptr;
	UBlinkDetectorSubsystem* Blinks = nullptr;
	UStaffDetectorSubsystem* Staff = nullptr;

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
		return true;
	}

	void Bind(FArenaSession& Session) const
	{
		Session.Bind(Hands, Sigils, Wards, Blinks, Staff);
		Session.SetScripted(true);
	}

	void Close()
	{
		if (Instance)
		{
			Instance->RemoveFromRoot();
			Instance->MarkAsGarbage();
		}
	}
};

// What the window saw, counted from the session's chain and the kernel's player actor.
struct FGestureTally
{
	double ActiveS = 0.0;
	double WorstOffPad = 0.0;
	int32 Sigils = 0;
	int32 SigilCasts = 0;
	int32 Bolts = 0;
	int32 BoltCasts = 0;
	int32 Blinks = 0;
	int32 BlinkRolls = 0;
	int32 Wards = 0;
	int32 KernelWardRaises = 0;
	int32 WardAbsorbs = 0;
};

double OffPad(const FArenaSession& Session)
{
	const FActor* Player = SimFindActor(Session.GetGames().State, Session.GetGames().PlayerId);
	double Best = 1.0e9;
	for (int32 Pad = 0; Player && Pad < 3; ++Pad)
	{
		Best = FMath::Min(Best, SimDistance(Player->Pos, Session.PadKernel(Pad)));
	}
	return Best;
}

// One tick of the window. The ward's kernel side is FActor::bAbsorb, which only the session's gesture ward input sets:
// a raise is a tick where it goes up. Absorbed hits (Perfects or Blocks rising while it was up) are counted and logged,
// not asserted: the recorded chains show 0 perfects in every scenario and a single ward at the bout's start.
void TickWindow(FArenaSession& Session, FGestureTally& Tally)
{
	const FActor* Before = SimFindActor(Session.GetGames().State, Session.GetGames().PlayerId);
	const bool bWardUp = Before && Before->bAbsorb;
	const int32 AbsorbedBefore = Before ? Before->Metrics.Perfects + Before->Metrics.Blocks : 0;
	Session.Advance(InvariantDt, true);
	Tally.ActiveS += InvariantDt;
	Tally.WorstOffPad = FMath::Max(Tally.WorstOffPad, OffPad(Session));
	const FActor* After = SimFindActor(Session.GetGames().State, Session.GetGames().PlayerId);
	Tally.KernelWardRaises += !bWardUp && After && After->bAbsorb ? 1 : 0;
	Tally.WardAbsorbs += bWardUp && After && After->Metrics.Perfects + After->Metrics.Blocks > AbsorbedBefore ? 1 : 0;
}

// Pairs each gesture with the kernel line that answers it, over the chain lines the window wrote.
void TallyChain(const TArray<FString>& Chain, int32 From, int32 To, FGestureTally& Tally)
{
	for (int32 Index = From; Index < To; ++Index)
	{
		const FString& Line = Chain[Index];
		const bool bSigil = Line.StartsWith(TEXT("gesture sigil"));
		const bool bBolt = Line.StartsWith(TEXT("gesture bolt -> slot"));
		const bool bBlink = Line.StartsWith(TEXT("gesture blink"));
		Tally.Wards += Line.StartsWith(TEXT("gesture ward onset=")) && Line.Contains(TEXT("-> absorb")) ? 1 : 0;
		if (!bSigil && !bBolt && !bBlink)
		{
			continue;
		}
		Tally.Sigils += bSigil ? 1 : 0;
		Tally.Bolts += bBolt ? 1 : 0;
		Tally.Blinks += bBlink ? 1 : 0;
		// A sigil or a bolt is answered by a kernel cast before the casting hand makes its next cast gesture; a blink by
		// a kernel roll before the next blink.
		const bool bCast = bSigil || bBolt;
		for (int32 Next = Index + 1; Next < To; ++Next)
		{
			const FString& Later = Chain[Next];
			if (bCast && Later.StartsWith(TEXT("kernel cast actor=")))
			{
				Tally.SigilCasts += bSigil ? 1 : 0;
				Tally.BoltCasts += bBolt ? 1 : 0;
				break;
			}
			if (bCast && (Later.StartsWith(TEXT("gesture sigil")) || Later.StartsWith(TEXT("gesture bolt"))))
			{
				break;
			}
			if (bBlink && Later.StartsWith(TEXT("kernel roll actor=")))
			{
				++Tally.BlinkRolls;
				break;
			}
			if (bBlink && Later.StartsWith(TEXT("gesture blink")))
			{
				break;
			}
		}
	}
}

// Plays the active part of a started bout for the fixed window, or until the bout ends.
FGestureTally PlayWindow(FArenaSession& Session, double WindowS = GestureWindowS)
{
	FGestureTally Tally;
	const int32 From = Session.GetChain().Num();
	while (Session.GetGames().Phase == TEXT("active") && Tally.ActiveS < WindowS)
	{
		TickWindow(Session, Tally);
	}
	TallyChain(Session.GetChain(), From, Session.GetChain().Num(), Tally);
	return Tally;
}

// bSigilScript: the reference script draws a sigil in this scenario's window. It does in the Wave 1 and day openings;
// in the Fire and Air duels it casts with bolts only (0 gesture sigil lines in runs/T19 and runs/T23), so there the cast
// invariant is the bolt's. Teaching the script to draw there would change the measuring stick (DF-011 C, not adopted).
bool AssertGestures(FAutomationTestBase& Test, const TCHAR* Scenario, const FGestureTally& Tally, bool bSigilScript)
{
	UE_LOG(LogMageArena, Log, TEXT("Invariants %s window=%.2fs sigils=%d->casts=%d bolts=%d->casts=%d blinks=%d->rolls=%d wards=%d->kernelRaises=%d absorbedHits=%d offPad=%.4f"),
	    Scenario, Tally.ActiveS, Tally.Sigils, Tally.SigilCasts, Tally.Bolts, Tally.BoltCasts, Tally.Blinks, Tally.BlinkRolls, Tally.Wards,
	    Tally.KernelWardRaises, Tally.WardAbsorbs, Tally.WorstOffPad);
	bool bPass = true;
	if (bSigilScript)
	{
		bPass &= Test.TestTrue(*FString::Printf(TEXT("%s: a played sigil clip -> gesture sigil -> kernel cast (%d of %d)"), Scenario,
		                           Tally.SigilCasts, Tally.Sigils),
		    Tally.SigilCasts > 0);
	}
	bPass &= Test.TestTrue(*FString::Printf(TEXT("%s: a played bolt clip -> gesture bolt -> kernel cast (%d of %d)"), Scenario,
	                           Tally.BoltCasts, Tally.Bolts),
	    Tally.BoltCasts > 0);
	bPass &= Test.TestTrue(*FString::Printf(TEXT("%s: a played blink clip -> gesture blink -> kernel roll actor= (%d of %d)"), Scenario,
	                           Tally.BlinkRolls, Tally.Blinks),
	    Tally.BlinkRolls > 0);
	bPass &= Test.TestTrue(*FString::Printf(TEXT("%s: a played ward clip -> gesture ward -> absorb, the kernel ward up (%d raises, %d wards)"),
	                           Scenario, Tally.KernelWardRaises, Tally.Wards),
	    Tally.Wards > 0 && Tally.KernelWardRaises > 0);
	bPass &= Test.TestTrue(*FString::Printf(TEXT("%s: the player stayed on a pad (worst %.4f m)"), Scenario, Tally.WorstOffPad),
	    Tally.WorstOffPad < 1.0e-3);
	return bPass;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaInvariantsWave1, "MageArena.Invariants.Wave1Seated",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaInvariantsWave1::RunTest(const FString& Parameters)
{
	FInvariantRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	FArenaSession Session;
	Rig.Bind(Session);
	bool bPass = TestTrue(TEXT("Session.Start"), Session.Start(1));
	if (bPass)
	{
		bPass &= AssertGestures(*this, TEXT("Wave1Seated"), PlayWindow(Session), true);
	}
	Session.Unbind();
	Rig.Close();
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaInvariantsCreatures, "MageArena.Invariants.CreaturesSeated",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaInvariantsCreatures::RunTest(const FString& Parameters)
{
	FInvariantRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	FArenaSession Session;
	Rig.Bind(Session);
	Session.SetBoutIndex(1);
	bool bPass = TestTrue(TEXT("Session.Start"), Session.Start(1));
	if (bPass)
	{
		bPass &= AssertGestures(*this, TEXT("CreaturesSeated"), PlayWindow(Session, CreaturesWindowS), true);
	}
	Session.Unbind();
	Rig.Close();
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaInvariantsFire, "MageArena.Invariants.FireSeated",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaInvariantsFire::RunTest(const FString& Parameters)
{
	FInvariantRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	FVrRuleset Loaded;
	FString RulesError;
	if (!LoadVrRuleset(Loaded, RulesError))
	{
		AddError(RulesError);
		Rig.Close();
		return false;
	}
	bool bPass = TestTrue(TEXT("combat.vr.json names a rival"), Loaded.Rivals.Num() > 0);
	// The design twin's brennic-c1 run: Brennic, the Tiro semifinal (wave index 2), competence 1, seed 1.
	FArenaSession Session;
	Rig.Bind(Session);
	Session.SetBout(2, true);
	FVrRuleset Rules = Loaded;
	Rules.MageCompetenceOverride = 1.0;
	Session.SetRulesOverride(Rules);
	bPass &= TestTrue(TEXT("brennic-c1 start"), Session.Start(1));
	if (bPass)
	{
		const FVrRuleset* Live = Session.GetVrRules();
		bPass &= TestTrue(TEXT("Brennic bound"), Live && Live->BoundRival() != nullptr);
		bPass &= AssertGestures(*this, TEXT("FireSeated brennic-c1"), PlayWindow(Session), false);
	}
	Session.Unbind();
	Rig.Close();
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaInvariantsAir, "MageArena.Invariants.AirSeated",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaInvariantsAir::RunTest(const FString& Parameters)
{
	FInvariantRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	FVrRuleset Loaded;
	FString RulesError;
	if (!LoadVrRuleset(Loaded, RulesError))
	{
		AddError(RulesError);
		Rig.Close();
		return false;
	}
	// The design twin's lio-c15 run: Lio in the Tiro final (wave index 3), competence 1.5, seed 1.
	FArenaSession Session;
	Rig.Bind(Session);
	Session.SetBout(3, true);
	Session.SetAirFinal(true);
	FVrRuleset Rules = Loaded;
	Rules.MageCompetenceOverride = 1.5;
	Session.SetRulesOverride(Rules);
	bool bPass = TestTrue(TEXT("lio-c15 start"), Session.Start(1));
	if (bPass)
	{
		const FVrRuleset* Live = Session.GetVrRules();
		bPass &= TestTrue(TEXT("Lio bound in the final"), Live && Live->BoundRival() != nullptr && Live->BoundRival()->Id == TEXT("lio"));
		const FActor* Lio = Live ? SimFindActor(Session.GetGames().State, Live->RivalRuntime.ActorId) : nullptr;
		bPass &= TestTrue(TEXT("Lio is the air mage"), Lio && Lio->Air.bSchool);
		bPass &= AssertGestures(*this, TEXT("AirSeated lio-c15"), PlayWindow(Session), false);
	}
	Session.Unbind();
	Rig.Close();
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaInvariantsFull, "MageArena.Invariants.FullSeated",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaInvariantsFull::RunTest(const FString& Parameters)
{
	// The design twin's Tiro day: ritual, the bouts with their intros and intermissions, the aftermath. The gesture
	// invariants are judged over the first GestureWindowS of active (bout) time; the day itself must end, by closing or
	// by the script losing a bout (the twin's stop rule), inside the twin's guard.
	FInvariantRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	FArenaSession Session;
	Rig.Bind(Session);
	const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArenaTests"), TEXT("session-save"), FGuid::NewGuid().ToString());
	Session.SetSaveDirectory(Dir);
	bool bPass = TestTrue(TEXT("BeginArc"), Session.BeginArc(1));
	if (bPass)
	{
		Session.SkipTeach();
		const int32 GuardLimit = 72 * 1800;
		int32 Guard = 0;
		bool bStopped = false;
		FGestureTally Tally;
		int32 From = -1;
		int32 To = -1;
		FString LastStage;
		while (Session.GetStage() != TEXT("closed") && Guard < GuardLimit)
		{
			const FString Stage = Session.GetStage();
			if (Stage == TEXT("lost") && Session.GetGames().Wave < 3)
			{
				bStopped = true;
				break;
			}
			if (Stage == TEXT("active") && Tally.ActiveS < GestureWindowS)
			{
				From = From < 0 ? Session.GetChain().Num() : From;
				TickWindow(Session, Tally);
				To = Session.GetChain().Num();
			}
			else
			{
				if (Stage != LastStage && Stage == TEXT("intermission"))
				{
					// The twin's way through an intermission. It is outside the gesture window, which ends first.
					Rig.Wards->OnWardRaised.Broadcast(0.0, FVector::ForwardVector);
				}
				Session.Advance(InvariantDt, true);
			}
			LastStage = Stage;
			++Guard;
		}
		bPass &= TestTrue(*FString::Printf(TEXT("the day ended (closed or a lost bout) inside the guard (%d ticks, stage %s, stopped %d)"),
		                      Guard, *Session.GetStage(), bStopped ? 1 : 0),
		    Guard < GuardLimit);
		bPass &= TestTrue(TEXT("the gesture window was filled before the day ended"), Tally.ActiveS >= GestureWindowS);
		TallyChain(Session.GetChain(), FMath::Max(0, From), FMath::Max(0, To), Tally);
		bPass &= AssertGestures(*this, TEXT("FullSeated"), Tally, true);
	}
	IFileManager::Get().DeleteDirectory(*Dir, false, true);
	Session.Unbind();
	Rig.Close();
	return bPass;
}

#endif
