#include "Session/StylePickCapture.h"

#include "Session/ArenaSession.h"
#include "Session/SessionPresentation.h"

#include "Camera/CameraComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Hands/MageArenaPawn.h"
#include "MageArenaVR.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

UStylePickCaptureDriver::UStylePickCaptureDriver()
{
}

void UStylePickCaptureDriver::Start()
{
	if (!FParse::Value(FCommandLine::Get(), TEXT("MageArenaStyle="), StyleId))
	{
		StyleId = TEXT("greybox");
	}

	UArenaSessionSubsystem* Sys = SessionSys();
	if (!Sys) return;

	Sys->GetSession().SetScripted(true);
	Sys->GetSession().SetBout(2, true);
	if (!Sys->GetSession().Start(1)) return;
	
	// Create text overlay
	if (UWorld* World = GetTickableGameObjectWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (AMageArenaPawn* Pawn = Cast<AMageArenaPawn>(PC->GetPawn()))
			{
				OverlayText = NewObject<UTextRenderComponent>(Pawn);
				OverlayText->RegisterComponent();
				OverlayText->AttachToComponent(Pawn->FindComponentByClass<UCameraComponent>(), FAttachmentTransformRules::KeepRelativeTransform);
				OverlayText->SetRelativeLocation(FVector(200.0, 0.0, -50.0)); // 2m ahead, slightly down
				OverlayText->SetRelativeRotation(FRotator(0.0, 180.0, 0.0)); // face camera
				OverlayText->SetHorizontalAlignment(EHTA_Center);
				OverlayText->SetTextRenderColor(FColor::White);
				OverlayText->SetWorldSize(15.0f);
			}
		}
	}

	bRunning = true;
	RunStartWall = FPlatformTime::Seconds();
}

void UStylePickCaptureDriver::Tick(float DeltaTime)
{
	if (!bRunning) return;
	if (ShotFinished()) return;

	UArenaSessionSubsystem* Sys = SessionSys();
	if (!Sys) return;

	FArenaSession& Session = Sys->GetSession();

	// Advance the session exactly one kernel tick
	Session.Advance(1.0 / 60.0, true);

	++SequenceIndex;
	RequestShot(*FString::Printf(TEXT("seq/frame_%04d.png"), SequenceIndex), true);

	if (SequenceIndex == 60)
	{
		checkf(Session.GetSimSeconds() > 0.0, TEXT("Simulation time must advance"));
		UE_LOG(LogTemp, Display, TEXT("Sim time after 1s: %f"), Session.GetSimSeconds());
	}

	// Read events for overlay and logging
	const FGames& Games = Session.GetGames();
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

	if (SequenceIndex >= 3600 || Session.GetStage() != TEXT("active"))
	{
		bRunning = false;
		
		// Write the event log
		FString RepoDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../../..")));
		FString OutputPath = FPaths::Combine(RepoDir, FString::Printf(TEXT("runs/T16/events-%s.jsonl"), *StyleId));
		FString LogContent;
		
		// Collect events
		for (int32 i = 0; i < Games.State.Events.Num(); ++i)
		{
			const FArenaEvent& E = Games.State.Events[i];
			FString MappedKind = E.Kind;
			if (E.Kind == TEXT("roll")) MappedKind = TEXT("blink");
			else if (E.Kind == TEXT("down")) MappedKind = TEXT("death");

			if (MappedKind == TEXT("cast") || MappedKind == TEXT("hit") || MappedKind == TEXT("perfect") ||
				MappedKind == TEXT("blink") || MappedKind == TEXT("death"))
			{
				LogContent += FString::Printf(TEXT("{\"tick\":%d,\"simTimeS\":%.3f,\"kind\":\"%s\",\"actorId\":%d,\"targetId\":%d}\n"),
					E.Tick, E.Tick / 60.0, *MappedKind, E.ActorId, E.TargetId.Get(-1));
			}
		}
		
		FFileHelper::SaveStringToFile(LogContent, *OutputPath);

		if (GEngine) GEngine->Exec(GetTickableGameObjectWorld(), TEXT("QUIT_EXEC"));
	}
}

bool UStylePickCaptureDriver::RequestShot(const TCHAR* FileName, bool bSequence)
{
	FString RepoPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../../..")));
	FString Path = FPaths::Combine(RepoPath, FString::Printf(TEXT("runs/T16/shots-%s/%s"), *StyleId, FileName));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	
	FScreenshotRequest::RequestScreenshot(Path, false, false);
	bShotOpen = true;
	PendingShot = Path;
	return true;
}

bool UStylePickCaptureDriver::ShotFinished() const
{
	if (!bShotOpen) return false;
	if (FScreenshotRequest::IsScreenshotRequested()) return true;
	const_cast<UStylePickCaptureDriver*>(this)->bShotOpen = false;
	return false;
}

TStatId UStylePickCaptureDriver::GetStatId() const { return TStatId(); }
bool UStylePickCaptureDriver::IsTickable() const { return bRunning; }
UWorld* UStylePickCaptureDriver::GetTickableGameObjectWorld() const { return GetOuter() ? GetOuter()->GetWorld() : nullptr; }
void UStylePickCaptureDriver::BeginDestroy() { bRunning = false; Super::BeginDestroy(); }

UArenaSessionSubsystem* UStylePickCaptureDriver::SessionSys() const
{
	if (UWorld* World = GetTickableGameObjectWorld())
	{
		return World->GetSubsystem<UArenaSessionSubsystem>();
	}
	return nullptr;
}
