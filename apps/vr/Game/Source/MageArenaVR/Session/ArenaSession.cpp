#include "Session/ArenaSession.h"

#include "Session/SessionPresentation.h"
#include "Session/SettingsCapture.h"
#include "Session/TeachCapture.h"
#include "Session/Wave1Capture.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Gestures/SigilRecognizerSubsystem.h"
#include "Gestures/StaffDetector.h"
#include "Gestures/WardDetector.h"
#include "HAL/IConsoleManager.h"
#include "Hands/ClipVariant.h"
#include "Hands/HandClipPlayer.h"
#include "Hands/HandInputSubsystem.h"
#include "Hands/MageArenaPawn.h"
#include "Hands/MageSettings.h"
#include "Kernel/ArenaKernel.h"
#include "Kernel/Catalog.h"
#include "Kernel/Enemies.h"
#include "Kernel/KernelData.h"
#include "MageArenaVR.h"
#include "Misc/Parse.h"

namespace
{
const FComposition* FindRotation()
{
	for (const FComposition& Preset : KernelData().Presets)
	{
		if (Preset.Name == TEXT("Rotation"))
		{
			return &Preset;
		}
	}
	return nullptr;
}

double NearestLiving(const FGames& Games, const FActor& Player, int32* OutWithin, double WithinM)
{
	double Best = 1.0e9;
	int32 Within = 0;
	for (const FActor& Actor : Games.State.Actors)
	{
		if (Actor.Id == Player.Id || Actor.bDown || Actor.Team == Player.Team)
		{
			continue;
		}
		const double Distance = SimDistance(Player.Pos, Actor.Pos);
		Best = FMath::Min(Best, Distance);
		if (Distance <= WithinM)
		{
			++Within;
		}
	}
	if (OutWithin)
	{
		*OutWithin = Within;
	}
	return Best;
}

bool SpellReady(const FGames& Games, const FActor& Player, int32 Slot)
{
	const FSpell* Spell = SpellFor(Player, Slot);
	if (!Spell)
	{
		return false;
	}
	if (Player.Mana < Spell->Mana)
	{
		return false;
	}
	return Games.State.Tick >= SimCooldownUntil(Player.Water, Spell->Line);
}

UWorld* FindGameWorld()
{
	if (!GEngine)
	{
		return nullptr;
	}
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (World && World->IsGameWorld())
		{
			return World;
		}
	}
	return nullptr;
}

void StartSessionCommand(const TArray<FString>& Args)
{
	UWorld* World = FindGameWorld();
	UArenaSessionSubsystem* Session = World ? World->GetSubsystem<UArenaSessionSubsystem>() : nullptr;
	if (!Session)
	{
		UE_LOG(LogMageArena, Error, TEXT("MageArena.Session.Start: no game world"));
		return;
	}
	uint32 Seed = 1;
	if (Args.Num() > 0)
	{
		Seed = static_cast<uint32>(FCString::Atoi(*Args[0]));
	}
	Session->Start(Seed, false);
}

FAutoConsoleCommand GSessionStart(
	TEXT("MageArena.Session.Start"),
	TEXT("Start pinned Tiro wave 1 (Rotation) against the human player. Optional seed."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&StartSessionCommand));

void SkipTeachCommand(const TArray<FString>& Args)
{
	(void)Args;
	UWorld* World = FindGameWorld();
	UArenaSessionSubsystem* Session = World ? World->GetSubsystem<UArenaSessionSubsystem>() : nullptr;
	if (!Session)
	{
		UE_LOG(LogMageArena, Error, TEXT("MageArena.Session.SkipTeach: no game world"));
		return;
	}
	Session->GetSession().SkipTeach();
	UE_LOG(LogMageArena, Log, TEXT("MageArena.Session.SkipTeach"));
}

FAutoConsoleCommand GSessionSkipTeach(
	TEXT("MageArena.Session.SkipTeach"),
	TEXT("Testing only. Skip cold start and the teach, and start the bout."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&SkipTeachCommand));
}

FArenaSession::FArenaSession() = default;

FArenaSession::~FArenaSession()
{
	Unbind();
}

void FArenaSession::Bind(
	UHandInputSubsystem* InHands,
	USigilRecognizerSubsystem* InSigils,
	UWardDetectorSubsystem* InWards,
	UBlinkDetectorSubsystem* InBlinks,
	UStaffDetectorSubsystem* InStaff)
{
	Unbind();
	Hands = InHands;
	Sigils = InSigils;
	Wards = InWards;
	Blinks = InBlinks;
	Staff = InStaff;
	if (Blinks)
	{
		Blinks->BindToHands(Hands);
		BlinkHandle = Blinks->OnBlink.AddRaw(this, &FArenaSession::HandleBlink);
		BoltHandle = Blinks->OnBolt.AddRaw(this, &FArenaSession::HandleBolt);
	}
	if (Sigils)
	{
		Sigils->BindToHands(Hands);
		SigilHandle = Sigils->OnSigilCast.AddRaw(this, &FArenaSession::HandleSigil);
	}
	if (Wards)
	{
		Wards->BindToHands(Hands);
		WardRaisedHandle = Wards->OnWardRaised.AddRaw(this, &FArenaSession::HandleWardRaised);
		WardLoweredHandle = Wards->OnWardLowered.AddRaw(this, &FArenaSession::HandleWardLowered);
	}
	if (Staff)
	{
		Staff->BindToHands(Hands);
		StaffPlantHandle = Staff->OnPlant.AddRaw(this, &FArenaSession::HandleStaffPlant);
		StaffLiftHandle = Staff->OnLift.AddRaw(this, &FArenaSession::HandleStaffLift);
	}
	if (Hands)
	{
		HandFrameHandle = Hands->OnHandFrame().AddRaw(this, &FArenaSession::NoteHandFrame);
	}
}

void FArenaSession::Unbind()
{
	if (Sigils)
	{
		Sigils->OnSigilCast.Remove(SigilHandle);
	}
	if (Wards)
	{
		Wards->OnWardRaised.Remove(WardRaisedHandle);
		Wards->OnWardLowered.Remove(WardLoweredHandle);
	}
	if (Blinks)
	{
		Blinks->OnBlink.Remove(BlinkHandle);
		Blinks->OnBolt.Remove(BoltHandle);
	}
	if (Staff)
	{
		Staff->OnPlant.Remove(StaffPlantHandle);
		Staff->OnLift.Remove(StaffLiftHandle);
	}
	if (Hands)
	{
		Hands->OnHandFrame().Remove(HandFrameHandle);
	}
	SigilHandle.Reset();
	WardRaisedHandle.Reset();
	WardLoweredHandle.Reset();
	BlinkHandle.Reset();
	BoltHandle.Reset();
	StaffPlantHandle.Reset();
	StaffLiftHandle.Reset();
	HandFrameHandle.Reset();
	Hands = nullptr;
	Sigils = nullptr;
	Wards = nullptr;
	Blinks = nullptr;
	Staff = nullptr;
}

