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
#include "Kernel/KernelData.h"
#include "Kernel/SimMath.h"
#include "Kernel/VrRules.h"
#include "MageArenaVR.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Session/ArenaSession.h"

#include <cmath>

namespace
{
struct FCandidate
{
	const TCHAR* Name = TEXT("baseline");
	const TCHAR* WhyWave = TEXT("Pinned Wave 1 counts and spawn timing.");
	const TCHAR* WhyPressure = TEXT("Enemy cadence and damage stay at the pinned scale.");
	const TCHAR* WhyPower = TEXT("Bolt, line, and mana regen stay at the pinned scale.");
	const TCHAR* WhyDefence = TEXT("Live defence proposal: ward drain unchanged while split, one-hand 0.6, staff as in combat.vr.json, fire wall 4 m / 90 deg / 3 s.");
	int32 Conscript = -1;
	int32 Slinger = -1;
	double SpawnDelay = 0.0;
	double Cadence = 1.0;
	double EnemyDamage = 1.0;
	double FireMageDamage = 1.0;
	double Bolt = 1.0;
	double Line = 1.0;
	double ManaRegen = 1.0;
	double WardDrain = 1.0;
	double OneHand = 0.6;
	double StaffDuration = 6.0;
	double StaffUpFront = 8.0;
	double StaffDrain = 4.0;
	double StaffMagic = 0.85;
	double StaffPhysical = 0.6;
	double WallDistance = 4.0;
	double WallArc = 90.0;
	double WallDuration = 3.0;
	double WallMana = 12.0;
	double WallHeat = 4.0;
	double WallBurn = 12.0;
};

struct FPolicyRun
{
	const TCHAR* Name = TEXT("reference");
	FSeatedPolicy Policy;
};

struct FRow
{
	FString Candidate;
	FString Policy;
	FString Scenario;
	int32 Seed = 0;
	FString Outcome;
	double TimeS = 0.0;
	double HpLeft = 0.0;
	double HpFrac = 0.0;
	double Dealt = 0.0;
	double Taken = 0.0;
	int32 Splits = 0;
	int32 Plants = 0;
	int32 Walls = 0;
	int32 Perfects = 0;
	int32 SunfallCasts = 0;
	int32 SunfallLoosed = 0;
};

struct FScore
{
	FString Name;
	bool bPass = false;
	int32 Knobs = 0;
	int32 EnemyKnobs = 0;
	double Distance = 0.0;
	double Penalty = 0.0;
	double WaveWindow = 0.0;
	double WaveHp = 0.0;
	double WaveTime = 0.0;
	double Duel1Time = 0.0;
	double Duel15Time = 0.0;
	double Duel1Sun = 0.0;
	double Duel15Sun = 0.0;
	double Duel1Win = 0.0;
	double Duel15Win = 0.0;
};

FString CensusDir()
{
	FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../../.."), TEXT("runs/T12/census"));
	FPaths::CollapseRelativeDirectories(Path);
	IFileManager::Get().MakeDirectory(*Path, true);
	return Path;
}

FString ProposalPath()
{
	FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../data/vr/calibration-proposal.json"));
	FPaths::CollapseRelativeDirectories(Path);
	return Path;
}

bool Same(double Left, double Right)
{
	return std::abs(Left - Right) <= 1.0e-9;
}

int32 KnobDistance(const FCandidate& Candidate, int32& EnemyKnobs, double& Distance)
{
	EnemyKnobs = 0;
	Distance = 0.0;
	int32 Knobs = 0;
	auto Count = [&](double Value, double Identity, bool bEnemy, double Scale)
	{
		if (Same(Value, Identity))
		{
			return;
		}
		++Knobs;
		if (bEnemy)
		{
			++EnemyKnobs;
		}
		Distance += std::abs(Value - Identity) / Scale;
	};
	auto CountInt = [&](int32 Value, int32 Identity, bool bEnemy)
	{
		if (Value == Identity)
		{
			return;
		}
		++Knobs;
		if (bEnemy)
		{
			++EnemyKnobs;
		}
		Distance += std::abs(static_cast<double>(Value - Identity));
	};
	CountInt(Candidate.Conscript, -1, true);
	CountInt(Candidate.Slinger, -1, true);
	Count(Candidate.SpawnDelay, 0.0, true, 1.0);
	Count(Candidate.Cadence, 1.0, true, 1.0);
	Count(Candidate.EnemyDamage, 1.0, true, 1.0);
	Count(Candidate.FireMageDamage, 1.0, true, 1.0);
	Count(Candidate.Bolt, 1.0, false, 1.0);
	Count(Candidate.Line, 1.0, false, 1.0);
	Count(Candidate.ManaRegen, 1.0, false, 1.0);
	Count(Candidate.WardDrain, 1.0, false, 1.0);
	Count(Candidate.OneHand, 0.6, false, 1.0);
	Count(Candidate.StaffDuration, 6.0, false, 6.0);
	Count(Candidate.StaffUpFront, 8.0, false, 8.0);
	Count(Candidate.StaffDrain, 4.0, false, 4.0);
	Count(Candidate.StaffMagic, 0.85, false, 1.0);
	Count(Candidate.StaffPhysical, 0.6, false, 1.0);
	Count(Candidate.WallDistance, 4.0, false, 4.0);
	Count(Candidate.WallArc, 90.0, false, 90.0);
	Count(Candidate.WallDuration, 3.0, false, 3.0);
	Count(Candidate.WallMana, 12.0, false, 12.0);
	Count(Candidate.WallHeat, 4.0, false, 4.0);
	Count(Candidate.WallBurn, 12.0, false, 12.0);
	return Knobs;
}

void ApplyCandidate(FVrRuleset& Rules, const FCandidate& Candidate)
{
	Rules.ClearRuntime();
	Rules.Wave.ConscriptCount = Candidate.Conscript;
	Rules.Wave.SlingerCount = Candidate.Slinger;
	Rules.Wave.SpawnDelayS = Candidate.SpawnDelay;
	Rules.Pressure.AttackCadence = Candidate.Cadence;
	Rules.Pressure.EnemyDamage = Candidate.EnemyDamage;
	Rules.Pressure.FireMageDamage = Candidate.FireMageDamage;
	Rules.Power.BoltDamage = Candidate.Bolt;
	Rules.Power.LineDamage = Candidate.Line;
	Rules.Power.ManaRegen = Candidate.ManaRegen;
	Rules.Defence.WardDrainSplit = Candidate.WardDrain;
	Rules.Split.OneHandPower = Candidate.OneHand;
	Rules.Staff.DurationS = Candidate.StaffDuration;
	Rules.Staff.ManaUpFront = Candidate.StaffUpFront;
	Rules.Staff.DrainPerSecond = Candidate.StaffDrain;
	Rules.Staff.MagicReduction = Candidate.StaffMagic;
	Rules.Staff.PhysicalReduction = Candidate.StaffPhysical;
	Rules.FireWall.DistanceM = Candidate.WallDistance;
	Rules.FireWall.ArcDeg = Candidate.WallArc;
	Rules.FireWall.DurationS = Candidate.WallDuration;
	Rules.FireWall.ManaCost = Candidate.WallMana;
	Rules.FireWall.HeatPerStop = Candidate.WallHeat;
	Rules.FireWall.BurnDamage = Candidate.WallBurn;
}

double Median(TArray<double> Values)
{
	if (Values.Num() == 0)
	{
		return 0.0;
	}
	Values.Sort();
	const int32 Mid = Values.Num() / 2;
	if (Values.Num() % 2 == 0)
	{
		return 0.5 * (Values[Mid - 1] + Values[Mid]);
	}
	return Values[Mid];
}

double BandGap(double Value, double Lo, double Hi)
{
	if (Value < Lo)
	{
		return Lo - Value;
	}
	if (Value > Hi)
	{
		return Value - Hi;
	}
	return 0.0;
}

FScore ScoreOf(const FCandidate& Candidate, const TArray<FRow>& Rows)
{
	FScore Score;
	Score.Name = Candidate.Name;
	Score.Knobs = KnobDistance(Candidate, Score.EnemyKnobs, Score.Distance);
	TArray<double> WaveHp;
	TArray<double> WaveTime;
	TArray<double> Time1;
	TArray<double> Time15;
	int32 WaveN = 0;
	int32 WaveWin = 0;
	int32 N1 = 0;
	int32 N15 = 0;
	int32 Sun1 = 0;
	int32 Sun15 = 0;
	int32 Win1 = 0;
	int32 Win15 = 0;
	for (const FRow& Row : Rows)
	{
		if (Row.Candidate != Candidate.Name || Row.Policy != TEXT("reference"))
		{
			continue;
		}
		const bool bWin = Row.Outcome == TEXT("win");
		if (Row.Scenario == TEXT("wave1"))
		{
			++WaveN;
			WaveTime.Add(Row.TimeS);
			if (bWin)
			{
				WaveHp.Add(Row.HpFrac);
			}
			if (bWin && Row.TimeS >= 25.0 && Row.TimeS <= 40.0)
			{
				++WaveWin;
			}
		}
		else if (Row.Scenario == TEXT("duel-c1"))
		{
			++N1;
			Time1.Add(Row.TimeS);
			Sun1 += Row.SunfallCasts > 0 ? 1 : 0;
			Win1 += bWin ? 1 : 0;
		}
		else if (Row.Scenario == TEXT("duel-c15"))
		{
			++N15;
			Time15.Add(Row.TimeS);
			Sun15 += Row.SunfallCasts > 0 ? 1 : 0;
			Win15 += bWin ? 1 : 0;
		}
	}
	Score.WaveWindow = WaveN > 0 ? static_cast<double>(WaveWin) / static_cast<double>(WaveN) : 0.0;
	Score.WaveHp = Median(WaveHp);
	Score.WaveTime = Median(WaveTime);
	Score.Duel1Time = Median(Time1);
	Score.Duel15Time = Median(Time15);
	Score.Duel1Sun = N1 > 0 ? static_cast<double>(Sun1) / static_cast<double>(N1) : 0.0;
	Score.Duel15Sun = N15 > 0 ? static_cast<double>(Sun15) / static_cast<double>(N15) : 0.0;
	Score.Duel1Win = N1 > 0 ? static_cast<double>(Win1) / static_cast<double>(N1) : 0.0;
	Score.Duel15Win = N15 > 0 ? static_cast<double>(Win15) / static_cast<double>(N15) : 0.0;
	const bool bWave = Score.WaveWindow >= 0.80 && Score.WaveHp >= 0.20 && Score.WaveHp <= 0.60;
	const bool bDuel = Score.Duel1Time >= 45.0 && Score.Duel1Time <= 80.0 && Score.Duel15Time >= 45.0 && Score.Duel15Time <= 80.0
		&& Score.Duel1Sun >= 0.70 && Score.Duel15Sun >= 0.70 && Score.Duel1Win >= 0.70
		&& Score.Duel15Win >= 0.40 && Score.Duel15Win <= 0.60;
	Score.bPass = bWave && bDuel && WaveN > 0 && N1 > 0 && N15 > 0;
	Score.Penalty = 0.0;
	Score.Penalty += std::max(0.0, 0.80 - Score.WaveWindow) * 8.0;
	Score.Penalty += BandGap(Score.WaveHp, 0.20, 0.60) * 4.0;
	Score.Penalty += BandGap(Score.Duel1Time, 45.0, 80.0) / 15.0;
	Score.Penalty += BandGap(Score.Duel15Time, 45.0, 80.0) / 15.0;
	Score.Penalty += std::max(0.0, 0.70 - Score.Duel1Sun) * 4.0;
	Score.Penalty += std::max(0.0, 0.70 - Score.Duel15Sun) * 4.0;
	Score.Penalty += std::max(0.0, 0.70 - Score.Duel1Win) * 5.0;
	Score.Penalty += BandGap(Score.Duel15Win, 0.40, 0.60) * 4.0;
	Score.Penalty += static_cast<double>(Score.Knobs) * 0.04 + static_cast<double>(Score.EnemyKnobs) * 0.15 + Score.Distance * 0.05;
	return Score;
}

bool Better(const FScore& Left, const FScore& Right)
{
	if (Left.bPass != Right.bPass)
	{
		return Left.bPass;
	}
	if (Left.bPass)
	{
		if (Left.Knobs != Right.Knobs)
		{
			return Left.Knobs < Right.Knobs;
		}
		if (Left.EnemyKnobs != Right.EnemyKnobs)
		{
			return Left.EnemyKnobs < Right.EnemyKnobs;
		}
		return Left.Distance < Right.Distance;
	}
	return Left.Penalty < Right.Penalty;
}

FRow RunBout(FArenaSession& Session, const FVrRuleset& Rules, const FPolicyRun& Policy, const FCandidate& Candidate, const TCHAR* Scenario, int32 Wave, bool bFire, uint32 Seed, double Cap)
{
	Session.SetBout(Wave, bFire);
	Session.SetPolicy(Policy.Policy);
	Session.SetRulesOverride(Rules);
	Session.SetScripted(true);
	FRow Row;
	Row.Candidate = Candidate.Name;
	Row.Policy = Policy.Name;
	Row.Scenario = Scenario;
	Row.Seed = static_cast<int32>(Seed);
	if (!Session.Start(Seed))
	{
		Row.Outcome = TEXT("start-failed");
		return Row;
	}
	const double Dt = 1.0 / 72.0;
	int32 Guard = 0;
	while (Session.GetGames().Phase == TEXT("active") && Session.GetSimSeconds() < Cap && Guard < 8000)
	{
		Session.Advance(Dt, true);
		++Guard;
	}
	const FString Phase = Session.GetGames().Phase;
	if (Phase == TEXT("intermission") || Phase == TEXT("complete"))
	{
		Row.Outcome = TEXT("win");
	}
	else if (Phase == TEXT("lost"))
	{
		Row.Outcome = TEXT("loss");
	}
	else
	{
		Row.Outcome = TEXT("timeout");
	}
	const FActor* Player = SimFindActor(Session.GetGames().State, Session.GetGames().PlayerId);
	Row.TimeS = Session.GetSimSeconds();
	Row.HpLeft = Player ? Player->Hp : 0.0;
	Row.HpFrac = Player && Player->MaxHp > 0.0 ? Player->Hp / Player->MaxHp : 0.0;
	Row.Dealt = Player ? Player->Metrics.DamageDealt : 0.0;
	Row.Taken = Player ? Player->Metrics.DamageTaken : 0.0;
	Row.Splits = Session.GetSplitCasts();
	Row.Plants = Session.GetStaffPlants();
	Row.Walls = Session.GetWallsRaised();
	Row.Perfects = Player ? Player->Metrics.Perfects : 0;
	for (const FActor& Actor : Session.GetGames().State.Actors)
	{
		if (!Actor.Fire.bSchool || Actor.Team == 0)
		{
			continue;
		}
		if (Actor.Fire.SunfallCastTick > 0)
		{
			Row.SunfallCasts = 1;
		}
		if (Actor.Fire.bSunfallLoosed)
		{
			Row.SunfallLoosed = 1;
			Row.SunfallCasts = 1;
		}
	}
	return Row;
}

FString CsvEscape(const FString& Text)
{
	return Text;
}

void WriteCsv(const FString& Path, const TArray<FRow>& Rows)
{
	FString Body = TEXT("candidate,policy,scenario,seed,outcome,time_s,hp_left,hp_frac,damage_dealt,damage_taken,split_casts,plants,walls,perfects,sunfall_casts,sunfall_loosed\n");
	for (const FRow& Row : Rows)
	{
		Body += FString::Printf(TEXT("%s,%s,%s,%d,%s,%.4f,%.4f,%.4f,%.4f,%.4f,%d,%d,%d,%d,%d,%d\n"),
			*Row.Candidate, *Row.Policy, *Row.Scenario, Row.Seed, *Row.Outcome,
			Row.TimeS, Row.HpLeft, Row.HpFrac, Row.Dealt, Row.Taken,
			Row.Splits, Row.Plants, Row.Walls, Row.Perfects, Row.SunfallCasts, Row.SunfallLoosed);
	}
	FFileHelper::SaveStringToFile(Body, *Path);
}

TSharedRef<FJsonObject> ScoreJson(const FScore& Score)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("name"), Score.Name);
	Object->SetBoolField(TEXT("targetsMet"), Score.bPass);
	Object->SetNumberField(TEXT("knobs"), Score.Knobs);
	Object->SetNumberField(TEXT("enemyKnobs"), Score.EnemyKnobs);
	Object->SetNumberField(TEXT("distance"), Score.Distance);
	Object->SetNumberField(TEXT("penalty"), Score.Penalty);
	Object->SetNumberField(TEXT("waveWinWindow"), Score.WaveWindow);
	Object->SetNumberField(TEXT("waveHpMedian"), Score.WaveHp);
	Object->SetNumberField(TEXT("waveTimeMedian"), Score.WaveTime);
	Object->SetNumberField(TEXT("duelC1TimeMedian"), Score.Duel1Time);
	Object->SetNumberField(TEXT("duelC15TimeMedian"), Score.Duel15Time);
	Object->SetNumberField(TEXT("duelC1Sunfall"), Score.Duel1Sun);
	Object->SetNumberField(TEXT("duelC15Sunfall"), Score.Duel15Sun);
	Object->SetNumberField(TEXT("duelC1Win"), Score.Duel1Win);
	Object->SetNumberField(TEXT("duelC15Win"), Score.Duel15Win);
	return Object;
}

