#include "Session/ArenaSession.h"

#include "Dom/JsonObject.h"
#include "Hands/MageSettings.h"
#include "Gestures/BlinkDetector.h"
#include "Gestures/SigilRecognizerSubsystem.h"
#include "Gestures/StaffDetector.h"
#include "Gestures/WardDetector.h"
#include "HAL/FileManager.h"
#include "Hands/HandInputSubsystem.h"
#include "Kernel/ArenaKernel.h"
#include "Kernel/Catalog.h"
#include "Kernel/KernelData.h"
#include "MageArenaVR.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
FString VrDataFile(const TCHAR* Name)
{
	FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../data/vr"), Name);
	FPaths::CollapseRelativeDirectories(Path);
	return Path;
}

TSharedPtr<FJsonObject> ReadJsonFile(const FString& Path)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		return nullptr;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return nullptr;
	}
	return Root;
}

double FrameCap()
{
	return KernelData().MaxFrameDeltaS > 0.0 ? KernelData().MaxFrameDeltaS : 0.25;
}
}

bool FArenaSession::LoadTeachData()
{
	const FString Path = VrDataFile(TEXT("teach.json"));
	const TSharedPtr<FJsonObject> Root = ReadJsonFile(Path);
	if (!Root.IsValid())
	{
		Tuning = FTeachTuning();
		UE_LOG(LogMageArena, Error, TEXT("Teach data missing: %s; using built-in defaults"), *Path);
		return true;
	}
	FTeachTuning Next;
	bool bMissing = false;
	auto Take = [&Root, &bMissing](const TCHAR* Key, double& Out)
	{
		double Value = 0.0;
		if (Root->TryGetNumberField(Key, Value))
		{
			Out = Value;
		}
		else
		{
			bMissing = true;
			UE_LOG(LogMageArena, Warning, TEXT("teach.json missing %s; using built-in default %.4f"), Key, Out);
		}
	};
	Take(TEXT("coldHoldS"), Next.ColdHoldS);
	Take(TEXT("absorbLeadS"), Next.AbsorbLeadS);
	Take(TEXT("perfectLeadS"), Next.PerfectLeadS);
	Take(TEXT("wardDistanceM"), Next.WardDistanceM);
	Take(TEXT("globSpeedMps"), Next.GlobSpeedMps);
	Take(TEXT("wardWindupS"), Next.WardWindupS);
	Take(TEXT("globDamage"), Next.GlobDamage);
	Take(TEXT("globRadiusM"), Next.GlobRadiusM);
	Take(TEXT("globMarkerWidthM"), Next.GlobMarkerWidthM);
	Take(TEXT("globRangeMarginM"), Next.GlobRangeMarginM);
	Take(TEXT("globSettleS"), Next.GlobSettleS);
	Take(TEXT("perfectRingS"), Next.PerfectRingS);
	Take(TEXT("beatAfterAbsorbS"), Next.BeatAfterAbsorbS);
	Take(TEXT("beatAfterWardS"), Next.BeatAfterWardS);
	Take(TEXT("dummyHp"), Next.DummyHp);
	Take(TEXT("dummyDistanceM"), Next.DummyDistanceM);
	Take(TEXT("sigilPromptS"), Next.SigilPromptS);
	Take(TEXT("sigilRetryS"), Next.SigilRetryS);
	Take(TEXT("beatAfterSigilS"), Next.BeatAfterSigilS);
	Take(TEXT("blinkWindupS"), Next.BlinkWindupS);
	Take(TEXT("blinkWidthM"), Next.BlinkWidthM);
	Take(TEXT("blinkRangeM"), Next.BlinkRangeM);
	Take(TEXT("blinkDamage"), Next.BlinkDamage);
	Take(TEXT("blinkLeadS"), Next.BlinkLeadS);
	Take(TEXT("blinkSettleS"), Next.BlinkSettleS);
	Take(TEXT("beatAfterBlinkS"), Next.BeatAfterBlinkS);
	Take(TEXT("trackingLossS"), Next.TrackingLossS);
	// UE's JSON object is case-insensitive, so countStepS and countSteps would be one key.
	Take(TEXT("resumeStepSeconds"), Next.CountStepS);
	Take(TEXT("offerHoldS"), Next.OfferHoldS);
	double Tries = 0.0;
	if (Root->TryGetNumberField(TEXT("perfectTries"), Tries))
	{
		Next.PerfectTries = FMath::Max(1, FMath::RoundToInt(Tries));
	}
	else
	{
		bMissing = true;
		UE_LOG(LogMageArena, Warning, TEXT("teach.json missing perfectTries; using built-in default %d"), Next.PerfectTries);
	}
	double CountSteps = 0.0;
	if (Root->TryGetNumberField(TEXT("resumeStepCount"), CountSteps))
	{
		Next.CountSteps = FMath::Max(1, FMath::RoundToInt(CountSteps));
	}
	else
	{
		bMissing = true;
		UE_LOG(LogMageArena, Warning, TEXT("teach.json missing resumeStepCount; using built-in default %d"), Next.CountSteps);
	}
	if (bMissing)
	{
		UE_LOG(LogMageArena, Warning, TEXT("Teach data incomplete: %s; missing fields kept their built-in defaults"), *Path);
	}
	Next.bLoaded = !bMissing;
	Tuning = Next;
	return true;
}

bool FArenaSession::LoadStrings()
{
	const FString Path = VrDataFile(TEXT("strings.json"));
	const TSharedPtr<FJsonObject> Root = ReadJsonFile(Path);
	if (!Root.IsValid())
	{
		UE_LOG(LogMageArena, Error, TEXT("String table missing: %s"), *Path);
		return false;
	}
	Strings.Reset();
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Root->Values)
	{
		FString Text;
		if (Pair.Value.IsValid() && Pair.Value->TryGetString(Text))
		{
			Strings.Add(Pair.Key, Text);
		}
	}
	return Strings.Contains(TEXT("cold.start"));
}

FString FArenaSession::TeachString(const TCHAR* Key) const
{
	if (const FString* Found = Strings.Find(Key))
	{
		return *Found;
	}
	return Key;
}

FString FArenaSession::SaveFilePath() const
{
	const FString Dir = SaveDir.IsEmpty()
		? FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArena"))
		: SaveDir;
	return FPaths::Combine(Dir, TEXT("bout.txt"));
}

void FArenaSession::SetSaveDirectory(const FString& Directory)
{
	SaveDir = Directory;
}

bool FArenaSession::IsTiroWave(int32 Wave) const
{
	const FKernelData& Data = KernelData();
	return Data.bReady && Data.ArenaTiers.Num() > 0 && Wave >= 0 && Wave < Data.ArenaTiers[0].Waves.Num();
}