bool FArenaSession::Start(uint32 Seed)
{
	if (!KernelData().bReady)
	{
		UE_LOG(LogMageArena, Error, TEXT("Session start failed: kernel data is not ready (%s)"), *KernelData().Error);
		return false;
	}
	const FComposition* Preset = FindRotation();
	if (!Preset)
	{
		UE_LOG(LogMageArena, Error, TEXT("Session start failed: Rotation preset is missing"));
		return false;
	}
	FString Error;
	bHasLayout = Layout.LoadFromFile(FArenaLayout::DefaultFilePath(), Error);
	if (!bHasLayout)
	{
		UE_LOG(LogMageArena, Error, TEXT("Session layout failed: %s"), *Error);
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
		UE_LOG(LogMageArena, Error, TEXT("Session start failed: %s"), *RulesError);
		return false;
	}
	ActivePad = 1;
	ApplyComfort(Rules);
	if (!IsTiroWave(BoutWave))
	{
		const int32 Waves = (KernelData().bReady && KernelData().ArenaTiers.Num() > 0)
			? KernelData().ArenaTiers[0].Waves.Num()
			: 0;
		UE_LOG(LogMageArena, Error, TEXT("Session bout %d is outside Tiro waves [0, %d); refusing to spawn"), BoutWave, Waves);
		return false;
	}
	if (!TryCreateGames(Games, Seed, Preset, BoutWave, false, bFireMages, &Rules))
	{
		UE_LOG(LogMageArena, Error, TEXT("Session start failed: TryCreateGames"));
		return false;
	}
	bRunning = true;
	bPaused = false;
	bFrameHold = false;
	bLoggedEnd = false;
	Accumulator = 0.0;
	bAbsorb = false;
	bSuppressWard = false;
	bBlinkQueued = false;
	bCastPulse = false;
	bPlantPulse = false;
	bLiftPulse = false;
	bBoltFlicked = false;
	BoltFlickSim = -1.0;
	bLastCastSigil = false;
	ActivePad = 1;
	BlinkAccepts = 0;
	bCameraDirty = false;
	CameraPad = 1;
	bWardStarted = false;
	bWardReleased = false;
	bTideStarted = false;
	SideToggle = 0;
	bSawSigilHit = false;
	bSawWardOnStone = false;
	EventCursor = Games.State.Events.Num();
	if (!bKeepChain)
	{
		Chain.Reset();
	}
	bKeepChain = false;
	bResumeGate = false;
	bResumeCounting = false;
	ResumeElapsed = 0.0;
	LostTrackingS = 0.0;
	bHandLeftSeen = false;
	bHandRightSeen = false;
	bHandLeftTracked = false;
	bHandRightTracked = false;
	TeachStep = ETeachStep::None;
	TeachClock = 0.0;
	ArcClock = 0.0;
	OfferHold = 0.0;
	bCuePlayed = false;
	bColdRaised = false;
	bColdPlayed = false;
	bStoneHeld[0] = false;
	bStoneHeld[1] = false;
	bStoneHeld[2] = false;
	bOfferContinue = false;
	bFlowCue = false;
	bClockCue = false;
	WristUntil = -1.0;
	WristText.Reset();
	PerfectRingUntil = -1.0;
	DummyId = 0;
	// TeachElapsed stays. The duration test reads it after the teach calls Start.
	LoadTeachData();
	LoadStrings();
	ViewAim = FSimVec{IdleAimX, IdleAimY};
	AimPoint = ViewAim;
	bClipWasPlaying = false;
	SplitCasts = 0;
	StaffPlants = 0;
	bWingSeat = bFireMages;
	bWingSeated = false;
	SunfallPad = -1;
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
	if (Blinks)
	{
		Blinks->ResetDetector();
		Blinks->SetActivePad(1);
	}
	if (FActor* Player = SimFindActor(Games.State, Games.PlayerId))
	{
		SeatOnActivePad(*Player);
	}
	Note(FString::Printf(TEXT("Session start seed=%u wave=%d preset=Rotation"), Seed, BoutWave + 1));
	return true;
}

void FArenaSession::SetBout(int32 WaveIndex, bool bInFireMages)
{
	BoutWave = WaveIndex;
	bFireMages = bInFireMages;
}

void FArenaSession::SetPolicy(const FSeatedPolicy& InPolicy)
{
	Policy = InPolicy;
}

void FArenaSession::SetRulesOverride(const FVrRuleset& Rules)
{
	RulesOverride = Rules;
}

void FArenaSession::SetQuiet(bool bInQuiet)
{
	bQuiet = bInQuiet;
}

int32 FArenaSession::GetWallsRaised() const
{
	const FVrRuleset* Rules = GetVrRules();
	return Rules ? Rules->WallsRaised : 0;
}

void FArenaSession::SetScripted(bool bInScripted)
{
	bScripted = bInScripted;
}

void FArenaSession::SetPaused(bool bInPaused)
{
	if (bPaused == bInPaused)
	{
		return;
	}
	bPaused = bInPaused;
	if (bPaused)
	{
		Accumulator = 0.0;
	}
	Note(bPaused ? TEXT("Session pause") : TEXT("Session resume"));
}

void FArenaSession::TogglePause()
{
	// Esc stays instant. A safety pause drops the palm gate and the count.
	if (bResumeGate || bResumeCounting)
	{
		bResumeGate = false;
		bResumeCounting = false;
		ResumeElapsed = 0.0;
	}
	SetPaused(!bPaused);
}

void FArenaSession::SetViewAim(const FSimVec& Aim)
{
	ViewAim = Aim;
}

double FArenaSession::GetSimSeconds() const
{
	return Games.State.Tick * SimDt();
}

bool FArenaSession::ConsumeCameraPad(int32& OutPad)
{
	if (!bCameraDirty)
	{
		return false;
	}
	bCameraDirty = false;
	OutPad = CameraPad;
	return true;
}

FVector FArenaSession::KernelToUnrealCm(const FSimVec& Pos) const
{
	const FRunePad* Centre = bHasLayout ? Layout.FindPad(1) : nullptr;
	const FSimVec Spawn = KernelData().PlayerSpawn;
	const double Xm = (Centre ? Centre->PositionM.X : -12.0) + (Pos.X - Spawn.X);
	const double Ym = (Centre ? Centre->PositionM.Y : 0.0) + (Pos.Y - Spawn.Y);
	const double Zm = bHasLayout ? Layout.SurfaceHeightM(Xm, Ym) : 0.0;
	return FVector(Xm, Ym, Zm) * 100.0;
}

void FArenaSession::ApplyComfort(FVrRuleset& Rules) const
{
	FMageSettings::EnsureLoaded();
	Rules.bNarrow = FMageSettings::IsNarrow();
	Rules.bGentle = FMageSettings::IsGentle();
	if (!Rules.bNarrow)
	{
		return;
	}
	const int32 PadIndex = ActivePad >= 0 ? ActivePad : 1;
	Rules.NarrowOrigin = PadKernel(PadIndex);
	FSimVec Forward{1.0, 0.0};
	if (bHasLayout)
	{
		if (const FRunePad* Pad = Layout.FindPad(PadIndex))
		{
			const FVector Flat = Layout.FlatForwardM(*Pad);
			if (!Flat.IsNearlyZero())
			{
				Forward.X = Flat.X;
				Forward.Y = Flat.Y;
			}
		}
	}
	Rules.NarrowForward = Forward;
}

void FArenaSession::PollComfortToggles()
{
	// The settings still plays a sigil during cold. That stroke must not palm a stone.
	if (FParse::Param(FCommandLine::Get(), TEXT("MageArenaSettingsCapture")))
	{
		return;
	}
	if (bScripted || !Hands || Games.Phase != TEXT("cold"))
	{
		return;
	}
	// Clip centimetres, seated origin. Clear of the scripted ward palm at (36, -12, 36).
	const FVector Stones[] = {
		FVector(55.0, -28.0, 18.0),
		FVector(55.0, 0.0, 18.0),
		FVector(55.0, 28.0, 18.0),
	};
	const double Radius = 6.0;
	bool bInside[3] = {false, false, false};
	auto Consider = [&](const FHandFrame& Frame)
	{
		const int32 Palm = static_cast<int32>(EHandKeypoint::Palm);
		const int32 Tip = static_cast<int32>(EHandKeypoint::IndexTip);
		for (int32 Index = 0; Index < 3; ++Index)
		{
			if (Frame.Joints.IsValidIndex(Palm) && FVector::Dist(Frame.Joints[Palm].Location, Stones[Index]) <= Radius)
			{
				bInside[Index] = true;
			}
			if (Frame.Joints.IsValidIndex(Tip) && FVector::Dist(Frame.Joints[Tip].Location, Stones[Index]) <= Radius)
			{
				bInside[Index] = true;
			}
		}
	};
	FHandFrame Left;
	FHandFrame Right;
	if (Hands->GetLatest(EControllerHand::Left, Left))
	{
		Consider(Left);
	}
	if (Hands->GetLatest(EControllerHand::Right, Right))
	{
		Consider(Right);
	}
	for (int32 Index = 0; Index < 3; ++Index)
	{
		if (bInside[Index] && !bStoneHeld[Index])
		{
			if (Index == 0)
			{
				FMageSettings::ToggleHand();
			}
			else if (Index == 1)
			{
				FMageSettings::ToggleFov();
			}
			else
			{
				FMageSettings::ToggleGentle();
			}
			bStoneHeld[Index] = true;
			Note(FString::Printf(TEXT("settings stone %d"), Index));
		}
		else if (!bInside[Index])
		{
			bStoneHeld[Index] = false;
		}
	}
}

