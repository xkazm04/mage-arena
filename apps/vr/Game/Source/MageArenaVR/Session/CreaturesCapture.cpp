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
#include "Kernel/SimMath.h"
#include "MageArenaVR.h"
#include "Misc/App.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"

UCreaturesCaptureDriver::UCreaturesCaptureDriver()
{
	SetTickableTickType(ETickableTickType::Never);
}

void UCreaturesCaptureDriver::Start()
{
	RunId = TEXT("T15");
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
	// The first pawn, or none. A for loop that returns on its first pass is an error under clang
	// (-Wunreachable-code-loop-increment, part of UBT's -Wunreachable-code-aggressive).
	TActorIterator<AMageArenaPawn> It(World);
	return It ? *It : nullptr;
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
	if (Step != EStep::Quit && Step != EStep::Finish && Now - RunStartWall > 300.0)
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
		Enter(EStep::WaitDeathBurst);
		break;
	case EStep::WaitDeathBurst:
	{
		bool bDeathBurst = false;
		if (Bout && Bout->GetGames().Phase == TEXT("active"))
		{
			const double StepDT = SimDt(); // kernel ticks are 60 Hz, not the 72 Hz presentation frame
			for (const FTelegraph& T : Bout->GetGames().State.Telegraphs)
			{
				if (T.Family == TEXT("magic") && T.Kind == TEXT("area"))
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
			Bout->SetScripted(false); // Stop player from killing maw
		}
		Enter(EStep::WaitTonguePull);
		break;
	case EStep::WaitTonguePull:
	{
		bool bTongue = false;
		if (Bout && Bout->GetGames().Phase == TEXT("active"))
		{
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
		if (ShotCount >= 4)
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