int32 FArenaSession::ReadSavedBout(FString* OutPick) const
{
	if (OutPick)
	{
		OutPick->Reset();
	}
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *SaveFilePath()))
	{
		return -1;
	}
	Text.ReplaceInline(TEXT("\r"), TEXT(""));
	Text.TrimStartAndEndInline();
	// Line 1 is bout=N. T20 adds an optional tideOrbIV=A|B line (the stone pick). A file without it still loads.
	TArray<FString> Lines;
	Text.ParseIntoArray(Lines, TEXT("\n"), true);
	const FString First = Lines.Num() > 0 ? Lines[0] : FString();
	const FString Prefix = TEXT("bout=");
	if (!First.StartsWith(Prefix))
	{
		UE_LOG(LogMageArena, Warning, TEXT("Saved bout is corrupt (%s); starting from the teach"), *SaveFilePath());
		return -1;
	}
	const FString Number = First.Mid(Prefix.Len());
	int32 Index = 0;
	if (Number.IsEmpty())
	{
		UE_LOG(LogMageArena, Warning, TEXT("Saved bout is junk (%s); starting from the teach"), *Text);
		return -1;
	}
	if (Number[0] == TCHAR('-'))
	{
		Index = 1;
	}
	bool bDigits = Index < Number.Len();
	for (; Index < Number.Len(); ++Index)
	{
		if (!FChar::IsDigit(Number[Index]))
		{
			bDigits = false;
			break;
		}
	}
	int32 Bout = 0;
	if (!bDigits || !LexTryParseString(Bout, *Number) || FString::FromInt(Bout) != Number)
	{
		UE_LOG(LogMageArena, Warning, TEXT("Saved bout is junk (%s); starting from the teach"), *Text);
		return -1;
	}
	FString Pick;
	for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
	{
		const FString Line = Lines[LineIndex].TrimStartAndEnd();
		if (Line == TEXT("tideOrbIV=A") || Line == TEXT("tideOrbIV=B"))
		{
			if (!Pick.IsEmpty())
			{
				UE_LOG(LogMageArena, Warning, TEXT("Saved bout has two tideOrbIV lines (%s); starting from the teach"), *Text);
				return -1;
			}
			Pick = Line.Right(1);
			continue;
		}
		UE_LOG(LogMageArena, Warning, TEXT("Saved bout has a junk line (%s); starting from the teach"), *Line);
		return -1;
	}
	if (!IsTiroWave(Bout))
	{
		const int32 Waves = (KernelData().bReady && KernelData().ArenaTiers.Num() > 0)
			? KernelData().ArenaTiers[0].Waves.Num()
			: 0;
		UE_LOG(LogMageArena, Warning, TEXT("Saved bout %d is outside Tiro waves [0, %d); starting from the teach"), Bout, Waves);
		return -1;
	}
	if (OutPick)
	{
		*OutPick = Pick;
	}
	return Bout;
}

void FArenaSession::WriteSavedBout(int32 Bout) const
{
	const FString Path = SaveFilePath();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	FString Text = FString::Printf(TEXT("bout=%d\n"), Bout);
	if (!StonePick.IsEmpty())
	{
		Text += FString::Printf(TEXT("tideOrbIV=%s\n"), *StonePick);
	}
	if (!FFileHelper::SaveStringToFile(Text, *Path))
	{
		UE_LOG(LogMageArena, Error, TEXT("Could not save the bout to %s; the next launch will start from the teach"), *Path);
	}
}

void FArenaSession::ClearSavedBout() const
{
	IFileManager::Get().Delete(*SaveFilePath(), false, true, true);
}

bool FArenaSession::MakeQuietArena(uint32 Seed)
{
	if (!KernelData().bReady)
	{
		UE_LOG(LogMageArena, Error, TEXT("Arc failed: kernel data is not ready (%s)"), *KernelData().Error);
		return false;
	}
	if (!LoadPlayerPresetFile(TEXT("Arc failed")))
	{
		return false;
	}
	const FComposition Composition = PlayerComposition();
	FString Error;
	bHasLayout = Layout.LoadFromFile(FArenaLayout::DefaultFilePath(), Error);
	if (!bHasLayout)
	{
		UE_LOG(LogMageArena, Error, TEXT("Arc layout failed: %s"), *Error);
	}
	FVrRuleset Rules;
	FString RulesError;
	if (RulesOverride.IsSet())
	{
		Rules = RulesOverride.GetValue();
		Rules.ClearRuntime();
	}
	else if (!LoadVrRuleset(Rules, RulesError))
	{
		UE_LOG(LogMageArena, Error, TEXT("Arc failed: %s"), *RulesError);
		return false;
	}
	Games = FGames();
	Games.State = CreateArena(Seed);
	Games.VrRules = Rules;
	Games.PlayerId = AddMage(Games.State, 0, KernelData().PlayerSpawn, TEXT("Cassia")).Id;
	if (FActor* Player = SimFindActor(Games.State, Games.PlayerId))
	{
		Player->Water = NewWaterState(&Composition);
		ActivePad = 1;
		SeatOnActivePad(*Player);
	}
	bRunning = true;
	bPaused = false;
	bLoggedEnd = false;
	Accumulator = 0.0;
	EventCursor = Games.State.Events.Num();
	bAbsorb = false;
	bSuppressWard = false;
	bBlinkQueued = false;
	bCastPulse = false;
	DummyId = 0;
	return true;
}

bool FArenaSession::BeginArc(uint32 Seed)
{
	BoutSeed = Seed;
	if (!LoadTeachData() || !LoadStrings() || !LoadDayData())
	{
		return false;
	}
	bDay = true;
	bFinalWon = false;
	PhaseClock = 0.0;
	StoryHold = 0.0;
	StoryHoldStone = -1;
	bStoryCue = false;
	Flags.Load(FlagsFilePath());
	bKeepChain = false;
	Chain.Reset();
	bResumeGate = false;
	bResumeCounting = false;
	ResumeElapsed = 0.0;
	LostTrackingS = 0.0;
	bHandLeftSeen = false;
	bHandRightSeen = false;
	bHandLeftTracked = false;
	bHandRightTracked = false;
	TeachElapsed = 0.0;
	TeachClock = 0.0;
	ArcClock = 0.0;
	OfferHold = 0.0;
	TeachStep = ETeachStep::None;
	bColdRaised = false;
	bColdPlayed = false;
	bOfferContinue = false;
	bFlowCue = false;
	bClockCue = false;
	WristUntil = -1.0;
	PerfectRingUntil = -1.0;
	PerfectTryCount = 0;
	if (Hands)
	{
		Hands->StopAll();
	}
	if (Wards)
	{
		Wards->GetDetector().ResetStream();
	}
	if (Blinks)
	{
		Blinks->ResetDetector();
		Blinks->SetActivePad(1);
	}
	if (!MakeQuietArena(Seed))
	{
		return false;
	}
	FString SavedPick;
	const int32 Saved = ReadSavedBout(&SavedPick);
	StonePick.Reset();
	if (Saved >= 0)
	{
		StonePick = SavedPick;
		BoutWave = Saved;
		Games.Phase = TEXT("offer");
		Note(FString::Printf(TEXT("teach offer bout=%d"), BoutWave));
	}
	else
	{
		BoutWave = 0;
		Games.Phase = TEXT("cold");
		Note(TEXT("teach cold"));
	}
	return true;
}

