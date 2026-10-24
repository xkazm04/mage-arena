#include "Session/StylePickCapture.h"

#include "Session/ArenaSession.h"

#include "Camera/CameraComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Hands/MageArenaPawn.h"
#include "MageArenaVR.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"

// Staging, stated so the frames are read for what they are (T45):
// - The duel starts through UArenaSessionSubsystem::Start, the path the game uses: the subsystem's Tick then advances
//   the kernel, syncs ASessionPresentation and applies the camera. Before T45 the driver called FArenaSession::Start
//   and stepped the kernel itself, so the subsystem never ticked and nothing drew the duel (T16 Retry 1: "an empty
//   arena, no hands, no Fire mage").
// - Frame N is kernel tick N: the launcher runs -usefixedtimestep -fps=60, so each engine frame advances the kernel by
//   one 1/60 s step. The driver ticks after the subsystem (it registers later) and shoots the frame just synced. A frame
//   whose tick is not the previous frame's plus one is a FAIL.
// - Nothing waits on a screenshot, so the kernel never stalls behind one: each frame's shot is watched by
//   FCaptureEvidence, which fails the run when a shot does not land within its budget or lands under the pixel floor.
// - Proof knobs: -MageArenaStylePickFrames=<n> ends the run after n frames; -MageArenaStylePickBout=<n> starts another
//   bout (an index outside Tiro's waves cannot start).

namespace
{
// Wall-clock budget for the whole run, inside the launcher's 20 minute timeout so the driver FAILs first.
constexpr double RunBudgetS = 1080.0;
}

UStylePickCaptureDriver::UStylePickCaptureDriver()
{
	SetTickableTickType(ETickableTickType::Never);
}

void UStylePickCaptureDriver::Start()
{
	if (!FParse::Value(FCommandLine::Get(), TEXT("MageArenaStyle="), StyleId))
	{
		StyleId = TEXT("greybox");
	}
	FParse::Value(FCommandLine::Get(), TEXT("MageArenaStylePickFrames="), SequenceEnd);
	FParse::Value(FCommandLine::Get(), TEXT("MageArenaStylePickBout="), BoutWave);
	Evidence.Begin(TEXT("StylePick"));
	Evidence.Note(TEXT("style"), StyleId);
	bRunning = true;
	RunStartWall = FPlatformTime::Seconds();
	Enter(EStep::WaitStable);
	SetTickableTickType(ETickableTickType::Always);
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_STYLEPICK start style=%s frames=%d bout=%d"), *StyleId, SequenceEnd, BoutWave);
	if (!SessionSys())
	{
		Fail(TEXT("no session subsystem"));
		return;
	}
	if (!MageArenaCapture::CanCapture())
	{
		Fail(TEXT("no rhi"));
		return;
	}
	if (!FApp::UseFixedTimeStep() || !FMath::IsNearlyEqual(FApp::GetFixedDeltaTime(), 1.0 / 60.0, 1.0e-6))
	{
		Fail(FString::Printf(TEXT("frame N is tick N only under -usefixedtimestep -fps=60 (fixed=%d dt=%.6f)"), FApp::UseFixedTimeStep() ? 1 : 0,
		    FApp::GetFixedDeltaTime()));
	}
}

bool UStylePickCaptureDriver::CheckRunLength(int32 Seq, const FString& Stage)
{
	if (Seq >= MinSequenceFrames)
	{
		return true;
	}
	UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CAPTURE_FAIL short run seq=%d stage=%s"), Seq, *Stage);
	return false;
}

void UStylePickCaptureDriver::Enter(EStep Next)
{
	Step = Next;
	StepStartWall = FPlatformTime::Seconds();
}

void UStylePickCaptureDriver::Fail(const FString& Reason)
{
	UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CAPTURE_FAIL %s"), *Reason);
	Enter(EStep::Quit);
}