FSimVec FArenaSession::PadKernel(int32 PadIndex) const
{
	const FSimVec Spawn = KernelData().PlayerSpawn;
	const FRunePad* Centre = bHasLayout ? Layout.FindPad(1) : nullptr;
	const FRunePad* Pad = bHasLayout ? Layout.FindPad(PadIndex) : nullptr;
	if (!Centre || !Pad)
	{
		return Spawn;
	}
	return FSimVec{
		Spawn.X + (Pad->PositionM.X - Centre->PositionM.X),
		Spawn.Y + (Pad->PositionM.Y - Centre->PositionM.Y)};
}

void FArenaSession::Note(const FString& Line)
{
	Chain.Add(Line);
	if (!bQuiet)
	{
		UE_LOG(LogMageArena, Log, TEXT("MAGEVR_CHAIN %s"), *Line);
	}
}

void FArenaSession::PlayAction(FName Action)
{
	if (!Hands)
	{
		return;
	}
	bBoltFlicked = false;
	const FString Name = Action.ToString();
	if (Name.StartsWith(TEXT("staff")) || Name == TEXT("mudra"))
	{
		bAbsorb = false;
		bSuppressWard = false;
		bWardStarted = false;
		bWardReleased = true;
	}
	Hands->PlayQuickAction(Action, EClipVariant::Normal);
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_SCRIPT play %s"), *Action.ToString());
}

FString FArenaSession::ClipAction() const
{
	return Hands ? Hands->GetActionName() : FString();
}

bool FArenaSession::ClipPlaying() const
{
	return Hands && Hands->IsActionPlaying();
}

bool FArenaSession::IsStaffPlanted() const
{
	const FVrRuleset* Rules = GetVrRules();
	return Rules && Rules->IsPlanted(Games.PlayerId);
}

bool FArenaSession::IsSplitPose() const
{
	return Hands && Hands->IsWardHeld() && Hands->IsActionPlaying() && Hands->GetActionName().StartsWith(TEXT("sigil"));
}

bool FArenaSession::WouldSplitRefuse(int32 Slot) const
{
	const FActor* Player = SimFindActor(Games.State, Games.PlayerId);
	const FVrRuleset* Rules = GetVrRules();
	if (!Player || !bAbsorb || IsStaffPlanted() || !Rules || !Rules->Split.bEnabled)
	{
		return false;
	}
	if (Player->Water.Flow >= KernelData().FlowMax)
	{
		return true;
	}
	const FSpell* Spell = SpellFor(*Player, Slot);
	return Spell && Spell->Tier > Rules->Split.MaxTier;
}

void FArenaSession::BeginCast(int32 Slot, const TCHAR* Gesture, bool bKeepWard)
{
	const FActor* Player = SimFindActor(Games.State, Games.PlayerId);
	bCastPulse = true;
	CastSlot = Slot;
	CastsBeforePulse = Player ? Player->Metrics.Casts : 0;
	CastExpireTick = Games.State.Tick + 90;
	bLastCastSigil = Slot != 0;
	if (bKeepWard && bAbsorb)
	{
		++SplitCasts;
		Note(FString::Printf(TEXT("script split slot %d"), Slot));
	}
	else
	{
		bSuppressWard = true;
		bAbsorb = false;
	}
	Note(Gesture);
}

void FArenaSession::HandleSigil(FName Line, float Score, double LatencyMs)
{
	const FString Action = ClipAction();
	if (Action == TEXT("bolt") || Action.StartsWith(TEXT("blink")))
	{
		return;
	}
	int32 Slot = 0;
	if (Line == TEXT("line1"))
	{
		Slot = 1;
	}
	else if (Line == TEXT("line2"))
	{
		Slot = 2;
	}
	else if (Line == TEXT("line3"))
	{
		Slot = 3;
	}
	else
	{
		return;
	}
	if (WouldSplitRefuse(Slot))
	{
		Note(FString::Printf(TEXT("script split refused slot %d"), Slot));
		return;
	}
	const bool bKeepWard = bAbsorb && !IsStaffPlanted();
	BeginCast(Slot, *FString::Printf(TEXT("gesture sigil %s score=%.2f latency=%.0fms -> slot %d"),
		*Line.ToString(), Score, LatencyMs, Slot), bKeepWard);
}

void FArenaSession::HandleWardRaised(double OnsetTime, FVector Facing)
{
	const bool bBoth = ClipAction() == TEXT("both-palms");
	if (bResumeGate || bBoth)
	{
		return;
	}
	if (Games.Phase == TEXT("cold"))
	{
		bColdRaised = true;
		return;
	}
	if (Games.Phase == TEXT("offer"))
	{
		bOfferContinue = true;
		return;
	}
	if (bSuppressWard)
	{
		return;
	}
	bAbsorb = true;
	Note(FString::Printf(TEXT("gesture ward onset=%.3f -> absorb facing=(%.2f,%.2f,%.2f)"),
		OnsetTime, Facing.X, Facing.Y, Facing.Z));
}

void FArenaSession::HandleWardLowered()
{
	bAbsorb = false;
	bSuppressWard = false;
	Note(TEXT("gesture ward lower"));
}

void FArenaSession::HandleBlink(const FBlinkEvent& Event)
{
	const TCHAR* Name = TEXT("blink-back");
	if (Event.Direction == EBlinkDirection::Left)
	{
		Name = TEXT("blink-left");
	}
	else if (Event.Direction == EBlinkDirection::Right)
	{
		Name = TEXT("blink-right");
	}
	Note(FString::Printf(TEXT("gesture %s -> pad %d"), Name, Event.PadIndex));
	bBlinkQueued = true;
	QueuedPad = Event.PadIndex;
	bSuppressWard = true;
	bAbsorb = false;
}

void FArenaSession::HandleBolt(const FBoltFlickEvent& Event)
{
	if (ClipAction().StartsWith(TEXT("sigil")))
	{
		return;
	}
	bBoltFlicked = true;
	BoltFlickSim = GetSimSeconds();
	const FActor* Player = SimFindActor(Games.State, Games.PlayerId);
	if (Player && NearestLiving(Games, *Player, nullptr, 0.0) <= KernelData().StaffRangeM + 0.05)
	{
		Note(FString::Printf(TEXT("gesture bolt -> suppressed in melee yaw=%.1f"), Event.YawDegrees));
		return;
	}
	if (WouldSplitRefuse(0))
	{
		Note(TEXT("script split refused slot 0"));
		return;
	}
	const bool bKeepWard = bAbsorb && !IsStaffPlanted();
	BeginCast(0, *FString::Printf(TEXT("gesture bolt -> slot 0 yaw=%.1f"), Event.YawDegrees), bKeepWard);
}

void FArenaSession::HandleStaffPlant()
{
	bPlantPulse = true;
	++StaffPlants;
	Note(TEXT("gesture staff-plant"));
}

void FArenaSession::HandleStaffLift()
{
	bLiftPulse = true;
	Note(TEXT("gesture staff-lift"));
}