void FArenaSession::SkipTeach()
{
	if (Games.Phase == TEXT("cold") || Games.Phase == TEXT("prologue") || Games.Phase == TEXT("offer") || Games.Phase == TEXT("teach"))
	{
		Note(TEXT("day skip to the ritual"));
		EnterRitual();
	}
}

void FArenaSession::NotifyFocusLost()
{
	HoldPause(TEXT("focus"));
}

void FArenaSession::NotifyHeadsetRemoved()
{
	HoldPause(TEXT("headset"));
}

void FArenaSession::NotifyQuit()
{
	if (Games.Phase == TEXT("active") || Games.Phase == TEXT("intro"))
	{
		WriteSavedBout(Games.Wave);
	}
	else if (Games.Phase == TEXT("ritual"))
	{
		WriteSavedBout(BoutWave);
	}
}

void FArenaSession::HoldPause(const TCHAR* Reason)
{
	if (!bRunning || bPaused)
	{
		return;
	}
	bResumeGate = true;
	bResumeCounting = false;
	ResumeElapsed = 0.0;
	SetPaused(true);
	Note(FString::Printf(TEXT("Session pause reason=%s"), Reason));
}

void FArenaSession::NoteHandFrame(const FHandFrame& Frame)
{
	const bool bTracked = Frame.Confidence > 0.0f;
	if (Frame.Hand == EControllerHand::Left)
	{
		bHandLeftSeen = true;
		bHandLeftTracked = bTracked;
	}
	else if (Frame.Hand == EControllerHand::Right)
	{
		bHandRightSeen = true;
		bHandRightTracked = bTracked;
	}
}

void FArenaSession::PollTracking(double DeltaSeconds)
{
	const bool bSeen = bHandLeftSeen || bHandRightSeen;
	const bool bTracked = (bHandLeftSeen && bHandLeftTracked) || (bHandRightSeen && bHandRightTracked);
	bHandLeftSeen = false;
	bHandRightSeen = false;
	bHandLeftTracked = false;
	bHandRightTracked = false;
	// Combat only. A fresh frame with confidence above zero on either hand is tracked.
	// Confidence 0, or a frame that is not tracked, is a loss. Silence between desktop
	// clips is not: the clip player emits the untracked frames while F9 or the test drop is on.
	const bool bCombat = Games.Phase == TEXT("active") && !bPaused;
	if (!bCombat || !bSeen || bTracked)
	{
		LostTrackingS = 0.0;
		return;
	}
	LostTrackingS += DeltaSeconds;
	if (LostTrackingS + 1.0e-9 >= Tuning.TrackingLossS)
	{
		LostTrackingS = 0.0;
		HoldPause(TEXT("tracking"));
	}
}

bool FArenaSession::IsBothPalmsClip() const
{
	return ClipAction() == TEXT("both-palms");
}

bool FArenaSession::BothPalmsRaised() const
{
	if (!Hands)
	{
		return false;
	}
	FHandFrame Left;
	FHandFrame Right;
	if (!Hands->GetLatest(EControllerHand::Left, Left) || !Hands->GetLatest(EControllerHand::Right, Right))
	{
		return false;
	}
	if (Left.Confidence < 0.2f || Right.Confidence < 0.2f)
	{
		return false;
	}
	const FVector Forward(1.0, 0.0, 0.0);
	auto Raised = [&Forward](const FHandFrame& Frame)
	{
		return FWardDetector::PalmHeightMetres(Frame) >= FWardThresholds::RaiseHeightM
			&& FWardDetector::AngleToFacingDeg(FWardDetector::PalmNormal(Frame), Forward) <= FWardThresholds::RaiseAngleDeg;
	};
	return Raised(Left) && Raised(Right);
}

void FArenaSession::UpdateResume(double DeltaSeconds)
{
	if (!bResumeGate || !bPaused)
	{
		return;
	}
	if (!BothPalmsRaised())
	{
		bResumeCounting = false;
		ResumeElapsed = 0.0;
		return;
	}
	if (!bResumeCounting)
	{
		bResumeCounting = true;
		ResumeElapsed = 0.0;
		Note(TEXT("Session resume count"));
		return;
	}
	ResumeElapsed += DeltaSeconds;
	const double Total = Tuning.CountSteps * Tuning.CountStepS;
	if (ResumeElapsed + 1.0e-9 >= Total)
	{
		bResumeCounting = false;
		bResumeGate = false;
		ResumeElapsed = 0.0;
		SetPaused(false);
	}
}

int32 FArenaSession::GetResumeCount() const
{
	if (!bResumeCounting || Tuning.CountStepS <= 0.0)
	{
		return 0;
	}
	const int32 Shown = Tuning.CountSteps - FMath::FloorToInt(ResumeElapsed / Tuning.CountStepS);
	return FMath::Clamp(Shown, 1, Tuning.CountSteps);
}

FString FArenaSession::GetStage() const
{
	return Games.Phase;
}

FString FArenaSession::GetTeachStep() const
{
	switch (TeachStep)
	{
	case ETeachStep::Ward: return TEXT("ward");
	case ETeachStep::Gap: return TEXT("gap");
	case ETeachStep::Perfect: return TEXT("perfect");
	case ETeachStep::WardBeat: return TEXT("ward-beat");
	case ETeachStep::Sigil: return TEXT("sigil");
	case ETeachStep::SigilBeat: return TEXT("sigil-beat");
	case ETeachStep::Blink: return TEXT("blink");
	case ETeachStep::BlinkBeat: return TEXT("blink-beat");
	case ETeachStep::Done: return TEXT("done");
	default: return TEXT("");
	}
}

