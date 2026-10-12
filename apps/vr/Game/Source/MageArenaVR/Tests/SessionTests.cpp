#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Gestures/BlinkDetector.h"
#include "Gestures/SigilRecognizerSubsystem.h"
#include "Gestures/StaffDetector.h"
#include "Gestures/WardDetector.h"
#include "Hands/HandInputSubsystem.h"
#include "Kernel/ArenaKernel.h"
#include "Kernel/KernelData.h"
#include "Kernel/SimTypes.h"
#include "Kernel/VrRules.h"
#include "HAL/FileManager.h"
#include "MageArenaVR.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Session/ArenaSession.h"

namespace
{
struct FSessionRig
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

	void Close(FArenaSession& Session)
	{
		Session.Unbind();
		if (Instance)
		{
			Instance->RemoveFromRoot();
			Instance->MarkAsGarbage();
		}
	}
};

FString ChainPath(const TCHAR* Name)
{
	// T12 is this measurement. T11 keeps the split-hand record and is not overwritten.
	FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../../.."), TEXT("runs/T12"), Name);
	FPaths::CollapseRelativeDirectories(Path);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	return Path;
}

// A bout save in a directory of its own. The default one, Saved/MageArena/bout.txt, is the file a desktop
// playthrough resumes from, so a test that advances an intermission must never write there.
FString ScratchSaveDir()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArenaTests"), TEXT("session-save"), FGuid::NewGuid().ToString());
}