void FArenaSession::ScanThreats(double& MeleeEta, double& ProjectileEta) const
{
	MeleeEta = 1.0e6;
	ProjectileEta = 1.0e6;
	const FActor* Player = SimFindActor(Games.State, Games.PlayerId);
	if (!Player)
	{
		return;
	}
	const double Step = SimDt();
	for (const FTelegraph& Telegraph : Games.State.Telegraphs)
	{
		if (Telegraph.OwnerId == Games.PlayerId)
		{
			continue;
		}
		const double UntilResolve = static_cast<double>(Telegraph.ResolveTick - Games.State.Tick) * Step;
		if (UntilResolve < 0.0)
		{
			continue;
		}
		if (Telegraph.Kind == TEXT("projectile"))
		{
			if (SimDistance(Telegraph.Target, Player->Pos) > 1.35)
			{
				continue;
			}
			const double Flight = Telegraph.SpeedMps > 0.1 ? SimDistance(Telegraph.Origin, Telegraph.Target) / Telegraph.SpeedMps : 0.0;
			ProjectileEta = FMath::Min(ProjectileEta, UntilResolve + Flight);
			continue;
		}
		const double Reach = Telegraph.RangeM + Player->Radius + 0.45;
		const bool bClose = SimDistance(Telegraph.Origin, Player->Pos) <= Reach
			|| SimDistance(Telegraph.Target, Player->Pos) <= 0.9;
		if (!bClose)
		{
			continue;
		}
		if (Telegraph.Family == TEXT("magic"))
		{
			ProjectileEta = FMath::Min(ProjectileEta, UntilResolve);
		}
		else
		{
			MeleeEta = FMath::Min(MeleeEta, UntilResolve);
		}
	}
	for (const FProjectile& Projectile : Games.State.Projectiles)
	{
		if (Projectile.OwnerId == Games.PlayerId)
		{
			continue;
		}
		const double Speed = SimLength(Projectile.Velocity);
		if (Speed < 0.1)
		{
			continue;
		}
		const FSimVec ToPlayer = SimSub(Player->Pos, Projectile.Pos);
		const FSimVec Direction = SimUnit(Projectile.Velocity);
		const double Along = SimDot(ToPlayer, Direction);
		if (Along < 0.0)
		{
			continue;
		}
		const FSimVec Closest = SimAdd(Projectile.Pos, SimScale(Direction, Along));
		if (SimDistance(Closest, Player->Pos) > Player->Radius + Projectile.Radius + 0.4)
		{
			continue;
		}
		ProjectileEta = FMath::Min(ProjectileEta, Along / Speed);
	}
}

bool FArenaSession::CanBlink(const FActor& Player) const
{
	const FKernelData& Data = KernelData();
	if (IsStaffPlanted() || !Hands || Player.Stamina < Data.RollStaminaCost || bBlinkQueued)
	{
		return false;
	}
	if (ClipPlaying() && ClipAction().StartsWith(TEXT("blink")))
	{
		return false;
	}
	// The flick lands ~0.21 s after the clip starts. Recovery is 0.50 s, so a second spear half a
	// second behind the first is only iframeable if the clip is already in flight when recovery ends.
	const double Step = SimDt();
	const double RecoverIn = FMath::Max(0.0, static_cast<double>(Player.RecoveryUntil - Games.State.Tick) * Step);
	const double RollIn = FMath::Max(0.0, static_cast<double>(Player.RollUntil - Games.State.Tick) * Step);
	return RecoverIn <= 0.21 && RollIn <= 0.21;
}

double FArenaSession::SoonestThreat(double MeleeEta, double ProjectileEta, bool bMeleeOnly) const
{
	double Best = bMeleeOnly ? MeleeEta : FMath::Min(MeleeEta, ProjectileEta);
	const FActor* Player = SimFindActor(Games.State, Games.PlayerId);
	if (!Player)
	{
		return Best;
	}
	const double Step = SimDt();
	const FKernelData& Data = KernelData();
	const FVrRuleset* Rules = Games.VrRules.IsSet() ? &Games.VrRules.GetValue() : nullptr;
	for (const FActor& Actor : Games.State.Actors)
	{
		if (Actor.Id == Player->Id || Actor.bDown || Actor.Team == Player->Team || !Actor.Enemy.IsSet())
		{
			continue;
		}
		const FEnemySpec* Spec = EnemySpecFor(Actor);
		if (!Spec || Spec->Attacks.Num() == 0)
		{
			continue;
		}
		// A throw-mode conscript is a projectile for the existing script. Thresholds stay put.
		FAttackSpec Attack = Spec->Attacks[Actor.Enemy->AttackIndex % Spec->Attacks.Num()];
		if (Rules && Rules->bActive)
		{
			if (const FVrAttackMode* Mode = Rules->FindThrow(Spec->Id))
			{
				Attack.ProjectileMps = Mode->ProjectileMps;
				Attack.RangeM = Mode->RangeM;
			}
		}
		const bool bRanged = Attack.ProjectileMps.IsSet();
		if (bMeleeOnly && bRanged)
		{
			continue;
		}
		bool bWinding = false;
		for (const FTelegraph& Telegraph : Games.State.Telegraphs)
		{
			if (Telegraph.OwnerId == Actor.Id)
			{
				bWinding = true;
				break;
			}
		}
		if (bWinding)
		{
			continue;
		}
		const double Range = Attack.RangeM.Get(bRanged ? Data.RangedRangeM : Data.DefaultMeleeRangeM);
		const double Distance = SimDistance(Player->Pos, Actor.Pos);
		const double Speed = FMath::Max(Actor.SpeedMps.Get(Spec->SpeedMps), 0.1);
		const double ReadyIn = FMath::Max(0.0, static_cast<double>(Actor.Enemy->ReadyTick - Games.State.Tick) * Step);
		double Travel = Distance > Range ? (Distance - Range) / Speed : 0.0;
		// A conscript who just lunged is walking out to the backoff line. ReadyTick already waits that out,
		// but the body is still inside spear range, so a plain "inside range means zero travel" estimate
		// would swing during the walk back.
		if (Spec->Id == TEXT("conscript") && Games.State.Tick < Actor.Enemy->BackoffUntil)
		{
			const double BackoffLeft = static_cast<double>(Actor.Enemy->BackoffUntil - Games.State.Tick) * Step;
			const double TimeToEdge = FMath::Max(0.0, Data.ConscriptBackoffM - Distance) / Speed;
			const double PosWhenReady = Distance + Speed * FMath::Min(TimeToEdge, BackoffLeft);
			Travel = FMath::Max(0.0, PosWhenReady - Range) / Speed;
		}
		double Flight = 0.0;
		if (bRanged)
		{
			Flight = FMath::Min(Distance, Range) / FMath::Max(Attack.ProjectileMps.GetValue(), 0.1);
		}
		Best = FMath::Min(Best, ReadyIn + Travel + Attack.WindupS + Flight);
	}
	return Best;
}

bool FArenaSession::TryScriptBlink()
{
	const FActor* Player = SimFindActor(Games.State, Games.PlayerId);
	if (!Player || !CanBlink(*Player))
	{
		return false;
	}
	if (Hands->IsWardHeld())
	{
		Hands->SetWardHeld(false, EClipVariant::Normal);
	}
	bSuppressWard = true;
	bAbsorb = false;

	auto Clearance = [this](const FSimVec& Pos) {
		double Nearest = 1.0e9;
		for (const FActor& Actor : Games.State.Actors)
		{
			if (Actor.Id == Games.PlayerId || Actor.bDown || Actor.Team == 0)
			{
				continue;
			}
			Nearest = FMath::Min(Nearest, SimDistance(Pos, Actor.Pos));
		}
		return Nearest;
	};
	int32 Want = ActivePad == 1 ? 0 : 1;
	double Best = -1.0;
	const int32 Options[2] = {ActivePad == 1 ? 0 : 1, ActivePad == 1 ? 2 : 1};
	const int32 OptionCount = ActivePad == 1 ? 2 : 1;
	for (int32 Index = 0; Index < OptionCount; ++Index)
	{
		const double Room = Clearance(PadKernel(Options[Index]));
		if (Room > Best)
		{
			Best = Room;
			Want = Options[Index];
		}
	}
	FName Clip = TEXT("blink-back");
	if (Want < ActivePad)
	{
		Clip = TEXT("blink-left");
	}
	else if (Want > ActivePad)
	{
		Clip = TEXT("blink-right");
	}
	else if (ActivePad <= 0)
	{
		Clip = TEXT("blink-right");
	}
	else if (ActivePad >= 2)
	{
		Clip = TEXT("blink-left");
	}
	PlayAction(Clip);
	return true;
}