FString FArenaSession::GetPromptText() const
{
	if (bResumeCounting)
	{
		return TeachString(*FString::Printf(TEXT("resume.%d"), GetResumeCount()));
	}
	if (bPaused)
	{
		return TeachString(TEXT("pause"));
	}
	if (Games.Phase == TEXT("cold"))
	{
		if (FMageSettings::ShouldOfferNarrow())
		{
			return TeachString(TEXT("settings.offer.narrow"));
		}
		return TeachString(TEXT("cold.start"));
	}
	if (Games.Phase == TEXT("offer"))
	{
		return TeachString(TEXT("offer.continue"));
	}
	if (Games.Phase == TEXT("prologue"))
	{
		return TeachString(TEXT("story.prologue.tableau"));
	}
	if (Games.Phase == TEXT("ritual"))
	{
		return TeachString(PhaseClock + 1.0e-9 >= DayTuning.RitualLineS ? TEXT("story.ritual.offer") : TEXT("story.ritual.septima"));
	}
	if (Games.Phase == TEXT("intro"))
	{
		const bool bFinal = KernelData().bReady && KernelData().ArenaTiers.Num() > 0 && Games.Wave == KernelData().ArenaTiers[0].Waves.Num() - 1;
		if (bFinal)
		{
			const FVrRuleset* Rules = GetVrRules();
			const FString Rival = Rules && !Rules->TiroFinal.Rival.IsEmpty() ? Rules->TiroFinal.Rival : FString(TEXT("corvo"));
			return TeachString(*FString::Printf(TEXT("story.bout4.intro.%s"), *Rival));
		}
		return TeachString(*FString::Printf(TEXT("story.bout%d.intro"), Games.Wave + 1));
	}
	if (Games.Phase == TEXT("aftermath"))
	{
		if (PhaseClock + 1.0e-9 < DayTuning.AftermathTabletS)
		{
			return TeachString(TEXT("story.aftermath.tablet"));
		}
		return TeachString(bFinalWon ? TEXT("story.aftermath.close") : TEXT("story.aftermath.close.lost"));
	}
	if (Games.Phase == TEXT("closed"))
	{
		return TeachString(TEXT("story.closed"));
	}
	if (Games.Phase == TEXT("intermission"))
	{
		return TeachString(IsPickOffered() ? TEXT("intermission.pick") : TEXT("intermission"));
	}
	if (Games.Phase == TEXT("lost"))
	{
		if (AreStoryStonesShown())
		{
			return TeachString(TEXT("lost.final"));
		}
		return TeachString(TEXT("offer.continue"));
	}
	if (Games.Phase != TEXT("teach"))
	{
		return FString();
	}
	switch (TeachStep)
	{
	case ETeachStep::Ward:
	case ETeachStep::Gap:
		return TeachString(TEXT("teach.ward"));
	case ETeachStep::Perfect:
	case ETeachStep::WardBeat:
		return TeachString(TEXT("teach.ward.perfect"));
	case ETeachStep::Sigil:
	case ETeachStep::SigilBeat:
		return TeachString(TEXT("teach.sigil"));
	case ETeachStep::Blink:
	case ETeachStep::BlinkBeat:
		return TeachString(TEXT("teach.blink"));
	default:
		return FString();
	}
}

FString FArenaSession::GetGlyphId() const
{
	if (Games.Phase == TEXT("teach") && (TeachStep == ETeachStep::Sigil || TeachStep == ETeachStep::SigilBeat))
	{
		return TEXT("sigil-line1");
	}
	return FString();
}

FString FArenaSession::GetWristCue() const
{
	if (WristUntil >= 0.0 && GetSimSeconds() <= WristUntil)
	{
		return WristText;
	}
	return FString();
}

FString FArenaSession::GetStateHash() const
{
	return StateHash(Games.State);
}

bool FArenaSession::IsPerfectRingVisible() const
{
	return PerfectRingUntil >= 0.0 && GetSimSeconds() <= PerfectRingUntil;
}

double FArenaSession::GlobImpactS() const
{
	const double Gap = FMath::Max(0.0, Tuning.WardDistanceM - KernelData().MageRadiusM - Tuning.GlobRadiusM);
	const double Step = SimDt();
	const int32 WindupSteps = FMath::Max(1, SimTicks(Tuning.WardWindupS));
	const double Move = Tuning.GlobSpeedMps * Step;
	const int32 Moves = Move > 1.0e-8 ? FMath::Max(1, FMath::CeilToInt(Gap / Move - 1.0e-9)) : 1;
	return static_cast<double>(WindupSteps + Moves - 1) * Step;
}

double FArenaSession::BlinkImpactS() const
{
	return static_cast<double>(FMath::Max(1, SimTicks(Tuning.BlinkWindupS))) * SimDt();
}

double FArenaSession::GetScheduledImpactS() const
{
	if (TeachStep == ETeachStep::Ward || TeachStep == ETeachStep::Perfect)
	{
		return GlobImpactS();
	}
	if (TeachStep == ETeachStep::Blink)
	{
		return BlinkImpactS();
	}
	return 0.0;
}

void FArenaSession::ClearThreats()
{
	Games.State.Projectiles.Reset();
	Games.State.Telegraphs.Reset();
	bGlobLive = false;
}

FSimVec FArenaSession::TeachAim() const
{
	if (const FActor* Dummy = SimFindActor(Games.State, DummyId))
	{
		return Dummy->Pos;
	}
	if (const FActor* Player = SimFindActor(Games.State, Games.PlayerId))
	{
		return FSimVec{Player->Pos.X + Tuning.WardDistanceM, Player->Pos.Y};
	}
	return FSimVec{IdleAimX, IdleAimY};
}

void FArenaSession::PlaceDummy(double DistanceM)
{
	FActor* Player = SimFindActor(Games.State, Games.PlayerId);
	if (!Player)
	{
		return;
	}
	const FSimVec Pos{Player->Pos.X + DistanceM, Player->Pos.Y};
	if (DummyId == 0 || !SimFindActor(Games.State, DummyId))
	{
		DummyId = AddMage(Games.State, 1, Pos, TEXT("Training dummy")).Id;
	}
	if (FActor* Dummy = SimFindActor(Games.State, DummyId))
	{
		Dummy->Pos = Pos;
		Dummy->PreviousPos = Pos;
		Dummy->Facing = FSimVec{-1.0, 0.0};
		Dummy->bDummy = true;
		Dummy->bDown = false;
		Dummy->Hp = Tuning.DummyHp;
		Dummy->MaxHp = Tuning.DummyHp;
		Dummy->Team = 1;
	}
}

void FArenaSession::SnapshotTeach()
{
	const FActor* Player = SimFindActor(Games.State, Games.PlayerId);
	const FActor* Dummy = SimFindActor(Games.State, DummyId);
	SnapHp = Player ? Player->Hp : 0.0;
	SnapDamage = Player ? Player->Metrics.DamageTaken : 0.0;
	SnapBlocks = Player ? Player->Metrics.Blocks : 0;
	SnapPerfects = Player ? Player->Metrics.Perfects : 0;
	SnapHits = Player ? Player->Metrics.Hits : 0;
	SnapRolls = Player ? Player->Metrics.Rolls : 0;
	SnapDummyHp = Dummy ? Dummy->Hp : 0.0;
	StepPad = ActivePad;
}

