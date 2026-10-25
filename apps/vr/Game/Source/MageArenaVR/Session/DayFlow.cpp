// T21: the Tiro day around the bouts. Prologue (first launch), Septima's collar ritual (every launch), an intro line
// before each bout, the arena flags after each bout, and the aftermath tablet after the final. The phase order is
// documented on FArenaSession::BeginArc.

#include "Session/ArenaSession.h"

#include "Dom/JsonObject.h"
#include "Gestures/BlinkDetector.h"
#include "Gestures/SigilRecognizerSubsystem.h"
#include "Gestures/StaffDetector.h"
#include "Gestures/WardDetector.h"
#include "HAL/FileManager.h"
#include "Hands/HandInputSubsystem.h"
#include "Kernel/ArenaKernel.h"
#include "Kernel/KernelData.h"
#include "MageArenaVR.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
constexpr int32 CentreStone = 1;
// T25 card: no choice in 5 s spares the rival, so the default never blocks the day.
constexpr double MissioTimeoutS = 5.0;

const FArenaTier* Tiro()
{
	const FKernelData& Data = KernelData();
	return Data.bReady && Data.ArenaTiers.Num() > 0 ? &Data.ArenaTiers[0] : nullptr;
}
}

bool FArenaSession::IsArcPhase(const FString& Phase)
{
	if (Phase == TEXT("missio"))
	{
		return true;
	}
	return Phase == TEXT("cold") || Phase == TEXT("prologue") || Phase == TEXT("offer") || Phase == TEXT("teach")
		|| Phase == TEXT("ritual") || Phase == TEXT("intro") || Phase == TEXT("intermission") || Phase == TEXT("lost")
		|| Phase == TEXT("aftermath") || Phase == TEXT("closed");
}

bool FArenaSession::LoadDayData()
{
	FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../data/vr/day.json"));
	FPaths::CollapseRelativeDirectories(Path);
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root)
		|| !Root.IsValid())
	{
		UE_LOG(LogMageArena, Error, TEXT("Day data missing or not JSON: %s"), *Path);
		return false;
	}
	double Version = 0.0;
	if (!Root->TryGetNumberField(TEXT("version"), Version) || Version != 1.0)
	{
		UE_LOG(LogMageArena, Error, TEXT("day.json version must be 1 (%s)"), *Path);
		return false;
	}
	FDayTuning Next;
	struct FField
	{
		const TCHAR* Key;
		double* Out;
	};
	const FField Fields[] = {
	    {TEXT("prologueMaxS"), &Next.PrologueMaxS},
	    {TEXT("ritualTimeoutS"), &Next.RitualTimeoutS},
	    {TEXT("aftermathTabletS"), &Next.AftermathTabletS},
	    {TEXT("closingCardS"), &Next.ClosingCardS},
	};
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Root->Values)
	{
		bool bKnown = Pair.Key.StartsWith(TEXT("_")) || Pair.Key == TEXT("version") || Pair.Key == TEXT("introS") || Pair.Key == TEXT("ritualLineS");
		for (const FField& Field : Fields)
		{
			bKnown |= Pair.Key == Field.Key;
		}
		if (!bKnown)
		{
			UE_LOG(LogMageArena, Error, TEXT("day.json has an unknown key %s"), *Pair.Key);
			return false;
		}
	}
	for (const FField& Field : Fields)
	{
		double Value = 0.0;
		if (!Root->TryGetNumberField(Field.Key, Value) || !(Value > 0.0 && Value <= 120.0))
		{
			UE_LOG(LogMageArena, Error, TEXT("day.json %s must be a number in (0, 120]"), Field.Key);
			return false;
		}
		*Field.Out = Value;
	}
	// A number, or an array of 1-8 numbers, each in (0, 120].
	auto ReadSeconds = [&Root](const TCHAR* Key, TArray<double>& Out)
	{
		Out.Reset();
		const TSharedPtr<FJsonValue> Field = Root->TryGetField(Key);
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (Field.IsValid() && Field->Type == EJson::Number)
		{
			Out.Add(Field->AsNumber());
		}
		else if (Root->TryGetArrayField(Key, Values) && Values)
		{
			for (const TSharedPtr<FJsonValue>& Value : *Values)
			{
				Out.Add(Value.IsValid() && Value->Type == EJson::Number ? Value->AsNumber() : -1.0);
			}
		}
		bool bOk = Out.Num() > 0 && Out.Num() <= 8;
		for (const double Seconds : Out)
		{
			bOk &= Seconds > 0.0 && Seconds <= 120.0;
		}
		if (!bOk)
		{
			UE_LOG(LogMageArena, Error, TEXT("day.json %s must be a number or an array of 1-8 numbers, each in (0, 120]"), Key);
		}
		return bOk;
	};
	// T28: introS is one number per Tiro wave index (C2 section 4: [8, 8, 12, 12]); a single number still means every bout.
	// T27: ritualLineS is one dwell per ritual line (C2 section 5: [12, 14, 12]).
	if (!ReadSeconds(TEXT("introS"), Next.IntroS) || !ReadSeconds(TEXT("ritualLineS"), Next.RitualLineS))
	{
		return false;
	}
	// Every ritual line but the last must start before the timeout continues the day.
	if (Next.RitualNaturalS() - Next.RitualLineS.Last() >= Next.RitualTimeoutS)
	{
		UE_LOG(LogMageArena, Error, TEXT("day.json ritualLineS: every line but the last must start before ritualTimeoutS"));
		return false;
	}
	DayTuning = Next;
	return true;
}