bool FArenaSession::PlayBlinkToward(int32 WantPad)
{
	const FActor* Player = SimFindActor(Games.State, Games.PlayerId);
	if (!Player || !Hands)
	{
		return false;
	}
	if (IsStaffPlanted())
	{
		if (!(ClipPlaying() && ClipAction() == TEXT("staff-lift")))
		{
			if (Hands->IsWardHeld())
			{
				Hands->SetWardHeld(false, EClipVariant::Normal);
			}
			bAbsorb = false;
			bSuppressWard = false;
			bWardStarted = false;
			bWardReleased = true;
			PlayAction(TEXT("staff-lift"));
			Note(TEXT("script staff lift sunfall"));
		}
		return true;
	}
	int32 NextPad = WantPad;
	if (FMath::Abs(WantPad - ActivePad) > 1)
	{
		NextPad = ActivePad < WantPad ? ActivePad + 1 : ActivePad - 1;
	}
	if (NextPad == ActivePad)
	{
		return false;
	}
	if (!CanBlink(*Player))
	{
		return ClipPlaying() && ClipAction().StartsWith(TEXT("blink"));
	}
	if (Hands->IsWardHeld())
	{
		Hands->SetWardHeld(false, EClipVariant::Normal);
	}
	bSuppressWard = true;
	bAbsorb = false;
	FName Clip = TEXT("blink-back");
	if (ActivePad != 1 && NextPad == 1)
	{
		Clip = TEXT("blink-back");
	}
	else if (NextPad > ActivePad)
	{
		Clip = TEXT("blink-right");
	}
	else
	{
		Clip = TEXT("blink-left");
	}
	PlayAction(Clip);
	return true;
}

bool FArenaSession::TrySunfallEscape(double MeleeEta, double ProjectileEta)
{
	if (!bWingSeat || !Hands)
	{
		return false;
	}
	const FActor* Player = SimFindActor(Games.State, Games.PlayerId);
	if (!Player || Player->bDown)
	{
		return false;
	}
	const FTelegraph* Meteor = nullptr;
	for (const FTelegraph& Telegraph : Games.State.Telegraphs)
	{
		if (Telegraph.OwnerId != Games.PlayerId && Telegraph.Kind == TEXT("area") && Telegraph.Family == TEXT("unblockable"))
		{
			Meteor = &Telegraph;
			break;
		}
	}
	const FPendingCast* PendingSun = nullptr;
	FSimVec PendingAim{0.0, 0.0};
	if (!Meteor)
	{
		for (const FActor& Actor : Games.State.Actors)
		{
			if (Actor.bDown || Actor.Team == Player->Team || !Actor.Pending.IsSet() || !Actor.Pending->SpellId.IsSet())
			{
				continue;
			}
			if (Actor.Pending->SpellId.GetValue() == TEXT("fire_sunfall"))
			{
				PendingSun = &Actor.Pending.GetValue();
				PendingAim = PendingSun->Aim;
				break;
			}
		}
	}
	if (Meteor || PendingSun)
	{
		const FSimVec Impact = Meteor ? Meteor->Target : PendingAim;
		int32 BestPad = ActivePad;
		double BestDist = -1.0;
		for (int32 Pad = 0; Pad < 3; ++Pad)
		{
			const double Dist = SimDistance(PadKernel(Pad), Impact);
			if (Dist > BestDist)
			{
				BestDist = Dist;
				BestPad = Pad;
			}
		}
		SunfallPad = BestPad;
		const double Reach = (Meteor ? Meteor->WidthM : 3.5) + Player->Radius + 0.15;
		const bool bInside = SimDistance(Impact, Player->Pos) <= Reach;
		if (ActivePad != BestPad || IsStaffPlanted() || bInside)
		{
			if (PlayBlinkToward(BestPad))
			{
				Note(FString::Printf(TEXT("script sunfall pad=%d from=%d"), BestPad, ActivePad));
				return true;
			}
			if (ActivePad != BestPad)
			{
				return true;
			}
		}
		return false;
	}
	const bool bMeleeWindow = MeleeEta <= 0.30 && MeleeEta >= 0.22;
	const bool bStoneWindow = ProjectileEta <= 0.30 && ProjectileEta >= 0.22;
	if (!bWingSeated && ActivePad == 1 && !bMeleeWindow && !bStoneWindow && GetSimSeconds() >= 0.4)
	{
		if (PlayBlinkToward(0))
		{
			bWingSeated = true;
			Note(TEXT("script wing seat"));
			return true;
		}
	}
	return false;
}