void FArenaSession::RefundTeachPlayer(FActor& Player, double BaseHp, double BaseDamage) const
{
	Player.Hp = BaseHp;
	Player.Metrics.DamageTaken = BaseDamage;
	if (Player.bDown)
	{
		Player.bDown = false;
		Player.bAbsorb = false;
	}
	if (Player.Water.RootUntil > Games.State.Tick)
	{
		Player.Water.RootUntil = Games.State.Tick;
	}
}

void FArenaSession::FreezeTeachClock(FActor& Player) const
{
	Player.WaveStartTick = Games.State.Tick;
	Player.ClockAdvanceTicks = 0;
	Player.Tier = 1;
}

void FArenaSession::ArmThreat()
{
	ClearThreats();
	FActor* Player = SimFindActor(Games.State, Games.PlayerId);
	if (!Player)
	{
		return;
	}
	if (TeachStep == ETeachStep::Ward || TeachStep == ETeachStep::Perfect)
	{
		PlaceDummy(Tuning.WardDistanceM);
		FActor* Dummy = SimFindActor(Games.State, DummyId);
		if (!Dummy)
		{
			return;
		}
		FTelegraph Marker;
		Marker.Id = Games.State.NextId++;
		Marker.ActivationId = Marker.Id;
		Marker.OwnerId = Dummy->Id;
		Marker.Family = TEXT("magic");
		Marker.Damage = Tuning.GlobDamage;
		Marker.Tier = 1;
		Marker.Source = Dummy->Pos;
		Marker.Origin = Dummy->Pos;
		Marker.Target = Player->Pos;
		Marker.StartTick = Games.State.Tick;
		Marker.ResolveTick = Games.State.Tick + 1000000;
		Marker.Kind = TEXT("projectile");
		Marker.SpeedMps = Tuning.GlobSpeedMps;
		Marker.RangeM = Tuning.WardDistanceM + Tuning.GlobRangeMarginM;
		Marker.WidthM = Tuning.GlobMarkerWidthM;
		Games.State.Telegraphs.Add(Marker);
		GlobSpawnTick = Games.State.Tick + FMath::Max(1, SimTicks(Tuning.WardWindupS));
		bGlobLive = false;
	}
	else if (TeachStep == ETeachStep::Blink)
	{
		PlaceDummy(Tuning.DummyDistanceM);
		FActor* Dummy = SimFindActor(Games.State, DummyId);
		if (!Dummy)
		{
			return;
		}
		FTelegraph Lane;
		Lane.Id = Games.State.NextId++;
		Lane.ActivationId = Lane.Id;
		Lane.OwnerId = Dummy->Id;
		Lane.Family = TEXT("unblockable");
		Lane.Damage = Tuning.BlinkDamage;
		Lane.Tier = 1;
		Lane.Source = Player->Pos;
		Lane.Origin = Player->Pos;
		Lane.Target = FSimVec{Player->Pos.X + Tuning.BlinkRangeM, Player->Pos.Y};
		Lane.StartTick = Games.State.Tick;
		Lane.ResolveTick = Games.State.Tick + FMath::Max(1, SimTicks(Tuning.BlinkWindupS));
		Lane.Kind = TEXT("lane");
		Lane.SpeedMps = 0.0;
		Lane.RangeM = Tuning.BlinkRangeM;
		Lane.WidthM = Tuning.BlinkWidthM;
		Games.State.Telegraphs.Add(Lane);
	}
	else if (TeachStep == ETeachStep::Sigil)
	{
		PlaceDummy(Tuning.DummyDistanceM);
	}
}

void FArenaSession::EnterTeachStep(ETeachStep Step)
{
	const ETeachStep Previous = TeachStep;
	TeachStep = Step;
	TeachClock = 0.0;
	Accumulator = 0.0;
	bCuePlayed = false;
	bCastPulse = false;
	bBlinkQueued = false;
	bBlinkEscaped = false;
	if (FActor* Player = SimFindActor(Games.State, Games.PlayerId))
	{
		Player->Mana = Player->MaxMana;
		Player->bAbsorb = false;
		Player->bAbsorbExhausted = false;
		Player->ReleaseTick = Games.State.Tick - 30;
		Player->Pending.Reset();
		if (Step == ETeachStep::Blink)
		{
			Player->Stamina = Player->MaxStamina;
		}
		SeatOnActivePad(*Player);
	}
	bAbsorb = false;
	bSuppressWard = false;
	if (Step == ETeachStep::Ward || Step == ETeachStep::Perfect || Step == ETeachStep::Sigil || Step == ETeachStep::Blink)
	{
		ArmThreat();
	}
	else
	{
		ClearThreats();
	}
	SnapshotTeach();
	if (Step != Previous)
	{
		if (Step == ETeachStep::Ward)
		{
			Note(TEXT("teach step ward"));
		}
		else if (Step == ETeachStep::Perfect)
		{
			Note(TEXT("teach step perfect"));
		}
		else if (Step == ETeachStep::Sigil)
		{
			Note(TEXT("teach step sigil"));
		}
		else if (Step == ETeachStep::Blink)
		{
			Note(TEXT("teach step blink"));
		}
	}
}

void FArenaSession::EnterTeach()
{
	if (Hands)
	{
		Hands->StopAll();
	}
	if (Wards)
	{
		Wards->GetDetector().ResetStream();
	}
	if (Sigils)
	{
		Sigils->ResetStrokes();
	}
	bAbsorb = false;
	bSuppressWard = false;
	bColdRaised = false;
	bOfferContinue = false;
	bCastPulse = false;
	bBlinkQueued = false;
	Games.Phase = TEXT("teach");
	TeachElapsed = 0.0;
	TeachClock = 0.0;
	Accumulator = 0.0;
	PerfectTryCount = 0;
	TeachStep = ETeachStep::None;
	// A new run from the teach forgets the stone pick: the preset default is back until the next intermission.
	StonePick.Reset();
	if (LoadPlayerPresetFile(TEXT("Teach kept the arc composition")))
	{
		const FComposition Composition = PlayerComposition();
		if (FActor* Player = SimFindActor(Games.State, Games.PlayerId))
		{
			Player->Water = NewWaterState(&Composition);
			Player->Mana = Player->MaxMana;
			SeatOnActivePad(*Player);
		}
	}
	EnterTeachStep(ETeachStep::Ward);
}

void FArenaSession::FinishTeach()
{
	Note(TEXT("teach complete"));
	Note(FString::Printf(TEXT("teach seconds=%.3f"), TeachElapsed));
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_TEACH_SECONDS %.3f"), TeachElapsed);
	TeachStep = ETeachStep::Done;
	bKeepChain = true;
	// The teach is done for good: the next launch offers Bout 1 instead of the prologue and the teach.
	BoutWave = 0;
	WriteSavedBout(0);
	EnterRitual();
}