void WriteProposal(const FCandidate& Candidate, const FScore& Score, int32 Seeds)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), 1);
	Root->SetStringField(TEXT("_about"), TEXT("not live until the owner signs off; applied only with -MageArenaProposal"));
	TSharedRef<FJsonObject> Knobs = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> Wave = MakeShared<FJsonObject>();
	Wave->SetNumberField(TEXT("conscriptCount"), Candidate.Conscript);
	Wave->SetNumberField(TEXT("slingerCount"), Candidate.Slinger);
	Wave->SetNumberField(TEXT("spawnDelayS"), Candidate.SpawnDelay);
	Wave->SetStringField(TEXT("_why"), Candidate.WhyWave);
	TSharedRef<FJsonObject> Pressure = MakeShared<FJsonObject>();
	Pressure->SetNumberField(TEXT("attackCadence"), Candidate.Cadence);
	Pressure->SetNumberField(TEXT("enemyDamage"), Candidate.EnemyDamage);
	Pressure->SetNumberField(TEXT("fireMageDamage"), Candidate.FireMageDamage);
	Pressure->SetStringField(TEXT("_why"), Candidate.WhyPressure);
	TSharedRef<FJsonObject> Power = MakeShared<FJsonObject>();
	Power->SetNumberField(TEXT("boltDamage"), Candidate.Bolt);
	Power->SetNumberField(TEXT("lineDamage"), Candidate.Line);
	Power->SetNumberField(TEXT("manaRegen"), Candidate.ManaRegen);
	Power->SetStringField(TEXT("_why"), Candidate.WhyPower);
	TSharedRef<FJsonObject> Defence = MakeShared<FJsonObject>();
	Defence->SetNumberField(TEXT("wardDrainSplit"), Candidate.WardDrain);
	Defence->SetNumberField(TEXT("oneHandPower"), Candidate.OneHand);
	Defence->SetNumberField(TEXT("staffDurationS"), Candidate.StaffDuration);
	Defence->SetNumberField(TEXT("staffManaUpFront"), Candidate.StaffUpFront);
	Defence->SetNumberField(TEXT("staffDrainPerSecond"), Candidate.StaffDrain);
	Defence->SetNumberField(TEXT("staffMagicReduction"), Candidate.StaffMagic);
	Defence->SetNumberField(TEXT("staffPhysicalReduction"), Candidate.StaffPhysical);
	Defence->SetNumberField(TEXT("fireWallDistanceM"), Candidate.WallDistance);
	Defence->SetNumberField(TEXT("fireWallArcDeg"), Candidate.WallArc);
	Defence->SetNumberField(TEXT("fireWallDurationS"), Candidate.WallDuration);
	Defence->SetNumberField(TEXT("fireWallManaCost"), Candidate.WallMana);
	Defence->SetNumberField(TEXT("fireWallHeatPerStop"), Candidate.WallHeat);
	Defence->SetNumberField(TEXT("fireWallBurnDamage"), Candidate.WallBurn);
	Defence->SetStringField(TEXT("_why"), Candidate.WhyDefence);
	Knobs->SetObjectField(TEXT("wave"), Wave);
	Knobs->SetObjectField(TEXT("pressure"), Pressure);
	Knobs->SetObjectField(TEXT("power"), Power);
	Knobs->SetObjectField(TEXT("defence"), Defence);
	Root->SetObjectField(TEXT("knobs"), Knobs);
	TSharedRef<FJsonObject> Measured = MakeShared<FJsonObject>();
	Measured->SetNumberField(TEXT("seeds"), Seeds);
	Measured->SetBoolField(TEXT("targetsMet"), Score.bPass);
	Measured->SetNumberField(TEXT("waveWinWindow"), Score.WaveWindow);
	Measured->SetNumberField(TEXT("waveHpMedian"), Score.WaveHp);
	Measured->SetNumberField(TEXT("waveTimeMedian"), Score.WaveTime);
	Measured->SetNumberField(TEXT("duelC1TimeMedian"), Score.Duel1Time);
	Measured->SetNumberField(TEXT("duelC15TimeMedian"), Score.Duel15Time);
	Measured->SetNumberField(TEXT("duelC1Sunfall"), Score.Duel1Sun);
	Measured->SetNumberField(TEXT("duelC15Sunfall"), Score.Duel15Sun);
	Measured->SetNumberField(TEXT("duelC1Win"), Score.Duel1Win);
	Measured->SetNumberField(TEXT("duelC15Win"), Score.Duel15Win);
	Root->SetObjectField(TEXT("measured"), Measured);
	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	FJsonSerializer::Serialize(Root, Writer);
	FFileHelper::SaveStringToFile(Text, *ProposalPath());
}