void FArenaSession::DecideScript()
{
	if (!Hands || Games.Phase != TEXT("active"))
	{
		return;
	}
	const FActor* Player = SimFindActor(Games.State, Games.PlayerId);
	if (!Player || Player->bDown)
	{
		return;
	}
	double MeleeEta = 1.0e6;
	double ProjectileEta = 1.0e6;
	ScanThreats(MeleeEta, ProjectileEta);
	const double Now = GetSimSeconds();
	const bool bPlaying = ClipPlaying();
	const FString Action = ClipAction();

	// The blink clip's flick lands about 0.12-0.20 s after PlayAction, and the roll's i-frames last 0.25 s.
	// Starting as ETA crosses 0.30 puts that roll on the hit.
	const bool bMeleeWindow = MeleeEta <= 0.30 && MeleeEta >= 0.22;
	const bool bStoneWindow = ProjectileEta <= 0.30 && ProjectileEta >= 0.22;
	const bool bBlinkReady = CanBlink(*Player);
	const double MeleeGap = SoonestThreat(MeleeEta, ProjectileEta, true);
	const bool bPlanted = IsStaffPlanted();
	const FVrRuleset* Rules = GetVrRules();
	const double Nearest = NearestLiving(Games, *Player, nullptr, 3.1);
	const bool bInMelee = Nearest <= KernelData().StaffRangeM + 0.05;

	if (TrySunfallEscape(MeleeEta, ProjectileEta))
	{
		return;
	}

	// A sigil, staff, mudra, or blink owns the casting hand until the clip ends.
	// Returning here keeps a held ward up through the sigil, which is the split-hand pose.
	if (bPlaying && (Action.StartsWith(TEXT("sigil")) || Action.StartsWith(TEXT("staff")) || Action == TEXT("mudra") || Action.StartsWith(TEXT("blink"))))
	{
		return;
	}

	auto RaiseWard = [this, Now]()
	{
		bSuppressWard = false;
		bWardStarted = true;
		bWardReleased = false;
		WardReleaseSim = Now + Policy.WardHoldS;
		Hands->SetWardHeld(true, EClipVariant::Normal);
	};

	auto ReleaseForCast = [this]()
	{
		if (Policy.bSplit || !Hands)
		{
			return;
		}
		if (Hands->IsWardHeld())
		{
			Hands->SetWardHeld(false, EClipVariant::Normal);
		}
		bAbsorb = false;
		bWardStarted = false;
		bWardReleased = true;
	};

	auto TryBolt = [this, Player, &bInMelee, &ReleaseForCast]()
	{
		const FSpell* Bolt = SpellFor(*Player, 0);
		const double BoltRange = Bolt ? Bolt->RangeM : KernelData().BoltRangeM;
		const double Distance = NearestLiving(Games, *Player, nullptr, 0.0);
		if (!bInMelee && Distance <= BoltRange - 0.15 && SpellReady(Games, *Player, 0) && !WouldSplitRefuse(0))
		{
			if (!IsStaffPlanted())
			{
				ReleaseForCast();
			}
			PlayAction(TEXT("bolt"));
			return true;
		}
		return false;
	};

	// Planted dome: both hands cast at full power. Blink stays refused.
	if (bPlanted)
	{
		if (bPlaying && Action == TEXT("bolt") && bBoltFlicked && Now >= BoltFlickSim + 0.06 && Player->Mana >= 9.0 && !bInMelee
			&& !Player->Pending.IsSet())
		{
			PlayAction(TEXT("bolt"));
			return;
		}
		if (bPlaying || Player->Pending.IsSet())
		{
			return;
		}
		if (Player->Tier >= 2 && Nearest <= 2.9 && Player->Mana >= 16.0 && SpellReady(Games, *Player, 2))
		{
			PlayAction(TEXT("sigil-line2"));
			return;
		}
		if (!bTideStarted && Player->Mana >= 12.0 && SpellReady(Games, *Player, 1))
		{
			PlayAction(TEXT("sigil-line1"));
			bTideStarted = true;
			return;
		}
		TryBolt();
		return;
	}

	// Spears first, while the dome is down. A blink drops the ward.
	if (bMeleeWindow && TryScriptBlink())
	{
		Note(FString::Printf(TEXT("script blink melee eta=%.3f t=%.3f"), MeleeEta, Now));
		return;
	}
	if (bStoneWindow && MeleeGap > 1.2 && Player->Stamina >= 50.0 && TryScriptBlink())
	{
		Note(FString::Printf(TEXT("script blink stone eta=%.3f t=%.3f"), ProjectileEta, Now));
		return;
	}

	const double UpFront = Rules && Rules->Staff.bEnabled ? Rules->Staff.ManaUpFront : 1.0e9;
	const bool bStaffFree = !bPlaying && !Player->Pending.IsSet();
	if (Policy.bPlant && bStaffFree && Rules && Rules->Staff.bEnabled && Player->Mana >= UpFront + 12.0 && MeleeGap > 0.8)
	{
		const bool bOpening = StaffPlants == 0 && Now >= 1.5 && Now < Policy.OpeningPlantEndS && (bTideStarted || Now >= 2.5);
		const bool bHurt = Player->Hp < Policy.PlantHurtHp && Now >= 3.0;
		const int32 SplitCap = Rules->Split.bEnabled ? Rules->Split.MaxTier : 2;
		const bool bHighTier = bAbsorb && Player->Tier > SplitCap;
		if (bOpening || bHurt || bHighTier)
		{
			if (Hands->IsWardHeld())
			{
				Hands->SetWardHeld(false, EClipVariant::Normal);
			}
			bAbsorb = false;
			bWardStarted = false;
			bWardReleased = true;
			PlayAction(TEXT("staff-plant"));
			Note(TEXT("script staff plant"));
			return;
		}
	}

	// Drop the palm once the steel has passed, mana is low, or a blink needs the hand.
	// While it stays up, fall through and cast with the other hand.
	if (bWardStarted && !bWardReleased)
	{
		const bool bHeldMin = Now >= WardReleaseSim - 1.25;
		const bool bClear = ProjectileEta > 0.85 && MeleeEta > 0.85;
		const bool bCap = Now >= WardReleaseSim || Player->Mana < Policy.WardDropMana;
		const bool bYield = bBlinkReady && (bMeleeWindow || (bStoneWindow && MeleeGap > 1.2 && Player->Stamina >= 50.0));
		if (bCap || bYield || (bClear && bHeldMin))
		{
			if (Hands->IsWardHeld())
			{
				Hands->SetWardHeld(false, EClipVariant::Normal);
			}
			bWardReleased = true;
			bWardStarted = false;
		}
	}

	const bool bBlinkClip = bPlaying && Action.StartsWith(TEXT("blink"));
	const bool bSpearFallback = MeleeEta <= 0.70 && MeleeEta >= 0.05 && !bBlinkReady;
	const bool bFirstStone = !WasWardOnStone() && ProjectileEta <= 1.15 && ProjectileEta >= 0.20;
	const bool bStoneFallback = ProjectileEta <= 1.05 && ProjectileEta >= 0.20 && MeleeGap > 0.9 && Player->Stamina < 50.0;
	if (!bAbsorb && !bBlinkClip && !bPlaying && Player->Mana >= Policy.WardRaiseMana && (bSpearFallback || bFirstStone || bStoneFallback || Now < 0.4))
	{
		RaiseWard();
	}

	if (bPlaying && Action == TEXT("bolt") && bBoltFlicked && Now >= BoltFlickSim + 0.06 && Player->Mana >= 9.0 && !bInMelee
		&& !Player->Pending.IsSet())
	{
		PlayAction(TEXT("bolt"));
		return;
	}
	if (bPlaying || Player->Pending.IsSet())
	{
		return;
	}

	// Opening tide with the ward already up. The right hand draws; the left keeps the palm.
	if (!bTideStarted && MeleeGap > 1.2 && Now < 4.0 && Player->Mana >= Policy.SplitSigilMana)
	{
		ReleaseForCast();
		PlayAction(TEXT("sigil-line1"));
		bTideStarted = true;
		return;
	}

	const bool bWardUp = Hands->IsWardHeld() || bAbsorb;
	const int32 MaxTier = Rules && Rules->Split.bEnabled ? Rules->Split.MaxTier : 4;
	if (!(bWardUp && Player->Tier > MaxTier)
		&& Player->Tier >= 2 && Nearest <= 2.9 && MeleeGap > 0.9 && Player->Mana >= 16.0 && SpellReady(Games, *Player, 2)
		&& !WouldSplitRefuse(2))
	{
		ReleaseForCast();
		PlayAction(TEXT("sigil-line2"));
		return;
	}

	if (bInMelee)
	{
		return;
	}
	TryBolt();
}

FSimVec FArenaSession::ComputeAim(int32 Slot) const
{
	const FActor* Player = SimFindActor(Games.State, Games.PlayerId);
	if (!Player)
	{
		return ViewAim;
	}
	const FSpell* Spell = SpellFor(*Player, Slot);
	const double Speed = (Spell && Spell->SpeedMps > 1.0) ? Spell->SpeedMps : (Slot == 0 ? KernelData().BoltSpeedMps : 0.0);
	const double Hz = KernelData().SimStepHz > 0.0 ? KernelData().SimStepHz : 60.0;
	const FActor* Best = nullptr;
	double BestScore = 1.0e9;
	FSimVec Centroid{0.0, 0.0};
	int32 Cluster = 0;
	for (const FActor& Actor : Games.State.Actors)
	{
		if (Actor.Id == Player->Id || Actor.bDown || Actor.Team == Player->Team)
		{
			continue;
		}
		const double Distance = SimDistance(Player->Pos, Actor.Pos);
		if (Slot == 2 && Distance <= 3.2)
		{
			Centroid = SimAdd(Centroid, Actor.Pos);
			++Cluster;
		}
		// Finish a conscript before spreading. A live spearman is a full hit every backoff; a slinger is a stone.
		double Score = Actor.Hp + Distance * 0.2;
		if (Actor.Enemy.IsSet() && Actor.Enemy->Id == TEXT("conscript"))
		{
			Score -= 18.0;
		}
		if (Slot != 0 && Distance <= 3.0)
		{
			Score -= 8.0;
		}
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = &Actor;
		}
	}
	if (Slot == 2 && Cluster > 0)
	{
		return SimScale(Centroid, 1.0 / static_cast<double>(Cluster));
	}
	if (!Best)
	{
		return FSimVec{Player->Pos.X + 12.0, Player->Pos.Y};
	}
	const FSimVec Velocity = SimScale(SimSub(Best->Pos, Best->PreviousPos), Hz);
	const double Distance = SimDistance(Player->Pos, Best->Pos);
	double Flight = Speed > 1.0 ? FMath::Clamp(Distance / Speed, 0.0, 1.0) : 0.0;
	// A slinger strafes, then plants for the throw. Lead only until that plant or the bolt passes beside them.
	if (Best->Enemy.IsSet())
	{
		const double UntilPlant = static_cast<double>(Best->Enemy->ReadyTick - Games.State.Tick) / Hz;
		if (UntilPlant < Flight)
		{
			Flight = FMath::Max(0.0, UntilPlant);
		}
	}
	return SimAdd(Best->Pos, SimScale(Velocity, Flight));
}

