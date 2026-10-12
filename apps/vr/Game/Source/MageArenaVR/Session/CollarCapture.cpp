#include "Session/CollarCapture.h"

#include "Session/ArenaSession.h"
#include "Kernel/ArenaKernel.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Gestures/WardDetector.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Hands/MageArenaPawn.h"
#include "MageArenaVR.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"

// Staging, stated so the stills are read for what they are:
// - The day runs from a save at Bout 2 (bout=1, the teach done) in a capture save directory with no collar ledger (a
//   new collar), through the real offer, ritual, intro and aftermath. The offer and each intermission are continued by
//   a ward-raised event (what a raised palm sends), as MageArena.Session.ChainAdvance does.
// - Bout 2 (creatures) and Bout 3 (Brennic) are fought by the reference script (scripted clips through the gesture
//   pipeline, the live overlay). The live script perfects in Bout 2 (MageArenaDesign.Session.CreaturesSeated: 8) and
//   never in Bout 1, so the Crack still waits for the ledger's first Crack from a real perfect in Bout 2. Bout 2 is then
//   won by staging, and the Tithe still waits in Bout 3 for a real Tithe point once the day's Tithe is at least 10 (or,
//   after 18 s of the bout, any). At each, the kernel is held (SetFrameHold) so the
//   0.35 s seam or the 0.5 s pulse is still on when the frame is taken; the hold is released after the shot.
// - After each still, the bout is won by downing its opponents in the kernel state (Bout 4 after about 2.5 s of the
//   script), as the T21 day capture stages them. The tablet's Bout 2 and 3 rows are the real tallies up to the staging;
//   Bout 4's is its 2.5 s of script; Bout 1 was not fought today (the save resumed at Bout 2).

namespace
{
constexpr double TitheShotAt = 10.0;
constexpr double TitheFallbackS = 18.0;
}

UCollarCaptureDriver::UCollarCaptureDriver()
{
	SetTickableTickType(ETickableTickType::Never);
}

void UCollarCaptureDriver::Start()
{
	FParse::Value(FCommandLine::Get(), TEXT("MageArenaRun="), RunId);
	bRunning = true;
	RunStartWall = FPlatformTime::Seconds();
	Enter(EStep::WaitStable);
	SetTickableTickType(ETickableTickType::Always);
}

void UCollarCaptureDriver::Enter(EStep Next)
{
	Step = Next;
	StepStartWall = FPlatformTime::Seconds();
	Frames = 0;
}

UArenaSessionSubsystem* UCollarCaptureDriver::SessionSys() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UArenaSessionSubsystem>() : nullptr;
}

AMageArenaPawn* UCollarCaptureDriver::FindPawn() const
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

FString UCollarCaptureDriver::RepoPath(const TCHAR* Relative) const
{
	FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../../.."), Relative);
	FPaths::CollapseRelativeDirectories(Path);
	return FPaths::ConvertRelativePathToFull(Path);
}

void UCollarCaptureDriver::RequestShot(const TCHAR* FileName)
{
	const FString Path = RepoPath(*FString::Printf(TEXT("runs/%s/shots/%s"), *RunId, FileName));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	FScreenshotRequest::RequestScreenshot(Path, false, false, false);
	PendingShot = Path;
	bShotOpen = true;
	++ShotCount;
	const UArenaSessionSubsystem* Session = SessionSys();
	if (Session)
	{
		const FArenaSession& Bout = Session->GetSession();
		const FCollarLedger& Collar = Bout.GetCollar();
		UE_LOG(LogMageArena, Log, TEXT("MAGEVR_SHOT name=%s stage=%s t=%.3f boutCracks=%d boutTithe=%d dayTithe=%d lastCrack=%.3f lastTithe=%.3f %s path=%s"),
			FileName, *Bout.GetStage(), Bout.GetSimSeconds(), Collar.GetBoutCracks(), Collar.GetBoutTithe(), Bout.GetDayTithe(),
			Collar.GetLastCrackSim(), Collar.GetLastTitheSim(), *Collar.Breakdown(), *Path);
	}
}

bool UCollarCaptureDriver::ShotFinished() const
{
	return !PendingShot.IsEmpty() && !FScreenshotRequest::IsScreenshotRequested() && IFileManager::Get().FileSize(*PendingShot) > 0;
}

void UCollarCaptureDriver::Fail(const TCHAR* Reason)
{
	UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CAPTURE_FAIL %s"), Reason);
	Enter(EStep::Quit);
}

void UCollarCaptureDriver::RaisePalm()
{
	UWorld* World = GetWorld();
	UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
	if (UWardDetectorSubsystem* Wards = Instance ? Instance->GetSubsystem<UWardDetectorSubsystem>() : nullptr)
	{
		Wards->OnWardRaised.Broadcast(0.0, FVector::ForwardVector);
	}
}