TArray<FCandidate> Candidates()
{
	TArray<FCandidate> List;
	List.Add(FCandidate{});
	// Twelve seconds at 6 mana is the duel lever. Competence 1 reached about 56 s and saw Sunfall.
	// Wave 1 does not move, because soldiers raise no wall.
	FCandidate Shade;
	Shade.Name = TEXT("shade");
	Shade.WhyWave = TEXT("Wave 1 keeps the pinned four conscripts and two slingers. Spawn timing stays pinned.");
	Shade.WhyPressure = TEXT("Soldiers and the Fire mage keep their pinned damage and cadence.");
	Shade.WhyPower = TEXT("Bolt, line, and mana regen stay pinned. The duel lengthens because the curtain stays up, not because bolts hit harder.");
	Shade.WhyDefence = TEXT("The fire wall lasts 12 s and costs 6 mana. Distance, arc, heat, and burn stay at the live proposal. The dome and the split hands stay live.");
	Shade.WallDuration = 12.0;
	Shade.WallMana = 6.0;
	List.Add(Shade);
	// The one Wave 1 clear in the probes was a 10 s dome at 0.8 block with a second plant.
	// Fourteen seconds at 0.85 block is that clear, held a little longer so the other seeds are not caught in the gap.
	FCandidate OpenDome;
	OpenDome.Name = TEXT("opendome");
	OpenDome.WhyWave = TEXT("Wave 1 keeps the pinned four conscripts and two slingers. Spawn timing stays pinned.");
	OpenDome.WhyPressure = TEXT("Soldiers and the Fire mage keep their pinned damage and cadence.");
	OpenDome.WhyPower = TEXT("Mana regen 1.25 so the second plant can be paid. Bolt and line stay pinned.");
	OpenDome.WhyDefence = TEXT("The dome lasts 14 s, drains 1.5/s, and blocks 0.85 of a physical hit, so a 10-damage spear becomes 1.5 while it is planted. One-hand power is 1.0, so the one split cast hits at full strength. The fire wall stays at the live 3 s / 12 mana.");
	OpenDome.ManaRegen = 1.25;
	OpenDome.OneHand = 1.0;
	OpenDome.StaffDuration = 14.0;
	OpenDome.StaffDrain = 1.5;
	OpenDome.StaffPhysical = 0.85;
	List.Add(OpenDome);
	// Opendome clears Wave 1 with almost no health left, and the 90-degree curtain still lets the planted player
	// kill the mage around 35 s. A thicker dome is the wave lever. A wider arc is the duel lever: blinks put bolts
	// outside the live 90 degrees, so a longer curtain alone does not stretch the fight.
	FCandidate Wide = OpenDome;
	Wide.Name = TEXT("wide");
	Wide.WhyPower = TEXT("Mana regen 1.25 so the second plant can be paid. Bolt and line stay pinned.");
	Wide.WhyDefence = TEXT("The dome lasts 14 s, drains 1.5/s, and blocks 0.9 of a physical hit, so a 10-damage spear becomes 1 while it is planted. It blocks 0.92 of a magic hit. One-hand power is 1.0. The fire wall lasts 12 s, costs 6 mana, and covers 150 degrees so a blink does not step outside it.");
	Wide.StaffPhysical = 0.9;
	Wide.StaffMagic = 0.92;
	Wide.WallDuration = 12.0;
	Wide.WallMana = 6.0;
	Wide.WallArc = 150.0;
	List.Add(Wide);
	// DF-003 left ward drain at the live 27/s. The reference script pays that while it split-casts.
	// 0.5 and 0.25 of that drain, on the wide dome and wall, are the follow-up the finding named.
	FCandidate Half = Wide;
	Half.Name = TEXT("halfward");
	Half.WhyDefence = TEXT("wide's dome and wall, and the palm ward drains half of the live 27/s (13.5/s) while the other hand is casting.");
	Half.WardDrain = 0.5;
	List.Add(Half);
	FCandidate Quarter = Wide;
	Quarter.Name = TEXT("quarterward");
	Quarter.WhyDefence = TEXT("wide's dome and wall, and the palm ward drains a quarter of the live 27/s (6.75/s) while the other hand is casting.");
	Quarter.WardDrain = 0.25;
	List.Add(Quarter);
	return List;
}