FString FArenaSession::CollarFilePath() const
{
	return FPaths::Combine(FPaths::GetPath(SaveFilePath()), TEXT("collar.ledger.json"));
}

bool FArenaSession::LoadCollarFile(const TCHAR* Context)
{
	FCollarWeights Weights;
	FString Error;
	if (!LoadCollarWeights(Weights, Error))
	{
		UE_LOG(LogMageArena, Error, TEXT("%s: %s"), Context, *Error);
		return false;
	}
	Collar.SetWeights(Weights);
	return true;
}

int32 FArenaSession::DayCollar(const TCHAR* Field, int32 BoutValue) const
{
	if (!bDay)
	{
		return BoutValue;
	}
	int32 Sum = 0;
	if (const FArenaTier* Tier = Tiro())
	{
		for (const FArenaWave& Wave : Tier->Waves)
		{
			double Value = 0.0;
			if (Flags.GetNumber(FArenaFlags::TiroKey(Wave.N, Field), Value))
			{
				Sum += static_cast<int32>(Value);
			}
		}
	}
	// The bout in progress is not in the flags until it ends.
	if (Games.Phase == TEXT("active") && !bRecorded)
	{
		Sum += BoutValue;
	}
	return Sum;
}

int32 FArenaSession::GetDayCracks() const
{
	return DayCollar(TEXT("cracks"), Collar.GetBoutCracks());
}

int32 FArenaSession::GetDayTithe() const
{
	return DayCollar(TEXT("tithe"), Collar.GetBoutTithe());
}

FString FArenaSession::FlagsFilePath() const
{
	return FPaths::Combine(FPaths::GetPath(SaveFilePath()), TEXT("arena-flags.json"));
}

void FArenaSession::SaveFlags() const
{
	if (!Flags.Save(FlagsFilePath()))
	{
		UE_LOG(LogMageArena, Error, TEXT("Could not save the arena flags to %s"), *FlagsFilePath());
	}
}

FString FArenaSession::DaySchool(int32 WaveIndex) const
{
	if (!DayFire.IsValidIndex(WaveIndex) || !DayFire[WaveIndex])
	{
		return TEXT("water");
	}
	// T23 merge: an Air final rides on the school-mage switch (see Start), so the last wave reports tiroFinal.school.
	return WaveIndex == DayFire.Num() - 1 && GetVrRules() && GetVrRules()->TiroFinal.School == TEXT("air") ? TEXT("air") : TEXT("fire");
}

double FArenaSession::BoutSeconds() const
{
	return static_cast<double>(Games.State.Tick - BoutStartTick) * SimDt();
}

void FArenaSession::ResetBoutScript()
{
	bAbsorb = false;
	bSuppressWard = false;
	bBlinkQueued = false;
	bCastPulse = false;
	bPlantPulse = false;
	bLiftPulse = false;
	bBoltFlicked = false;
	BoltFlickSim = -1.0;
	bLastCastSigil = false;
	bWardStarted = false;
	bWardReleased = false;
	bEmberWard = false;
	EmberWardUntil = 0.0;
	bTideStarted = false;
	SideToggle = 0;
	bWingSeat = Games.bFireMages;
	bWingSeated = false;
	SunfallPad = -1;
	bLoggedEnd = false;
	bRecorded = false;
	BoutStartTick = Games.State.Tick;
	StaffPlantsAtBout = StaffPlants;
	const FActor* Player = SimFindActor(Games.State, Games.PlayerId);
	BoutPerfectsStart = Player ? Player->Metrics.Perfects : 0;
	Collar.BeginBout(Player ? Player->Water.Crests : 0);
	if (Hands)
	{
		Hands->StopAll();
	}
	if (Wards)
	{
		Wards->GetDetector().ResetStream();
	}
	if (Staff)
	{
		Staff->GetDetector().Reset();
	}
	if (Sigils)
	{
		Sigils->ResetStrokes();
	}
	// Each bout starts on the centre pad, as Start() does.
	ActivePad = 1;
	bCameraDirty = true;
	CameraPad = 1;
	if (Blinks)
	{
		Blinks->ResetDetector();
		Blinks->SetActivePad(1);
	}
	if (FActor* Seated = SimFindActor(Games.State, Games.PlayerId))
	{
		SeatOnActivePad(*Seated);
	}
}

void FArenaSession::EnterPrologue()
{
	Games.Phase = TEXT("prologue");
	PhaseClock = 0.0;
	StoryHold = 0.0;
	StoryHoldStone = -1;
	bStoryCue = false;
	if (Hands)
	{
		Hands->StopAll();
	}
	Note(TEXT("day prologue"));
}

void FArenaSession::EnterRitual()
{
	StartOverPhase.Reset();
	ClearThreats();
	if (DummyId != 0)
	{
		Games.State.Actors.RemoveAll([this](const FActor& Actor) { return Actor.Id == DummyId; });
		DummyId = 0;
	}
	if (Hands)
	{
		Hands->StopAll();
	}
	if (Wards)
	{
		Wards->GetDetector().ResetStream();
	}
	bAbsorb = false;
	bOfferContinue = false;
	OfferHold = 0.0;
	TeachStep = ETeachStep::None;
	Games.Phase = TEXT("ritual");
	PhaseClock = 0.0;
	StoryHold = 0.0;
	StoryHoldStone = -1;
	bStoryCue = false;
	Note(FString::Printf(TEXT("day ritual bout=%d"), BoutWave + 1));
}

