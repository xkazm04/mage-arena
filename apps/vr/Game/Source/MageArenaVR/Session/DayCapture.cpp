#include "Session/DayCapture.h"

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
#include "Hands/ClipVariant.h"
#include "Hands/HandInputSubsystem.h"
#include "Hands/MageArenaPawn.h"
#include "MageArenaVR.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"

// Staging, stated so the stills are read for what they are:
// - The day runs from a save at Bout 1 (bout=0, the teach done) in a capture save directory, through the real offer,
//   ritual, intros, intermissions and aftermath. The offer and each intermission are continued by a ward-raised event
//   (what a raised palm sends), as MageArena.Session.ChainAdvance does.
// - Still 02: the kernel is held (SetFrameHold) on the offer line while the both-palms clip raises the hands, so the
//   0.40 s hold does not end the ritual before the shot; the hold is released after it and the ritual completes on the
//   real pipeline.
// - The bouts are not fought for real: each runs about 2.5 s of wall time with an idle seat (an idle seat loses Bout 1 at 5.48 s), then its opponents are
//   downed in the kernel state (as the T20 capture stages Bout 1). The tablet's times are those staged bouts.

UDayCaptureDriver::UDayCaptureDriver()
{
	SetTickableTickType(ETickableTickType::Never);
}

void UDayCaptureDriver::Start()
{
	FParse::Value(FCommandLine::Get(), TEXT("MageArenaRun="), RunId);
	bRunning = true;
	RunStartWall = FPlatformTime::Seconds();
	Enter(EStep::WaitStable);
	SetTickableTickType(ETickableTickType::Always);
}

void UDayCaptureDriver::Enter(EStep Next)
{
	Step = Next;
	StepStartWall = FPlatformTime::Seconds();
	Frames = 0;
}

UArenaSessionSubsystem* UDayCaptureDriver::SessionSys() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UArenaSessionSubsystem>() : nullptr;
}

UHandInputSubsystem* UDayCaptureDriver::HandsSys() const
{
	UWorld* World = GetWorld();
	UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
	return Instance ? Instance->GetSubsystem<UHandInputSubsystem>() : nullptr;
}

AMageArenaPawn* UDayCaptureDriver::FindPawn() const
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

FString UDayCaptureDriver::RepoPath(const TCHAR* Relative) const
{
	FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../../.."), Relative);
	FPaths::CollapseRelativeDirectories(Path);
	return FPaths::ConvertRelativePathToFull(Path);
}

void UDayCaptureDriver::RequestShot(const TCHAR* FileName)
{
	const FString Path = RepoPath(*FString::Printf(TEXT("runs/%s/shots/%s"), *RunId, FileName));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	FScreenshotRequest::RequestScreenshot(Path, false, false, false);
	PendingShot = Path;
	bShotOpen = true;
	++ShotCount;
	const UArenaSessionSubsystem* Session = SessionSys();
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_SHOT name=%s stage=%s prompt=\"%s\" path=%s"), FileName,
		Session ? *Session->GetSession().GetStage() : TEXT("-"), Session ? *Session->GetSession().GetPromptText() : TEXT("-"), *Path);
}

bool UDayCaptureDriver::ShotFinished() const
{
	return !PendingShot.IsEmpty() && !FScreenshotRequest::IsScreenshotRequested() && IFileManager::Get().FileSize(*PendingShot) > 0;
}

void UDayCaptureDriver::Fail(const TCHAR* Reason)
{
	UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CAPTURE_FAIL %s"), Reason);
	Enter(EStep::Quit);
}

void UDayCaptureDriver::RaisePalm()
{
	UWorld* World = GetWorld();
	UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
	if (UWardDetectorSubsystem* Wards = Instance ? Instance->GetSubsystem<UWardDetectorSubsystem>() : nullptr)
	{
		Wards->OnWardRaised.Broadcast(0.0, FVector::ForwardVector);
	}
}

void UDayCaptureDriver::Tick(float DeltaTime)
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