void UStylePickCaptureDriver::Tick(float DeltaTime)
{
	(void)DeltaTime;
	if (!bRunning)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	if (Step != EStep::Quit && Now - RunStartWall > RunBudgetS)
	{
		Fail(FString::Printf(TEXT("timeout seq=%d"), SequenceIndex));
	}
	ECaptureShotPoll Shots = ECaptureShotPoll::Landed;
	if (Step == EStep::Running || Step == EStep::WaitShots)
	{
		Shots = Evidence.Poll();
		if (Shots == ECaptureShotPoll::Failed)
		{
			Fail(Evidence.GetFailure());
		}
	}

	switch (Step)
	{
	case EStep::WaitStable:
	{
		UWorld* World = GetWorld();
		AMageArenaPawn* Pawn = nullptr;
		if (World)
		{
			// The first pawn, or none.
			TActorIterator<AMageArenaPawn> It(World);
			Pawn = It ? *It : nullptr;
		}
		const bool bShadersIdle = GShaderCompilingManager == nullptr || !GShaderCompilingManager->IsCompiling();
		StableFrames = (Pawn && Pawn->HasLayout() && bShadersIdle) ? StableFrames + 1 : 0;
		const double Elapsed = Now - StepStartWall;
		if ((StableFrames >= 20 && Elapsed >= 1.5) || Elapsed >= 90.0)
		{
			if (!Pawn)
			{
				Fail(TEXT("pawn missing"));
				break;
			}
			Pawn->ApplyFlatPresentation();
			Pawn->LookAtArena();
			if (GEngine)
			{
				GEngine->Exec(World, TEXT("r.MotionBlurQuality 0"));
			}
			if (UCameraComponent* Camera = Pawn->FindComponentByClass<UCameraComponent>())
			{
				OverlayText = NewObject<UTextRenderComponent>(Pawn);
				OverlayText->RegisterComponent();
				OverlayText->AttachToComponent(Camera, FAttachmentTransformRules::KeepRelativeTransform);
				OverlayText->SetRelativeLocation(FVector(200.0, 0.0, -50.0)); // 2m ahead, slightly down
				OverlayText->SetRelativeRotation(FRotator(0.0, 180.0, 0.0)); // face camera
				OverlayText->SetHorizontalAlignment(EHTA_Center);
				OverlayText->SetTextRenderColor(FColor::White);
				OverlayText->SetWorldSize(15.0f);
			}
			StartDuel();
		}
		break;
	}
	case EStep::Running:
		Record();
		break;
	case EStep::WaitShots:
		if (Shots == ECaptureShotPoll::Landed)
		{
			Finish();
		}
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

void UStylePickCaptureDriver::StartDuel()
{
	UArenaSessionSubsystem* Sys = SessionSys();
	if (!Sys)
	{
		Fail(TEXT("no session subsystem"));
		return;
	}
	Sys->GetSession().SetSaveDirectory(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArenaTests"), TEXT("stylepick-capture")));
	Sys->GetSession().SetBout(BoutWave, true);
	if (!Sys->Start(1, true))
	{
		Fail(FString::Printf(TEXT("session did not start bout=%d"), BoutWave));
		return;
	}
	Enter(EStep::Running);
}

void UStylePickCaptureDriver::Record()
{
	UArenaSessionSubsystem* Sys = SessionSys();
	if (!Sys)
	{
		Fail(TEXT("no session subsystem"));
		return;
	}
	FArenaSession& Session = Sys->GetSession();
	const FGames& Games = Session.GetGames();
	const int32 Tick = Games.State.Tick;
	if (LastFrameTick >= 0 && Tick != LastFrameTick + 1)
	{
		Fail(FString::Printf(TEXT("frame skipped seq=%d tick=%d previous=%d"), SequenceIndex, Tick, LastFrameTick));
		return;
	}
	if (FScreenshotRequest::IsScreenshotRequested())
	{
		Fail(FString::Printf(TEXT("frame skipped seq=%d tick=%d: the previous screenshot request is still open"), SequenceIndex, Tick));
		return;
	}
	LastFrameTick = Tick;
	++SequenceIndex;
	RequestShot(FString::Printf(TEXT("seq/frame_%04d.png"), SequenceIndex));

	if (SequenceIndex == 60)
	{
		UE_LOG(LogMageArena, Log, TEXT("MAGEVR_STYLEPICK sim after 1s: %f"), Session.GetSimSeconds());
		if (Session.GetSimSeconds() <= 0.0)
		{
			Fail(TEXT("sim did not advance"));
			return;
		}
	}

	// Read events for overlay and logging
	for (int32 i = FMath::Max(0, LastTick); i < Games.State.Events.Num(); ++i)
	{
		const FArenaEvent& E = Games.State.Events[i];
		FString MappedKind = E.Kind;
		if (E.Kind == TEXT("roll")) MappedKind = TEXT("blink");
		else if (E.Kind == TEXT("down")) MappedKind = TEXT("death");

		if (MappedKind == TEXT("cast") || MappedKind == TEXT("hit") || MappedKind == TEXT("perfect") ||
			MappedKind == TEXT("blink") || MappedKind == TEXT("death"))
		{
			LastEventKind = MappedKind;
			LastEventActorId = E.ActorId;
		}
	}
	LastTick = Games.State.Events.Num();

	if (OverlayText)
	{
		OverlayText->SetText(FText::FromString(FString::Printf(TEXT("Sim: %.2fs | Last: %s (Actor %d)"), Session.GetSimSeconds(), *LastEventKind, LastEventActorId)));
	}

	if (SequenceIndex >= SequenceEnd || Session.GetStage() != TEXT("active"))
	{
		EndRun(Session.GetStage());
	}
}

void UStylePickCaptureDriver::EndRun(const FString& Stage)
{
	WriteEventLog();
	const UArenaSessionSubsystem* Sys = SessionSys();
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_STYLEPICK end seq=%d tick=%d stage=%s sim=%.3f"), SequenceIndex, LastFrameTick, *Stage,
	    Sys ? Sys->GetSession().GetSimSeconds() : -1.0);
	if (!CheckRunLength(SequenceIndex, Stage))
	{
		Enter(EStep::Quit);
		return;
	}
	Enter(EStep::WaitShots);
}

void UStylePickCaptureDriver::Finish()
{
	FString Error;
	const FString Path = MageArenaCapture::RepoPath(FString::Printf(TEXT("runs/T16/shots-%s/manifest.json"), *StyleId));
	if (!Evidence.WriteManifest(Path, Error))
	{
		Fail(Error);
		return;
	}
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_CAPTURE_DONE shots=%d manifest=%s"), Evidence.GetManifest().Shots.Num(), *Path);
	Enter(EStep::Quit);
}

void UStylePickCaptureDriver::WriteEventLog() const
{
	const UArenaSessionSubsystem* Sys = SessionSys();
	if (!Sys)
	{
		return;
	}
	const FGames& Games = Sys->GetSession().GetGames();
	const FString OutputPath = MageArenaCapture::RepoPath(FString::Printf(TEXT("runs/T16/events-%s.jsonl"), *StyleId));
	FString LogContent;

	// Collect events
	for (int32 i = 0; i < Games.State.Events.Num(); ++i)
	{
		const FArenaEvent& E = Games.State.Events[i];
		FString MappedKind = E.Kind;
		if (E.Kind == TEXT("roll"))
			MappedKind = TEXT("blink");
		else if (E.Kind == TEXT("down"))
			MappedKind = TEXT("death");

		if (MappedKind == TEXT("cast") || MappedKind == TEXT("hit") || MappedKind == TEXT("perfect") ||
		    MappedKind == TEXT("blink") || MappedKind == TEXT("death"))
		{
			LogContent += FString::Printf(TEXT("{\"tick\":%d,\"simTimeS\":%.3f,\"kind\":\"%s\",\"actorId\":%d,\"targetId\":%d}\n"),
			    E.Tick, E.Tick / 60.0, *MappedKind, E.ActorId, E.TargetId.Get(-1));
		}
	}

	FFileHelper::SaveStringToFile(LogContent, *OutputPath);
}

void UStylePickCaptureDriver::RequestShot(const FString& FileName)
{
	const FString Path = MageArenaCapture::RepoPath(FString::Printf(TEXT("runs/T16/shots-%s/%s"), *StyleId, *FileName));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	FScreenshotRequest::RequestScreenshot(Path, false, false, false);
	Evidence.Requested(FileName, Path, SessionSys());
}

TStatId UStylePickCaptureDriver::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UStylePickCaptureDriver, STATGROUP_Tickables);
}

bool UStylePickCaptureDriver::IsTickable() const
{
	return bRunning;
}

UWorld* UStylePickCaptureDriver::GetTickableGameObjectWorld() const
{
	return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}

void UStylePickCaptureDriver::BeginDestroy()
{
	bRunning = false;
	SetTickableTickType(ETickableTickType::Never);
	Super::BeginDestroy();
}

UArenaSessionSubsystem* UStylePickCaptureDriver::SessionSys() const
{
	if (UWorld* World = GetTickableGameObjectWorld())
	{
		return World->GetSubsystem<UArenaSessionSubsystem>();
	}
	return nullptr;
}