void FArenaSession::StartDayBout(int32 Wave)
{
	// A finished day's flags give way to the next day's when that day's first bout begins.
	bool bClosed = false;
	if (Wave == 0 && Flags.GetBool(TEXT("arena.day.closed"), bClosed) && bClosed)
	{
		Flags.Reset();
		SaveFlags();
		Note(TEXT("day flags reset for a new day"));
	}
	BoutWave = Wave;
	bKeepChain = true;
	if (!Start(BoutSeed))
	{
		UE_LOG(LogMageArena, Error, TEXT("Day bout %d failed to start"), Wave + 1);
		bRunning = false;
		return;
	}
	EnterIntro();
}

void FArenaSession::EnterIntro()
{
	StartOverPhase.Reset();
	Games.Phase = TEXT("intro");
	PhaseClock = 0.0;
	StoryHold = 0.0;
	StoryHoldStone = -1;
	bStoryCue = false;
	const FActor* Opponent = nullptr;
	for (const FActor& Actor : Games.State.Actors)
	{
		if (Actor.Id != Games.PlayerId && !Actor.Enemy.IsSet() && !Actor.bDummy)
		{
			Opponent = &Actor;
		}
	}
	const FVrRuleset* Rules = GetVrRules();
	const FVrRival* Rival = Rules ? Rules->BoundRival() : nullptr;
	Note(FString::Printf(TEXT("day intro bout=%d school=%s rival=%s competence=%s"), Games.Wave + 1, *DaySchool(Games.Wave),
		Rival ? *Rival->Id : TEXT("-"),
		Opponent && Opponent->MageAI.IsSet() ? *FString::Printf(TEXT("%.2f"), Opponent->MageAI->Competence) : TEXT("-")));
}

void FArenaSession::RecordBout(bool bWon)
{
	if (!bDay || bRecorded)
	{
		return;
	}
	bRecorded = true;
	const FArenaTier* Tier = Tiro();
	const int32 WaveN = Tier && Tier->Waves.IsValidIndex(Games.Wave) ? Tier->Waves[Games.Wave].N : Games.Wave + 1;
	const FActor* Player = SimFindActor(Games.State, Games.PlayerId);
	const int32 Perfects = Player ? Player->Metrics.Perfects - BoutPerfectsStart : 0;
	const double Seconds = BoutSeconds();
	double Attempts = 0.0;
	Flags.GetNumber(FArenaFlags::TiroKey(WaveN, TEXT("attempts")), Attempts);
	bool bWonBefore = false;
	Flags.GetBool(FArenaFlags::TiroKey(WaveN, TEXT("won")), bWonBefore);
	// won stays true once the bout was won this day; perfects and time are the attempt that just ended.
	Flags.SetBool(FArenaFlags::TiroKey(WaveN, TEXT("won")), bWon || bWonBefore);
	Flags.SetNumber(FArenaFlags::TiroKey(WaveN, TEXT("attempts")), Attempts + 1.0);
	Flags.SetNumber(FArenaFlags::TiroKey(WaveN, TEXT("perfects")), static_cast<double>(Perfects));
	Flags.SetNumber(FArenaFlags::TiroKey(WaveN, TEXT("time")), Seconds);
	// T22: cracks and tithe add up over every attempt of the bout this day (a lost attempt still cracked the collar and
	// still spilled magic); the day's totals are the sum over the bouts.
	double DayCracks = 0.0;
	double DayTithe = 0.0;
	Flags.GetNumber(FArenaFlags::TiroKey(WaveN, TEXT("cracks")), DayCracks);
	Flags.GetNumber(FArenaFlags::TiroKey(WaveN, TEXT("tithe")), DayTithe);
	Flags.SetNumber(FArenaFlags::TiroKey(WaveN, TEXT("cracks")), DayCracks + Collar.GetBoutCracks());
	Flags.SetNumber(FArenaFlags::TiroKey(WaveN, TEXT("tithe")), DayTithe + Collar.GetBoutTithe());
	SaveFlags();
	Collar.CommitBout();
	if (!Collar.SaveLifetime(CollarFilePath()))
	{
		UE_LOG(LogMageArena, Error, TEXT("Could not save the collar ledger to %s"), *CollarFilePath());
	}
	Note(FString::Printf(TEXT("day bout=%d %s t=%.2f attempts=%.0f perfects=%d"), WaveN, bWon ? TEXT("won") : TEXT("lost"), Seconds,
		Attempts + 1.0, Perfects));
	Note(FString::Printf(TEXT("day collar bout=%d cracks=%d tithe=%d lifetimeCracks=%d lifetimeTithe=%d"), WaveN, Collar.GetBoutCracks(),
		Collar.GetBoutTithe(), Collar.GetLifetimeCracks(), Collar.GetLifetimeTithe()));
}

void FArenaSession::EnterMissio()
{
	MissioNext = Games.Phase;
	Games.Phase = TEXT("missio");
	PhaseClock = 0.0;
	MissioHold = 0.0;
	bMissioPalmArmed[0] = false;
	bMissioPalmArmed[1] = false;
	if (Hands)
	{
		Hands->StopAll();
	}
	Fist.Reset();
	const FVrRival* Rival = GetVrRules() ? GetVrRules()->BoundRival() : nullptr;
	Note(FString::Printf(TEXT("day missio open bout=%d rival=%s"), Games.Wave + 1, Rival ? *Rival->Id : TEXT("-")));
}

