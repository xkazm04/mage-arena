#include "Session/TeachCapture.h"

#include "Session/ArenaSession.h"

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

UTeachCaptureDriver::UTeachCaptureDriver()
{
	SetTickableTickType(ETickableTickType::Never);
}

void UTeachCaptureDriver::Start()
{
	RunId = TEXT("T13");
	FParse::Value(FCommandLine::Get(), TEXT("MageArenaRun="), RunId);
	bRunning = true;
	RunStartWall = FPlatformTime::Seconds();
	Enter(EStep::WaitStable);
	SetTickableTickType(ETickableTickType::Always);
}

void UTeachCaptureDriver::Enter(EStep Next)
{
	Step = Next;
	StepStartWall = FPlatformTime::Seconds();
	PromptHold = 0;
}

bool UTeachCaptureDriver::PromptSettled(const FString& Prompt, const TCHAR* Expected)
{
	if (!Expected || Prompt != Expected)
	{
		PromptHold = 0;
		return false;
	}
	// The dais widget redraws a frame after SetText. Shoot only once that frame has rendered.
	return ++PromptHold >= 4;
}

UArenaSessionSubsystem* UTeachCaptureDriver::SessionSys() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UArenaSessionSubsystem>() : nullptr;
}

AMageArenaPawn* UTeachCaptureDriver::FindPawn() const
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

FString UTeachCaptureDriver::RepoPath(const TCHAR* Relative) const
{
	FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../../.."), Relative);
	FPaths::CollapseRelativeDirectories(Path);
	return FPaths::ConvertRelativePathToFull(Path);
}

bool UTeachCaptureDriver::RequestShot(const TCHAR* FileName)
{
	const FString Path = RepoPath(*FString::Printf(TEXT("runs/%s/shots/%s"), *RunId, FileName));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	FScreenshotRequest::RequestScreenshot(Path, false, false, false);
	PendingShot = Path;
	bShotOpen = true;
	++ShotCount;
	const FString Prompt = SessionSys() ? SessionSys()->GetSession().GetPromptText() : FString();
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_SHOT name=%s prompt=%s path=%s"), FileName, *Prompt, *Path);
	return true;
}

bool UTeachCaptureDriver::ShotFinished() const
{
	return !PendingShot.IsEmpty() && !FScreenshotRequest::IsScreenshotRequested() && IFileManager::Get().FileSize(*PendingShot) > 0;
}

void UTeachCaptureDriver::Fail(const TCHAR* Reason)
{
	UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CAPTURE_FAIL %s"), Reason);
	Enter(EStep::Quit);
}

void UTeachCaptureDriver::Tick(float DeltaTime)
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

