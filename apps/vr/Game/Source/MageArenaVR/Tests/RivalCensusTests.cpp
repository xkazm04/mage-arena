#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Gestures/BlinkDetector.h"
#include "Gestures/SigilRecognizerSubsystem.h"
#include "Gestures/StaffDetector.h"
#include "Gestures/WardDetector.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Hands/HandInputSubsystem.h"
#include "Kernel/Air.h"
#include "Kernel/ArenaKernel.h"
#include "Kernel/KernelData.h"
#include "Kernel/SimMath.h"
#include "Kernel/VrRules.h"
#include "MageArenaVR.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Session/ArenaSession.h"

#include <cmath>

// T19: the seated Brennic duel (Tiro semifinal, wave index 2, Fire) with HP-gated phases, live and with the
// proposal, at competence 1 and 1.5. The "-off" sets drop the rivals block so the same seeds show the duel
// without phases. Nothing here writes calibration-proposal.json: the owner picks.
// T23: the seated Lio duel (Tiro final, wave index 3, Air, signature Tempest Lance), live and proposal, at competence 1
// and 1.5, written to runs/T23/census/. The "signature" columns are Sunfall for Brennic and Tempest Lance for Lio.

namespace
{
struct FRivalRow
{
	FString Set;
	double Competence = 1.0;
	int32 Seed = 0;
	FString Outcome;
	double TimeS = 0.0;
	double HpFrac = 0.0;
	double RivalHpFrac = 0.0;
	bool bRivalBound = false;
	int32 Phases = 0;
	double Phase2S = -1.0;
	double Phase3S = -1.0;
	bool bSignature = false;
	double SignatureS = -1.0;
	int32 PlayerTier = 0;
	int32 RivalTier = 0;
	int32 SunfallCasts = 0;
	int32 SunfallLoosed = 0;
	int32 SunfallBlinked = 0;
	int32 SunfallHit = 0;
	int32 SunfallMissed = 0;
	int32 Walls = 0;
	int32 Splits = 0;
	int32 Plants = 0;
};

struct FRivalSet
{
	FString Name;
	FVrRuleset Rules;
	bool bPhases = true;
};

struct FRivalSummary
{
	FString Set;
	double Competence = 1.0;
	int32 N = 0;
	double MedianS = 0.0;
	double WinRate = 0.0;
	double ReachPhase2 = 0.0;
	double ReachPhase3 = 0.0;
	int32 ReachedLast = 0;
	int32 SignatureInLast = 0;
	double SignatureRate = 0.0;
	double AnySunfall = 0.0;
	int32 Loosed = 0;
	int32 Blinked = 0;
	int32 Hit = 0;
	int32 Missed = 0;
	bool bSignatureTarget = false;
	bool bMedianTarget = false;
	bool bWinTarget = false;
};

FString RivalCensusDir(const TCHAR* Run = TEXT("T19"))
{
	FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../../.."), FString::Printf(TEXT("runs/%s/census"), Run));
	FPaths::CollapseRelativeDirectories(Path);
	IFileManager::Get().MakeDirectory(*Path, true);
	return Path;
}

double RivalMedian(TArray<double> Values)
{
	if (Values.Num() == 0)
	{
		return 0.0;
	}
	Values.Sort();
	const int32 Mid = Values.Num() / 2;
	return Values.Num() % 2 == 0 ? 0.5 * (Values[Mid - 1] + Values[Mid]) : Values[Mid];
}

struct FMeteor
{
	int32 Id = 0;
	int32 StartTick = 0;
	int32 ResolveTick = 0;
};

// Wave 2 is Brennic's semifinal (fire, Sunfall); wave 3 is Lio's final (air, Tempest Lance).
FRivalRow RunRivalBout(FArenaSession& Session, const FRivalSet& Set, double Competence, uint32 Seed, double Cap, int32 Wave = 2)
{
	FVrRuleset Rules = Set.Rules;
	Rules.MageCompetenceOverride = Competence;
	Session.SetBout(Wave, true);
	Session.SetAirFinal(true);
	Session.SetPolicy(FSeatedPolicy());
	Session.SetRulesOverride(Rules);
	Session.SetScripted(true);
	FRivalRow Row;
	Row.Set = Set.Name;
	Row.Competence = Competence;
	Row.Seed = static_cast<int32>(Seed);
	if (!Session.Start(Seed))
	{
		Row.Outcome = TEXT("start-failed");
		return Row;
	}
	int32 RivalId = -1;
	for (const FActor& Actor : Session.GetGames().State.Actors)
	{
		if (Actor.Team != 0 && (Actor.Fire.bSchool || Actor.Air.bSchool) && Actor.MageAI.IsSet())
		{
			RivalId = Actor.Id;
			break;
		}
	}
	const FActor* Spawned = SimFindActor(Session.GetGames().State, RivalId);
	const bool bAir = Spawned && Spawned->Air.bSchool;
	// The Tempest Lance is a line that resolves when its cast releases; a "meteor" here is that cast, start to release.
	TArray<FMeteor> Meteors;
	const double Dt = 1.0 / 72.0;
	int32 Guard = 0;
	while (Session.GetGames().Phase == TEXT("active") && Session.GetSimSeconds() < Cap && Guard < 9000)
	{
		Session.Advance(Dt, true);
		++Guard;
		for (const FTelegraph& Telegraph : Session.GetGames().State.Telegraphs)
		{
			if (Telegraph.OwnerId != RivalId || Telegraph.Kind != TEXT("area") || Telegraph.Family != TEXT("unblockable"))
			{
				continue;
			}
			if (!Meteors.ContainsByPredicate([&](const FMeteor& Seen) { return Seen.Id == Telegraph.Id; }))
			{
				Meteors.Add({Telegraph.Id, Telegraph.StartTick, Telegraph.ResolveTick});
			}
		}
		const FActor* Caster = SimFindActor(Session.GetGames().State, RivalId);
		if (bAir && Caster && Caster->Pending.IsSet() && Caster->Pending->SpellId.IsSet() && Caster->Pending->SpellId.GetValue() == TEXT("air_tempest"))
		{
			const FPendingCast& Lance = Caster->Pending.GetValue();
			if (!Meteors.ContainsByPredicate([&](const FMeteor& Seen) { return Seen.Id == Lance.ActivationId; }))
			{
				Meteors.Add({Lance.ActivationId, Lance.StartTick, Lance.ReleaseTick});
			}
		}
	}
	const FGames& Games = Session.GetGames();
	const FString Phase = Games.Phase;
	Row.Outcome = (Phase == TEXT("intermission") || Phase == TEXT("complete")) ? TEXT("win") : (Phase == TEXT("lost") ? TEXT("loss") : TEXT("timeout"));
	Row.TimeS = Session.GetSimSeconds();
	const FActor* Player = SimFindActor(Games.State, Games.PlayerId);
	const FActor* Rival = SimFindActor(Games.State, RivalId);
	Row.HpFrac = Player && Player->MaxHp > 0.0 ? Player->Hp / Player->MaxHp : 0.0;
	Row.RivalHpFrac = Rival && Rival->MaxHp > 0.0 ? Rival->Hp / Rival->MaxHp : 0.0;
	Row.PlayerTier = Player ? Player->Tier : 0;
	Row.RivalTier = Rival ? Rival->Tier : 0;
	Row.Walls = Session.GetWallsRaised();
	Row.Splits = Session.GetSplitCasts();
	Row.Plants = Session.GetStaffPlants();
	if (const FVrRuleset* Live = Session.GetVrRules())
	{
		Row.bRivalBound = Live->RivalRuntime.ActorId == RivalId && RivalId > 0;
		Row.Phases = Live->RivalRuntime.PhasesBroken;
		if (Live->RivalRuntime.BreakTicks.Num() > 0)
		{
			Row.Phase2S = SimSeconds(Live->RivalRuntime.BreakTicks[0]);
		}
		if (Live->RivalRuntime.BreakTicks.Num() > 1)
		{
			Row.Phase3S = SimSeconds(Live->RivalRuntime.BreakTicks[1]);
		}
		Row.bSignature = Live->RivalRuntime.GrantTick >= 0;
		Row.SignatureS = Row.bSignature ? SimSeconds(Live->RivalRuntime.GrantTick) : -1.0;
	}
	for (const FArenaEvent& Event : Games.State.Events)
	{
		if (!bAir && Event.Kind == TEXT("cast") && Event.ActorId == RivalId && Event.Value == 4.0)
		{
			++Row.SunfallCasts;
		}
	}
	// Air: count Tempest Lance casts by the pending casts seen (Cyclone is tier 4 too, so the tier-4 count would mix them).
	Row.SunfallCasts += bAir ? Meteors.Num() : 0;
	for (const FMeteor& Meteor : Meteors)
	{
		if (bAir && Meteor.ResolveTick > Games.State.Tick)
		{
			continue;
		}
		++Row.SunfallLoosed;
		if (Meteor.ResolveTick > Games.State.Tick)
		{
			continue;
		}
		bool bHit = false;
		bool bRolled = false;
		for (const FArenaEvent& Event : Games.State.Events)
		{
			if (Event.Kind == TEXT("hit") && Event.ActorId == Games.PlayerId && Event.Tick == Meteor.ResolveTick
				&& Event.TargetId.Get(-1) == RivalId && Event.Value > 0.0)
			{
				bHit = true;
			}
			if (Event.Kind == TEXT("roll") && Event.ActorId == Games.PlayerId && Event.Tick >= Meteor.StartTick && Event.Tick <= Meteor.ResolveTick)
			{
				bRolled = true;
			}
		}
		if (bHit)
		{
			++Row.SunfallHit;
		}
		else if (bRolled)
		{
			++Row.SunfallBlinked;
		}
		else
		{
			++Row.SunfallMissed;
		}
	}
	return Row;
}

FRivalSummary Summarise(const FString& Set, double Competence, const TArray<FRivalRow>& Rows)
{
	FRivalSummary Out;
	Out.Set = Set;
	Out.Competence = Competence;
	TArray<double> Times;
	int32 Wins = 0;
	int32 Reach2 = 0;
	int32 Any = 0;
	for (const FRivalRow& Row : Rows)
	{
		if (Row.Set != Set || std::abs(Row.Competence - Competence) > 1.0e-9)
		{
			continue;
		}
		++Out.N;
		Times.Add(Row.TimeS);
		Wins += Row.Outcome == TEXT("win") ? 1 : 0;
		Reach2 += Row.Phases >= 1 ? 1 : 0;
		if (Row.Phases >= 2)
		{
			++Out.ReachedLast;
			Out.SignatureInLast += Row.bSignature ? 1 : 0;
		}
		Any += Row.SunfallCasts > 0 ? 1 : 0;
		Out.Loosed += Row.SunfallLoosed;
		Out.Blinked += Row.SunfallBlinked;
		Out.Hit += Row.SunfallHit;
		Out.Missed += Row.SunfallMissed;
	}
	const double N = FMath::Max(1, Out.N);
	Out.MedianS = RivalMedian(Times);
	Out.WinRate = Wins / N;
	Out.ReachPhase2 = Reach2 / N;
	Out.ReachPhase3 = Out.ReachedLast / N;
	Out.SignatureRate = Out.ReachedLast > 0 ? static_cast<double>(Out.SignatureInLast) / static_cast<double>(Out.ReachedLast) : 0.0;
	Out.AnySunfall = Any / N;
	// DECISIONS 2026-10-07 (DF-003 answered): signature in 100 % of duels that reach the last phase, median 45-80 s,
	// win >= 70 % at competence 1 and 40-60 % at 1.5.
	Out.bSignatureTarget = Out.ReachedLast > 0 && Out.SignatureInLast == Out.ReachedLast;
	Out.bMedianTarget = Out.MedianS >= 45.0 && Out.MedianS <= 80.0;
	Out.bWinTarget = Competence < 1.25 ? Out.WinRate >= 0.70 : (Out.WinRate >= 0.40 && Out.WinRate <= 0.60);
	return Out;
}

void WriteRivalCsv(const FString& Path, const TArray<FRivalRow>& Rows, const TCHAR* Signature = TEXT("sunfall"))
{
	FString Body = FString::Printf(TEXT("set,competence,seed,outcome,time_s,hp_frac,rival_hp_frac,rival_bound,phases_broken,phase2_s,phase3_s,signature,signature_s,player_tier,rival_tier,%s_casts,%s_loosed,%s_blinked,%s_hit,%s_missed,walls,split_casts,plants\n"),
		Signature, Signature, Signature, Signature, Signature);
	for (const FRivalRow& Row : Rows)
	{
		Body += FString::Printf(TEXT("%s,%.2f,%d,%s,%.4f,%.4f,%.4f,%d,%d,%.4f,%.4f,%d,%.4f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n"),
			*Row.Set, Row.Competence, Row.Seed, *Row.Outcome, Row.TimeS, Row.HpFrac, Row.RivalHpFrac,
			Row.bRivalBound ? 1 : 0, Row.Phases, Row.Phase2S, Row.Phase3S, Row.bSignature ? 1 : 0, Row.SignatureS,
			Row.PlayerTier, Row.RivalTier, Row.SunfallCasts, Row.SunfallLoosed, Row.SunfallBlinked, Row.SunfallHit, Row.SunfallMissed,
			Row.Walls, Row.Splits, Row.Plants);
	}
	FFileHelper::SaveStringToFile(Body, *Path);
}

TSharedRef<FJsonObject> SummaryJson(const FRivalSummary& Summary)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("set"), Summary.Set);
	Object->SetNumberField(TEXT("competence"), Summary.Competence);
	Object->SetNumberField(TEXT("duels"), Summary.N);
	Object->SetNumberField(TEXT("medianS"), Summary.MedianS);
	Object->SetNumberField(TEXT("winRate"), Summary.WinRate);
	Object->SetNumberField(TEXT("reachPhase2"), Summary.ReachPhase2);
	Object->SetNumberField(TEXT("reachPhase3"), Summary.ReachPhase3);
	Object->SetNumberField(TEXT("reachedLast"), Summary.ReachedLast);
	Object->SetNumberField(TEXT("signatureInLast"), Summary.SignatureInLast);
	Object->SetNumberField(TEXT("signatureRate"), Summary.SignatureRate);
	Object->SetNumberField(TEXT("anySunfallCast"), Summary.AnySunfall);
	Object->SetNumberField(TEXT("sunfallLoosed"), Summary.Loosed);
	Object->SetNumberField(TEXT("sunfallBlinked"), Summary.Blinked);
	Object->SetNumberField(TEXT("sunfallHit"), Summary.Hit);
	Object->SetNumberField(TEXT("sunfallMissed"), Summary.Missed);
	Object->SetBoolField(TEXT("signatureTarget"), Summary.bSignatureTarget);
	Object->SetBoolField(TEXT("medianTarget"), Summary.bMedianTarget);
	Object->SetBoolField(TEXT("winTarget"), Summary.bWinTarget);
	return Object;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCensusRivals, "MageArenaDesign.Census.Rivals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCensusRivals::RunTest(const FString& Parameters)
{
	if (!KernelData().bReady)
	{
		AddError(KernelData().Error.IsEmpty() ? TEXT("kernel data failed to load") : KernelData().Error);
		return false;
	}
	FString RulesError;
	FVrRuleset Live;
	if (!LoadVrRuleset(Live, RulesError, false))
	{
		AddError(RulesError);
		return false;
	}
	if (!TestTrue(TEXT("combat.vr.json has a rivals block"), Live.Rivals.Num() > 0))
	{
		return false;
	}
	FVrRuleset Proposal = Live;
	if (!ApplyVrCalibrationProposal(Proposal, RulesError))
	{
		AddError(RulesError);
		return false;
	}
	TArray<FRivalSet> Sets;
	Sets.Add({TEXT("live"), Live, true});
	Sets.Add({TEXT("proposal"), Proposal, true});
	FRivalSet LiveOff{TEXT("live-off"), Live, false};
	LiveOff.Rules.Rivals.Reset();
	Sets.Add(LiveOff);
	FRivalSet ProposalOff{TEXT("proposal-off"), Proposal, false};
	ProposalOff.Rules.Rivals.Reset();
	Sets.Add(ProposalOff);

	UGameInstance* Instance = NewObject<UGameInstance>(GetTransientPackage());
	Instance->AddToRoot();
	UHandInputSubsystem* Hands = NewObject<UHandInputSubsystem>(Instance);
	USigilRecognizerSubsystem* Sigils = NewObject<USigilRecognizerSubsystem>(Instance);
	Sigils->LoadTemplatesFromDirectory(FPaths::Combine(FPaths::ProjectDir(), TEXT("Clips"), TEXT("templates")));
	UWardDetectorSubsystem* Wards = NewObject<UWardDetectorSubsystem>(Instance);
	FString WardError;
	if (!Wards->InitDetector(WardError))
	{
		AddError(WardError);
		Instance->RemoveFromRoot();
		return false;
	}
	UBlinkDetectorSubsystem* Blinks = NewObject<UBlinkDetectorSubsystem>(Instance);
	UStaffDetectorSubsystem* Staff = NewObject<UStaffDetectorSubsystem>(Instance);
	FArenaSession Session;
	Session.Bind(Hands, Sigils, Wards, Blinks, Staff);
	Session.SetQuiet(true);

	int32 Seeds = 20;
	FParse::Value(FCommandLine::Get(), TEXT("MageArenaCensusSeeds="), Seeds);
	Seeds = FMath::Clamp(Seeds, 1, 40);
	const double Competences[] = {1.0, 1.5};
	TArray<FRivalRow> Rows;
	const double Started = FPlatformTime::Seconds();
	for (const FRivalSet& Set : Sets)
	{
		for (const double Competence : Competences)
		{
			for (int32 Seed = 1; Seed <= Seeds; ++Seed)
			{
				Rows.Add(RunRivalBout(Session, Set, Competence, static_cast<uint32>(Seed), 120.0));
			}
		}
	}
	const double WallSeconds = FPlatformTime::Seconds() - Started;
	Session.Unbind();

	TArray<FRivalSummary> Summaries;
	for (const FRivalSet& Set : Sets)
	{
		for (const double Competence : Competences)
		{
			Summaries.Add(Summarise(Set.Name, Competence, Rows));
		}
	}
	const bool bOfficial = Seeds >= 20;
	const FString Dir = RivalCensusDir();
	WriteRivalCsv(FPaths::Combine(Dir, bOfficial ? TEXT("rivals.csv") : TEXT("rivals-probe.csv")), Rows);
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("seeds"), Seeds);
	Root->SetNumberField(TEXT("wallSeconds"), WallSeconds);
	Root->SetBoolField(TEXT("official"), bOfficial);
	Root->SetStringField(TEXT("scenario"), TEXT("Tiro semifinal (wave index 2), Fire, seated reference script, 120 s cap; competence set on every opponent mage"));
	Root->SetStringField(TEXT("targets"), TEXT("DECISIONS 2026-10-07: signature in 100% of duels reaching the last phase; median 45-80 s; win >= 0.70 at competence 1, 0.40-0.60 at 1.5"));
	TArray<TSharedPtr<FJsonValue>> Values;
	for (const FRivalSummary& Summary : Summaries)
	{
		Values.Add(MakeShared<FJsonValueObject>(SummaryJson(Summary)));
		UE_LOG(LogMageArena, Log, TEXT("RivalCensus %s c=%.1f n=%d median=%.2f win=%.2f reach2=%.2f reach3=%.2f signature=%d/%d anySunfall=%.2f loosed=%d blinked=%d hit=%d missed=%d targets sig=%d median=%d win=%d"),
			*Summary.Set, Summary.Competence, Summary.N, Summary.MedianS, Summary.WinRate, Summary.ReachPhase2, Summary.ReachPhase3,
			Summary.SignatureInLast, Summary.ReachedLast, Summary.AnySunfall, Summary.Loosed, Summary.Blinked, Summary.Hit, Summary.Missed,
			Summary.bSignatureTarget ? 1 : 0, Summary.bMedianTarget ? 1 : 0, Summary.bWinTarget ? 1 : 0);
	}
	Root->SetArrayField(TEXT("sets"), Values);
	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	FJsonSerializer::Serialize(Root, Writer);
	FFileHelper::SaveStringToFile(Text, *FPaths::Combine(Dir, bOfficial ? TEXT("rivals-summary.json") : TEXT("rivals-probe-summary.json")));
	UE_LOG(LogMageArena, Log, TEXT("RivalCensus done seeds=%d rows=%d wall=%.1fs official=%d"), Seeds, Rows.Num(), WallSeconds, bOfficial ? 1 : 0);

	// T23: Lio in the Tiro final, live and proposal (no "-off" sets: the air final is not a phase experiment).
	TArray<FRivalRow> LioRows;
	{
		FArenaSession LioSession;
		LioSession.Bind(Hands, Sigils, Wards, Blinks, Staff);
		LioSession.SetQuiet(true);
		const double LioStarted = FPlatformTime::Seconds();
		for (int32 SetIndex = 0; SetIndex < 2; ++SetIndex)
		{
			for (const double Competence : Competences)
			{
				for (int32 Seed = 1; Seed <= Seeds; ++Seed)
				{
					LioRows.Add(RunRivalBout(LioSession, Sets[SetIndex], Competence, static_cast<uint32>(Seed), 120.0, 3));
				}
			}
		}
		LioSession.Unbind();
		const double LioWall = FPlatformTime::Seconds() - LioStarted;
		const FString LioDir = RivalCensusDir(TEXT("T23"));
		WriteRivalCsv(FPaths::Combine(LioDir, bOfficial ? TEXT("lio.csv") : TEXT("lio-probe.csv")), LioRows, TEXT("tempest"));
		TSharedRef<FJsonObject> LioRoot = MakeShared<FJsonObject>();
		LioRoot->SetNumberField(TEXT("seeds"), Seeds);
		LioRoot->SetNumberField(TEXT("wallSeconds"), LioWall);
		LioRoot->SetBoolField(TEXT("official"), bOfficial);
		LioRoot->SetStringField(TEXT("scenario"), TEXT("Tiro final (wave index 3), Lio, Air, seated reference script, 120 s cap; competence set on every opponent mage; the signature columns are Tempest Lance"));
		LioRoot->SetStringField(TEXT("targets"), TEXT("DECISIONS 2026-10-07: signature in 100% of duels reaching the last phase; median 45-80 s; win >= 0.70 at competence 1, 0.40-0.60 at 1.5"));
		TArray<TSharedPtr<FJsonValue>> LioValues;
		for (int32 SetIndex = 0; SetIndex < 2; ++SetIndex)
		{
			for (const double Competence : Competences)
			{
				const FRivalSummary Summary = Summarise(Sets[SetIndex].Name, Competence, LioRows);
				LioValues.Add(MakeShared<FJsonValueObject>(SummaryJson(Summary)));
				UE_LOG(LogMageArena, Log, TEXT("LioCensus %s c=%.1f n=%d median=%.2f win=%.2f reach2=%.2f reach3=%.2f signature=%d/%d anyTempest=%.2f loosed=%d blinked=%d hit=%d missed=%d targets sig=%d median=%d win=%d"),
					*Summary.Set, Summary.Competence, Summary.N, Summary.MedianS, Summary.WinRate, Summary.ReachPhase2, Summary.ReachPhase3,
					Summary.SignatureInLast, Summary.ReachedLast, Summary.AnySunfall, Summary.Loosed, Summary.Blinked, Summary.Hit, Summary.Missed,
					Summary.bSignatureTarget ? 1 : 0, Summary.bMedianTarget ? 1 : 0, Summary.bWinTarget ? 1 : 0);
			}
		}
		LioRoot->SetArrayField(TEXT("sets"), LioValues);
		FString LioText;
		FJsonSerializer::Serialize(LioRoot, TJsonWriterFactory<>::Create(&LioText));
		FFileHelper::SaveStringToFile(LioText, *FPaths::Combine(LioDir, bOfficial ? TEXT("lio-summary.json") : TEXT("lio-probe-summary.json")));
		UE_LOG(LogMageArena, Log, TEXT("LioCensus done seeds=%d rows=%d wall=%.1fs official=%d"), Seeds, LioRows.Num(), LioWall, bOfficial ? 1 : 0);
	}

	// Structure only. The targets are measured and reported, not asserted: the owner moves the knobs.
	bool bPass = TestEqual(TEXT("row count"), Rows.Num(), Seeds * Sets.Num() * 2);
	bPass &= TestEqual(TEXT("lio row count"), LioRows.Num(), Seeds * 2 * 2);
	bool bLio = true;
	for (const FRivalRow& Row : LioRows)
	{
		bLio &= Row.Outcome != TEXT("start-failed") && Row.bRivalBound && std::isfinite(Row.TimeS);
	}
	bPass &= TestTrue(TEXT("every Lio bout started with Lio bound"), bLio);
	bool bStarted = true;
	bool bFinite = true;
	bool bBinding = true;
	for (const FRivalRow& Row : Rows)
	{
		bStarted &= Row.Outcome != TEXT("start-failed");
		bFinite &= std::isfinite(Row.TimeS) && std::isfinite(Row.HpFrac) && std::isfinite(Row.RivalHpFrac);
		const bool bOnSet = !Row.Set.EndsWith(TEXT("-off"));
		if (bOnSet != Row.bRivalBound || (!bOnSet && (Row.Phases != 0 || Row.bSignature)))
		{
			bBinding = false;
			AddError(FString::Printf(TEXT("%s c=%.1f seed %d: bound=%d phases=%d signature=%d"), *Row.Set, Row.Competence, Row.Seed,
				Row.bRivalBound ? 1 : 0, Row.Phases, Row.bSignature ? 1 : 0));
		}
	}
	bPass &= TestTrue(TEXT("every bout started"), bStarted);
	bPass &= TestTrue(TEXT("finite rows"), bFinite);
	bPass &= TestTrue(TEXT("Brennic is bound exactly on the phase sets"), bBinding);
	Instance->RemoveFromRoot();
	Instance->MarkAsGarbage();
	return bPass;
}

#endif
