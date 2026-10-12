#include "Session/AirCapture.h"

#include "Session/ArenaSession.h"
#include "Kernel/Air.h"
#include "Kernel/ArenaKernel.h"
#include "Kernel/VrRules.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Hands/MageArenaPawn.h"
#include "Kernel/SimMath.h"
#include "MageArenaVR.h"
#include "Misc/App.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"

// Staging, stated so the stills are read for what they are:
// - The bout is the session's Tiro final with the school mages on (SetBout(3, true), air final): Lio's air mage, bound
//   to rivals.lio, against the scripted seated player. The kernel, the presentation and the threat colours are live.
// - Lio's four casts are started with StartGrantedAirCast, the real cast path minus the tier gate, the cooldown and the
//   mana (the rival-signature entry point), so each row is on screen when the still is taken instead of whenever the
//   level-1.5 random draw picks it. Air Form is granted the same way, so it spends no Momentum here.
// - Before each grant Lio and the player are healed to full so the duel does not end between stills.
// - After the first still the seated script is switched off (SetScripted(false)): the player sits idle on its pad, so
//   no blink tunnel, staff dome or water bolt covers the air telegraphs. The bout itself keeps running.
// - The air-form, squall and arc stills wait for a frame with no bolt of Lio's within 3 m of the seat.
// - The kernel is frozen (frame hold) for the shot itself: two rendered frames, then the screenshot.

namespace
{
const TCHAR* const StageSpell[] = {TEXT("air_veer"), TEXT("air_squall"), TEXT("air_form"), TEXT("air_tempest")};
const TCHAR* const StageShot[] = {TEXT("02-veering-bolt-arc.png"), TEXT("03-squall-volley.png"), TEXT("04-air-form.png"), TEXT("05-tempest-lance-line.png")};
const float StagePitch[] = {-10.f, -8.f, -8.f, -22.f};

void FaceKernel(AMageArenaPawn* Pawn, const FArenaSession& Bout, const FSimVec& From, const FSimVec& To, float Pitch)
{
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller)
	{
		return;
	}
	const FVector Delta = Bout.KernelToUnrealCm(To) - Bout.KernelToUnrealCm(From);
	const float Yaw = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X)));
	Controller->SetControlRotation(Pawn->ClampLookRotation(FRotator(Pitch, Yaw, 0.f)));
}

FActor* FindLio(FArenaSession& Bout)
{
	FArenaState& State = const_cast<FArenaState&>(Bout.GetGames().State);
	for (FActor& Actor : State.Actors)
	{
		if (Actor.Air.bSchool && Actor.Team != 0 && !Actor.bDown)
		{
			return &Actor;
		}
	}
	return nullptr;
}
}

UAirCaptureDriver::UAirCaptureDriver()
{
	SetTickableTickType(ETickableTickType::Never);
}

void UAirCaptureDriver::Start()
{
	FParse::Value(FCommandLine::Get(), TEXT("MageArenaRun="), RunId);
	bRunning = true;
	RunStartWall = FPlatformTime::Seconds();
	Enter(EStep::WaitStable);
	SetTickableTickType(ETickableTickType::Always);
}

void UAirCaptureDriver::Enter(EStep Next)
{
	Step = Next;
	StepStartWall = FPlatformTime::Seconds();
	Frames = 0;
}

UArenaSessionSubsystem* UAirCaptureDriver::SessionSys() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UArenaSessionSubsystem>() : nullptr;
}

AMageArenaPawn* UAirCaptureDriver::FindPawn() const
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

FString UAirCaptureDriver::RepoPath(const TCHAR* Relative) const
{
	FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../../.."), Relative);
	FPaths::CollapseRelativeDirectories(Path);
	return FPaths::ConvertRelativePathToFull(Path);
}

void UAirCaptureDriver::RequestShot(const TCHAR* FileName)
{
	const FString Path = RepoPath(*FString::Printf(TEXT("runs/%s/shots/%s"), *RunId, FileName));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	FScreenshotRequest::RequestScreenshot(Path, false, false, false);
	PendingShot = Path;
	bShotOpen = true;
	++ShotCount;
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_SHOT name=%s path=%s"), FileName, *Path);
}

bool UAirCaptureDriver::ShotFinished() const
{
	return !PendingShot.IsEmpty() && !FScreenshotRequest::IsScreenshotRequested() && IFileManager::Get().FileSize(*PendingShot) > 0;
}

