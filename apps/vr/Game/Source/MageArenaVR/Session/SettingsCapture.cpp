#include "Session/SettingsCapture.h"

#include "Session/ArenaSession.h"
#include "Session/SessionPresentation.h"

#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Hands/ClipVariant.h"
#include "Hands/HandClipPlayer.h"
#include "Hands/HandInputSubsystem.h"
#include "Hands/MageArenaPawn.h"
#include "Hands/MageSettings.h"
#include "MageArenaVR.h"
#include "Misc/App.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"

USettingsCaptureDriver::USettingsCaptureDriver()
{
	SetTickableTickType(ETickableTickType::Never);
}

void USettingsCaptureDriver::Start()
{
	RunId = TEXT("T14");
	FParse::Value(FCommandLine::Get(), TEXT("MageArenaRun="), RunId);
	const FString Scratch = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArenaTests"), TEXT("capture-settings.json"));
	FMageSettings::SetFileOverride(Scratch);
	FMageSettings::SetHand(EMageHand::Right, false);
	FMageSettings::SetFov(EMageFovMode::Wide, false);
	FMageSettings::SetGentle(false, false);
	bRunning = true;
	RunStartWall = FPlatformTime::Seconds();
	Enter(EStep::WaitStable);
	SetTickableTickType(ETickableTickType::Always);
}

void USettingsCaptureDriver::Enter(EStep Next)
{
	Step = Next;
	StepStartWall = FPlatformTime::Seconds();
	PromptHold = 0;
}

bool USettingsCaptureDriver::PromptSettled(const FString& Prompt, const TCHAR* Expected)
{
	if (!Expected || Prompt != Expected)
	{
		PromptHold = 0;
		return false;
	}
	return ++PromptHold >= 4;
}

UArenaSessionSubsystem* USettingsCaptureDriver::SessionSys() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UArenaSessionSubsystem>() : nullptr;
}

AMageArenaPawn* USettingsCaptureDriver::FindPawn() const
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

FString USettingsCaptureDriver::RepoPath(const TCHAR* Relative) const
{
	FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../../.."), Relative);
	FPaths::CollapseRelativeDirectories(Path);
	return FPaths::ConvertRelativePathToFull(Path);
}

bool USettingsCaptureDriver::RequestShot(const TCHAR* FileName)
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

bool USettingsCaptureDriver::ShotFinished() const
{
	return !PendingShot.IsEmpty() && !FScreenshotRequest::IsScreenshotRequested() && IFileManager::Get().FileSize(*PendingShot) > 0;
}

void USettingsCaptureDriver::Fail(const TCHAR* Reason)
{
	UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CAPTURE_FAIL %s"), Reason);
	Enter(EStep::Quit);
}

void USettingsCaptureDriver::Tick(float DeltaTime)
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