void FArenaSession::ContinueOffer()
{
	// T21: the save stays. It remembers the bout to play, and that the teach is done, until the day moves on.
	bOfferContinue = false;
	EnterRitual();
}

void FArenaSession::MaybeSpawnGlob()
{
	if (bGlobLive || (TeachStep != ETeachStep::Ward && TeachStep != ETeachStep::Perfect))
	{
		return;
	}
	if (Games.State.Tick + 1 < GlobSpawnTick)
	{
		return;
	}
	FActor* Player = SimFindActor(Games.State, Games.PlayerId);
	FActor* Dummy = SimFindActor(Games.State, DummyId);
	if (!Player || !Dummy)
	{
		return;
	}
	Games.State.Telegraphs.Reset();
	FHit Hit;
	Hit.OwnerId = Dummy->Id;
	Hit.ActivationId = Games.State.NextId++;
	Hit.Damage = Tuning.GlobDamage;
	Hit.Family = TEXT("magic");
	Hit.Tier = 1;
	Hit.Source = Dummy->Pos;
	SpawnProjectile(
		Games.State,
		Hit,
		Dummy->Pos,
		SimSub(Player->Pos, Dummy->Pos),
		Tuning.GlobSpeedMps,
		Tuning.WardDistanceM + Tuning.GlobRangeMarginM,
		TOptional<double>(Tuning.GlobRadiusM),
		Player->Pos);
	bGlobLive = true;
}

bool FArenaSession::HasLane() const
{
	for (const FTelegraph& Telegraph : Games.State.Telegraphs)
	{
		if (Telegraph.Kind == TEXT("lane") || Telegraph.Family == TEXT("unblockable"))
		{
			return true;
		}
	}
	return false;
}

void FArenaSession::ObserveTeach()
{
	FActor* Player = SimFindActor(Games.State, Games.PlayerId);
	if (!Player)
	{
		return;
	}
	const double Damage = Player->Metrics.DamageTaken - SnapDamage;
	const bool bBlocked = Player->Metrics.Blocks > SnapBlocks
		|| (Player->Metrics.Hits > SnapHits && Damage < Tuning.GlobDamage * 0.5);
	const bool bPerfect = Player->Metrics.Perfects > SnapPerfects;
	const bool bHit = Player->Metrics.Hits > SnapHits;
	const FActor* Dummy = SimFindActor(Games.State, DummyId);
	const bool bDummyHurt = Dummy && Dummy->Hp < SnapDummyHp - 0.5;

	auto Miss = [this]()
	{
		Note(TEXT("teach miss"));
		EnterTeachStep(TeachStep);
	};
	auto To = [this](ETeachStep Next)
	{
		EnterTeachStep(Next);
	};

	if (TeachStep == ETeachStep::Gap)
	{
		if (TeachClock + 1.0e-9 >= Tuning.BeatAfterAbsorbS)
		{
			To(ETeachStep::Perfect);
		}
		return;
	}
	if (TeachStep == ETeachStep::WardBeat)
	{
		if (TeachClock + 1.0e-9 >= Tuning.BeatAfterWardS)
		{
			To(ETeachStep::Sigil);
		}
		return;
	}
	if (TeachStep == ETeachStep::SigilBeat)
	{
		if (TeachClock + 1.0e-9 >= Tuning.BeatAfterSigilS)
		{
			To(ETeachStep::Blink);
		}
		return;
	}
	if (TeachStep == ETeachStep::BlinkBeat)
	{
		if (TeachClock + 1.0e-9 >= Tuning.BeatAfterBlinkS)
		{
			FinishTeach();
		}
		return;
	}
	if (TeachStep == ETeachStep::Ward)
	{
		if (!bHit && TeachClock < GlobImpactS() + Tuning.GlobSettleS)
		{
			return;
		}
		if (bPerfect || bBlocked)
		{
			Note(TEXT("teach absorb"));
			To(ETeachStep::Gap);
		}
		else
		{
			Miss();
		}
		return;
	}
	if (TeachStep == ETeachStep::Perfect)
	{
		if (!bHit && TeachClock < GlobImpactS() + Tuning.GlobSettleS)
		{
			return;
		}
		if (bPerfect)
		{
			Note(TEXT("teach perfect"));
			PerfectRingUntil = GetSimSeconds() + Tuning.PerfectRingS;
			To(ETeachStep::WardBeat);
			return;
		}
		if (bBlocked)
		{
			++PerfectTryCount;
			if (PerfectTryCount >= Tuning.PerfectTries)
			{
				Note(FString::Printf(TEXT("teach perfect waived tries=%d"), PerfectTryCount));
				To(ETeachStep::WardBeat);
			}
			else
			{
				Note(TEXT("teach perfect retry"));
				EnterTeachStep(ETeachStep::Perfect);
			}
			return;
		}
		Miss();
		return;
	}
	if (TeachStep == ETeachStep::Sigil)
	{
		if (bDummyHurt)
		{
			Note(TEXT("teach sigil hit"));
			To(ETeachStep::SigilBeat);
			return;
		}
		if (TeachClock > Tuning.SigilPromptS + Tuning.SigilRetryS && !ClipPlaying())
		{
			Miss();
		}
		return;
	}
	if (TeachStep == ETeachStep::Blink)
	{
		const bool bMoved = bBlinkEscaped || (Player->Metrics.Rolls > SnapRolls && ActivePad != StepPad);
		if (Damage > 0.5)
		{
			Miss();
			return;
		}
		if (!HasLane() && bMoved)
		{
			Note(TEXT("teach blink"));
			To(ETeachStep::BlinkBeat);
			return;
		}
		if (!HasLane() && TeachClock > BlinkImpactS() + Tuning.BlinkSettleS)
		{
			Miss();
		}
	}
}