bool FArenaSession::MissioPalmRaised(EControllerHand Hand) const
{
	FHandFrame Frame;
	if (!Hands || !Hands->GetLatest(Hand, Frame))
	{
		return false;
	}
	if (!FFistDetector::IsOpenHand(Frame) || FWardDetector::PalmHeightMetres(Frame) < FWardThresholds::RaiseHeightM)
	{
		return false;
	}
	const FVector Forward(1.0, 0.0, 0.0);
	return FWardDetector::AngleToFacingDeg(FWardDetector::PalmNormal(Frame), Forward) <= FWardThresholds::RaiseAngleDeg;
}

void FArenaSession::ResolveMissio(bool bFinish, const TCHAR* How)
{
	const FArenaTier* Tier = Tiro();
	const int32 WaveN = Tier && Tier->Waves.IsValidIndex(Games.Wave) ? Tier->Waves[Games.Wave].N : Games.Wave + 1;
	Flags.SetBool(FArenaFlags::TiroKey(WaveN, TEXT("finish")), bFinish);
	// arena.missio.total is the rivals granted missio (spared) this day, counted from the .finish flags so a bout
	// replayed after a quit is not counted twice.
	int32 Spared = 0;
	if (Tier)
	{
		for (const FArenaWave& Wave : Tier->Waves)
		{
			bool bFinished = false;
			Spared += Flags.GetBool(FArenaFlags::TiroKey(Wave.N, TEXT("finish")), bFinished) && !bFinished ? 1 : 0;
		}
	}
	Flags.SetNumber(TEXT("arena.missio.total"), static_cast<double>(Spared));
	SaveFlags();
	const TCHAR* Choice = bFinish ? TEXT("finish") : TEXT("spare");
	Note(FString::Printf(TEXT("day missio bout=%d %s %s t=%.2f total=%d"), WaveN, Choice, How, PhaseClock, Spared));
	MissioHold = 0.0;
	Games.Phase = MissioNext;
	MissioNext.Reset();
	if (Games.Phase == TEXT("complete"))
	{
		EnterAftermath(true);
	}
}

void FArenaSession::EnterAftermath(bool bWon)
{
	bFinalWon = bWon;
	Games.Phase = TEXT("aftermath");
	PhaseClock = 0.0;
	StoryHold = 0.0;
	StoryHoldStone = -1;
	bStoryCue = false;
	Games.State.Projectiles.Reset();
	Games.State.Telegraphs.Reset();
	if (Hands)
	{
		Hands->StopAll();
	}
	Flags.SetBool(TEXT("arena.day.closed"), true);
	SaveFlags();
	// The day is over: the next launch is a new day from Bout 1 (the ritual, no teach). The stone pick is kept.
	WriteSavedBout(0);
	Note(FString::Printf(TEXT("day aftermath final=%s"), bWon ? TEXT("won") : TEXT("lost")));
}

void FArenaSession::EndDay()
{
	Games.Phase = TEXT("closed");
	PhaseClock = 0.0;
	StoryHold = 0.0;
	StoryHoldStone = -1;
	if (Hands)
	{
		Hands->StopAll();
	}
	Note(TEXT("day closed"));
}

bool FArenaSession::HoldCentreStone(double DeltaSeconds)
{
	bool bInside[3] = {false, false, false};
	StonesTouched(bInside);
	const int32 Touched = bInside[CentreStone] ? CentreStone : -1;
	if (Touched != StoryHoldStone)
	{
		StoryHold = 0.0;
		StoryHoldStone = Touched;
	}
	if (Touched < 0)
	{
		return false;
	}
	StoryHold += DeltaSeconds;
	return StoryHold + 1.0e-9 >= Tuning.OfferHoldS;
}

double FArenaSession::GetStoryHoldFraction() const
{
	if (StoryHoldStone < 0 || Tuning.OfferHoldS <= 0.0)
	{
		return 0.0;
	}
	return FMath::Clamp(StoryHold / Tuning.OfferHoldS, 0.0, 1.0);
}

bool FArenaSession::AreStoryStonesShown() const
{
	if (!bDay || bPaused)
	{
		return false;
	}
	const FString& Phase = Games.Phase;
	// T28: a skip stone shows once it works, from its beat's floor.
	if (Phase == TEXT("prologue"))
	{
		return PhaseClock + 1.0e-9 >= BeatFloorS(TEXT("prologue"));
	}
	if (Phase == TEXT("aftermath"))
	{
		return PhaseClock + 1.0e-9 >= DayTuning.AftermathTabletS + BeatFloorS(TEXT("closing"));
	}
	if (Phase == TEXT("lost"))
	{
		const FArenaTier* Tier = Tiro();
		return Tier && Games.Wave == Tier->Waves.Num() - 1;
	}
	return false;
}

FString FArenaSession::StoryStoneKey(int32 Index) const
{
	if (Index != CentreStone || !AreStoryStonesShown())
	{
		return FString();
	}
	return Games.Phase == TEXT("prologue") ? TEXT("stone.skip") : TEXT("stone.end");
}