void FArenaSession::DrainEvents(const FActor* Player)
{
	for (int32 Index = EventCursor; Index < Games.State.Events.Num(); ++Index)
	{
		const FArenaEvent& Event = Games.State.Events[Index];
		if (Event.Kind == TEXT("cast") && Event.ActorId == Games.PlayerId)
		{
			Note(FString::Printf(TEXT("kernel cast actor=%d tier=%.0f"), Event.ActorId, Event.Value));
		}
		else if (Event.Kind == TEXT("roll") && Event.ActorId == Games.PlayerId)
		{
			Note(FString::Printf(TEXT("kernel roll actor=%d stamina=%.1f t=%.3f"), Event.ActorId, Player ? Player->Stamina : 0.0, GetSimSeconds()));
		}
		else if (Event.Kind == TEXT("hit"))
		{
			const int32 Owner = Event.TargetId.Get(-1);
			Note(FString::Printf(TEXT("kernel hit target=%d value=%.2f owner=%d t=%.3f"), Event.ActorId, Event.Value, Owner, GetSimSeconds()));
			if (Owner == Games.PlayerId && Event.ActorId != Games.PlayerId && Event.Value >= 3.5)
			{
				bSawSigilHit = true;
			}
		}
		else if (Event.Kind == TEXT("perfect") && Event.ActorId == Games.PlayerId)
		{
			Note(FString::Printf(TEXT("kernel perfect target=%d"), Event.ActorId));
			if (Games.Phase == TEXT("teach"))
			{
				PerfectRingUntil = GetSimSeconds() + Tuning.PerfectRingS;
				Note(TEXT("cue bell perfect-absorb"));
			}
		}
		else if (Event.Kind == TEXT("unlock") && Event.ActorId == Games.PlayerId)
		{
			Note(FString::Printf(TEXT("kernel unlock tier=%.0f"), Event.Value));
			// TODO(T13): Flow and the tier clock get one wrist cue each during Wave 1.
			if (Games.Phase == TEXT("active") && !bClockCue)
			{
				bClockCue = true;
				WristText = TeachString(TEXT("wave.clock"));
				WristUntil = GetSimSeconds() + 4.0;
				Note(TEXT("cue wrist clock"));
			}
		}
	}
	// TODO(T13): Flow and the tier clock get one wrist cue each during Wave 1.
	if (Games.Phase == TEXT("active") && Player && !bFlowCue && Player->Water.Flow > 0)
	{
		bFlowCue = true;
		WristText = TeachString(TEXT("wave.flow"));
		WristUntil = GetSimSeconds() + 4.0;
		Note(TEXT("cue wrist flow"));
	}
	EventCursor = Games.State.Events.Num();
}

void FArenaSession::SeatOnActivePad(FActor& Player) const
{
	const FSimVec Seat = PadKernel(ActivePad);
	Player.Pos = Seat;
	Player.PreviousPos = Seat;
}

void FArenaSession::CheckEnd()
{
	if (bLoggedEnd)
	{
		return;
	}
	if (Games.Phase == TEXT("intermission") || Games.Phase == TEXT("complete"))
	{
		bLoggedEnd = true;
		Note(FString::Printf(TEXT("Session victory t=%.2f waves=%d tick=%d"), GetSimSeconds(), Games.WavesCleared, Games.State.Tick));
	}
	else if (Games.Phase == TEXT("lost"))
	{
		bLoggedEnd = true;
		const FActor* Player = SimFindActor(Games.State, Games.PlayerId);
		Note(FString::Printf(TEXT("Session defeat t=%.2f tick=%d hp=%.1f"), GetSimSeconds(), Games.State.Tick, Player ? Player->Hp : 0.0));
	}
}

void FArenaSession::StepKernel()
{
	const FActor* Before = SimFindActor(Games.State, Games.PlayerId);
	if (!Before)
	{
		bRunning = false;
		return;
	}
	int32 AimSlot = bCastPulse ? CastSlot : 0;
	if (!bCastPulse)
	{
		const FString Action = ClipAction();
		if (Action == TEXT("sigil-line1"))
		{
			AimSlot = 1;
		}
		else if (Action == TEXT("sigil-line2"))
		{
			AimSlot = 2;
		}
		else if (Action == TEXT("sigil-line3"))
		{
			AimSlot = 3;
		}
	}
	if (bScripted)
	{
		AimPoint = ComputeAim(AimSlot);
		// While the palm is up, face the lunging pack so the 140 degree arc chips more than the one focus target.
		if (bAbsorb && !bCastPulse)
		{
			FSimVec Sum{0.0, 0.0};
			int32 Packed = 0;
			for (const FActor& Actor : Games.State.Actors)
			{
				if (Actor.Id == Before->Id || Actor.bDown || Actor.Team == Before->Team || !Actor.Enemy.IsSet())
				{
					continue;
				}
				if (Actor.Enemy->Id != TEXT("conscript") || SimDistance(Before->Pos, Actor.Pos) > 4.0)
				{
					continue;
				}
				Sum = SimAdd(Sum, Actor.Pos);
				++Packed;
			}
			if (Packed > 0)
			{
				AimPoint = SimScale(Sum, 1.0 / static_cast<double>(Packed));
			}
		}
	}
	const int32 RollsBefore = Before->Metrics.Rolls;
	const FSimVec PlayerPos = Before->Pos;
	// A flick can arrive during recovery. Hold the edge until the kernel will accept it, so the
	// rising edge is the first legal tick and not a rejected one that burns the queue.
	const bool bRollLegal = Games.State.Tick >= Before->RollUntil && Games.State.Tick >= Before->RecoveryUntil
		&& Before->Stamina >= KernelData().RollStaminaCost;
	const bool bWantRoll = bBlinkQueued && bRollLegal;
	if (bBlinkQueued && Games.State.Tick >= Before->RollUntil && Games.State.Tick >= Before->RecoveryUntil && !bRollLegal)
	{
		bBlinkQueued = false;
		Note(FString::Printf(TEXT("kernel roll rejected pad %d stamina=%.1f"), QueuedPad, Before->Stamina));
	}
	const int32 RollPad = QueuedPad;
	bool bStoneNear = false;
	if (Before->bAbsorb || bAbsorb)
	{
		for (const FProjectile& Projectile : Games.State.Projectiles)
		{
			if (Projectile.OwnerId != Games.PlayerId && Projectile.Family != TEXT("magic")
				&& SimDistance(Projectile.Pos, PlayerPos) <= 3.0)
			{
				bStoneNear = true;
			}
		}
	}
	// Seated. Move stays zero on every tick, including the blink. The roll still spends
	// stamina and opens the i-frame window. The pad pin below is the position change.
	FInputFrame Input = SimIdleInput(bScripted ? AimPoint : ViewAim);
	Input.Aim = bScripted ? AimPoint : ViewAim;
	Input.Slot = CastSlot;
	Input.bCast = bCastPulse && !bWantRoll;
	Input.bAbsorb = bAbsorb && !bWantRoll;
	Input.bPlantStaff = bPlantPulse;
	Input.bLiftStaff = bLiftPulse;
	bPlantPulse = false;
	bLiftPulse = false;
	int32 PadBefore = ActivePad;
	if (bWantRoll)
	{
		Input.bRoll = true;
		bBlinkQueued = false;
		ActivePad = RollPad;
	}
	if (Games.VrRules.IsSet())
	{
		ApplyComfort(Games.VrRules.GetValue());
	}
	StepGames(Games, &Input);
	if (bStoneNear)
	{
		bSawWardOnStone = true;
	}
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
			ActivePad = PadBefore;
			Note(FString::Printf(TEXT("kernel roll rejected pad %d stamina=%.1f"), RollPad, After->Stamina));
		}
	}
	// The kernel may have travelled along the roll this tick. The body stays on the pad.
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
	if (After->bAbsorb)
	{
		for (const FProjectile& Projectile : Games.State.Projectiles)
		{
			if (Projectile.OwnerId != Games.PlayerId && Projectile.Family != TEXT("magic")
				&& SimDistance(Projectile.Pos, After->Pos) <= 2.5)
			{
				bSawWardOnStone = true;
			}
		}
	}
	DrainEvents(After);
	CheckEnd();
}