int32 CountChain(const TArray<FString>& Chain, const TCHAR* Piece)
{
	int32 Count = 0;
	for (const FString& Line : Chain)
	{
		if (Line.Contains(Piece))
		{
			++Count;
		}
	}
	return Count;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaWave1Scripted, "MageArenaDesign.Session.Wave1Seated",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaWave1Scripted::RunTest(const FString& Parameters)
{
	FSessionRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	FArenaSession Session;
	Session.Bind(Rig.Hands, Rig.Sigils, Rig.Wards, Rig.Blinks, Rig.Staff);
	Session.SetScripted(true);
	if (!Session.Start(1))
	{
		AddError(TEXT("Session.Start failed"));
		Rig.Close(Session);
		return false;
	}

	// arena-tiers.json pins Tiro wave 1 at 25-40 s for the walking desktop game.
	// The seated bout does not walk. Victory has to land inside this cap. The window is logged.
	const double PinnedWindowLo = 25.0;
	const double PinnedWindowHi = 40.0;
	const double GenerousCap = 120.0;
	const double Dt = 1.0 / 72.0;
	int32 Guard = 0;
	double Mark = 5.0;
	double WorstOffPad = 0.0;
	while (Session.GetGames().Phase == TEXT("active") && Session.GetSimSeconds() < GenerousCap && Guard < 10000)
	{
		Session.Advance(Dt, true);
		++Guard;
		const FActor* Marked = SimFindActor(Session.GetGames().State, Session.GetGames().PlayerId);
		double OffPad = 1.0e9;
		if (Marked)
		{
			for (int32 Pad = 0; Pad < 3; ++Pad)
			{
				OffPad = FMath::Min(OffPad, SimDistance(Marked->Pos, Session.PadKernel(Pad)));
			}
		}
		WorstOffPad = FMath::Max(WorstOffPad, OffPad);
		if (Session.GetSimSeconds() >= Mark)
		{
			UE_LOG(LogMageArena, Log, TEXT("Wave1Scripted t=%.1f hp=%.1f mana=%.1f stam=%.1f casts=%d rolls=%d offPad=%.4f"),
				Session.GetSimSeconds(),
				Marked ? Marked->Hp : -1.0,
				Marked ? Marked->Mana : -1.0,
				Marked ? Marked->Stamina : -1.0,
				Marked ? Marked->Metrics.Casts : -1,
				Marked ? Marked->Metrics.Rolls : -1,
				OffPad);
			Mark += 5.0;
		}
	}

	const FString ChainText = FString::Join(Session.GetChain(), TEXT("\n"));
	FFileHelper::SaveStringToFile(ChainText, *ChainPath(TEXT("wave1-chain.log")));
	const FActor* Player = SimFindActor(Session.GetGames().State, Session.GetGames().PlayerId);
	const double Measured = Session.GetSimSeconds();
	const bool bWon = Session.GetGames().Phase == TEXT("intermission");
	const bool bInsideWindow = bWon && Measured >= PinnedWindowLo && Measured <= PinnedWindowHi;
	const double DamageTaken = Player ? Player->Metrics.DamageTaken : -1.0;
	UE_LOG(LogMageArena, Log, TEXT("Wave1Scripted end phase=%s t=%.2f waves=%d hp=%.1f dealt=%.1f taken=%.1f blinks=%d perfects=%d chain=%d"),
		*Session.GetGames().Phase, Measured, Session.GetGames().WavesCleared,
		Player ? Player->Hp : -1.0,
		Player ? Player->Metrics.DamageDealt : -1.0,
		DamageTaken,
		Session.GetBlinkAccepts(),
		Player ? Player->Metrics.Perfects : -1,
		Session.GetChain().Num());
	UE_LOG(LogMageArena, Log, TEXT("Wave1Scripted seated measured=%.2fs pinned-window=%.0f-%.0f %s damageTaken=%.1f offPad=%.4f"),
		Measured, PinnedWindowLo, PinnedWindowHi,
		bInsideWindow ? TEXT("INSIDE") : TEXT("OUTSIDE"),
		DamageTaken, WorstOffPad);
	UE_LOG(LogMageArena, Log, TEXT("Wave1Scripted defence splitCasts=%d staff=%d"), Session.GetSplitCasts(), Session.GetStaffPlants());

	bool bPass = true;
	bPass &= TestEqual(TEXT("phase"), Session.GetGames().Phase, FString(TEXT("intermission")));
	bPass &= TestTrue(TEXT("waves cleared"), Session.GetGames().WavesCleared >= 1);
	bPass &= TestTrue(*FString::Printf(TEXT("victory within %.0fs (measured %.2fs, phase %s)"),
		GenerousCap, Measured, *Session.GetGames().Phase), bWon && Measured <= GenerousCap + 0.05);
	bPass &= TestTrue(*FString::Printf(TEXT("player stayed on a pad (worst %.4f m)"), WorstOffPad), WorstOffPad < 1.0e-3);
	bPass &= TestTrue(TEXT("blinked"), Session.GetBlinkAccepts() > 0);
	bPass &= TestTrue(TEXT("ward gesture"), CountChain(Session.GetChain(), TEXT("gesture ward")) > 0);
	bPass &= TestTrue(TEXT("sigil gesture"), CountChain(Session.GetChain(), TEXT("gesture sigil")) > 0);
	bPass &= TestTrue(TEXT("kernel roll"), CountChain(Session.GetChain(), TEXT("kernel roll actor=")) > 0);
	bPass &= TestTrue(TEXT("kernel cast"), CountChain(Session.GetChain(), TEXT("kernel cast")) > 0);
	bPass &= TestTrue(TEXT("victory line"), CountChain(Session.GetChain(), TEXT("Session victory")) == 1);

	Rig.Close(Session);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCreaturesSeated, "MageArenaDesign.Session.CreaturesSeated",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCreaturesSeated::RunTest(const FString& Parameters)
{
	FSessionRig Rig;
	if (!Rig.Open(*this)) return false;
	FArenaSession Session;
	Session.Bind(Rig.Hands, Rig.Sigils, Rig.Wards, Rig.Blinks, Rig.Staff);
	Session.SetScripted(true);
	Session.SetBoutIndex(1); // Bout 2 (creatures)
	if (!Session.Start(1))
	{
		AddError(TEXT("Session.Start failed"));
		Rig.Close(Session);
		return false;
	}

	// combat.json pacingTargets.creatureWaveS = [30, 45] (pinned). Do not move the window to make a bout pass.
	const double PinnedWindowLo = 30.0;
	const double PinnedWindowHi = 45.0;
	const double GenerousCap = 120.0;
	const double Dt = 1.0 / 72.0;
	int32 Guard = 0;
	while (Session.GetGames().Phase == TEXT("active") && Session.GetSimSeconds() < GenerousCap && Guard < 10000)
	{
		Session.Advance(Dt, true);
		++Guard;
	}

	const FActor* Player = SimFindActor(Session.GetGames().State, Session.GetGames().PlayerId);
	const double Measured = Session.GetSimSeconds();
	const bool bWon = Session.GetGames().Phase == TEXT("intermission") || Session.GetGames().Phase == TEXT("complete");
	const bool bInsideWindow = bWon && Measured >= PinnedWindowLo && Measured <= PinnedWindowHi;
	const double DamageTaken = Player ? Player->Metrics.DamageTaken : -1.0;
	
	const int32 Perfects = Player ? Player->Metrics.Perfects : -1;
	UE_LOG(LogMageArena, Log, TEXT("CreaturesSeated end phase=%s t=%.2f hp=%.1f dealt=%.1f taken=%.1f blinks=%d perfects=%d"),
		*Session.GetGames().Phase, Measured,
		Player ? Player->Hp : -1.0,
		Player ? Player->Metrics.DamageDealt : -1.0,
		DamageTaken,
		Session.GetBlinkAccepts(),
		Perfects);
	// T22, measured, not asserted: the bout's collar tally.
	UE_LOG(LogMageArena, Log, TEXT("CreaturesSeated collar %s cracks=%d tithe=%d %s"),
		FParse::Param(FCommandLine::Get(), TEXT("MageArenaProposal")) ? TEXT("proposal") : TEXT("live"),
		Session.GetCollar().GetBoutCracks(), Session.GetCollar().GetBoutTithe(), *Session.GetCollar().Breakdown());
	UE_LOG(LogMageArena, Log, TEXT("CreaturesSeated seated measured=%.2fs pinned-window=%.0f-%.0f %s damageTaken=%.1f"),
		Measured, PinnedWindowLo, PinnedWindowHi,
		bInsideWindow ? TEXT("INSIDE") : TEXT("OUTSIDE"),
		DamageTaken);

	bool bPass = true;
	bPass &= TestTrue(TEXT("won bout 2"), bWon);
	bPass &= TestTrue(*FString::Printf(TEXT("victory inside %.0f-%.0fs window (measured %.2fs, phase %s)"),
		PinnedWindowLo, PinnedWindowHi, Measured, *Session.GetGames().Phase), bInsideWindow);

	Rig.Close(Session);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaFireSeated, "MageArenaDesign.Duel.FireSeated",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaFireSeated::RunTest(const FString& Parameters)
{
	FSessionRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	const double Dt = 1.0 / 72.0;
	// DECISIONS 2026-10-07 (DF-003 answered): median 45-80 s, the signature in every duel that reaches the last phase,
	// and the player wins at competence 1. T19: both runs are Brennic, the Tiro semifinal Fire mage (wave index 2), with
	// his HP-gated phases; competence 1.5 is the same duel with the opponent's brain at 1.5.
	const double Lo = 45.0;
	const double Hi = 80.0;
	bool bPass = true;
	FVrRuleset Loaded;
	FString RulesError;
	if (!LoadVrRuleset(Loaded, RulesError))
	{
		AddError(RulesError);
		FArenaSession Dummy;
		Rig.Close(Dummy);
		return false;
	}
	bPass &= TestTrue(TEXT("combat.vr.json names a rival"), Loaded.Rivals.Num() > 0);
	auto RunOne = [&](double Competence, uint32 Seed, const TCHAR* Label, bool bRequireWin) -> bool
	{
		FArenaSession Session;
		Session.Bind(Rig.Hands, Rig.Sigils, Rig.Wards, Rig.Blinks, Rig.Staff);
		Session.SetScripted(true);
		Session.SetBout(2, true);
		FVrRuleset Rules = Loaded;
		Rules.MageCompetenceOverride = Competence;
		Session.SetRulesOverride(Rules);
		if (!Session.Start(Seed))
		{
			AddError(FString::Printf(TEXT("%s start failed"), Label));
			Session.Unbind();
			return false;
		}
		int32 Guard = 0;
		double WorstOffPad = 0.0;
		while (Session.GetGames().Phase == TEXT("active") && Session.GetSimSeconds() < 120.0 && Guard < 10000)
		{
			Session.Advance(Dt, true);
			++Guard;
			const FActor* Marked = SimFindActor(Session.GetGames().State, Session.GetGames().PlayerId);
			double OffPad = 1.0e9;
			if (Marked)
			{
				for (int32 Pad = 0; Pad < 3; ++Pad)
				{
					OffPad = FMath::Min(OffPad, SimDistance(Marked->Pos, Session.PadKernel(Pad)));
				}
			}
			WorstOffPad = FMath::Max(WorstOffPad, OffPad);
		}
		const FString ChainText = FString::Join(Session.GetChain(), TEXT("\n"));
		// T19 owns this measurement. The proposal run keeps its own chain so it does not overwrite the live one.
		const TCHAR* Variant = FParse::Param(FCommandLine::Get(), TEXT("MageArenaProposal")) ? TEXT("-proposal") : TEXT("");
		FFileHelper::SaveStringToFile(ChainText, *ChainPath(*FString::Printf(TEXT("../T19/%s%s-chain.log"), Label, Variant)));
		const FActor* Player = SimFindActor(Session.GetGames().State, Session.GetGames().PlayerId);
		bool bSunfall = false;
		for (const FActor& Actor : Session.GetGames().State.Actors)
		{
			if (Actor.Fire.bSchool && Actor.Team != 0 && (Actor.Fire.SunfallCastTick > 0 || Actor.Fire.bSunfallLoosed))
			{
				bSunfall = true;
			}
		}
		const double Measured = Session.GetSimSeconds();
		const bool bWon = Session.GetGames().Phase == TEXT("intermission") || Session.GetGames().Phase == TEXT("complete");
		const bool bBand = Measured >= Lo && Measured <= Hi && Session.GetGames().Phase != TEXT("active");
		const FVrRuleset* Live = Session.GetVrRules();
		const bool bBound = Live && Live->BoundRival() != nullptr;
		const int32 Breaks = Live ? Live->RivalRuntime.PhasesBroken : 0;
		const int32 LastBreaks = bBound ? Live->BoundRival()->Phases.Num() : 2;
		const bool bSignature = Live && Live->RivalRuntime.GrantTick >= 0;
		// T20: the player plays cassia (Mirror II A), so perfects can reflect. Measured, not tuned.
		int32 Reflects = 0;
		for (const FArenaEvent& Event : Session.GetGames().State.Events)
		{
			Reflects += Event.Kind == TEXT("reflect") && Player && Event.ActorId == Player->Id ? 1 : 0;
		}
		UE_LOG(LogMageArena, Log, TEXT("FireSeated %s phase=%s t=%.2f won=%d sunfall=%d breaks=%d signature=%d hp=%.1f dealt=%.1f walls=%d splits=%d plants=%d perfects=%d reflects=%d"),
			Label, *Session.GetGames().Phase, Measured, bWon ? 1 : 0, bSunfall ? 1 : 0, Breaks, bSignature ? 1 : 0,
			Player ? Player->Hp : -1.0, Player ? Player->Metrics.DamageDealt : -1.0,
			Session.GetWallsRaised(), Session.GetSplitCasts(), Session.GetStaffPlants(),
			Player ? Player->Metrics.Perfects : -1, Reflects);
		bool bOne = true;
		bOne &= TestTrue(*FString::Printf(TEXT("%s Brennic bound"), Label), bBound);
		bOne &= TestTrue(*FString::Printf(TEXT("%s length %.2fs in %.0f-%.0f"), Label, Measured, Lo, Hi), bBand);
		bOne &= TestTrue(*FString::Printf(TEXT("%s reached the last phase (%d of %d breaks)"), Label, Breaks, LastBreaks), Breaks >= LastBreaks);
		bOne &= TestTrue(*FString::Printf(TEXT("%s signature cast in the last phase"), Label), Breaks < LastBreaks || bSignature);
		bOne &= TestTrue(*FString::Printf(TEXT("%s stayed on a pad (worst %.4f m)"), Label, WorstOffPad), WorstOffPad < 1.0e-3);
		if (bRequireWin)
		{
			bOne &= TestTrue(*FString::Printf(TEXT("%s player wins (phase %s)"), Label, *Session.GetGames().Phase), bWon);
		}
		Session.Unbind();
		return bOne;
	};
	bPass &= RunOne(1.0, 1, TEXT("brennic-c1"), true);
	bPass &= RunOne(1.5, 1, TEXT("brennic-c15"), false);
	FArenaSession Dummy;
	Rig.Close(Dummy);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaAirSeated, "MageArenaDesign.Duel.AirSeated",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaAirSeated::RunTest(const FString& Parameters)
{
	FSessionRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	// T23: the seated player against Lio, the Tiro final's air mage (wave index 3, pinned competence 1.5), with his
	// HP-gated phases. DECISIONS 2026-10-07 (DF-003 answered): the duel lasts 45-80 s, the signature is cast in a duel that
	// reaches the last phase, and the player wins. Measured, not tuned: this test may be red, and the report says so.
	const double Lo = 45.0;
	const double Hi = 80.0;
	FVrRuleset Loaded;
	FString RulesError;
	if (!LoadVrRuleset(Loaded, RulesError))
	{
		AddError(RulesError);
		FArenaSession Dummy;
		Rig.Close(Dummy);
		return false;
	}
	FArenaSession Session;
	Session.Bind(Rig.Hands, Rig.Sigils, Rig.Wards, Rig.Blinks, Rig.Staff);
	Session.SetScripted(true);
	Session.SetBout(3, true);
	Session.SetAirFinal(true);
	FVrRuleset Rules = Loaded;
	Rules.MageCompetenceOverride = 1.5;
	Session.SetRulesOverride(Rules);
	if (!Session.Start(1))
	{
		AddError(TEXT("lio start failed"));
		Rig.Close(Session);
		return false;
	}
	const double Dt = 1.0 / 72.0;
	int32 Guard = 0;
	double WorstOffPad = 0.0;
	// What Lio cast, by row (each pending cast once, by activation id), and how many of his bolts flew.
	TMap<int32, FString> LioCasts;
	TSet<int32> LioShots;
	while (Session.GetGames().Phase == TEXT("active") && Session.GetSimSeconds() < 120.0 && Guard < 10000)
	{
		Session.Advance(Dt, true);
		++Guard;
		if (const FVrRuleset* Bound = Session.GetVrRules())
		{
			if (const FActor* Rival = SimFindActor(Session.GetGames().State, Bound->RivalRuntime.ActorId))
			{
				if (Rival->Pending.IsSet() && Rival->Pending->SpellId.IsSet())
				{
					LioCasts.Add(Rival->Pending->ActivationId, Rival->Pending->SpellId.GetValue());
				}
				for (const FProjectile& Shot : Session.GetGames().State.Projectiles)
				{
					if (Shot.OwnerId == Rival->Id)
					{
						LioShots.Add(Shot.Id);
					}
				}
			}
		}
		if (const FActor* Marked = SimFindActor(Session.GetGames().State, Session.GetGames().PlayerId))
		{
			double OffPad = 1.0e9;
			for (int32 Pad = 0; Pad < 3; ++Pad)
			{
				OffPad = FMath::Min(OffPad, SimDistance(Marked->Pos, Session.PadKernel(Pad)));
			}
			WorstOffPad = FMath::Max(WorstOffPad, OffPad);
		}
	}
	const TCHAR* Variant = FParse::Param(FCommandLine::Get(), TEXT("MageArenaProposal")) ? TEXT("-proposal") : TEXT("");
	FFileHelper::SaveStringToFile(FString::Join(Session.GetChain(), TEXT("\n")), *ChainPath(*FString::Printf(TEXT("../T23/lio-c15%s-chain.log"), Variant)));
	const FGames& Games = Session.GetGames();
	const FActor* Player = SimFindActor(Games.State, Games.PlayerId);
	const FVrRuleset* Live = Session.GetVrRules();
	const FActor* Lio = Live ? SimFindActor(Games.State, Live->RivalRuntime.ActorId) : nullptr;
	const double Measured = Session.GetSimSeconds();
	const bool bWon = Games.Phase == TEXT("intermission") || Games.Phase == TEXT("complete");
	const bool bBand = Measured >= Lo && Measured <= Hi && Games.Phase != TEXT("active");
	const bool bBound = Live && Live->BoundRival() != nullptr && Live->BoundRival()->Id == TEXT("lio");
	const int32 Breaks = Live ? Live->RivalRuntime.PhasesBroken : 0;
	const int32 LastBreaks = bBound ? Live->BoundRival()->Phases.Num() : 2;
	const bool bSignature = Live && Live->RivalRuntime.GrantTick >= 0;
	int32 Tempests = 0;
	int32 Evades = 0;
	int32 Deflects = 0;
	int32 Forms = 0;
	for (const FArenaEvent& Event : Games.State.Events)
	{
		const bool bLio = Lio && Event.ActorId == Lio->Id;
		Tempests += bLio && Event.Kind == TEXT("cast") && Event.Value == 4.0 ? 1 : 0;
		Evades += bLio && Event.Kind == TEXT("evade") ? 1 : 0;
		Deflects += bLio && Event.Kind == TEXT("deflect") ? 1 : 0;
		Forms += bLio && Event.Kind == TEXT("airform") ? 1 : 0;
	}
	UE_LOG(LogMageArena, Log, TEXT("AirSeated lio-c15 phase=%s t=%.2f won=%d breaks=%d signature=%d signatureS=%.2f tier4casts=%d hp=%.1f dealt=%.1f lioHp=%.1f lioMomentumMax=%.1f forms=%d evades=%d deflects=%d perfects=%d walls=%d splits=%d plants=%d"),
		*Games.Phase, Measured, bWon ? 1 : 0, Breaks, bSignature ? 1 : 0, bSignature ? SimSeconds(Live->RivalRuntime.GrantTick) : -1.0, Tempests,
		Player ? Player->Hp : -1.0, Player ? Player->Metrics.DamageDealt : -1.0, Lio ? Lio->Hp : -1.0, Lio ? Lio->Air.MaxMomentum : -1.0,
		Forms, Evades, Deflects, Player ? Player->Metrics.Perfects : -1, Session.GetWallsRaised(), Session.GetSplitCasts(), Session.GetStaffPlants());
	TMap<FString, int32> ByRow;
	for (const TPair<int32, FString>& Cast : LioCasts)
	{
		ByRow.FindOrAdd(Cast.Value)++;
	}
	ByRow.KeySort([](const FString& A, const FString& B) { return A < B; });
	FString Rows;
	for (const TPair<FString, int32>& Row : ByRow)
	{
		Rows += FString::Printf(TEXT("%s=%d "), *Row.Key, Row.Value);
	}
	UE_LOG(LogMageArena, Log, TEXT("AirSeated lio-c15 windup casts: %s(zero-windup rows release on the press and are not listed) shots=%d lioCastEvents=%d lioDealt=%.1f lioPerfects=%d lioBlocks=%d"),
		*Rows, LioShots.Num(), Lio ? Lio->Metrics.Casts : -1, Lio ? Lio->Metrics.DamageDealt : -1.0, Lio ? Lio->Metrics.Perfects : -1, Lio ? Lio->Metrics.Blocks : -1);
	bool bPass = TestTrue(TEXT("Lio bound in the final"), bBound);
	bPass &= TestTrue(TEXT("Lio is the air mage"), Lio && Lio->Air.bSchool);
	bPass &= TestTrue(*FString::Printf(TEXT("length %.2fs in %.0f-%.0f"), Measured, Lo, Hi), bBand);
	bPass &= TestTrue(*FString::Printf(TEXT("reached the last phase (%d of %d breaks)"), Breaks, LastBreaks), Breaks >= LastBreaks);
	bPass &= TestTrue(TEXT("signature cast in the last phase"), Breaks < LastBreaks || bSignature);
	bPass &= TestTrue(*FString::Printf(TEXT("stayed on a pad (worst %.4f m)"), WorstOffPad), WorstOffPad < 1.0e-3);
	bPass &= TestTrue(*FString::Printf(TEXT("player wins (phase %s)"), *Games.Phase), bWon);
	Rig.Close(Session);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSessionDefeat, "MageArena.Session.Defeat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSessionDefeat::RunTest(const FString& Parameters)
{
	FSessionRig Rig;
	if (!Rig.Open(*this))
	{
		return false;
	}
	FArenaSession Session;
	Session.Bind(Rig.Hands, Rig.Sigils, Rig.Wards, Rig.Blinks);
	Session.SetScripted(false);
	if (!Session.Start(1))
	{
		AddError(TEXT("Session.Start failed"));
		Rig.Close(Session);
		return false;
	}

	const double Dt = 1.0 / 72.0;
	int32 Guard = 0;
	while (Session.GetGames().Phase == TEXT("active") && Session.GetSimSeconds() < 90.0 && Guard < 8000)
	{
		Session.Advance(Dt, true);
		++Guard;
	}

	const FString ChainText = FString::Join(Session.GetChain(), TEXT("\n"));
	FFileHelper::SaveStringToFile(ChainText, *ChainPath(TEXT("defeat-chain.log")));
	UE_LOG(LogMageArena, Log, TEXT("Session defeat test phase=%s t=%.2f"), *Session.GetGames().Phase, Session.GetSimSeconds());

	bool bPass = true;
	bPass &= TestEqual(TEXT("phase"), Session.GetGames().Phase, FString(TEXT("lost")));
	bPass &= TestTrue(*FString::Printf(TEXT("died at %.2fs"), Session.GetSimSeconds()), Session.GetSimSeconds() > 2.0);
	bPass &= TestTrue(TEXT("defeat line"), CountChain(Session.GetChain(), TEXT("Session defeat")) == 1);
	const FActor* Player = SimFindActor(Session.GetGames().State, Session.GetGames().PlayerId);
	bPass &= TestTrue(TEXT("player down"), Player && Player->bDown);

	Rig.Close(Session);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSessionChainAdvance, "MageArena.Session.ChainAdvance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSessionChainAdvance::RunTest(const FString& Parameters)
{
	FSessionRig Rig;
	if (!Rig.Open(*this)) return false;
	FArenaSession Session;
	Session.Bind(Rig.Hands, Rig.Sigils, Rig.Wards, Rig.Blinks);
	Session.SetScripted(false);
	Session.SetBoutIndex(0);
	const FString SaveDir = ScratchSaveDir();
	Session.SetSaveDirectory(SaveDir);
	if (!Session.Start(1)) return false;

	// Mock killing all enemies to force intermission
	FArenaState& MutableState = const_cast<FArenaState&>(Session.GetGames().State);
	FActor* Player = SimFindActor(MutableState, Session.GetGames().PlayerId);
	Player->Hp = Player->MaxHp * 0.5; // lose some HP
	Player->Mana = 0.0;
	Player->Stamina = 0.0;

	for (FActor& Actor : MutableState.Actors)
	{
		if (Actor.Id != Session.GetGames().PlayerId) Actor.bDown = true;
	}
	MutableState.Projectiles.Reset();
	MutableState.Telegraphs.Reset();
	Session.Advance(1.0, false); // should transition to intermission

	bool bPass = true;
	bPass &= TestEqual(TEXT("phase intermission"), Session.GetGames().Phase, FString(TEXT("intermission")));
	// The only way out of an intermission is a raised palm, so the prompt has to say so.
	bPass &= TestTrue(*FString::Printf(TEXT("intermission prompt names the gesture (%s)"), *Session.GetPromptText()),
		Session.GetPromptText().Contains(TEXT("palm")));

	// pause in an intermission does nothing harmful
	Session.TogglePause();
	Session.Advance(1.0, false);
	bPass &= TestEqual(TEXT("still intermission after pause"), Session.GetGames().Phase, FString(TEXT("intermission")));
	Session.TogglePause();

	// Hand raise to advance
	Rig.Wards->OnWardRaised.Broadcast(0.0, FVector::ForwardVector);
	Session.Advance(0.1, false);

	bPass &= TestEqual(TEXT("phase active"), Session.GetGames().Phase, FString(TEXT("active")));
	bPass &= TestEqual(TEXT("wave advanced"), Session.GetGames().Wave, 1);
	
	const FActor* ConstPlayer = SimFindActor(Session.GetGames().State, Session.GetGames().PlayerId);
	// Hand-computed. stats.csv rank 1: max HP 80 + 15 = 95, max mana 80 + 15 = 95, max stamina 60 + 10 = 70.
	// HP was halved to 47.5, so 47.5 is missing; combat.json betweenWaves heals 0.30 of it = 14.25 -> 61.75.
	// manaRefill and staminaRefill are 1.0, so both end full.
	bPass &= TestTrue(TEXT("HP healed by 30% of missing (combat.json betweenWaves.healFractionOfMissingHp)"), FMath::IsNearlyEqual(ConstPlayer->Hp, 61.75, 1.0e-6));
	bPass &= TestTrue(TEXT("Mana refilled to 95 (combat.json betweenWaves.manaRefill)"), FMath::IsNearlyEqual(ConstPlayer->Mana, 95.0, 1.0e-6));
	bPass &= TestTrue(TEXT("Stamina refilled to 70 (combat.json betweenWaves.staminaRefill)"), FMath::IsNearlyEqual(ConstPlayer->Stamina, 70.0, 1.0e-6));

	// Save file stores the current bout
	FString SaveText;
	FFileHelper::LoadFileToString(SaveText, *FPaths::Combine(SaveDir, TEXT("bout.txt")));
	bPass &= TestTrue(TEXT("save file stores bout 1"), SaveText.Contains(TEXT("bout=1")));
	IFileManager::Get().DeleteDirectory(*SaveDir, false, true);

	Rig.Close(Session);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSessionSaveFailureLogged, "MageArena.Session.SaveFailureLogged",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSessionSaveFailureLogged::RunTest(const FString& Parameters)
{
	// A save that cannot be written must say so. Quitting mid-wave is the only moment the bout is saved, so a silent
	// failure loses the player's place with nothing in the log to explain it.
	FSessionRig Rig;
	if (!Rig.Open(*this)) return false;
	const FString Blocker = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArenaTests"), TEXT("save-blocker.txt"));
	FFileHelper::SaveStringToFile(TEXT("a file where the save directory should be"), *Blocker);
	FArenaSession Session;
	Session.Bind(Rig.Hands, Rig.Sigils, Rig.Wards, Rig.Blinks);
	Session.SetScripted(false);
	Session.SetBoutIndex(0);
	Session.SetSaveDirectory(FPaths::Combine(Blocker, TEXT("sub")));
	bool bPass = TestTrue(TEXT("start wave 1"), Session.Start(1));
	bPass &= TestEqual(TEXT("the wave is live"), Session.GetStage(), FString(TEXT("active")));
	AddExpectedError(TEXT("Could not save the bout"), EAutomationExpectedErrorFlags::Contains, 1);
	Session.NotifyQuit();
	IFileManager::Get().Delete(*Blocker, false, true, true);
	Rig.Close(Session);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSessionChainDefeat, "MageArena.Session.ChainDefeat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSessionChainDefeat::RunTest(const FString& Parameters)
{
	FSessionRig Rig;
	if (!Rig.Open(*this)) return false;
	FArenaSession Session;
	Session.Bind(Rig.Hands, Rig.Sigils, Rig.Wards, Rig.Blinks);
	Session.SetScripted(false);
	Session.SetBoutIndex(1); // Bout 2
	if (!Session.Start(1)) return false;

	FArenaState& MutableState = const_cast<FArenaState&>(Session.GetGames().State);
	FActor* Player = SimFindActor(MutableState, Session.GetGames().PlayerId);
	Player->Hp = 0.0;
	Player->bDown = true;
	Session.Advance(1.0, false); // should transition to lost

	bool bPass = true;
	bPass &= TestEqual(TEXT("phase lost"), Session.GetGames().Phase, FString(TEXT("lost")));

	// Hand raise to retry
	Rig.Wards->OnWardRaised.Broadcast(0.0, FVector::ForwardVector);
	Session.Advance(0.1, false);

	bPass &= TestEqual(TEXT("phase active"), Session.GetGames().Phase, FString(TEXT("active")));
	bPass &= TestEqual(TEXT("wave remains 1 (Bout 2)"), Session.GetGames().Wave, 1);

	Rig.Close(Session);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaFullSeated, "MageArenaDesign.Session.FullSeated",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaFullSeated::RunTest(const FString& Parameters)
{
	// T21: the whole Tiro day as the reference script plays it, measured, not tuned. The ritual (the script holds both
	// palms), four bouts with their intro lines, the intermissions (continued at once by a raised palm, as before; the
	// T20 pick is not taken), and the aftermath (the script palms the centre stone). The prologue and the teach are
	// skipped as before; a second, first-launch session measures cold + prologue + teach for the total with the teach.
	// Plan target: a 7-10 minute session (docs/campaign/04-vr-campaign-design.md section 1, Games day 6-10 min, and the
	// V2-ROADMAP mechanics gate). A bout the script loses is the end of the run: the script is deterministic, so a retry
	// of bouts 1-3 loses again; a lost final goes to the aftermath on the centre stone.
	const bool bProposal = FParse::Param(FCommandLine::Get(), TEXT("MageArenaProposal"));
	const double PlanLoS = 7.0 * 60.0;
	const double PlanHiS = 10.0 * 60.0;
	const double Dt = 1.0 / 72.0;
	FSessionRig Rig;
	if (!Rig.Open(*this)) return false;

	// First launch: cold -> prologue -> teach, until the ritual.
	double FirstLaunchS = 0.0;
	{
		FArenaSession First;
		First.Bind(Rig.Hands, Rig.Sigils, Rig.Wards, Rig.Blinks, Rig.Staff);
		First.SetScripted(true);
		const FString FirstDir = ScratchSaveDir();
		First.SetSaveDirectory(FirstDir);
		if (!First.BeginArc(1))
		{
			AddError(TEXT("first launch BeginArc failed"));
			Rig.Close(First);
			return false;
		}
		int32 Guard = 0;
		while (First.GetStage() != TEXT("ritual") && Guard < 72 * 300)
		{
			First.Advance(Dt, true);
			FirstLaunchS += Dt;
			++Guard;
		}
		UE_LOG(LogMageArena, Log, TEXT("FullSeated first launch: cold+prologue+teach=%.2f s (teach %.3f s) stage=%s"),
			FirstLaunchS, First.GetTeachSeconds(), *First.GetStage());
		TestEqual(TEXT("the first launch reaches the ritual"), First.GetStage(), FString(TEXT("ritual")));
		First.Unbind();
		IFileManager::Get().DeleteDirectory(*FirstDir, false, true);
	}

	FArenaSession Session;
	Session.Bind(Rig.Hands, Rig.Sigils, Rig.Wards, Rig.Blinks, Rig.Staff);
	Session.SetScripted(true);
	const FString Dir = ScratchSaveDir();
	Session.SetSaveDirectory(Dir);
	if (!Session.BeginArc(1))
	{
		AddError(TEXT("BeginArc failed"));
		Rig.Close(Session);
		return false;
	}
	Session.SkipTeach();

	TMap<FString, double> PhaseS;
	double IntroS[4] = {0.0, 0.0, 0.0, 0.0};
	double DayS = 0.0;
	int32 Guard = 0;
	bool bStopped = false;
	FString LastStage;
	while (Session.GetStage() != TEXT("closed") && Guard < 72 * 1800)
	{
		const FString Stage = Session.GetStage();
		if (Stage != LastStage)
		{
			UE_LOG(LogMageArena, Log, TEXT("FullSeated stage=%s bout=%d t=%.2f"), *Stage, Session.GetGames().Wave + 1, DayS);
			if (Stage == TEXT("intermission"))
			{
				Rig.Wards->OnWardRaised.Broadcast(0.0, FVector::ForwardVector);
			}
			LastStage = Stage;
		}
		if (Stage == TEXT("lost") && Session.GetGames().Wave < 3)
		{
			bStopped = true;
			break;
		}
		Session.Advance(Dt, true);
		PhaseS.FindOrAdd(Stage) += Dt;
		if (Stage == TEXT("intro") && Session.GetGames().Wave >= 0 && Session.GetGames().Wave < 4)
		{
			IntroS[Session.GetGames().Wave] += Dt;
		}
		DayS += Dt;
		++Guard;
	}

	const FString ChainText = FString::Join(Session.GetChain(), TEXT("\n"));
	FFileHelper::SaveStringToFile(ChainText, *ChainPath(*FString::Printf(TEXT("../T21/fullseated%s-chain.log"), bProposal ? TEXT("-proposal") : TEXT(""))));
	int32 Won = 0;
	double FightS = 0.0;
	for (int32 WaveN = 1; WaveN <= 4; ++WaveN)
	{
		bool bWon = false;
		double Seconds = -1.0;
		double Attempts = 0.0;
		double Perfects = 0.0;
		const bool bFought = Session.GetFlags().GetBool(FArenaFlags::TiroKey(WaveN, TEXT("won")), bWon);
		Session.GetFlags().GetNumber(FArenaFlags::TiroKey(WaveN, TEXT("time")), Seconds);
		Session.GetFlags().GetNumber(FArenaFlags::TiroKey(WaveN, TEXT("attempts")), Attempts);
		Session.GetFlags().GetNumber(FArenaFlags::TiroKey(WaveN, TEXT("perfects")), Perfects);
		double Cracks = -1.0;
		double Tithe = -1.0;
		Session.GetFlags().GetNumber(FArenaFlags::TiroKey(WaveN, TEXT("cracks")), Cracks);
		Session.GetFlags().GetNumber(FArenaFlags::TiroKey(WaveN, TEXT("tithe")), Tithe);
		Won += bFought && bWon ? 1 : 0;
		FightS += bFought ? Seconds : 0.0;
		UE_LOG(LogMageArena, Log, TEXT("FullSeated bout %d %s outcome=%s fight=%.2f s intro=%.2f s attempts=%.0f perfects=%.0f cracks=%.0f tithe=%.0f school=%s"),
			WaveN, bProposal ? TEXT("proposal") : TEXT("live"), bFought ? (bWon ? TEXT("won") : TEXT("lost")) : TEXT("not-fought"),
			Seconds, IntroS[WaveN - 1], Attempts, Perfects, Cracks, Tithe, *Session.DaySchool(WaveN - 1));
	}
	// T22, measured, not asserted: the day's collar (the -1 rows above were not fought) and the collar's lifetime.
	UE_LOG(LogMageArena, Log, TEXT("FullSeated collar %s day cracks=%d tithe=%d lifetimeCracks=%d lifetimeTithe=%d"),
		bProposal ? TEXT("proposal") : TEXT("live"), Session.GetDayCracks(), Session.GetDayTithe(), Session.GetCollar().GetLifetimeCracks(),
		Session.GetCollar().GetLifetimeTithe());
	const double WithTeachS = FirstLaunchS + DayS;
	UE_LOG(LogMageArena, Log, TEXT("FullSeated phases ritual=%.2f intro=%.2f active=%.2f intermission=%.2f aftermath=%.2f lost=%.2f"),
		PhaseS.FindRef(TEXT("ritual")), PhaseS.FindRef(TEXT("intro")), PhaseS.FindRef(TEXT("active")), PhaseS.FindRef(TEXT("intermission")),
		PhaseS.FindRef(TEXT("aftermath")), PhaseS.FindRef(TEXT("lost")));
	UE_LOG(LogMageArena, Log, TEXT("FullSeated day %s: stage=%s won=%d/4 fights=%.2f s day=%.2f s (%.2f min) with teach=%.2f s (%.2f min) plan=7-10 min %s"),
		bProposal ? TEXT("proposal") : TEXT("live"), *Session.GetStage(), Won, FightS, DayS, DayS / 60.0, WithTeachS, WithTeachS / 60.0,
		WithTeachS >= PlanLoS && WithTeachS <= PlanHiS ? TEXT("INSIDE") : TEXT("OUTSIDE"));

	bool bPass = true;
	bPass &= TestTrue(TEXT("Did not stop in an infinite loop"), Guard < 72 * 1800);
	bPass &= TestFalse(*FString::Printf(TEXT("the script did not lose bout %d"), Session.GetGames().Wave + 1), bStopped);
	bPass &= TestEqual(TEXT("the day closes"), Session.GetStage(), FString(TEXT("closed")));
	bPass &= TestEqual(TEXT("all four bouts won"), Won, 4);
	bPass &= TestTrue(*FString::Printf(TEXT("the day with the teach (%.2f s) is in the plan's 7-10 min"), WithTeachS), WithTeachS >= PlanLoS && WithTeachS <= PlanHiS);
	IFileManager::Get().DeleteDirectory(*Dir, false, true);
	Rig.Close(Session);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaDeathBurstPerfect, "MageArena.Session.DeathBurstPerfect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaDeathBurstPerfect::RunTest(const FString& Parameters)
{
	FSessionRig Rig;
	if (!Rig.Open(*this)) return false;
	FArenaSession Session;
	Session.Bind(Rig.Hands, Rig.Sigils, Rig.Wards, Rig.Blinks);
	Session.SetScripted(false);
	if (!Session.Start(1)) return false;
	
	// Force transition to active phase BEFORE adding dummy owner
	Rig.Wards->OnWardRaised.Broadcast(0.0, FVector::ForwardVector);
	Session.Advance(0.0, false); // Start wave, which resets Actors
	Rig.Wards->OnWardLowered.Broadcast(); // Lower ward so it's fresh for the perfect absorb

	FArenaState& MutableState = const_cast<FArenaState&>(Session.GetGames().State);
	FActor DummyOwner;
	DummyOwner.Id = 99;
	DummyOwner.Team = 1;
	DummyOwner.Hp = 100.0;
	MutableState.Actors.Add(DummyOwner);
	
	const FActor* TargetPlayer = SimFindActor(MutableState, Session.GetGames().PlayerId);
	FSimVec PlayerPos = TargetPlayer ? TargetPlayer->Pos : FSimVec{8.0, 10.0};
	
	FTelegraph Telegraph;
	Telegraph.Id = 100;
	Telegraph.ActivationId = 100;
	Telegraph.OwnerId = 99;
	Telegraph.Family = TEXT("magic");
	Telegraph.Tier = 0;
	Telegraph.Damage = 5.0;
	Telegraph.Kind = TEXT("area");
	Telegraph.Origin = FSimVec{PlayerPos.X + 1.0, PlayerPos.Y}; // offset to give it a direction for the ward arc
	Telegraph.Target = Telegraph.Origin;
	Telegraph.StartTick = MutableState.Tick;
	Telegraph.ResolveTick = MutableState.Tick + static_cast<int32>(0.6 * 60.0); // enemies.json death burst delay
	Telegraph.SpeedMps = 0.0;
	Telegraph.RangeM = 1.5;
	Telegraph.WidthM = 1.5;
	Telegraph.bSurvivesOwner = true;
	MutableState.Telegraphs.Add(Telegraph);
	
	const double Dt = 1.0 / 60.0;
	
	const double WaitS = 0.5; // Window is 0.15s, so 0.1s before resolve is perfect
	double SimS = 0.0;
	while (SimS < WaitS)
	{
		Session.Advance(Dt, false);
		SimS += Dt;
	}
	Rig.Wards->OnWardRaised.Broadcast(Session.GetSimSeconds(), FVector::ForwardVector);
	
	while (SimS < 0.65)
	{
		Session.Advance(Dt, false);
		SimS += Dt;
	}

	const FActor* Player = SimFindActor(Session.GetGames().State, Session.GetGames().PlayerId);
	UE_LOG(LogMageArena, Log, TEXT("Final Tick=%d bAbsorb=%d Telegraphs=%d Perfects=%d"), Session.GetGames().State.Tick, Player ? Player->bAbsorb : 0, Session.GetGames().State.Telegraphs.Num(), Player ? Player->Metrics.Perfects : -1);
	
	bool bPass = true;
	bPass &= TestTrue(TEXT("Death burst was perfect absorbed"), Player && Player->Metrics.Perfects == 1);
	
	Rig.Close(Session);
	return bPass;
}

#endif
