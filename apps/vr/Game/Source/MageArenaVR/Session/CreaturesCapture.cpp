#include "Session/CreaturesCapture.h"

#include "Session/ArenaSession.h"
#include "Kernel/ArenaKernel.h"

#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Hands/ClipVariant.h"
#include "Hands/HandInputSubsystem.h"
#include "Hands/MageArenaPawn.h"
#include "Kernel/KernelData.h"
#include "Kernel/SimMath.h"
#include "Kernel/VrRules.h"
#include "MageArenaVR.h"
#include "Misc/App.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"

namespace
{
// DF-004 option A. A magic telegraph or shot owned by an enemy whose overlay attack is a spit (a cinder hound ember).
bool OwnedByHound(const FArenaSession& Bout, int32 OwnerId, const FString& Family)
{
	const FVrRuleset* Rules = Bout.GetVrRules();
	const FActor* Owner = SimFindActor(Bout.GetGames().State, OwnerId);
	return Rules && Rules->bActive && Family == TEXT("magic") && Owner && Owner->Enemy.IsSet()
		&& Rules->FindSpit(Owner->Enemy->Id) != nullptr;
}

// True when a kernel point is inside a cone around the seated camera's view direction, so a still frames it.
bool InView(const AMageArenaPawn* Pawn, const FArenaSession& Bout, const FSimVec& Point, double ConeDeg)
{
	const UCameraComponent* Camera = Pawn ? Pawn->FindComponentByClass<UCameraComponent>() : nullptr;
	if (!Camera)
	{
		return true;
	}
	FVector To = Bout.KernelToUnrealCm(Point) - Camera->GetComponentLocation();
	FVector Forward = Camera->GetForwardVector();
	To.Z = 0.0;
	Forward.Z = 0.0;
	return FVector::DotProduct(To.GetSafeNormal(), Forward.GetSafeNormal()) >= FMath::Cos(FMath::DegreesToRadians(ConeDeg));
}

// The player's own bolts leave the seat at eye height, so a fresh one fills the lens. A still waits for a frame with
// none of them within this many metres of the player.
bool BoltsClear(const FArenaSession& Bout, double Metres)
{
	const FGames& Games = Bout.GetGames();
	const FActor* Player = SimFindActor(Games.State, Games.PlayerId);
	if (!Player)
	{
		return true;
	}
	for (const FProjectile& P : Games.State.Projectiles)
	{
		if (P.OwnerId == Player->Id && SimDistance(P.Pos, Player->Pos) < Metres)
		{
			return false;
		}
	}
	return true;
}

// Seconds until the shot touches the player's body, or a negative number when it is moving away.
double ContactS(const FProjectile& Shot, const FActor& Player)
{
	const double Speed = SimLength(Shot.Velocity);
	const double Along = Speed > 0.1 ? SimDot(SimSub(Player.Pos, Shot.Pos), SimUnit(Shot.Velocity)) : -1.0;
	return Along < 0.0 ? -1.0 : FMath::Max(0.0, Along - Player.Radius - Shot.Radius) / Speed;
}
}

UCreaturesCaptureDriver::UCreaturesCaptureDriver()
{
	SetTickableTickType(ETickableTickType::Never);
}

void UCreaturesCaptureDriver::Start()
{
	RunId = TEXT("T18");
	FParse::Value(FCommandLine::Get(), TEXT("MageArenaRun="), RunId);
	bRunning = true;
	RunStartWall = FPlatformTime::Seconds();
	Enter(EStep::WaitStable);
	SetTickableTickType(ETickableTickType::Always);
}

void UCreaturesCaptureDriver::Enter(EStep Next)
{
	Step = Next;
	StepStartWall = FPlatformTime::Seconds();
	PromptHold = 0;
}

bool UCreaturesCaptureDriver::PromptSettled(const FString& Prompt, const TCHAR* Expected)
{
	if (!Expected || Prompt != Expected)
	{
		PromptHold = 0;
		return false;
	}
	return ++PromptHold >= 4;
}

UArenaSessionSubsystem* UCreaturesCaptureDriver::SessionSys() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UArenaSessionSubsystem>() : nullptr;
}