void UCollarCaptureDriver::DownOpponents(FArenaSession& Bout) const
{
	FArenaState& State = const_cast<FArenaState&>(Bout.GetGames().State);
	for (FActor& Actor : State.Actors)
	{
		if (Actor.Id != Bout.GetGames().PlayerId)
		{
			Actor.bDown = true;
		}
	}
	State.Projectiles.Reset();
	State.Telegraphs.Reset();
}

void UCollarCaptureDriver::Tick(float DeltaTime)
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

void UCollarCaptureDriver::Advance()
{
	const double Now = FPlatformTime::Seconds();
	const double Elapsed = Now - StepStartWall;
	if (Step != EStep::Quit && Step != EStep::Finish && Now - RunStartWall > 420.0)
	{
		Fail(TEXT("timeout"));
		return;
	}
	UArenaSessionSubsystem* Session = SessionSys();
	AMageArenaPawn* Pawn = FindPawn();
	FArenaSession* Bout = Session ? &Session->GetSession() : nullptr;
	++Frames;
	if (Bout && Bout->GetStage() != LastStage)
	{
		LastStage = Bout->GetStage();
		UE_LOG(LogMageArena, Log, TEXT("MAGEVR_COLLAR stage=%s bout=%d dayCracks=%d dayTithe=%d"), *LastStage, Bout->GetGames().Wave + 1,
			Bout->GetDayCracks(), Bout->GetDayTithe());
	}

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
			Pawn->SetLookPitch(-12.f);
			if (GEngine)
			{
				GEngine->Exec(GetWorld(), TEXT("r.MotionBlurQuality 0"));
			}
			SaveDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArenaTests"), TEXT("collar-capture"));
			IFileManager::Get().DeleteDirectory(*SaveDir, false, true);
			IFileManager::Get().MakeDirectory(*SaveDir, true);
			FFileHelper::SaveStringToFile(TEXT("bout=1\n"), *FPaths::Combine(SaveDir, TEXT("bout.txt")));
			Bout->SetSaveDirectory(SaveDir);
			if (!Session->BeginArc(1, true))
			{
				Fail(TEXT("BeginArc failed"));
				break;
			}
			Enter(EStep::WaitBout);
		}
		break;
	}
	case EStep::WaitBout:
		if (Bout && Bout->GetStage() == TEXT("offer") && Frames % 20 == 10)
		{
			RaisePalm();
		}
		if (Bout && Bout->GetStage() == TEXT("active"))
		{
			SeenPerfects = Bout->GetCollar().GetBoutCount(ECollarSource::Perfect);
			Enter(EStep::WaitCrack);
		}
		else if (Elapsed > 40.0)
		{
			Fail(TEXT("no bout"));
		}
		break;
	case EStep::WaitCrack:
		if (!Bout || Bout->GetStage() != TEXT("active"))
		{
			Fail(TEXT("Bout 2 ended before a perfect"));
			break;
		}
		if (Bout->GetCollar().GetBoutCount(ECollarSource::Perfect) > SeenPerfects
			&& Bout->GetSimSeconds() - Bout->GetCollar().GetLastCrackSim() < 0.1)
		{
			Bout->SetFrameHold(true);
			UE_LOG(LogMageArena, Log, TEXT("MAGEVR_COLLAR crack held t=%.3f lastCrack=%.3f cracks=%d"), Bout->GetSimSeconds(),
				Bout->GetCollar().GetLastCrackSim(), Bout->GetCollar().GetBoutCracks());
			Enter(EStep::CrackHeld);
		}
		break;
	case EStep::CrackHeld:
		if (Frames >= 4)
		{
			RequestShot(TEXT("01-crack-seam-wrists.png"));
			Enter(EStep::WaitCrackShot);
		}
		break;
	case EStep::WaitCrackShot:
		if (Bout)
		{
			Bout->SetFrameHold(false);
			SeenTitheSim = Bout->GetCollar().GetLastTitheSim();
		}
		if (Pawn)
		{
			Pawn->SetLookPitch(-4.f);
		}
		if (Bout)
		{
			UE_LOG(LogMageArena, Log, TEXT("MAGEVR_COLLAR staging the Bout 2 win from t=%.3f %s"), Bout->GetSimSeconds(), *Bout->GetCollar().Breakdown());
			DownOpponents(*Bout);
		}
		Enter(EStep::ToTitheBout);
		break;
	case EStep::ToTitheBout:
		if (!Bout)
		{
			Fail(TEXT("session missing"));
			break;
		}
		if (Bout->GetStage() == TEXT("active") && Bout->GetGames().Wave == 1)
		{
			DownOpponents(*Bout);
		}
		else if (Bout->GetStage() == TEXT("intermission") && Frames % 20 == 0)
		{
			RaisePalm();
		}
		else if (Bout->GetStage() == TEXT("active") && Bout->GetGames().Wave == 2)
		{
			SeenTitheSim = Bout->GetCollar().GetLastTitheSim();
			Enter(EStep::WaitTithe);
		}
		else if (Bout->GetStage() == TEXT("lost") || Elapsed > 30.0)
		{
			Fail(TEXT("no Bout 3"));
		}
		break;
	case EStep::WaitTithe:
	{
		if (!Bout || Bout->GetStage() != TEXT("active"))
		{
			Fail(TEXT("Bout 3 ended before the Tithe still"));
			break;
		}
		const double LastTithe = Bout->GetCollar().GetLastTitheSim();
		const bool bFresh = LastTithe > SeenTitheSim && Bout->GetSimSeconds() - LastTithe < 0.1;
		SeenTitheSim = FMath::Max(SeenTitheSim, LastTithe);
		const bool bEnough = Bout->GetDayTithe() >= TitheShotAt || (Bout->GetSimSeconds() >= TitheFallbackS && Bout->GetDayTithe() > 0);
		if (bFresh && bEnough)
		{
			Bout->SetFrameHold(true);
			UE_LOG(LogMageArena, Log, TEXT("MAGEVR_COLLAR tithe held t=%.3f dayTithe=%d"), Bout->GetSimSeconds(), Bout->GetDayTithe());
			Enter(EStep::TitheHeld);
		}
		break;
	}
	case EStep::TitheHeld:
		if (Frames >= 4)
		{
			RequestShot(TEXT("02-wardstones-tithe.png"));
			Enter(EStep::WaitTitheShot);
		}
		break;
	case EStep::WaitTitheShot:
		if (Bout)
		{
			Bout->SetFrameHold(false);
			UE_LOG(LogMageArena, Log, TEXT("MAGEVR_COLLAR staging the rest of the day from t=%.3f %s"), Bout->GetSimSeconds(),
				*Bout->GetCollar().Breakdown());
			DownOpponents(*Bout);
		}
		if (Pawn)
		{
			Pawn->SetLookPitch(-12.f);
		}
		FightStartWall = Now;
		Enter(EStep::Day);
		break;
	case EStep::Day:
	{
		if (!Bout)
		{
			Fail(TEXT("session missing in the day"));
			break;
		}
		const FString Stage = Bout->GetStage();
		if (Stage == TEXT("intro"))
		{
			FightStartWall = Now;
		}
		else if (Stage == TEXT("active") && Now - FightStartWall >= 2.5)
		{
			DownOpponents(*Bout);
		}
		else if (Stage == TEXT("intermission") && Frames % 20 == 0)
		{
			RaisePalm();
		}
		else if (Stage == TEXT("aftermath"))
		{
			Enter(EStep::WaitTablet);
		}
		else if (Stage == TEXT("lost"))
		{
			Fail(TEXT("a staged bout was lost"));
		}
		break;
	}
	case EStep::WaitTablet:
		if (Bout && Bout->GetStage() == TEXT("aftermath") && Bout->GetPhaseClock() >= Bout->GetDayTuning().AftermathTabletS + 0.8)
		{
			if (Pawn)
			{
				Pawn->LookAtArena();
				Pawn->SetLookPitch(-8.f);
			}
			if (Frames >= 10)
			{
				UE_LOG(LogMageArena, Log, TEXT("MAGEVR_TABLET \"%s\""), *Bout->GetTabletText().Replace(TEXT("\n"), TEXT(" | ")));
				RequestShot(TEXT("03-aftermath-tablet.png"));
				Enter(EStep::WaitTabletShot);
			}
		}
		else if (Elapsed > 15.0)
		{
			Fail(TEXT("no aftermath closing line"));
		}
		break;
	case EStep::WaitTabletShot:
		Enter(EStep::Finish);
		break;
	case EStep::Finish:
		if (ShotCount >= 3)
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

TStatId UCollarCaptureDriver::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCollarCaptureDriver, STATGROUP_Tickables);
}

bool UCollarCaptureDriver::IsTickable() const
{
	return bRunning;
}

UWorld* UCollarCaptureDriver::GetTickableGameObjectWorld() const
{
	return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}

void UCollarCaptureDriver::BeginDestroy()
{
	SetTickableTickType(ETickableTickType::Never);
	Super::BeginDestroy();
}