void UAirCaptureDriver::Fail(const TCHAR* Reason)
{
	UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CAPTURE_FAIL %s"), Reason);
	Enter(EStep::Quit);
}

void UAirCaptureDriver::Tick(float DeltaTime)
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

void UAirCaptureDriver::Advance()
{
	const double Now = FPlatformTime::Seconds();
	const double Elapsed = Now - StepStartWall;
	if (Step != EStep::Quit && Step != EStep::Finish && Now - RunStartWall > 400.0)
	{
		Fail(TEXT("timeout"));
		return;
	}
	UArenaSessionSubsystem* Session = SessionSys();
	AMageArenaPawn* Pawn = FindPawn();
	FArenaSession* Bout = Session ? &Session->GetSession() : nullptr;
	++Frames;

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
			if (!Session || !Pawn || !Bout)
			{
				Fail(TEXT("session or pawn missing"));
				break;
			}
			Pawn->ApplyFlatPresentation();
			Pawn->LookAtArena();
			if (GEngine)
			{
				GEngine->Exec(GetWorld(), TEXT("r.MotionBlurQuality 0"));
			}
			Bout->SetSaveDirectory(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArenaTests"), TEXT("air-capture")));
			Bout->SetBout(3, true);
			Bout->SetAirFinal(true);
			if (!Session->Start(1, true))
			{
				Fail(TEXT("Start failed"));
				break;
			}
			const FVrRuleset* Rules = Bout->GetVrRules();
			const FVrRival* Rival = Rules ? Rules->BoundRival() : nullptr;
			FActor* Lio = FindLio(*Bout);
			UE_LOG(LogMageArena, Log, TEXT("MAGEVR_AIR start rival=%s lio=%s competence=%.2f"), Rival ? *Rival->Id : TEXT("-"),
				Lio ? *Lio->Label : TEXT("-"), Lio && Lio->MageAI.IsSet() ? Lio->MageAI->Competence : -1.0);
			if (!Lio || !Rival || Rival->Id != TEXT("lio"))
			{
				Fail(TEXT("Lio is not the bound air mage"));
				break;
			}
			Enter(EStep::WaitLio);
		}
		break;
	}
	case EStep::WaitLio:
		if (Bout && Elapsed >= 1.2)
		{
			FActor* Lio = FindLio(*Bout);
			const FActor* Me = SimFindActor(Bout->GetGames().State, Bout->GetGames().PlayerId);
			if (!Lio || !Me)
			{
				Fail(TEXT("Lio or the player is missing"));
				break;
			}
			Bout->SetFrameHold(true);
			FaceKernel(Pawn, *Bout, Me->Pos, Lio->Pos, -6.f);
			if (Frames >= 4)
			{
				UE_LOG(LogMageArena, Log, TEXT("MAGEVR_AIR lio at (%.2f, %.2f) player at (%.2f, %.2f) momentum=%.1f"), Lio->Pos.X, Lio->Pos.Y,
					Me->Pos.X, Me->Pos.Y, Lio->Air.Momentum);
				RequestShot(TEXT("01-lio-on-the-floor.png"));
				Bout->SetScripted(false);
				Stage = 0;
				Enter(EStep::Grant);
			}
		}
		break;
	case EStep::Grant:
	{
		if (!Bout)
		{
			Fail(TEXT("session missing"));
			break;
		}
		Bout->SetFrameHold(false);
		if (Bout->GetGames().Phase != TEXT("active"))
		{
			Fail(TEXT("the duel ended before the stills"));
			break;
		}
		FArenaState& State = const_cast<FArenaState&>(Bout->GetGames().State);
		FActor* Lio = FindLio(*Bout);
		FActor* Me = SimFindActor(State, Bout->GetGames().PlayerId);
		if (!Lio || !Me)
		{
			Fail(TEXT("Lio or the player is missing at a grant"));
			break;
		}
		// Wait for Lio's own cast to finish, then start this stage's row.
		if (Lio->Pending.IsSet() || AirChannelBusy(*Lio) || State.Tick < Lio->RollUntil)
		{
			if (Elapsed > 10.0)
			{
				Fail(TEXT("Lio never came free"));
			}
			break;
		}
		Lio->Hp = Lio->MaxHp;
		Me->Hp = Me->MaxHp;
		if (!StartGrantedAirCast(State, *Lio, StageSpell[Stage], Me->Pos, Bout->GetVrRules()))
		{
			Fail(TEXT("the grant did not start"));
			break;
		}
		GrantTick = State.Tick;
		GrantActivation = Lio->Pending.IsSet() ? Lio->Pending->ActivationId : -1;
		FaceKernel(Pawn, *Bout, Me->Pos, Lio->Pos, StagePitch[Stage]);
		UE_LOG(LogMageArena, Log, TEXT("MAGEVR_AIR grant %s tick=%d release=%d"), StageSpell[Stage], GrantTick,
			Lio->Pending.IsSet() ? Lio->Pending->ReleaseTick : -1);
		Enter(EStep::WaitCast);
		break;
	}
	case EStep::WaitCast:
	{
		if (!Bout)
		{
			Fail(TEXT("session missing"));
			break;
		}
		const FArenaState& State = Bout->GetGames().State;
		const FActor* Lio = FindLio(*Bout);
		const FActor* Me = SimFindActor(State, Bout->GetGames().PlayerId);
		if (!Lio || !Me)
		{
			Fail(TEXT("Lio or the player is missing in a cast"));
			break;
		}
		bool bReady = false;
		FString What;
		if (Stage == 0 || Stage == 3)
		{
			// Halfway through the windup the arc (or the line) is drawn and nothing has flown yet.
			const bool bPending = Lio->Pending.IsSet() && Lio->Pending->ActivationId == GrantActivation;
			const int32 Span = bPending ? Lio->Pending->ReleaseTick - Lio->Pending->StartTick : 0;
			bReady = bPending && State.Tick - Lio->Pending->StartTick >= Span / 2;
			What = FString::Printf(TEXT("windup %d of %d ticks"), bPending ? State.Tick - Lio->Pending->StartTick : -1, Span);
		}
		else if (Stage == 1)
		{
			int32 Orbs = 0;
			for (const FProjectile& Shot : State.Projectiles)
			{
				Orbs += Shot.OwnerId == Lio->Id && Shot.bAir ? 1 : 0;
			}
			bReady = Orbs >= 3;
			What = FString::Printf(TEXT("orbs in flight %d"), Orbs);
		}
		else
		{
			bReady = State.Tick < Lio->Air.FormUntil && State.Tick - (Lio->Air.FormUntil - SimTicks(3.0)) >= 20;
			What = FString::Printf(TEXT("form until %d now %d"), Lio->Air.FormUntil, State.Tick);
		}
		// Nothing of Lio's at the lens: a bolt within 3 m of the seat fills the frame.
		for (const FProjectile& Shot : State.Projectiles)
		{
			if (Stage != 1 && SimDistance(Shot.Pos, Me->Pos) < 3.0)
			{
				bReady = false;
			}
		}
		if (bReady)
		{
			Bout->SetFrameHold(true);
			FaceKernel(Pawn, *Bout, Me->Pos, Lio->Pos, StagePitch[Stage]);
			if (Frames >= 4)
			{
				UE_LOG(LogMageArena, Log, TEXT("MAGEVR_AIR shot %s %s lio=(%.2f, %.2f) player=(%.2f, %.2f) projectiles=%d"), StageSpell[Stage], *What,
					Lio->Pos.X, Lio->Pos.Y, Me->Pos.X, Me->Pos.Y, State.Projectiles.Num());
				RequestShot(StageShot[Stage]);
				Enter(EStep::WaitShot);
			}
			break;
		}
		Frames = 0;
		if (Elapsed > 10.0)
		{
			Fail(*FString::Printf(TEXT("stage %s never reached its frame (%s)"), StageSpell[Stage], *What));
		}
		break;
	}
	case EStep::WaitShot:
		++Stage;
		Enter(Stage < 4 ? EStep::Grant : EStep::Finish);
		break;
	case EStep::Finish:
		if (Bout)
		{
			Bout->SetFrameHold(false);
		}
		if (ShotCount >= 5)
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

TStatId UAirCaptureDriver::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAirCaptureDriver, STATGROUP_Tickables);
}

bool UAirCaptureDriver::IsTickable() const
{
	return bRunning;
}

UWorld* UAirCaptureDriver::GetTickableGameObjectWorld() const
{
	return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}

void UAirCaptureDriver::BeginDestroy()
{
	SetTickableTickType(ETickableTickType::Never);
	Super::BeginDestroy();
}