AMageArenaPawn* UCreaturesCaptureDriver::FindPawn() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AMageArenaPawn> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

FString UCreaturesCaptureDriver::RepoPath(const TCHAR* Relative) const
{
	FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../../.."), Relative);
	FPaths::CollapseRelativeDirectories(Path);
	return FPaths::ConvertRelativePathToFull(Path);
}

bool UCreaturesCaptureDriver::RequestShot(const TCHAR* FileName)
{
	const FString Path = RepoPath(*FString::Printf(TEXT("runs/%s/shots/%s"), *RunId, FileName));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	FScreenshotRequest::RequestScreenshot(Path, false, false, false);
	PendingShot = Path;
	bShotOpen = true;
	++ShotCount;
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_SHOT name=%s path=%s"), FileName, *Path);
	return true;
}

bool UCreaturesCaptureDriver::ShotFinished() const
{
	return !PendingShot.IsEmpty() && !FScreenshotRequest::IsScreenshotRequested() && IFileManager::Get().FileSize(*PendingShot) > 0;
}

void UCreaturesCaptureDriver::Fail(const TCHAR* Reason)
{
	UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CAPTURE_FAIL %s"), Reason);
	Enter(EStep::Quit);
}

void UCreaturesCaptureDriver::Tick(float DeltaTime)
{
	(void)DeltaTime;
	if (!bRunning)
	{
		return;
	}
	if (bShotOpen)
	{
		if (!ShotFinished())
		{
			return;
		}
		bShotOpen = false;
		PendingShot.Reset();
	}
	Advance();
}