FString FArenaSession::GetTabletText() const
{
	if (Games.Phase != TEXT("aftermath") && Games.Phase != TEXT("closed"))
	{
		return FString();
	}
	const FArenaTier* Tier = Tiro();
	if (!Tier)
	{
		return FString();
	}
	// T22: the only place the collar's numbers show. Columns: bout, kind, result, time, tries, Cracks, Tithe.
	TArray<FString> Rows;
	Rows.Add(TeachString(TEXT("tablet.title")));
	Rows.Add(FString::Printf(TEXT("%s  %s  %s"), *TeachString(TEXT("tablet.columns")), *TeachString(TEXT("tablet.cracks")),
		*TeachString(TEXT("tablet.tithe"))));
	int32 DayCracks = 0;
	int32 DayTithe = 0;
	for (int32 Index = 0; Index < Tier->Waves.Num(); ++Index)
	{
		const FArenaWave& Wave = Tier->Waves[Index];
		bool bWon = false;
		double Seconds = 0.0;
		double Attempts = 0.0;
		double Cracks = 0.0;
		double Tithe = 0.0;
		const bool bHave = Flags.GetBool(FArenaFlags::TiroKey(Wave.N, TEXT("won")), bWon)
			&& Flags.GetNumber(FArenaFlags::TiroKey(Wave.N, TEXT("time")), Seconds)
			&& Flags.GetNumber(FArenaFlags::TiroKey(Wave.N, TEXT("attempts")), Attempts);
		if (!bHave)
		{
			Rows.Add(FString::Printf(TEXT("%d  %s  -"), Wave.N, *Wave.Kind));
			continue;
		}
		Flags.GetNumber(FArenaFlags::TiroKey(Wave.N, TEXT("cracks")), Cracks);
		Flags.GetNumber(FArenaFlags::TiroKey(Wave.N, TEXT("tithe")), Tithe);
		DayCracks += static_cast<int32>(Cracks);
		DayTithe += static_cast<int32>(Tithe);
		Rows.Add(FString::Printf(TEXT("%d  %s  %s  %.1f s  x%.0f  %.0f  %.0f"), Wave.N, *Wave.Kind, bWon ? TEXT("won") : TEXT("lost"), Seconds,
			Attempts, Cracks, Tithe));
	}
	const FString CracksLabel = TeachString(TEXT("tablet.cracks"));
	const FString TitheLabel = TeachString(TEXT("tablet.tithe"));
	Rows.Add(FString::Printf(TEXT("%s:  %s %d  %s %d"), *TeachString(TEXT("tablet.day")), *CracksLabel, DayCracks, *TitheLabel, DayTithe));
	Rows.Add(FString::Printf(TEXT("%s:  %s %d"), *TeachString(TEXT("tablet.lifetime")), *CracksLabel, Collar.GetLifetimeCracks()));
	return FString::Join(Rows, TEXT("\n"));
}

bool FArenaSession::AdvanceDay(double DeltaSeconds)
{
	const FString Phase = Games.Phase;
	const FArenaTier* Tier = Tiro();
	const bool bFinalLost = Phase == TEXT("lost") && bDay && Tier && Games.Wave == Tier->Waves.Num() - 1;
	if (Phase == TEXT("missio"))
	{
		PhaseClock += DeltaSeconds;
		AdvanceMissio(DeltaSeconds);
		return true;
	}
	if (Phase != TEXT("prologue") && Phase != TEXT("ritual") && Phase != TEXT("intro") && Phase != TEXT("aftermath")
		&& Phase != TEXT("closed") && !bFinalLost)
	{
		return false;
	}
	if (bFinalLost)
	{
		// The palm (retry) is the lost phase's own path; the centre stone ends the day. Scripted: the script ends it.
		if (bScripted && !bStoryCue)
		{
			bStoryCue = true;
			PlayAction(TEXT("stone-centre"));
		}
		if (HoldCentreStone(DeltaSeconds))
		{
			Note(TEXT("day final lost, stone ends the day"));
			EnterAftermath(false);
			return true;
		}
		return false;
	}
	PhaseClock += DeltaSeconds;
	if (Phase == TEXT("prologue"))
	{
		// T28: below the beat's floor the centre stone does nothing; the script reaches for it once the floor passed.
		const double Floor = BeatFloorS(TEXT("prologue"));
		const bool bFloor = PhaseClock + 1.0e-9 >= Floor;
		if (PhaseClock + 1.0e-9 >= ScriptLeaveS(TEXT("prologue"), Floor) && bScripted && !bStoryCue)
		{
			bStoryCue = true;
			PlayAction(TEXT("stone-centre"));
		}
		const bool bSkipped = HoldCentreStone(DeltaSeconds) && bFloor;
		if (bSkipped || PhaseClock + 1.0e-9 >= DayTuning.PrologueMaxS)
		{
			Note(FString::Printf(TEXT("day prologue %s t=%.2f"), bSkipped ? TEXT("skipped") : TEXT("ended"), PhaseClock));
			EnterTeach();
		}
		return true;
	}
	if (Phase == TEXT("ritual"))
	{
		const double OfferS = GetRitualOfferS();
		const bool bOffering = PhaseClock + 1.0e-9 >= OfferS;
		if (PhaseClock + 1.0e-9 >= ScriptLeaveS(TEXT("ritual"), OfferS) && bScripted && !bStoryCue)
		{
			bStoryCue = true;
			PlayAction(TEXT("both-palms"));
		}
		// T27: the offer counts from the ritual's floor (C2 section 4); without the slice once the offer line stands.
		if (bOffering && BothPalmsRaised())
		{
			OfferHold += DeltaSeconds;
			if (OfferHold + 1.0e-9 >= Tuning.OfferHoldS)
			{
				Note(FString::Printf(TEXT("day ritual offered hold=%.3f t=%.2f"), OfferHold, PhaseClock));
				OfferHold = 0.0;
				StartDayBout(BoutWave);
				return true;
			}
		}
		else
		{
			OfferHold = 0.0;
		}
		if (PhaseClock + 1.0e-9 >= DayTuning.RitualTimeoutS)
		{
			Note(FString::Printf(TEXT("day ritual timeout t=%.2f"), PhaseClock));
			OfferHold = 0.0;
			StartDayBout(BoutWave);
		}
		return true;
	}
	if (Phase == TEXT("intro"))
	{
		if (PhaseClock + 1.0e-9 >= DayTuning.IntroSeconds(Games.Wave))
		{
			Games.Phase = TEXT("active");
			Note(FString::Printf(TEXT("day fight bout=%d"), Games.Wave + 1));
		}
		return true;
	}
	if (Phase == TEXT("aftermath"))
	{
		if (PhaseClock + 1.0e-9 < DayTuning.AftermathTabletS)
		{
			return true;
		}
		// T28: the closing card. The centre stone ends the day from the closing beat's floor; closingCardS ends it anyway.
		const double CardS = PhaseClock - DayTuning.AftermathTabletS;
		const double Floor = BeatFloorS(TEXT("closing"));
		const bool bFloor = CardS + 1.0e-9 >= Floor;
		if (CardS + 1.0e-9 >= ScriptLeaveS(TEXT("closing"), Floor) && bScripted && !bStoryCue)
		{
			bStoryCue = true;
			PlayAction(TEXT("stone-centre"));
		}
		if (HoldCentreStone(DeltaSeconds) && bFloor)
		{
			EndDay();
		}
		else if (CardS + 1.0e-9 >= DayTuning.ClosingCardS)
		{
			Note(FString::Printf(TEXT("day closing timeout t=%.2f"), CardS));
			EndDay();
		}
		return true;
	}
	// closed: nothing left to do.
	return true;
}

