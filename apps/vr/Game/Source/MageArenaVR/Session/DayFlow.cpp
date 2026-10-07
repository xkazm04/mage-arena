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

const FArenaTier* Tiro()
{
	const FKernelData& Data = KernelData();
	return Data.bReady && Data.ArenaTiers.Num() > 0 ? &Data.ArenaTiers[0] : nullptr;
}
}

bool FArenaSession::IsArcPhase(const FString& Phase)
{
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
		{TEXT("ritualLineS"), &Next.RitualLineS},
		{TEXT("ritualTimeoutS"), &Next.RitualTimeoutS},
		{TEXT("introS"), &Next.IntroS},
		{TEXT("aftermathTabletS"), &Next.AftermathTabletS},
	};
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Root->Values)
	{
		bool bKnown = Pair.Key.StartsWith(TEXT("_")) || Pair.Key == TEXT("version");
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
	if (Next.RitualLineS >= Next.RitualTimeoutS)
	{
		UE_LOG(LogMageArena, Error, TEXT("day.json ritualLineS must be shorter than ritualTimeoutS"));
		return false;
	}
	DayTuning = Next;
	return true;
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
	return DayFire.IsValidIndex(WaveIndex) && DayFire[WaveIndex] ? TEXT("fire") : TEXT("water");
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
	SaveFlags();
	Note(FString::Printf(TEXT("day bout=%d %s t=%.2f attempts=%.0f perfects=%d"), WaveN, bWon ? TEXT("won") : TEXT("lost"), Seconds,
		Attempts + 1.0, Perfects));
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
	if (Phase == TEXT("prologue"))
	{
		return true;
	}
	if (Phase == TEXT("aftermath"))
	{
		return PhaseClock + 1.0e-9 >= DayTuning.AftermathTabletS;
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
	TArray<FString> Rows;
	Rows.Add(TeachString(TEXT("tablet.title")));
	for (int32 Index = 0; Index < Tier->Waves.Num(); ++Index)
	{
		const FArenaWave& Wave = Tier->Waves[Index];
		bool bWon = false;
		double Seconds = 0.0;
		double Attempts = 0.0;
		const bool bHave = Flags.GetBool(FArenaFlags::TiroKey(Wave.N, TEXT("won")), bWon)
			&& Flags.GetNumber(FArenaFlags::TiroKey(Wave.N, TEXT("time")), Seconds)
			&& Flags.GetNumber(FArenaFlags::TiroKey(Wave.N, TEXT("attempts")), Attempts);
		if (!bHave)
		{
			Rows.Add(FString::Printf(TEXT("%d  %s  -"), Wave.N, *Wave.Kind));
			continue;
		}
		Rows.Add(FString::Printf(TEXT("%d  %s  %s  %.1f s  tries %.0f"), Wave.N, *Wave.Kind, bWon ? TEXT("won") : TEXT("lost"), Seconds, Attempts));
	}
	return FString::Join(Rows, TEXT("\n"));
}

bool FArenaSession::AdvanceDay(double DeltaSeconds)
{
	const FString Phase = Games.Phase;
	const FArenaTier* Tier = Tiro();
	const bool bFinalLost = Phase == TEXT("lost") && bDay && Tier && Games.Wave == Tier->Waves.Num() - 1;
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
		if (bScripted && !bStoryCue)
		{
			bStoryCue = true;
			PlayAction(TEXT("stone-centre"));
		}
		const bool bSkipped = HoldCentreStone(DeltaSeconds);
		if (bSkipped || PhaseClock + 1.0e-9 >= DayTuning.PrologueMaxS)
		{
			Note(FString::Printf(TEXT("day prologue %s t=%.2f"), bSkipped ? TEXT("skipped") : TEXT("ended"), PhaseClock));
			EnterTeach();
		}
		return true;
	}
	if (Phase == TEXT("ritual"))
	{
		const bool bOffering = PhaseClock + 1.0e-9 >= DayTuning.RitualLineS;
		if (bOffering && bScripted && !bStoryCue)
		{
			bStoryCue = true;
			PlayAction(TEXT("both-palms"));
		}
		// The offer counts only once the offer line stands: Septima speaks first.
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
		if (PhaseClock + 1.0e-9 >= DayTuning.IntroS)
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
		if (bScripted && !bStoryCue)
		{
			bStoryCue = true;
			PlayAction(TEXT("stone-centre"));
		}
		if (HoldCentreStone(DeltaSeconds))
		{
			EndDay();
		}
		return true;
	}
	// closed: nothing left to do.
	return true;
}