TArray<FPolicyRun> ExtraPolicies()
{
	TArray<FPolicyRun> List;
	FPolicyRun Eager;
	Eager.Name = TEXT("eager-split");
	Eager.Policy.WardDropMana = 6.0;
	Eager.Policy.WardRaiseMana = 12.0;
	Eager.Policy.SplitSigilMana = 8.0;
	Eager.Policy.WardHoldS = 2.6;
	List.Add(Eager);
	FPolicyRun NoSplit;
	NoSplit.Name = TEXT("no-split");
	NoSplit.Policy.bSplit = false;
	List.Add(NoSplit);
	FPolicyRun NoPlant;
	NoPlant.Name = TEXT("no-plant");
	NoPlant.Policy.bPlant = false;
	List.Add(NoPlant);
	return List;
}

bool NearField(FAutomationTestBase& Test, const FJsonObject& Object, const TCHAR* Field, double Expected)
{
	double Value = 0.0;
	if (!Object.TryGetNumberField(Field, Value) || !(std::abs(Value - Expected) <= 1.0e-6))
	{
		Test.AddError(FString::Printf(TEXT("proposal field %s"), Field));
		return false;
	}
	return true;
}

bool WhyField(FAutomationTestBase& Test, const FJsonObject& Object, const TCHAR* Expected)
{
	FString Why;
	if (!Object.TryGetStringField(TEXT("_why"), Why) || Why != Expected)
	{
		Test.AddError(TEXT("proposal _why does not match the winner"));
		return false;
	}
	return true;
}