void FArenaSession::AdvanceMissio(double DeltaSeconds)
{
	// The fist is timed by the detector on the hand clock, the palm by the session as the stone holds are. The script
	// makes no choice, so a scripted day goes on through the timeout's spare.
	if (Fist.GetHeldCount() > 0)
	{
		ResolveMissio(true, TEXT("fist"));
		return;
	}
	bool bPalm = false;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		if (!MissioPalmRaised(Index == 0 ? EControllerHand::Left : EControllerHand::Right))
		{
			bMissioPalmArmed[Index] = true;
		}
		else
		{
			bPalm |= bMissioPalmArmed[Index];
		}
	}
	MissioHold = bPalm ? MissioHold + DeltaSeconds : 0.0;
	if (bPalm && MissioHold + 1.0e-9 >= Tuning.OfferHoldS)
	{
		ResolveMissio(false, TEXT("palm"));
		return;
	}
	if (PhaseClock + 1.0e-9 >= MissioTimeoutS)
	{
		ResolveMissio(false, TEXT("timeout"));
	}
}

void FArenaSession::LoadStorySlice()
{
	const FString Path = SlicePath.IsEmpty() ? FStorySlice::DefaultFilePath() : SlicePath;
	FString Error;
	if (!Slice.Load(Path, Error))
	{
		// C2 section 8: the story content never breaks the slice. The day plays one line per beat from strings.json.
		UE_LOG(LogMageArena, Warning, TEXT("Story slice skipped (%s); the day plays one line per beat from strings.json"), *Error);
		return;
	}
	Note(FString::Printf(TEXT("day slice scenes=%d"), Slice.GetScenes().Num()));
}

const FStoryScene* FArenaSession::GetActiveScene() const
{
	if (!bDay || Games.Phase != TEXT("intermission") || !Slice.IsLoaded())
	{
		return nullptr;
	}
	const FArenaTier* Tier = Tiro();
	const int32 WaveN = Tier && Tier->Waves.IsValidIndex(Games.Wave) ? Tier->Waves[Games.Wave].N : Games.Wave + 1;
	return Slice.FindSceneAfterWave(WaveN);
}

FString FArenaSession::GetSceneVariantId() const
{
	const FStoryScene* Scene = GetActiveScene();
	if (!Scene || SceneWave != Games.Wave || !Scene->Variants.IsValidIndex(SceneVariant))
	{
		return FString();
	}
	return Scene->Variants[SceneVariant].Id;
}

bool FArenaSession::AreSceneStonesShown() const
{
	const FStoryScene* Scene = GetActiveScene();
	// The T20 pick keeps its own stones and labels (IsPickOffered); a scene that wraps it shows no second set.
	return Scene && !bPaused && SceneWave == Games.Wave && !Scene->bKeepsExistingPick && Scene->Stones.Num() == 3 && !IsPickOffered();
}

FString FArenaSession::SceneStoneKey(int32 Index) const
{
	const FStoryScene* Scene = GetActiveScene();
	if (!AreSceneStonesShown() || !Scene || !Scene->Stones.IsValidIndex(Index))
	{
		return FString();
	}
	return Scene->Stones[Index];
}