void FArenaSession::StepTeachTick()
{
	MaybeSpawnGlob();
	const FActor* Before = SimFindActor(Games.State, Games.PlayerId);
	if (!Before)
	{
		bRunning = false;
		return;
	}
	const int32 RollsBefore = Before->Metrics.Rolls;
	const bool bRollLegal = Games.State.Tick >= Before->RollUntil && Games.State.Tick >= Before->RecoveryUntil
		&& Before->Stamina >= KernelData().RollStaminaCost;
	const bool bWantRoll = bBlinkQueued && bRollLegal;
	if (bBlinkQueued && Games.State.Tick >= Before->RollUntil && Games.State.Tick >= Before->RecoveryUntil && !bRollLegal)
	{
		bBlinkQueued = false;
		Note(FString::Printf(TEXT("kernel roll rejected pad %d stamina=%.1f"), QueuedPad, Before->Stamina));
	}
	const int32 RollPad = QueuedPad;
	const FSimVec Aim = TeachAim();
	FInputFrame Input = SimIdleInput(Aim);
	Input.Aim = Aim;
	Input.Slot = CastSlot;
	Input.bCast = bCastPulse && !bWantRoll;
	Input.bAbsorb = bAbsorb && !bWantRoll;
	if (bWantRoll)
	{
		Input.bRoll = true;
		bBlinkQueued = false;
	}
	TMap<int32, FInputFrame> Inputs;
	Inputs.Add(Games.PlayerId, Input);
	StepArena(Games.State, Inputs, nullptr);
	FActor* After = SimFindActor(Games.State, Games.PlayerId);
	if (!After)
	{
		bRunning = false;
		return;
	}
	if (bWantRoll)
	{
		if (After->Metrics.Rolls > RollsBefore)
		{
			ActivePad = RollPad;
			if (TeachStep == ETeachStep::Blink && ActivePad != StepPad)
			{
				bBlinkEscaped = true;
			}
			++BlinkAccepts;
			bCameraDirty = true;
			CameraPad = RollPad;
			if (Blinks)
			{
				Blinks->SetActivePad(ActivePad);
			}
		}
		else
		{
			Note(FString::Printf(TEXT("kernel roll rejected pad %d stamina=%.1f"), RollPad, After->Stamina));
		}
	}
	SeatOnActivePad(*After);
	if (bCastPulse)
	{
		if (After->Metrics.Casts > CastsBeforePulse || After->Pending.IsSet())
		{
			bCastPulse = false;
		}
		else if (Games.State.Tick >= CastExpireTick)
		{
			bCastPulse = false;
			Note(TEXT("kernel cast rejected"));
		}
	}
	// Observe may open the next step and snapshot the hit. Refund the pre-hit
	// baseline, then snapshot again so the next tick is measured from full HP.
	const double BaseHp = SnapHp;
	const double BaseDamage = SnapDamage;
	ObserveTeach();
	if (FActor* Refunded = SimFindActor(Games.State, Games.PlayerId))
	{
		if (Games.Phase == TEXT("teach"))
		{
			RefundTeachPlayer(*Refunded, BaseHp, BaseDamage);
			FreezeTeachClock(*Refunded);
			SeatOnActivePad(*Refunded);
			SnapshotTeach();
		}
	}
	DrainEvents(SimFindActor(Games.State, Games.PlayerId));
}

void FArenaSession::DecideCold()
{
	if (!bScripted || bColdPlayed || ArcClock + 1.0e-9 < Tuning.ColdHoldS)
	{
		return;
	}
	bColdPlayed = true;
	PlayAction(TEXT("ward-raise"));
}

void FArenaSession::DecideTeach()
{
	if (!bScripted || bCuePlayed)
	{
		return;
	}
	const double Impact = GetScheduledImpactS();
	if (TeachStep == ETeachStep::Ward && TeachClock + 1.0e-9 >= Impact - Tuning.AbsorbLeadS)
	{
		bCuePlayed = true;
		PlayAction(TEXT("ward-raise"));
	}
	else if (TeachStep == ETeachStep::Perfect && TeachClock + 1.0e-9 >= Impact - Tuning.PerfectLeadS)
	{
		bCuePlayed = true;
		PlayAction(TEXT("ward-raise"));
	}
	else if (TeachStep == ETeachStep::Sigil && TeachClock + 1.0e-9 >= Tuning.SigilPromptS)
	{
		bCuePlayed = true;
		PlayAction(TEXT("sigil-line1"));
	}
	else if (TeachStep == ETeachStep::Blink && TeachClock + 1.0e-9 >= Impact - Tuning.BlinkLeadS)
	{
		bCuePlayed = true;
		PlayAction(TEXT("blink-left"));
	}
}

void FArenaSession::AdvanceArc(double DeltaSeconds)
{
	auto Pump = [this](TFunctionRef<void()> TickFn)
	{
		Accumulator = FMath::Min(Accumulator + 0.0, FrameCap());
		const double Step = SimDt();
		int32 Steps = 0;
		while (Accumulator + 1.0e-9 >= Step && Steps < 8 && bRunning)
		{
			if (!IsArcPhase(Games.Phase))
			{
				break;
			}
			Accumulator -= Step;
			TickFn();
			++Steps;
		}
	};

	if (AdvanceDay(DeltaSeconds))
	{
		return;
	}
	if (Games.Phase == TEXT("cold"))
	{
		ArcClock += DeltaSeconds;
		if (bScripted)
		{
			DecideCold();
		}
		else
		{
			PollComfortToggles();
		}
		if (bColdRaised)
		{
			// Cold only opens a first launch (no bout.txt), so the prologue follows it.
			bColdRaised = false;
			EnterPrologue();
			return;
		}
		Accumulator = FMath::Min(Accumulator + DeltaSeconds, FrameCap());
		Pump([this]()
		{
			TMap<int32, FInputFrame> Inputs;
			if (FActor* Player = SimFindActor(Games.State, Games.PlayerId))
			{
				FInputFrame Input = SimIdleInput(FSimVec{Player->Pos.X + 8.0, Player->Pos.Y});
				Inputs.Add(Games.PlayerId, Input);
			}
			StepArena(Games.State, Inputs, nullptr);
			if (FActor* Player = SimFindActor(Games.State, Games.PlayerId))
			{
				FreezeTeachClock(*Player);
				SeatOnActivePad(*Player);
			}
		});
		return;
	}

	if (Games.Phase == TEXT("offer") || Games.Phase == TEXT("intermission") || Games.Phase == TEXT("lost"))
	{
		// T21: "both palms to start over" needs palms raised in this phase. Palms still up from the ritual's offer (a
		// desktop clip's last frame stays the hand's latest) must not restart the teach in the next intermission.
		if (StartOverPhase != Games.Phase)
		{
			StartOverPhase = Games.Phase;
			bStartOverArmed = false;
		}
		const bool bBothPalms = BothPalmsRaised();
		if (!bBothPalms)
		{
			bStartOverArmed = true;
		}
		if (bBothPalms && bStartOverArmed)
		{
			OfferHold += DeltaSeconds;
			bOfferContinue = false;
			if (OfferHold + 1.0e-9 >= Tuning.OfferHoldS)
			{
				BoutWave = 0;
				ClearSavedBout();
				// Starting over forgets the day's arena flags too.
				Flags.Reset();
				IFileManager::Get().Delete(*FlagsFilePath(), false, true, true);
				Note(TEXT("day start over"));
				EnterTeach();
				return;
			}
		}
		else
		{
			OfferHold = 0.0;
			if (!bOfferContinue && Games.Phase == TEXT("intermission") && AdvancePick(DeltaSeconds))
			{
				return;
			}
			if (bOfferContinue)
			{
				bOfferContinue = false;
				if (Games.Phase == TEXT("intermission"))
				{
					if (IsPickOffered())
					{
						bPickClosed = true;
						Note(FString::Printf(TEXT("pick none (palm raised) tideOrbIV=%s"), *PlayerComposition().Branches.TideOrb));
					}
					ContinueIntermission();
				}
				else if (Games.Phase == TEXT("lost"))
				{
					if (bDay)
					{
						// Defeat offers the same bout again, with its intro.
						StartDayBout(Games.Wave);
					}
					else
					{
						Start(BoutSeed);
					}
				}
				else
				{
					ContinueOffer();
				}
				return;
			}
		}
		Accumulator = FMath::Min(Accumulator + DeltaSeconds, FrameCap());
		Pump([this]()
		{
			TMap<int32, FInputFrame> Inputs;
			if (const FActor* Player = SimFindActor(Games.State, Games.PlayerId))
			{
				Inputs.Add(Games.PlayerId, SimIdleInput(FSimVec{Player->Pos.X + 8.0, Player->Pos.Y}));
			}
			StepArena(Games.State, Inputs, nullptr);
			if (FActor* Player = SimFindActor(Games.State, Games.PlayerId))
			{
				FreezeTeachClock(*Player);
				SeatOnActivePad(*Player);
			}
		});
		return;
	}

	if (Games.Phase != TEXT("teach"))
	{
		return;
	}
	TeachClock += DeltaSeconds;
	TeachElapsed += DeltaSeconds;
	if (bAbsorb && !ClipPlaying() && !(Hands && Hands->IsWardHeld()))
	{
		bAbsorb = false;
	}
	DecideTeach();
	Accumulator = FMath::Min(Accumulator + DeltaSeconds, FrameCap());
	const double Step = SimDt();
	int32 Steps = 0;
	while (Accumulator + 1.0e-9 >= Step && Steps < 8 && bRunning && Games.Phase == TEXT("teach"))
	{
		Accumulator -= Step;
		StepTeachTick();
		++Steps;
	}
}