void USettingsCaptureDriver::Advance()
{
	const double Now = FPlatformTime::Seconds();
	const double Elapsed = Now - StepStartWall;
	if (Step != EStep::Quit && Step != EStep::Finish && Now - RunStartWall > 180.0)
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
			if (!Session || !Pawn || !Bout)
			{
				Fail(TEXT("session or pawn missing"));
				break;
			}
			Pawn->ApplyFlatPresentation();
			Pawn->LookAtArena();
			Pawn->SetLookPitch(-20.f);
			const FString SaveDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArenaTests"), TEXT("settings-capture"));
			IFileManager::Get().Delete(*FPaths::Combine(SaveDir, TEXT("bout.txt")), false, true, true);
			Bout->SetSaveDirectory(SaveDir);
			Bout->SetBout(0, false);
			if (!Session->BeginArc(1, false))
			{
				Fail(TEXT("BeginArc failed"));
				break;
			}
			Enter(EStep::WaitToggles);
		}
		break;
	}
	case EStep::WaitToggles:
		if (Bout && Bout->GetStage() == TEXT("cold") && Elapsed >= 0.35
			&& PromptSettled(Bout->GetPromptText(), TEXT("Raise your palm to begin.")))
		{
			RequestShot(TEXT("01-settings-toggles.png"));
			Enter(EStep::WaitTogglesShot);
		}
		else if (Elapsed > 8.0)
		{
			const FString Prompt = Bout ? Bout->GetPromptText() : FString();
			UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CAPTURE_FAIL cold prompt was %s"), *Prompt);
			Fail(TEXT("settings stones did not hold"));
		}
		break;
	case EStep::WaitTogglesShot:
		Enter(EStep::WaitSigil);
		break;
	case EStep::WaitSigil:
	{
		if (!bSigilStarted && Bout && Bout->GetStage() == TEXT("cold"))
		{
			bSigilStarted = true;
			FMageSettings::SetHand(EMageHand::Left, false);
			if (UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
			{
				if (UHandInputSubsystem* Hands = Instance->GetSubsystem<UHandInputSubsystem>())
				{
					Hands->PlayQuickAction(TEXT("sigil-line3"), EClipVariant::Normal);
				}
			}
		}
		UHandClipPlayer* Player = nullptr;
		if (UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
		{
			if (UHandInputSubsystem* Hands = Instance->GetSubsystem<UHandInputSubsystem>())
			{
				Player = Hands->GetClipPlayer();
			}
		}
		const double Time = Player && Player->IsPlaying() ? Player->GetTime() : -1.0;
		if (Time >= 1.66)
		{
			RequestShot(TEXT("02-left-sigil.png"));
			Enter(EStep::WaitSigilShot);
		}
		else if (Elapsed > 8.0)
		{
			UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CAPTURE_FAIL sigil time %.3f"), Time);
			Fail(TEXT("left-hand sigil did not reach the stroke"));
		}
		break;
	}
	case EStep::WaitSigilShot:
		if (UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
		{
			if (UHandInputSubsystem* Hands = Instance->GetSubsystem<UHandInputSubsystem>())
			{
				Hands->StopAll();
			}
		}
		FMageSettings::SetHand(EMageHand::Right, false);
		FMageSettings::SetFov(EMageFovMode::Narrow, false);
		FMageSettings::SetGentle(false, false);
		if (!Bout)
		{
			Fail(TEXT("bout missing before the wave"));
			break;
		}
		Bout->SetBout(0, false);
		Bout->SkipTeach();
		if (Pawn)
		{
			Pawn->LookAtArena();
			Pawn->ApplyFlatPresentation();
		}
		Enter(EStep::WaitWave);
		break;
	case EStep::WaitWave:
	{
		float Fov = 0.f;
		if (Pawn)
		{
			if (UCameraComponent* Camera = Pawn->FindComponentByClass<UCameraComponent>())
			{
				Fov = Camera->FieldOfView;
			}
		}
		const int32 Seen = Session && Session->GetPresentation() ? Session->GetPresentation()->GetEnemiesOnScreen() : 0;
		const bool bSpawned = Bout && Bout->GetGames().SpawnLog.Num() > 0 && Bout->GetGames().Phase == TEXT("active");
		if (bSpawned && Seen >= 1 && FMath::IsNearlyEqual(Fov, FMageSettings::NarrowCameraFovDeg(), 0.5f) && Elapsed >= 0.6)
		{
			UE_LOG(LogMageArena, Log, TEXT("MAGEVR_NARROW_SHOT fov=%.1f enemies=%d spawns=%d"),
				Fov, Seen, Bout->GetGames().SpawnLog.Num());
			if (Pawn)
			{
				Pawn->LookAtArena();
			}
			RequestShot(TEXT("03-narrow-wave1.png"));
			Enter(EStep::WaitWaveShot);
		}
		else if (Elapsed > 20.0)
		{
			UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CAPTURE_FAIL wave fov=%.1f enemies=%d phase=%s"),
				Fov, Seen, Bout ? *Bout->GetGames().Phase : TEXT("none"));
			Fail(TEXT("narrow wave did not show an enemy"));
		}
		break;
	}
	case EStep::WaitWaveShot:
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

TStatId USettingsCaptureDriver::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USettingsCaptureDriver, STATGROUP_Tickables);
}

bool USettingsCaptureDriver::IsTickable() const
{
	return bRunning;
}

UWorld* USettingsCaptureDriver::GetTickableGameObjectWorld() const
{
	return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}

void USettingsCaptureDriver::BeginDestroy()
{
	SetTickableTickType(ETickableTickType::Never);
	Super::BeginDestroy();
}