double FArenaSession::GetSceneHoldFraction() const
{
	if (SceneHoldStone < 0 || Tuning.OfferHoldS <= 0.0)
	{
		return 0.0;
	}
	return FMath::Clamp(SceneHold / Tuning.OfferHoldS, 0.0, 1.0);
}

void FArenaSession::ResetScene()
{
	SceneWave = -1;
	SceneClock = 0.0;
	SceneVariant = INDEX_NONE;
	SceneHold = 0.0;
	SceneHoldStone = -1;
}

void FArenaSession::BeginScene(const FStoryScene& Scene)
{
	ResetScene();
	SceneWave = Games.Wave;
	// The flags of the bout just won are already recorded (CheckEnd, then missio), so the when clauses read this bout.
	SceneVariant = Scene.SelectVariant(Flags);
	const FString Variant = Scene.Variants.IsValidIndex(SceneVariant) ? Scene.Variants[SceneVariant].Id : FString(TEXT("-"));
	Note(FString::Printf(TEXT("intermission %d scene %s variant=%s"), Scene.Intermission, *Scene.Id, *Variant));
}

FString FArenaSession::SceneLine() const
{
	int32 Index = INDEX_NONE;
	const FStoryLine* Line = SceneStoryLine(Index);
	return Line ? Line->Text : FString();
}

const FStoryLine* FArenaSession::SceneStoryLine(int32& OutIndex) const
{
	OutIndex = INDEX_NONE;
	const FStoryScene* Scene = GetActiveScene();
	if (!Scene || SceneWave != Games.Wave)
	{
		return nullptr;
	}
	// One line at a time, each for its dwellS (C2 section 8: a line with no audio plays as text for its dwell).
	const TArray<const FStoryLine*> Lines = FStorySlice::SceneLines(*Scene, SceneVariant);
	double Until = 0.0;
	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		Until += Lines[Index]->DwellS;
		if (SceneClock + 1.0e-9 < Until)
		{
			OutIndex = Index;
			return Lines[Index];
		}
	}
	return nullptr;
}

double FArenaSession::GetRitualOfferS() const
{
	if (Slice.FindBeat(TEXT("ritual")))
	{
		return Slice.BeatFloorS(TEXT("ritual"));
	}
	return DayTuning.RitualLineS.Num() > 0 ? DayTuning.RitualLineS[0] : 0.0;
}

double FArenaSession::ScriptLeaveS(const TCHAR* BeatId, double FloorS) const
{
	if (!bScriptNatural)
	{
		return FloorS;
	}
	// The ritual's natural length is its dwells (day.json owns them); the other beats read slice.json naturalS.
	const double Natural = FCString::Strcmp(BeatId, TEXT("ritual")) == 0 ? DayTuning.RitualNaturalS() : Slice.BeatNaturalS(BeatId);
	return FMath::Max(FloorS, Natural);
}

namespace
{
// The line standing Clock seconds into Lines, each for its dwellS; the last one stays once every dwell ran.
int32 StoryLineIndexAt(const TArray<FStoryLine>& Lines, double Clock)
{
	double Until = 0.0;
	for (int32 Index = 0; Index + 1 < Lines.Num(); ++Index)
	{
		Until += Lines[Index].DwellS;
		if (Clock + 1.0e-9 < Until)
		{
			return Index;
		}
	}
	return Lines.Num() - 1;
}
}

const FStoryLine* FArenaSession::GetStoryLine(FString* OutEvent) const
{
	if (!bDay || !Slice.IsLoaded())
	{
		return nullptr;
	}
	const FString& Phase = Games.Phase;
	const FStoryBeat* Beat = nullptr;
	const TArray<FStoryLine>* Lines = nullptr;
	int32 Index = INDEX_NONE;
	if (Phase == TEXT("intermission"))
	{
		const FStoryScene* Scene = GetActiveScene();
		const FStoryLine* Line = SceneStoryLine(Index);
		if (Line && OutEvent)
		{
			*OutEvent = FStorySlice::LineEvent(Scene->Id, Index);
		}
		return Line;
	}
	const FVrRuleset* Rules = GetVrRules();
	const FString FinalRival = Rules && !Rules->TiroFinal.Rival.IsEmpty() ? Rules->TiroFinal.Rival : FString(TEXT("corvo"));
	if (Phase == TEXT("prologue") || Phase == TEXT("ritual"))
	{
		Beat = Slice.FindBeat(*Phase);
		Lines = Beat ? Beat->SelectLines(FinalRival, bFinalWon) : nullptr;
		if (Lines && Lines->Num() > 0)
		{
			// The ritual's dwells are day.json ritualLineS (T27); the prologue's are its lines' own.
			Index = Phase == TEXT("ritual") ? DayTuning.RitualLineAt(PhaseClock, Lines->Num()) : StoryLineIndexAt(*Lines, PhaseClock);
		}
	}
	else if (Phase == TEXT("intro"))
	{
		const FArenaTier* Tier = Tiro();
		const int32 WaveN = Tier && Tier->Waves.IsValidIndex(Games.Wave) ? Tier->Waves[Games.Wave].N : Games.Wave + 1;
		Beat = Slice.FindIntroBeat(WaveN);
		Lines = Beat ? Beat->SelectLines(FinalRival, bFinalWon) : nullptr;
		Index = Lines && Lines->Num() > 0 ? 0 : INDEX_NONE;
	}
	else if (Phase == TEXT("aftermath") || Phase == TEXT("closed"))
	{
		const bool bTablet = Phase == TEXT("aftermath") && PhaseClock + 1.0e-9 < DayTuning.AftermathTabletS;
		Beat = Slice.FindBeat(bTablet ? TEXT("aftermath-tablet") : TEXT("closing"));
		Lines = Beat ? Beat->SelectLines(FinalRival, bFinalWon) : nullptr;
		if (Lines && Lines->Num() > 0)
		{
			// Once the day is closed the closing card's last line stays (OPH-10, the same text as story.closed).
			Index = bTablet ? 0 : Phase == TEXT("closed") ? Lines->Num() - 1
			                                              : StoryLineIndexAt(*Lines, PhaseClock - DayTuning.AftermathTabletS);
		}
	}
	if (!Beat || !Lines || !Lines->IsValidIndex(Index))
	{
		return nullptr;
	}
	if (OutEvent)
	{
		*OutEvent = FStorySlice::LineEvent(Beat->Id, Index);
	}
	return &(*Lines)[Index];
}