void UDayCaptureDriver::Advance()
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
	if (Bout && Bout->GetStage() != LastStage)
	{
		LastStage = Bout->GetStage();
		UE_LOG(LogMageArena, Log, TEXT("MAGEVR_DAY stage=%s bout=%d prompt=\"%s\""), *LastStage, Bout->GetGames().Wave + 1, *Bout->GetPromptText());
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
			SaveDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArenaTests"), TEXT("day-capture"));
			IFileManager::Get().DeleteDirectory(*SaveDir, false, true);
			IFileManager::Get().MakeDirectory(*SaveDir, true);
			// A launch after the teach: the offer at Bout 1.
			FFileHelper::SaveStringToFile(TEXT("bout=0\n"), *FPaths::Combine(SaveDir, TEXT("bout.txt")));
			Bout->SetSaveDirectory(SaveDir);
			if (!Session->BeginArc(1, false))
			{
				Fail(TEXT("BeginArc failed"));
				break;
			}
			Enter(EStep::WaitRitual);
		}
		break;
	}
	case EStep::WaitRitual:
		if (Bout && Bout->GetStage() == TEXT("offer") && Frames == 10)
		{
			RaisePalm();
		}
		if (Bout && Bout->GetStage() == TEXT("ritual") && Bout->GetPhaseClock() >= 1.2)
		{
			RequestShot(TEXT("01-ritual-septima.png"));
			Enter(EStep::WaitSeptimaShot);
		}
		else if (Elapsed > 15.0)
		{
			Fail(TEXT("no ritual"));
		}
		break;
	case EStep::WaitSeptimaShot:
		Enter(EStep::WaitOfferLine);
		break;
	case EStep::WaitOfferLine:
		if (Bout && Bout->GetStage() == TEXT("ritual") && Bout->GetPhaseClock() >= Bout->GetDayTuning().RitualLineS + 0.05)
		{
			Bout->SetFrameHold(true);
			if (UHandInputSubsystem* Hands = HandsSys())
			{
				Hands->PlayQuickAction(TEXT("both-palms"), EClipVariant::Normal);
			}
			Enter(EStep::WaitPalmsUp);
		}
		else if (Elapsed > 10.0)
		{
			Fail(TEXT("no offer line"));
		}
		break;
	case EStep::WaitPalmsUp:
		if (Elapsed >= 1.6)
		{
			RequestShot(TEXT("02-ritual-both-palms.png"));
			Enter(EStep::WaitPalmsShot);
		}
		break;
	case EStep::WaitPalmsShot:
		if (Bout)
		{
			Bout->SetFrameHold(false);
		}
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
		const int32 Wave = Bout->GetGames().Wave;
		if (Stage == TEXT("intro") && (Wave == 2 || Wave == 3) && IntroShotWave != Wave && Bout->GetPhaseClock() >= 1.2)
		{
			IntroShotWave = Wave;
			RequestShot(Wave == 2 ? TEXT("03-bout3-intro.png") : TEXT("04-bout4-intro.png"));
			Enter(EStep::WaitIntroShot);
			break;
		}
		if (Stage == TEXT("intro"))
		{
			FightStartWall = Now;
		}
		else if (Stage == TEXT("active") && Now - FightStartWall >= 2.5)
		{
			// Staging: the bout is won by downing its opponents.
			FArenaState& State = const_cast<FArenaState&>(Bout->GetGames().State);
			for (FActor& Actor : State.Actors)
			{
				if (Actor.Id != Bout->GetGames().PlayerId)
				{
					Actor.bDown = true;
				}
			}
			State.Projectiles.Reset();
			State.Telegraphs.Reset();
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
	case EStep::WaitIntroShot:
		Enter(EStep::Day);
		break;
	case EStep::WaitTablet:
		if (Bout && Bout->GetStage() == TEXT("aftermath") && Bout->GetPhaseClock() >= Bout->GetDayTuning().AftermathTabletS + 0.8)
		{
			if (Pawn)
			{
				Pawn->LookAtArena();
				Pawn->SetLookPitch(-8.f);
			}
			UE_LOG(LogMageArena, Log, TEXT("MAGEVR_TABLET \"%s\""), *Bout->GetTabletText().Replace(TEXT("\n"), TEXT(" | ")));
			if (Frames >= 10)
			{
				RequestShot(TEXT("05-aftermath-tablet.png"));
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

TStatId UDayCaptureDriver::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDayCaptureDriver, STATGROUP_Tickables);
}

bool UDayCaptureDriver::IsTickable() const
{
	return bRunning;
}

UWorld* UDayCaptureDriver::GetTickableGameObjectWorld() const
{
	return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}

void UDayCaptureDriver::BeginDestroy()
{
	SetTickableTickType(ETickableTickType::Never);
	Super::BeginDestroy();
}