const FJsonObject* Child(FAutomationTestBase& Test, const FJsonObject& Object, const TCHAR* Field)
{
	const TSharedPtr<FJsonObject>* Found = nullptr;
	if (!Object.TryGetObjectField(Field, Found) || !Found || !Found->IsValid())
	{
		Test.AddError(FString::Printf(TEXT("proposal missing %s"), Field));
		return nullptr;
	}
	return Found->Get();
}

bool ProposalMatches(FAutomationTestBase& Test, const FJsonObject& Root, const FCandidate& Winner, const FScore& Score, int32 Seeds)
{
	const FJsonObject* Knobs = Child(Test, Root, TEXT("knobs"));
	const FJsonObject* Measured = Child(Test, Root, TEXT("measured"));
	if (!Knobs || !Measured)
	{
		return false;
	}
	const FJsonObject* Wave = Child(Test, *Knobs, TEXT("wave"));
	const FJsonObject* Pressure = Child(Test, *Knobs, TEXT("pressure"));
	const FJsonObject* Power = Child(Test, *Knobs, TEXT("power"));
	const FJsonObject* Defence = Child(Test, *Knobs, TEXT("defence"));
	if (!Wave || !Pressure || !Power || !Defence)
	{
		return false;
	}
	bool bPass = NearField(Test, *Wave, TEXT("conscriptCount"), static_cast<double>(Winner.Conscript));
	bPass &= NearField(Test, *Wave, TEXT("slingerCount"), static_cast<double>(Winner.Slinger));
	bPass &= NearField(Test, *Wave, TEXT("spawnDelayS"), Winner.SpawnDelay);
	bPass &= WhyField(Test, *Wave, Winner.WhyWave);
	bPass &= NearField(Test, *Pressure, TEXT("attackCadence"), Winner.Cadence);
	bPass &= NearField(Test, *Pressure, TEXT("enemyDamage"), Winner.EnemyDamage);
	bPass &= NearField(Test, *Pressure, TEXT("fireMageDamage"), Winner.FireMageDamage);
	bPass &= WhyField(Test, *Pressure, Winner.WhyPressure);
	bPass &= NearField(Test, *Power, TEXT("boltDamage"), Winner.Bolt);
	bPass &= NearField(Test, *Power, TEXT("lineDamage"), Winner.Line);
	bPass &= NearField(Test, *Power, TEXT("manaRegen"), Winner.ManaRegen);
	bPass &= WhyField(Test, *Power, Winner.WhyPower);
	bPass &= NearField(Test, *Defence, TEXT("wardDrainSplit"), Winner.WardDrain);
	bPass &= NearField(Test, *Defence, TEXT("oneHandPower"), Winner.OneHand);
	bPass &= NearField(Test, *Defence, TEXT("staffDurationS"), Winner.StaffDuration);
	bPass &= NearField(Test, *Defence, TEXT("staffManaUpFront"), Winner.StaffUpFront);
	bPass &= NearField(Test, *Defence, TEXT("staffDrainPerSecond"), Winner.StaffDrain);
	bPass &= NearField(Test, *Defence, TEXT("staffMagicReduction"), Winner.StaffMagic);
	bPass &= NearField(Test, *Defence, TEXT("staffPhysicalReduction"), Winner.StaffPhysical);
	bPass &= NearField(Test, *Defence, TEXT("fireWallDistanceM"), Winner.WallDistance);
	bPass &= NearField(Test, *Defence, TEXT("fireWallArcDeg"), Winner.WallArc);
	bPass &= NearField(Test, *Defence, TEXT("fireWallDurationS"), Winner.WallDuration);
	bPass &= NearField(Test, *Defence, TEXT("fireWallManaCost"), Winner.WallMana);
	bPass &= NearField(Test, *Defence, TEXT("fireWallHeatPerStop"), Winner.WallHeat);
	bPass &= NearField(Test, *Defence, TEXT("fireWallBurnDamage"), Winner.WallBurn);
	bPass &= WhyField(Test, *Defence, Winner.WhyDefence);
	bPass &= NearField(Test, *Measured, TEXT("seeds"), static_cast<double>(Seeds));
	bPass &= NearField(Test, *Measured, TEXT("waveWinWindow"), Score.WaveWindow);
	bPass &= NearField(Test, *Measured, TEXT("waveHpMedian"), Score.WaveHp);
	bPass &= NearField(Test, *Measured, TEXT("waveTimeMedian"), Score.WaveTime);
	bPass &= NearField(Test, *Measured, TEXT("duelC1TimeMedian"), Score.Duel1Time);
	bPass &= NearField(Test, *Measured, TEXT("duelC15TimeMedian"), Score.Duel15Time);
	bPass &= NearField(Test, *Measured, TEXT("duelC1Sunfall"), Score.Duel1Sun);
	bPass &= NearField(Test, *Measured, TEXT("duelC15Sunfall"), Score.Duel15Sun);
	bPass &= NearField(Test, *Measured, TEXT("duelC1Win"), Score.Duel1Win);
	bPass &= NearField(Test, *Measured, TEXT("duelC15Win"), Score.Duel15Win);
	bool bTargets = false;
	if (!Measured->TryGetBoolField(TEXT("targetsMet"), bTargets) || bTargets != Score.bPass)
	{
		Test.AddError(TEXT("proposal targetsMet does not match the winner"));
		bPass = false;
	}
	return bPass;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCensusSweep, "MageArenaDesign.Census.Sweep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCensusSweep::RunTest(const FString& Parameters)
{
	if (!KernelData().bReady)
	{
		AddError(KernelData().Error.IsEmpty() ? TEXT("kernel data failed to load") : KernelData().Error);
		return false;
	}
	FString RulesError;
	FVrRuleset Base;
	if (!LoadVrRuleset(Base, RulesError, false))
	{
		AddError(RulesError);
		return false;
	}
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
	const TArray<FCandidate> KnobSets = Candidates();
	const TArray<FPolicyRun> Policies = ExtraPolicies();
	FPolicyRun Reference;
	TArray<FRow> Rows;
	const double Started = FPlatformTime::Seconds();

	auto Sweep = [&](const FCandidate& Candidate, const FPolicyRun& Policy)
	{
		FVrRuleset Rules = Base;
		ApplyCandidate(Rules, Candidate);
		for (int32 Seed = 1; Seed <= Seeds; ++Seed)
		{
			Rows.Add(RunBout(Session, Rules, Policy, Candidate, TEXT("wave1"), 0, false, static_cast<uint32>(Seed), 50.0));
			Rows.Add(RunBout(Session, Rules, Policy, Candidate, TEXT("duel-c1"), 2, true, static_cast<uint32>(Seed), 95.0));
			Rows.Add(RunBout(Session, Rules, Policy, Candidate, TEXT("duel-c15"), 3, true, static_cast<uint32>(Seed), 95.0));
		}
	};

	for (const FCandidate& Candidate : KnobSets)
	{
		Sweep(Candidate, Reference);
		const FScore Partial = ScoreOf(Candidate, Rows);
		UE_LOG(LogMageArena, Log, TEXT("Census %s pass=%d wave=%.2f hp=%.2f c1 t=%.1f sun=%.2f win=%.2f c15 t=%.1f sun=%.2f win=%.2f knobs=%d"),
			*Partial.Name, Partial.bPass ? 1 : 0, Partial.WaveWindow, Partial.WaveHp,
			Partial.Duel1Time, Partial.Duel1Sun, Partial.Duel1Win,
			Partial.Duel15Time, Partial.Duel15Sun, Partial.Duel15Win, Partial.Knobs);
	}

	TArray<FScore> Scores;
	for (const FCandidate& Candidate : KnobSets)
	{
		Scores.Add(ScoreOf(Candidate, Rows));
	}
	Scores.Sort([](const FScore& Left, const FScore& Right) { return Better(Left, Right); });
	const FString WinnerName = Scores.Num() > 0 ? Scores[0].Name : TEXT("baseline");
	for (const FPolicyRun& Policy : Policies)
	{
		for (const FCandidate& Candidate : KnobSets)
		{
			if (FCString::Strcmp(Candidate.Name, TEXT("baseline")) == 0 || Candidate.Name == WinnerName)
			{
				Sweep(Candidate, Policy);
			}
		}
	}
	const double WallSeconds = FPlatformTime::Seconds() - Started;

	const FString Dir = CensusDir();
	const bool bOfficial = Seeds >= 20;
	WriteCsv(FPaths::Combine(Dir, bOfficial ? TEXT("sweep.csv") : TEXT("probe.csv")), Rows);

	TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
	Summary->SetNumberField(TEXT("seeds"), Seeds);
	Summary->SetNumberField(TEXT("wallSeconds"), WallSeconds);
	Summary->SetStringField(TEXT("winner"), WinnerName);
	Summary->SetBoolField(TEXT("official"), bOfficial);
	TArray<TSharedPtr<FJsonValue>> ScoreValues;
	for (const FScore& Score : Scores)
	{
		ScoreValues.Add(MakeShared<FJsonValueObject>(ScoreJson(Score)));
	}
	Summary->SetArrayField(TEXT("candidates"), ScoreValues);
	TMap<FString, TMap<FString, int32>> PolicyWins;
	for (const FRow& Row : Rows)
	{
		if (Row.Scenario == TEXT("wave1") && Row.Outcome == TEXT("win") && Row.TimeS >= 25.0 && Row.TimeS <= 40.0)
		{
			PolicyWins.FindOrAdd(Row.Candidate).FindOrAdd(Row.Policy)++;
		}
	}
	TSharedRef<FJsonObject> Sensitivity = MakeShared<FJsonObject>();
	for (const TPair<FString, TMap<FString, int32>>& CandidateWins : PolicyWins)
	{
		TSharedRef<FJsonObject> ByPolicy = MakeShared<FJsonObject>();
		for (const TPair<FString, int32>& Pair : CandidateWins.Value)
		{
			ByPolicy->SetNumberField(Pair.Key, static_cast<double>(Pair.Value) / static_cast<double>(Seeds));
		}
		Sensitivity->SetObjectField(CandidateWins.Key, ByPolicy);
	}
	Summary->SetObjectField(TEXT("waveWindowByPolicy"), Sensitivity);
	FString SummaryText;
	const TSharedRef<TJsonWriter<>> SummaryWriter = TJsonWriterFactory<>::Create(&SummaryText);
	FJsonSerializer::Serialize(Summary, SummaryWriter);
	FFileHelper::SaveStringToFile(SummaryText, *FPaths::Combine(Dir, bOfficial ? TEXT("summary.json") : TEXT("probe-summary.json")));

	if (bOfficial)
	{
		const FCandidate* Winner = &KnobSets[0];
		for (const FCandidate& Candidate : KnobSets)
		{
			if (Candidate.Name == WinnerName)
			{
				Winner = &Candidate;
				break;
			}
		}
		WriteProposal(*Winner, Scores[0], Seeds);
	}

	UE_LOG(LogMageArena, Log, TEXT("Census done seeds=%d rows=%d wall=%.1fs winner=%s official=%d"),
		Seeds, Rows.Num(), WallSeconds, *WinnerName, bOfficial ? 1 : 0);
	Session.Unbind();
	Instance->RemoveFromRoot();
	Instance->MarkAsGarbage();

	bool bPass = TestTrue(TEXT("winner is the first score"), Scores.Num() > 0 && Scores[0].Name == WinnerName);
	const bool bPolicyOnTwo = WinnerName != TEXT("baseline");
	const int32 PolicyTargets = bPolicyOnTwo ? 2 : 1;
	const int32 Bouts = 3;
	const int32 ExpectedRows = Seeds * Bouts * (KnobSets.Num() + Policies.Num() * PolicyTargets);
	bPass &= TestEqual(TEXT("row count"), Rows.Num(), ExpectedRows);

	TMap<FString, TSet<int32>> Groups;
	bool bDuplicate = false;
	bool bBadNumber = false;
	TSet<FString> SeenCandidates;
	for (const FRow& Row : Rows)
	{
		SeenCandidates.Add(Row.Candidate);
		if (!std::isfinite(Row.TimeS) || !std::isfinite(Row.HpLeft) || !std::isfinite(Row.HpFrac)
			|| !std::isfinite(Row.Dealt) || !std::isfinite(Row.Taken))
		{
			bBadNumber = true;
		}
		const FString Key = FString::Printf(TEXT("%s|%s|%s"), *Row.Candidate, *Row.Policy, *Row.Scenario);
		TSet<int32>& Seen = Groups.FindOrAdd(Key);
		if (Seen.Contains(Row.Seed))
		{
			bDuplicate = true;
		}
		Seen.Add(Row.Seed);
	}
	bPass &= TestFalse(TEXT("duplicate seed"), bDuplicate);
	bPass &= TestFalse(TEXT("non-finite row"), bBadNumber);
	bool bSeedSets = true;
	for (const TPair<FString, TSet<int32>>& Pair : Groups)
	{
		if (Pair.Value.Num() != Seeds)
		{
			bSeedSets = false;
			AddError(FString::Printf(TEXT("seed set %s has %d seeds"), *Pair.Key, Pair.Value.Num()));
			continue;
		}
		for (int32 Seed = 1; Seed <= Seeds; ++Seed)
		{
			if (!Pair.Value.Contains(Seed))
			{
				bSeedSets = false;
				AddError(FString::Printf(TEXT("seed set %s is missing %d"), *Pair.Key, Seed));
				break;
			}
		}
	}
	bPass &= TestTrue(TEXT("seeds are distinct and cover every bout"), bSeedSets);
	bool bEveryCandidate = true;
	for (const FCandidate& Candidate : KnobSets)
	{
		if (!SeenCandidates.Contains(Candidate.Name))
		{
			bEveryCandidate = false;
			AddError(FString::Printf(TEXT("candidate %s produced no rows"), Candidate.Name));
		}
	}
	bPass &= TestTrue(TEXT("every candidate produced rows"), bEveryCandidate);

	bool bFiniteScores = true;
	for (const FScore& Score : Scores)
	{
		const bool bFinite = std::isfinite(Score.WaveWindow) && std::isfinite(Score.WaveHp) && std::isfinite(Score.WaveTime)
			&& std::isfinite(Score.Duel1Time) && std::isfinite(Score.Duel15Time)
			&& std::isfinite(Score.Duel1Sun) && std::isfinite(Score.Duel15Sun)
			&& std::isfinite(Score.Duel1Win) && std::isfinite(Score.Duel15Win)
			&& std::isfinite(Score.Penalty) && std::isfinite(Score.Distance);
		if (!bFinite)
		{
			bFiniteScores = false;
			AddError(FString::Printf(TEXT("non-finite median for %s"), *Score.Name));
		}
	}
	bPass &= TestTrue(TEXT("no NaN medians"), bFiniteScores);

	if (bOfficial && Scores.Num() > 0)
	{
		const FCandidate* Winner = nullptr;
		for (const FCandidate& Candidate : KnobSets)
		{
			if (Candidate.Name == WinnerName)
			{
				Winner = &Candidate;
				break;
			}
		}
		bPass &= TestNotNull(TEXT("winner candidate"), Winner);
		FString ProposalText;
		TSharedPtr<FJsonObject> Proposal;
		if (!Winner || !FFileHelper::LoadFileToString(ProposalText, *ProposalPath())
			|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ProposalText), Proposal)
			|| !Proposal.IsValid())
		{
			AddError(TEXT("written proposal could not be read"));
			bPass = false;
		}
		else
		{
			bPass &= ProposalMatches(*this, *Proposal, *Winner, Scores[0], Seeds);
		}
		FString SummaryOnDiskText;
		TSharedPtr<FJsonObject> SummaryOnDisk;
		if (!FFileHelper::LoadFileToString(SummaryOnDiskText, *FPaths::Combine(Dir, TEXT("summary.json")))
			|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(SummaryOnDiskText), SummaryOnDisk)
			|| !SummaryOnDisk.IsValid())
		{
			AddError(TEXT("written summary could not be read"));
			bPass = false;
		}
		else
		{
			FString WrittenWinner;
			if (!SummaryOnDisk->TryGetStringField(TEXT("winner"), WrittenWinner) || WrittenWinner != WinnerName)
			{
				AddError(TEXT("written summary winner does not match the reported winner"));
				bPass = false;
			}
		}
	}
	return bPass;
}

#endif