void FArenaSession::TrackVoice()
{
	FString Event;
	const FStoryLine* Line = GetStoryLine(&Event);
	if (!Line || Line->Cue.IsEmpty())
	{
		VoiceEvent.Reset();
		return;
	}
	if (Event == VoiceEvent)
	{
		return;
	}
	VoiceEvent = Event;
	PushVoice(*Line, Event, Games.Phase == TEXT("intermission") ? SceneClock : PhaseClock);
}

void FArenaSession::PushVoice(const FStoryLine& Line, const FString& Event, double PhaseS)
{
	FVoiceCue Cue;
	Cue.Cue = Line.Cue;
	Cue.Speaker = Line.Speaker;
	Cue.Event = Event;
	Cue.Phase = Games.Phase;
	Cue.PhaseS = PhaseS;
	VoiceCues.Add(Cue);
	Note(FString::Printf(TEXT("voice %s %s speaker=%s t=%.2f"), *Cue.Cue, *Event, Cue.Speaker.IsEmpty() ? TEXT("-") : *Cue.Speaker, PhaseS));
}

bool FArenaSession::AdvanceScene(double DeltaSeconds)
{
	const FStoryScene* Scene = GetActiveScene();
	if (!Scene)
	{
		return false;
	}
	if (SceneWave != Games.Wave)
	{
		BeginScene(*Scene);
	}
	SceneClock += DeltaSeconds;
	// Below the scene's floor a raised palm does nothing: a ward still up from the bout must not skip the scene.
	if (bOfferContinue && SceneClock + 1.0e-9 < Scene->FloorS)
	{
		bOfferContinue = false;
		Note(FString::Printf(TEXT("intermission %d palm below the floor t=%.2f"), Scene->Intermission, SceneClock));
	}
	if (bOfferContinue)
	{
		// The shared continue in AdvanceArc takes it from here (and closes an open pick).
		return false;
	}
	if (IsPickOffered())
	{
		// The T20 pick is this scene's choice (C2 int-1 keepsExistingPick): its stones, hold, timeout and branch effect
		// are unchanged. ChoosePick also writes arena.intermission.<n>.choice.
		if (AdvancePick(DeltaSeconds))
		{
			return true;
		}
	}
	else if (!Scene->bKeepsExistingPick && AdvanceSceneChoice(*Scene, DeltaSeconds))
	{
		return true;
	}
	if (SceneClock + 1.0e-9 >= Scene->MaxS)
	{
		// Never blocking: the scene's seconds are up. No flag is written and nothing is charged.
		if (IsPickOffered())
		{
			bPickClosed = true;
		}
		Note(FString::Printf(TEXT("intermission %d timeout t=%.2f"), Scene->Intermission, SceneClock));
		ContinueIntermission();
		return true;
	}
	return false;
}

bool FArenaSession::AdvanceSceneChoice(const FStoryScene& Scene, double DeltaSeconds)
{
	if (Scene.Stones.Num() != 3)
	{
		SceneHold = 0.0;
		SceneHoldStone = -1;
		return false;
	}
	// The pick's detection: a palm or index tip within the stone radius, held offerHoldS. Desktop plays Clips/stones.
	bool bInside[3] = {false, false, false};
	StonesTouched(bInside);
	int32 Touched = -1;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		if (bInside[Index])
		{
			Touched = Index;
			break;
		}
	}
	if (Touched != SceneHoldStone)
	{
		SceneHold = 0.0;
		SceneHoldStone = Touched;
	}
	if (Touched < 0)
	{
		return false;
	}
	SceneHold += DeltaSeconds;
	if (SceneHold + 1.0e-9 < Tuning.OfferHoldS)
	{
		return false;
	}
	WriteSceneChoice(Scene, Touched);
	Note(FString::Printf(TEXT("intermission %d choice stone=%d t=%.2f"), Scene.Intermission, Touched, SceneClock));
	ContinueIntermission();
	return true;
}

void FArenaSession::WriteSceneChoice(const FStoryScene& Scene, int32 Stone)
{
	if (Scene.Writes.IsEmpty())
	{
		return;
	}
	Flags.SetNumber(Scene.Writes, static_cast<double>(Stone));
	SaveFlags();
}