void UTeachCaptureDriver::Advance()
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
			if (!Session->BeginArc(1, true))
			{
				Fail(TEXT("BeginArc failed"));
				break;
			}
			Enter(EStep::WaitCold);
		}
		break;
	}
	case EStep::WaitCold:
		if (Bout && Bout->GetStage() == TEXT("cold") && Elapsed >= 0.35
			&& PromptSettled(Bout->GetPromptText(), TEXT("Raise your palm to begin.")))
		{
			Bout->SetFrameHold(true);
			RequestShot(TEXT("01-cold-start.png"));
			Enter(EStep::WaitColdShot);
		}
		else if (Elapsed > 8.0)
		{
			Fail(TEXT("cold start did not hold"));
		}
		break;
	case EStep::WaitColdShot:
		if (Bout)
		{
			Bout->SetFrameHold(false);
		}
		Enter(EStep::WaitPerfect);
		break;
	case EStep::WaitPerfect:
		if (Bout && Bout->IsPerfectRingVisible())
		{
			// Freeze on the first frame the ring is up, then let the widget catch the line.
			if (PromptHold == 0)
			{
				Bout->SetFrameHold(true);
			}
			if (PromptSettled(Bout->GetPromptText(), TEXT("Raise as it arrives.")))
			{
				RequestShot(TEXT("02-ward-perfect.png"));
				Enter(EStep::WaitPerfectShot);
			}
		}
		else if (Elapsed > 45.0)
		{
			Fail(TEXT("perfect ring did not appear"));
		}
		break;
	case EStep::WaitPerfectShot:
		if (Bout)
		{
			Bout->SetFrameHold(false);
		}
		Enter(EStep::WaitSigil);
		break;
	case EStep::WaitSigil:
	{
		bool bOrb = false;
		if (Bout && Bout->GetGlyphId() == TEXT("sigil-line1"))
		{
			const FGames& Games = Bout->GetGames();
			const FActor* Dummy = nullptr;
			for (const FActor& Actor : Games.State.Actors)
			{
				if (Actor.bDummy && !Actor.bDown)
				{
					Dummy = &Actor;
					break;
				}
			}
			if (Dummy)
			{
				for (const FProjectile& Shot : Games.State.Projectiles)
				{
					if (Shot.OwnerId == Games.PlayerId && SimDistance(Shot.Pos, Dummy->Pos) < 2.5)
					{
						bOrb = true;
						break;
					}
				}
			}
		}
		if (bOrb)
		{
			if (PromptHold == 0)
			{
				Bout->SetFrameHold(true);
			}
			if (PromptSettled(Bout->GetPromptText(), TEXT("Circle, then the bar.")))
			{
				RequestShot(TEXT("03-sigil-dummy.png"));
				Enter(EStep::WaitSigilShot);
			}
		}
		else if (Elapsed > 80.0)
		{
			Fail(TEXT("Tide Orb did not reach the dummy"));
		}
		break;
	}
	case EStep::WaitSigilShot:
		if (Bout)
		{
			Bout->SetFrameHold(false);
		}
		Enter(EStep::WaitBlink);
		break;
	case EStep::WaitBlink:
	{
		bool bLane = false;
		if (Bout && Bout->GetTeachStep() == TEXT("blink"))
		{
			for (const FTelegraph& Telegraph : Bout->GetGames().State.Telegraphs)
			{
				if (Telegraph.Kind == TEXT("lane") || Telegraph.Family == TEXT("unblockable"))
				{
					bLane = true;
					break;
				}
			}
		}
		if (bLane && Bout->GetTeachClock() + 1.0e-9 < Bout->GetScheduledImpactS() - Bout->GetTuning().BlinkLeadS)
		{
			if (PromptHold == 0)
			{
				Bout->SetFrameHold(true);
			}
			if (PromptSettled(Bout->GetPromptText(), TEXT("Blink off the pad.")))
			{
				RequestShot(TEXT("04-blink-lane.png"));
				Enter(EStep::WaitBlinkShot);
			}
		}
		else if (Elapsed > 40.0)
		{
			Fail(TEXT("blink lane was not lit"));
		}
		break;
	}
	case EStep::WaitBlinkShot:
		if (Bout)
		{
			Bout->SetFrameHold(false);
		}
		Enter(EStep::WaitPause);
		break;
	case EStep::WaitPause:
		if (Bout && Bout->GetTeachStep() == TEXT("blink-beat"))
		{
			if (PromptHold == 0)
			{
				Bout->NotifyHeadsetRemoved();
			}
			if (PromptSettled(Bout->GetPromptText(), TEXT("Paused")))
			{
				RequestShot(TEXT("05-pause.png"));
				Enter(EStep::WaitPauseShot);
			}
		}
		else if (Elapsed > 20.0)
		{
			Fail(TEXT("teach did not reach the blink beat"));
		}
		break;
	case EStep::WaitPauseShot:
		Enter(EStep::WaitCount);
		break;
	case EStep::WaitCount:
		if (Bout && !bCountClip)
		{
			bCountClip = true;
			if (UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
			{
				if (UHandInputSubsystem* Hands = Instance->GetSubsystem<UHandInputSubsystem>())
				{
					Hands->PlayQuickAction(TEXT("both-palms"), EClipVariant::Normal);
				}
			}
		}
		if (Bout && PromptSettled(Bout->GetPromptText(), TEXT("3")))
		{
			RequestShot(TEXT("06-count.png"));
			Enter(EStep::WaitCountShot);
		}
		else if (Elapsed > 12.0)
		{
			Fail(TEXT("resume count did not start"));
		}
		break;
	case EStep::WaitCountShot:
		Enter(EStep::Finish);
		break;
	case EStep::Finish:
		if (ShotCount >= 6)
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

TStatId UTeachCaptureDriver::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTeachCaptureDriver, STATGROUP_Tickables);
}

bool UTeachCaptureDriver::IsTickable() const
{
	return bRunning;
}

UWorld* UTeachCaptureDriver::GetTickableGameObjectWorld() const
{
	return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}

void UTeachCaptureDriver::BeginDestroy()
{
	SetTickableTickType(ETickableTickType::Never);
	Super::BeginDestroy();
}