void FArenaSession::Advance(double DeltaSeconds, bool bStepHands)
{
	if (!bRunning || DeltaSeconds <= 0.0)
	{
		return;
	}
	// Capture stills hold the kernel on the current prompt. Pause would replace that text.
	if (bFrameHold)
	{
		return;
	}
	if (bStepHands && Hands)
	{
		Hands->Step(DeltaSeconds);
	}
	PollTracking(DeltaSeconds);
	// A flick that never slows before the clip ends still has to reach the kernel.
	if (bClipWasPlaying && !ClipPlaying() && Blinks)
	{
		Blinks->FlushPending();
	}
	if (bPaused)
	{
		UpdateResume(DeltaSeconds);
		bClipWasPlaying = ClipPlaying();
		return;
	}
	if (Games.Phase == TEXT("cold") || Games.Phase == TEXT("offer") || Games.Phase == TEXT("teach"))
	{
		AdvanceArc(DeltaSeconds);
		bClipWasPlaying = ClipPlaying();
		return;
	}
	if (bScripted && Games.Phase == TEXT("active"))
	{
		DecideScript();
	}
	const double MaxDelta = KernelData().MaxFrameDeltaS > 0.0 ? KernelData().MaxFrameDeltaS : 0.25;
	Accumulator = FMath::Min(Accumulator + DeltaSeconds, MaxDelta);
	const double Step = SimDt();
	int32 Steps = 0;
	while (Accumulator + 1.0e-9 >= Step && Steps < 8 && bRunning && Games.Phase == TEXT("active"))
	{
		Accumulator -= Step;
		StepKernel();
		++Steps;
	}
	bClipWasPlaying = ClipPlaying();
}

void UArenaSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UArenaSessionSubsystem::Deinitialize()
{
	if (FocusHandle.IsValid())
	{
		FCoreDelegates::ApplicationWillDeactivateDelegate.Remove(FocusHandle);
		FocusHandle.Reset();
	}
	Session.NotifyQuit();
	Session.Unbind();
	bRunning = false;
	Super::Deinitialize();
}

bool UArenaSessionSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UArenaSessionSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (bBegan)
	{
		return;
	}
	bBegan = true;
	UGameInstance* Instance = InWorld.GetGameInstance();
	Session.Bind(
		Instance ? Instance->GetSubsystem<UHandInputSubsystem>() : nullptr,
		Instance ? Instance->GetSubsystem<USigilRecognizerSubsystem>() : nullptr,
		Instance ? Instance->GetSubsystem<UWardDetectorSubsystem>() : nullptr,
		Instance ? Instance->GetSubsystem<UBlinkDetectorSubsystem>() : nullptr,
		Instance ? Instance->GetSubsystem<UStaffDetectorSubsystem>() : nullptr);

	const bool bNull = FParse::Param(FCommandLine::Get(), TEXT("nullrhi"));
	const bool bGreyboxCapture = FParse::Param(FCommandLine::Get(), TEXT("MageArenaGreyboxCapture"));
	const bool bWave1Capture = FParse::Param(FCommandLine::Get(), TEXT("MageArenaWave1Capture"));
	const bool bTeachCapture = FParse::Param(FCommandLine::Get(), TEXT("MageArenaTeachCapture"));
	const bool bSettingsCapture = FParse::Param(FCommandLine::Get(), TEXT("MageArenaSettingsCapture"));
	const bool bGame = FParse::Param(FCommandLine::Get(), TEXT("game"));
	if (!FApp::IsUnattended())
	{
		FocusHandle = FCoreDelegates::ApplicationWillDeactivateDelegate.AddUObject(this, &UArenaSessionSubsystem::OnAppDeactivate);
	}
	if (!bNull)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Presentation = InWorld.SpawnActor<ASessionPresentation>(ASessionPresentation::StaticClass(), FTransform::Identity, Params);
	}
	if (bWave1Capture)
	{
		Capture = NewObject<UWave1CaptureDriver>(this);
		Capture->Start();
		return;
	}
	if (bTeachCapture)
	{
		TeachCapture = NewObject<UTeachCaptureDriver>(this);
		TeachCapture->Start();
		return;
	}
	if (bSettingsCapture)
	{
		SettingsCapture = NewObject<USettingsCaptureDriver>(this);
		SettingsCapture->Start();
		return;
	}
	if (bGame && !bGreyboxCapture)
	{
		BeginArc(1, false);
	}
}

bool UArenaSessionSubsystem::BeginArc(uint32 Seed, bool bScripted)
{
	Session.SetScripted(bScripted);
	const bool bOk = Session.BeginArc(Seed);
	bRunning = bOk;
	UE_LOG(LogMageArena, Log, TEXT("MageArena.Session.BeginArc seed=%u scripted=%d ok=%d"), Seed, bScripted ? 1 : 0, bOk ? 1 : 0);
	return bOk;
}

void UArenaSessionSubsystem::OnAppDeactivate()
{
	bFocusLatched = true;
}

void UArenaSessionSubsystem::PollPlatform()
{
	if (bFocusLatched)
	{
		bFocusLatched = false;
		Session.NotifyFocusLost();
	}
	if (!GEngine || !GEngine->XRSystem.IsValid())
	{
		return;
	}
	IXRTrackingSystem* Tracking = GEngine->XRSystem.Get();
	IHeadMountedDisplay* Hmd = Tracking ? Tracking->GetHMDDevice() : nullptr;
	if (!Hmd)
	{
		return;
	}
	const EHMDWornState::Type Worn = Hmd->GetHMDWornState();
	if (bHaveWorn && LastWorn == static_cast<int32>(EHMDWornState::Worn) && Worn == EHMDWornState::NotWorn)
	{
		Session.NotifyHeadsetRemoved();
	}
	LastWorn = static_cast<int32>(Worn);
	bHaveWorn = true;
}

bool UArenaSessionSubsystem::Start(uint32 Seed, bool bScripted)
{
	Session.SetScripted(bScripted);
	const bool bOk = Session.Start(Seed);
	bRunning = bOk;
	UE_LOG(LogMageArena, Log, TEXT("MageArena.Session.Start seed=%u scripted=%d ok=%d"), Seed, bScripted ? 1 : 0, bOk ? 1 : 0);
	return bOk;
}

void UArenaSessionSubsystem::TogglePause()
{
	Session.TogglePause();
}

bool UArenaSessionSubsystem::IsTickable() const
{
	return bRunning;
}

TStatId UArenaSessionSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UArenaSessionSubsystem, STATGROUP_Tickables);
}

void UArenaSessionSubsystem::ApplyViewAim()
{
	if (Session.IsScripted() || !GetWorld())
	{
		return;
	}
	AMageArenaPawn* Pawn = nullptr;
	for (TActorIterator<AMageArenaPawn> It(GetWorld()); It; ++It)
	{
		Pawn = *It;
		break;
	}
	if (!Pawn)
	{
		return;
	}
	// Mouse look writes the control rotation. The camera component can lag that by a frame.
	const FVector Forward = Pawn->GetControlRotation().Vector();
	FVector Flat(Forward.X, Forward.Y, 0.0);
	if (Flat.IsNearlyZero())
	{
		Flat = FVector(1.0, 0.0, 0.0);
	}
	Flat = Flat.GetSafeNormal();
	const FActor* Player = SimFindActor(Session.GetGames().State, Session.GetGames().PlayerId);
	if (!Player)
	{
		return;
	}
	const FVector AimCm = Session.KernelToUnrealCm(Player->Pos) + Flat * 1200.0;
	const FRunePad* Centre = Session.HasLayout() ? Session.GetLayout().FindPad(1) : nullptr;
	const FSimVec Spawn = KernelData().PlayerSpawn;
	const double Cx = Centre ? Centre->PositionM.X : -12.0;
	const double Cy = Centre ? Centre->PositionM.Y : 0.0;
	Session.SetViewAim(FSimVec{Spawn.X + (AimCm.X / 100.0 - Cx), Spawn.Y + (AimCm.Y / 100.0 - Cy)});
}

void UArenaSessionSubsystem::ApplyCamera()
{
	int32 Pad = 1;
	if (!Session.ConsumeCameraPad(Pad) || !GetWorld())
	{
		return;
	}
	for (TActorIterator<AMageArenaPawn> It(GetWorld()); It; ++It)
	{
		It->PresentBlink(Pad);
		break;
	}
}

void UArenaSessionSubsystem::Tick(float DeltaTime)
{
	if (!bRunning)
	{
		return;
	}
	ApplyViewAim();
	if (!FApp::IsUnattended())
	{
		PollPlatform();
	}
	Session.Advance(DeltaTime, false);
	if (Presentation && Session.IsRunning())
	{
		Presentation->Sync(Session, Session.IsPaused());
	}
	ApplyCamera();
}