void UCreaturesCaptureDriver::Advance()
{
	const double Now = FPlatformTime::Seconds();
	const double Elapsed = Now - StepStartWall;
	if (Step != EStep::Quit && Step != EStep::Finish && Now - RunStartWall > 360.0)
	{
		Fail(TEXT("timeout"));
		return;
	}
	UArenaSessionSubsystem* Session = SessionSys();
	AMageArenaPawn* Pawn = FindPawn();
	FArenaSession* Bout = Session ? &Session->GetSession() : nullptr;

	switch (Step)
	{
	case EStep::WaitStable:
	{
		const float Delta = FApp::GetDeltaTime();
		const bool bShadersIdle = GShaderCompilingManager == nullptr || !GShaderCompilingManager->IsCompiling();
		if (Pawn && Pawn->HasLayout() && bShadersIdle && Delta > 0.f && Delta < 0.05f)
		{
			++StableFrames;
		}
		else
		{
			StableFrames = 0;
		}
		if ((StableFrames >= 20 && Elapsed >= 1.5) || Elapsed >= 90.0)
		{
			if (!Session || !Pawn)
			{
				Fail(TEXT("session or pawn missing"));
				break;
			}
			Pawn->ApplyFlatPresentation();
			Pawn->LookAtArena();
			Session->GetSession().SetBoutIndex(1); // Bout 2
			if (!Session->Start(1, true))
			{
				Fail(TEXT("Start failed"));
				break;
			}
			Enter(EStep::WaitHoundsApproaching);
		}
		break;
	}
	case EStep::WaitHoundsApproaching:
		if (Bout && Bout->GetSimSeconds() >= 2.0)
		{
			Bout->SetFrameHold(true);
			RequestShot(TEXT("01-hounds-approaching.png"));
			Enter(EStep::WaitHoundsApproachingShot);
		}
		else if (Elapsed > 8.0)
		{
			Fail(TEXT("hounds approach timeout"));
		}
		break;
	case EStep::WaitHoundsApproachingShot:
		if (Bout) Bout->SetFrameHold(false);
		Enter(EStep::WaitSpitAtLip);
		break;
	case EStep::WaitSpitAtLip:
	{
		// A living hound at the hold line, part way through its spit windup: the ember gathers at its mouth.
		bool bSpit = false;
		if (Bout && Bout->GetGames().Phase == TEXT("active") && Bout->GetVrRules())
		{
			const FArenaState& State = Bout->GetGames().State;
			for (const FTelegraph& T : State.Telegraphs)
			{
				const FActor* Owner = SimFindActor(State, T.OwnerId);
				if (T.Kind == TEXT("projectile") && Owner && !Owner->bDown && OwnedByHound(*Bout, T.OwnerId, T.Family))
				{
					const double Gap = Bout->GetVrRules()->OutsideGap(Owner->Pos);
					const int32 Left = T.ResolveTick - State.Tick;
					// In front of the seat, and no ember already in the player's face covering the view.
					bool bClearView = true;
					const FActor* Player = SimFindActor(State, Bout->GetGames().PlayerId);
					for (const FProjectile& P : State.Projectiles)
					{
						if (Player && P.OwnerId != Player->Id && SimDistance(P.Pos, Player->Pos) < 2.5)
						{
							bClearView = false;
						}
					}
					// A shot that just landed on the seat stays drawn as a ghost for a moment, right in front of the lens.
					for (int32 Index = State.Events.Num() - 1; Index >= 0 && State.Events[Index].Tick >= State.Tick - 24; --Index)
					{
						if (Player && State.Events[Index].Kind == TEXT("hit") && State.Events[Index].ActorId == Player->Id)
						{
							bClearView = false;
						}
					}
					if (Gap >= -0.05 && Gap <= 0.5 && Left <= 12 && Left >= 6 && bClearView && BoltsClear(*Bout, 3.0)
					&& InView(Pawn, *Bout, Owner->Pos, 35.0))
					{
						bSpit = true;
						UE_LOG(LogMageArena, Log, TEXT("MAGEVR_CAPTURE spit owner=%d gap=%.3f left=%d tick=%d"), T.OwnerId, Gap, Left, State.Tick);
						break;
					}
				}
			}
		}
		if (bSpit)
		{
			Bout->SetFrameHold(true);
			RequestShot(TEXT("05-hound-spit-at-lip.png"));
			Enter(EStep::WaitSpitAtLipShot);
		}
		else if (Elapsed > 60.0)
		{
			Fail(TEXT("spit at lip timeout"));
		}
		break;
	}
	case EStep::WaitSpitAtLipShot:
		if (Bout) Bout->SetFrameHold(false);
		Enter(EStep::WaitEmberCue);
		break;
	case EStep::WaitEmberCue:
	{
		// An ember in flight inside the perfect window (combat.json absorb.perfect.windowS): the cue beads are up.
		bool bCue = false;
		if (Bout && Bout->GetGames().Phase == TEXT("active"))
		{
			const FGames& Games = Bout->GetGames();
			const FActor* Player = SimFindActor(Games.State, Games.PlayerId);
			for (const FProjectile& P : Games.State.Projectiles)
			{
				if (!Player || !OwnedByHound(*Bout, P.OwnerId, P.Family))
				{
					continue;
				}
				const double Contact = ContactS(P, *Player);
				if (Contact >= 0.08 && Contact <= KernelData().Absorb.WindowS - 0.02 && BoltsClear(*Bout, 3.0)
					&& InView(Pawn, *Bout, P.Pos, 35.0))
				{
					bCue = true;
					UE_LOG(LogMageArena, Log, TEXT("MAGEVR_CAPTURE ember cue owner=%d contact=%.3f tick=%d"), P.OwnerId, Contact, Games.State.Tick);
					break;
				}
			}
		}
		if (bCue)
		{
			Bout->SetFrameHold(true);
			RequestShot(TEXT("06-ember-perfect-cue.png"));
			Enter(EStep::WaitEmberCueShot);
		}
		else if (Elapsed > 60.0)
		{
			Fail(TEXT("ember cue timeout"));
		}
		break;
	}
	case EStep::WaitEmberCueShot:
		if (Bout)
		{
			Bout->SetFrameHold(false);
			EventCursor = Bout->GetGames().State.Events.Num();
		}
		Enter(EStep::WaitEmberPerfect);
		break;
	case EStep::WaitEmberPerfect:
	{
		// The scripted seat perfects a hound ember: the frame the perfect event lands.
		bool bPerfect = false;
		if (Bout && Bout->GetGames().Phase == TEXT("active"))
		{
			const FGames& Games = Bout->GetGames();
			for (; EventCursor < Games.State.Events.Num(); ++EventCursor)
			{
				const FArenaEvent& Event = Games.State.Events[EventCursor];
				const FActor* Spitter = SimFindActor(Games.State, Event.TargetId.Get(-1));
				// Prefer a perfect on an ember from a hound in front of the seat; after 40 s take any.
				if (Event.Kind == TEXT("perfect") && Event.ActorId == Games.PlayerId && Spitter
					&& OwnedByHound(*Bout, Event.TargetId.Get(-1), TEXT("magic"))
					&& (Elapsed > 40.0 || (InView(Pawn, *Bout, Spitter->Pos, 40.0) && BoltsClear(*Bout, 3.0))))
				{
					bPerfect = true;
					UE_LOG(LogMageArena, Log, TEXT("MAGEVR_CAPTURE ember perfect owner=%d tick=%d"), Event.TargetId.Get(-1), Games.State.Tick);
					break;
				}
			}
		}
		if (bPerfect)
		{
			Bout->SetFrameHold(true);
			RequestShot(TEXT("07-ember-perfected.png"));
			Enter(EStep::WaitEmberPerfectShot);
		}
		else if (Elapsed > 90.0)
		{
			Fail(TEXT("ember perfect timeout"));
		}
		break;
	}
	case EStep::WaitEmberPerfectShot:
		if (Bout) Bout->SetFrameHold(false);
		Enter(EStep::WaitDeathBurst);
		break;
	case EStep::WaitDeathBurst:
	{
		bool bDeathBurst = false;
		if (Bout && Bout->GetGames().Phase == TEXT("active"))
		{
			const double StepDT = SimDt(); // kernel ticks are 60 Hz, not the 72 Hz presentation frame
			// T18: prefer a burst in front of the seat, so the death ember that follows it is framed too. Take any burst
			// after 30 s, or when no hound is left alive to die in view later.
			bool bHoundAlive = false;
			for (const FActor& A : Bout->GetGames().State.Actors)
			{
				bHoundAlive |= !A.bDown && A.Enemy.IsSet() && A.Enemy->Id == TEXT("cinder_hound");
			}
			for (const FTelegraph& T : Bout->GetGames().State.Telegraphs)
			{
				if (T.Family == TEXT("magic") && T.Kind == TEXT("area") && (Elapsed > 30.0 || !bHoundAlive || InView(Pawn, *Bout, T.Origin, 40.0)))
				{
					double UntilResolve = static_cast<double>(T.ResolveTick - Bout->GetGames().State.Tick) * StepDT;
					if (UntilResolve <= 0.12 && UntilResolve >= 0.05)
					{
						bDeathBurst = true;
						break;
					}
				}
			}
		}
		if (bDeathBurst)
		{
			Bout->SetFrameHold(true);
			RequestShot(TEXT("02-death-burst-perfect.png"));
			Enter(EStep::WaitDeathBurstShot);
		}
		else if (Elapsed > 120.0)
		{
			Fail(TEXT("death burst timeout"));
		}
		break;
	}
	case EStep::WaitDeathBurstShot:
		if (Bout)
		{
			Bout->SetFrameHold(false);
		}
		Enter(EStep::WaitDeathEmber);
		break;
	case EStep::WaitDeathEmber:
	{
		// The dying hound's last ember, in flight from where it fell (its owner is down).
		bool bEmber = false;
		if (Bout && Bout->GetGames().Phase == TEXT("active"))
		{
			const FGames& Games = Bout->GetGames();
			for (const FProjectile& P : Games.State.Projectiles)
			{
				const FActor* Owner = SimFindActor(Games.State, P.OwnerId);
				// The ember of the death the burst still caught.
				if (Owner && Owner->bDown && P.bHasAim && OwnedByHound(*Bout, P.OwnerId, P.Family))
				{
					const double Span = FMath::Max(SimDistance(P.OriginPos, P.AimedAt), 1.0e-4);
					const double Progress = SimDistance(P.OriginPos, P.Pos) / Span;
					// Between a third and nine tenths of the way, with no fresh bolt in the lens; late in flight take it anyway.
					if (Progress >= 0.3 && Progress <= 0.9 && (BoltsClear(*Bout, 3.0) || Progress >= 0.75))
					{
						bEmber = true;
						UE_LOG(LogMageArena, Log, TEXT("MAGEVR_CAPTURE death ember owner=%d progress=%.2f tick=%d"), P.OwnerId, Progress, Games.State.Tick);
						break;
					}
				}
			}
		}
		if (bEmber)
		{
			Bout->SetFrameHold(true);
			RequestShot(TEXT("08-death-ember.png"));
			Enter(EStep::WaitDeathEmberShot);
		}
		else if (Elapsed > 60.0)
		{
			Fail(TEXT("death ember timeout"));
		}
		break;
	}
	case EStep::WaitDeathEmberShot:
		if (Bout)
		{
			Bout->SetFrameHold(false);
		}
		Enter(EStep::WaitTonguePull);
		break;
	case EStep::WaitTonguePull:
	{
		bool bTongue = false;
		if (Bout && Bout->GetGames().Phase == TEXT("active"))
		{
			// Stop the player from killing the maw, but only once the hounds are down: an idle seat with hounds still
			// spitting can lose the bout before the tongue (T18, the ember stills now come earlier in the bout).
			bool bHoundAlive = false;
			for (const FActor& A : Bout->GetGames().State.Actors)
			{
				bHoundAlive |= !A.bDown && A.Enemy.IsSet() && A.Enemy->Id == TEXT("cinder_hound");
			}
			if (!bHoundAlive && Bout->IsScripted())
			{
				Bout->SetScripted(false);
			}
			for (const FTelegraph& T : Bout->GetGames().State.Telegraphs)
			{
				const FActor* Owner = nullptr;
				for (const FActor& A : Bout->GetGames().State.Actors) {
					if (A.Id == T.OwnerId) { Owner = &A; break; }
				}

				if (Owner && Owner->Label == TEXT("Mire maw") && T.Kind == TEXT("lane"))
				{
					bTongue = true;
					break;
				}
			}
		}
		if (bTongue)
		{
			Bout->SetFrameHold(true);
			RequestShot(TEXT("03-maw-tongue.png"));
			Enter(EStep::WaitTonguePullShot);
		}
		else if (Elapsed > 120.0)
		{
			Fail(TEXT("tongue pull timeout"));
		}
		break;
	}
	case EStep::WaitTonguePullShot:
		if (Bout)
		{
			Bout->SetFrameHold(false);
			Bout->SetScripted(true); // Resume player to clear the wave
		}
		Enter(EStep::WaitIntermission);
		break;
	case EStep::WaitIntermission:
		if (Bout && Bout->GetGames().Phase == TEXT("intermission"))
		{
			++PromptHold;
			if (PromptHold >= 30) // Wait a bit for UI to settle
			{
				Bout->SetFrameHold(true);
				RequestShot(TEXT("04-intermission.png"));
				Enter(EStep::WaitIntermissionShot);
			}
		}
		else if (Elapsed > 120.0)
		{
			Fail(TEXT("intermission timeout"));
		}
		break;
	case EStep::WaitIntermissionShot:
		if (Bout) Bout->SetFrameHold(false);
		Enter(EStep::Finish);
		break;
	case EStep::Finish:
		if (ShotCount >= 8)
		{
			UE_LOG(LogMageArena, Log, TEXT("MAGEVR_CAPTURE_DONE shots=%d"), ShotCount);
		}
		else
		{
			UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CAPTURE_FAIL shots=%d"), ShotCount);
		}
		Enter(EStep::Quit);
		break;
	case EStep::Quit:
		bRunning = false;
		SetTickableTickType(ETickableTickType::Never);
		if (UWorld* World = GetWorld())
		{
			if (APlayerController* Controller = World->GetFirstPlayerController())
			{
				Controller->ConsoleCommand(TEXT("Quit"));
			}
		}
		break;
	default:
		break;
	}
}

TStatId UCreaturesCaptureDriver::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCreaturesCaptureDriver, STATGROUP_Tickables);
}

bool UCreaturesCaptureDriver::IsTickable() const
{
	return bRunning;
}

UWorld* UCreaturesCaptureDriver::GetTickableGameObjectWorld() const
{
	return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}

void UCreaturesCaptureDriver::BeginDestroy()
{
	SetTickableTickType(ETickableTickType::Never);
	Super::BeginDestroy();
}