bool FArenaSession::LoadPlayerPresetFile(const TCHAR* Context)
{
	FPlayerPreset Loaded;
	FString Error;
	if (!LoadPlayerPreset(Loaded, Error, PresetPath))
	{
		UE_LOG(LogMageArena, Error, TEXT("%s: %s"), Context, *Error);
		return false;
	}
	PlayerPreset = MoveTemp(Loaded);
	return true;
}

FComposition FArenaSession::PlayerComposition() const
{
	return PlayerPreset.WithPick(StonePick);
}

bool FArenaSession::IsPickOffered() const
{
	return Games.Phase == TEXT("intermission") && PlayerPreset.Pick.bEnabled && !bPickClosed
		&& Games.Wave == PlayerPreset.Pick.AfterBout - 1;
}

double FArenaSession::GetPickHoldFraction() const
{
	if (PickHoldStone < 0 || Tuning.OfferHoldS <= 0.0)
	{
		return 0.0;
	}
	return FMath::Clamp(PickHold / Tuning.OfferHoldS, 0.0, 1.0);
}

FString FArenaSession::PickStoneKey(int32 Index) const
{
	if (Index == 0)
	{
		return FString::Printf(TEXT("pick.tideOrb.%s"), *PlayerPreset.Pick.Left);
	}
	if (Index == 2)
	{
		return FString::Printf(TEXT("pick.tideOrb.%s"), *PlayerPreset.Pick.Right);
	}
	return TEXT("pick.continue");
}

bool FArenaSession::AdvancePick(double DeltaSeconds)
{
	if (!IsPickOffered())
	{
		PickHold = 0.0;
		PickHoldStone = -1;
		return false;
	}
	PickClock += DeltaSeconds;
	// Same detection as the comfort stones: a palm or index tip within the stone radius. One palm held over one stone
	// for teach.json offerHoldS picks it; leaving the stone cancels the hold. Desktop reaches the stones through played
	// clips (Clips/stones), never a key shortcut into this function.
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
	if (Touched != PickHoldStone)
	{
		PickHold = 0.0;
		PickHoldStone = Touched;
		if (Touched >= 0)
		{
			Note(FString::Printf(TEXT("pick hold stone=%d"), Touched));
		}
	}
	if (Touched >= 0)
	{
		PickHold += DeltaSeconds;
		if (PickHold + 1.0e-9 >= Tuning.OfferHoldS)
		{
			ChoosePick(Touched, TEXT("stone"));
			return true;
		}
	}
	if (PickClock + 1.0e-9 >= PlayerPreset.Pick.TimeoutS)
	{
		bPickClosed = true;
		PickHold = 0.0;
		PickHoldStone = -1;
		Note(FString::Printf(TEXT("pick timeout t=%.2f tideOrbIV=%s"), PickClock, *PlayerComposition().Branches.TideOrb));
		ContinueIntermission();
		return true;
	}
	return false;
}

void FArenaSession::ChoosePick(int32 Stone, const TCHAR* Reason)
{
	const FStonePick& Pick = PlayerPreset.Pick;
	if (Stone == 0)
	{
		StonePick = Pick.Left;
	}
	else if (Stone == 2)
	{
		StonePick = Pick.Right;
	}
	// The centre stone continues and keeps whatever the composition has.
	const FComposition Composition = PlayerComposition();
	if (FActor* Player = SimFindActor(Games.State, Games.PlayerId))
	{
		// The player is idle in the intermission. The branch is read when a tier IV orb is cast, so it applies from the
		// next bout on.
		Player->Water.Composition.Branches.TideOrb = Composition.Branches.TideOrb;
	}
	bPickClosed = true;
	PickHold = 0.0;
	PickHoldStone = -1;
	Note(FString::Printf(TEXT("pick %s stone=%d t=%.2f tideOrbIV=%s"), Reason, Stone, PickClock, *Composition.Branches.TideOrb));
	ContinueIntermission();
}

void FArenaSession::ContinueIntermission()
{
	// The day picks the opponent school per bout (Brennic's Fire semifinal, the tiroFinal school). SetBout pins it.
	if (!bSchoolPinned && DayFire.IsValidIndex(Games.Wave + 1))
	{
		Games.bFireMages = DayFire[Games.Wave + 1];
	}
	if (TryAdvanceGames(Games))
	{
		BoutWave = Games.Wave;
		WriteSavedBout(BoutWave);
		ResetBoutScript();
		if (bDay)
		{
			EnterIntro();
		}
	}
	else
	{
		Games.Phase = TEXT("complete");
	}
}
